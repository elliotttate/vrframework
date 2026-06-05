// FH5 (ForzaTech, D3D12) engine adapter — the Layer-2/3 implementation of IEngineAdapter.
//
// The seam (apply_stereo) writes per-eye view+projection into ForzaTech's per-frame view/projection
// PRODUCER, sub_140BB1EE0 (Empress RVA 0xBB1EE0). That function is the ForzaTech equivalent of Anvil's
// onCalcFinalView + onCalcProjection: a 25-arg __fastcall that derives the frame's VIEW/VP from matrix
// inputs. We inline-hook its prologue and, while VR is active, overwrite:
//
//   a4  = camera-to-world orientation  (orthonormal basis in rows 0-2, camera-relative origin in ROW 3,
//                                        ROW-VECTOR convention — translation lives at m[12..14])
//   a7  = projection                   (reverse-Z; FOV scale at [0][0], near at [3][2])
//
// gated on a8/a9 (near 0.1 / far 50000) so ONLY the gameplay camera is touched — shadow/reflection/UI
// cameras pass through untouched. Culling FOLLOWS the camera because we patch UPSTREAM of all derivation
// (proven live: full 6-DOF freecam, culling-correct). See memory: fh5-upstream-hook-pivot, fh5-view-writer.
//
// Per-eye flow under AFR: the core calls apply_stereo(LEFT) before the engine renders the even frame and
// apply_stereo(RIGHT) before the odd frame. We latch the active eye's delta (HMD rotation + eye offset +
// per-eye projection) into g_stereo; the producer hook applies whatever is latched when it next fires.

#include "Fh5Adapter.hpp"

#include <Framework.hpp>
#include <mods/VR.hpp>
#include <memory/memory_mul.h>
#include <utility/Hooks.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <atomic>
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// Shared stereo state: written by apply_stereo (VR/core thread), read by the
// producer hook (engine render thread). Plain trivially-copyable POD guarded by
// a seqlock-ish generation counter; the hook copies a snapshot under the latch.
// ---------------------------------------------------------------------------
namespace {

struct StereoState {
    // a4 delta — applied to the engine's own camera-to-world basis.
    //   rot      : extra rotation (HMD orientation relative to recenter), camera-space.
    //   offset   : per-eye translation along the engine camera basis {right,up,fwd} (engine units).
    float rot[9]{ 1,0,0, 0,1,0, 0,0,1 };   // row-major 3x3, row-vector (v' = v * rot)
    float offset[3]{ 0, 0, 0 };            // {right, up, forward} in the camera's rotated basis

    // a7 replacement — per-eye projection in FH5 reverse-Z row-vector layout. When write_proj is
    // false we leave the engine projection untouched (parallax-only stereo; safe bring-up mode).
    float proj[16]{};
    bool  write_proj{ false };

    bool  active{ false };  // VR engaged + this eye latched
};

std::atomic<uint32_t> g_gen{ 0 };          // even = stable, odd = writer in progress
StereoState           g_stereo{};          // protected by g_gen

void publish_stereo(const StereoState& s) {
    g_gen.fetch_add(1, std::memory_order_acq_rel);   // -> odd
    g_stereo = s;
    g_gen.fetch_add(1, std::memory_order_release);   // -> even
}

// Lock-free reader: retry until we read across a stable (even, unchanged) generation.
bool snapshot_stereo(StereoState& out) {
    for (int spin = 0; spin < 8; ++spin) {
        uint32_t g0 = g_gen.load(std::memory_order_acquire);
        if (g0 & 1u) continue;                        // writer mid-update
        out = g_stereo;
        uint32_t g1 = g_gen.load(std::memory_order_acquire);
        if (g0 == g1) return out.active;
    }
    return false;
}

// ---- small row-vector matrix helpers (match the proven DxgiProxy math) -----
// 3x3 row-major multiply: r = a * b  (so v*a*b applies a then b under row-vector convention).
inline void mul3(const float a[9], const float b[9], float r[9]) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r[i*3+j] = a[i*3+0]*b[0*3+j] + a[i*3+1]*b[1*3+j] + a[i*3+2]*b[2*3+j];
}

} // namespace

// ---------------------------------------------------------------------------
// The 25-arg producer detour. Lifted from FH5CameraProbe/src/DxgiProxy.cpp (proven live), generalized
// from manual sliders to the per-eye StereoState the core publishes via apply_stereo.
// ---------------------------------------------------------------------------
using ProducerFn = void(__fastcall*)(void*, __int64, int, void*, double, void*, void*, float, float,
    void*, int, unsigned, __int64, void*, double, __int64, void*, void*, float, void*, void*,
    unsigned, unsigned, int, __int64);

static std::unique_ptr<FunctionHook> g_producer_hook;

static void __fastcall Hook_Producer(void* a1, __int64 a2, int a3, void* a4, double a5, void* a6,
    void* a7, float a8, float a9, void* a10, int a11, unsigned a12, __int64 a13, void* a14, double a15,
    __int64 a16, void* a17, void* a18, float a19, void* a20, void* a21, unsigned a22, unsigned a23,
    int a24, __int64 a25) {

    auto original = g_producer_hook->get_original<ProducerFn>();

    // Main-camera identity gate: only the gameplay view (near~0.1, far~50000). Everything else passes through.
    const bool is_main = (a8 > 0.08f && a8 < 0.2f && a9 > 45000.0f && a9 < 55000.0f);

    StereoState s{};
    const bool engaged = is_main && a4 != nullptr && snapshot_stereo(s);

    float saved_view[16];  bool did_view = false;
    float saved_proj[16];  bool did_proj = false;

    if (engaged) {
        __try {
            // ---- a4: rotate the engine basis, then offset the camera-relative origin (row 3) ----
            std::memcpy(saved_view, a4, 64);
            float* M = (float*)a4;          // row-vector: rows 0-2 basis, row 3 origin

            // Rotate the 3x3 basis: Mb' = rot * Mb  (apply HMD delta, then the engine's world basis).
            float Mb[9] = { M[0],M[1],M[2],  M[4],M[5],M[6],  M[8],M[9],M[10] };
            float Rb[9]; mul3(s.rot, Mb, Rb);
            M[0]=Rb[0]; M[1]=Rb[1]; M[2]=Rb[2];
            M[4]=Rb[3]; M[5]=Rb[4]; M[6]=Rb[5];
            M[8]=Rb[6]; M[9]=Rb[7]; M[10]=Rb[8];

            // Per-eye offset along the (rotated) basis rows: right=row0, up=row1, fwd=row2.
            if (s.offset[0] || s.offset[1] || s.offset[2]) {
                M[12] += s.offset[0]*Rb[0] + s.offset[1]*Rb[3] + s.offset[2]*Rb[6];
                M[13] += s.offset[0]*Rb[1] + s.offset[1]*Rb[4] + s.offset[2]*Rb[7];
                M[14] += s.offset[0]*Rb[2] + s.offset[1]*Rb[5] + s.offset[2]*Rb[8];
            }
            did_view = true;

            // ---- a7: per-eye asymmetric projection (optional; parallax-only when disabled) ----
            if (s.write_proj && a7 != nullptr) {
                std::memcpy(saved_proj, a7, 64);
                std::memcpy(a7, s.proj, 64);
                did_proj = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            did_view = did_proj = false;
        }
    }

    original(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19,
        a20, a21, a22, a23, a24, a25);

    // Restore the caller's matrices so other consumers of these inputs are unaffected.
    if (did_view) { __try { std::memcpy(a4, saved_view, 64); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    if (did_proj) { __try { std::memcpy(a7, saved_proj, 64); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
}

// ---------------------------------------------------------------------------
// IEngineAdapter
// ---------------------------------------------------------------------------
EngineCaps Fh5Adapter::capabilities() const {
    EngineCaps caps{};
    caps.graphics     = EngineCaps::Graphics::D3D12;
    caps.submission   = EngineCaps::Submission::AFR;   // left on even frames, right on odd
    caps.has_taa      = true;                          // ForzaTech runs TAA -> history reprojection fix
    caps.left_handed  = false;
    // ForzaTech VIEW basis is already the convention our producer math expects (row-vector camera-to-world),
    // so the engine<->VR basis is identity; per-eye orientation is composed in apply_stereo directly.
    caps.engine_to_vr_basis = glm::mat4{ 1.0f };
    caps.vr_to_engine_basis = glm::mat4{ 1.0f };
    caps.near_plane = 0.1f;
    caps.far_plane  = 50000.0f;
    return caps;
}

bool Fh5Adapter::install_hooks() {
    // sub_140BB1EE0 prologue (Empress RVA 0xBB1EE0). Scan-or-fallback so a single binary serves the build.
    const uintptr_t addr = memory::FuncRelocation(
        "fh5_view_projection_producer",
        "44 89 44 24 18 48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 D9 FF FF B8 E8",
        /*fallback*/ memory::module_base() + 0xBB1EE0);

    if (addr == 0) {
        return false;
    }

    g_producer_hook = std::make_unique<FunctionHook>(Address{ addr }, &Hook_Producer);
    return g_producer_hook->create();
}

void Fh5Adapter::apply_stereo(const StereoView& view) {
    const int eye = (int)view.current_render_eye;

    StereoState s{};
    s.active = true;

    // --- HMD orientation delta (camera-space) ---------------------------------------------------
    // view.view[eye] is world->view (glm, column-major). Its camera-to-world rotation is the transpose
    // of the upper-left 3x3. We feed that as the row-vector 3x3 the producer applies to the engine basis.
    const glm::mat4& V = view.view[eye];
    // Row-vector 3x3 = (column-major upper-left)^T read row-major == the column-major floats themselves.
    // glm column-major memory: V[col][row]; we want rot[i*3+j] = R_cam_to_world row-vector.
    const glm::mat3 R = glm::transpose(glm::mat3(V));        // world->view rotation
    const glm::mat3 Rc = glm::transpose(R);                  // camera->world (== mat3(V) again, kept explicit)
    s.rot[0]=Rc[0][0]; s.rot[1]=Rc[1][0]; s.rot[2]=Rc[2][0];
    s.rot[3]=Rc[0][1]; s.rot[4]=Rc[1][1]; s.rot[5]=Rc[2][1];
    s.rot[6]=Rc[0][2]; s.rot[7]=Rc[1][2]; s.rot[8]=Rc[2][2];

    // --- Per-eye IPD offset ---------------------------------------------------------------------
    // Eye position in head space lives in the inverse view translation. Scale by FH5_IpdScale.
    const glm::mat4 invV = glm::affineInverse(V);
    const glm::vec3 eye_pos_ws = glm::vec3(invV[3]);          // eye position (room space)
    const glm::vec3 head_pos_ws = glm::vec3(view.hmd_transform[3]);
    glm::vec3 eye_off = (eye_pos_ws - head_pos_ws) * m_ipd_scale->value();
    // Express the offset in the engine camera basis {right,up,fwd}. The producer applies offset along
    // its rotated basis rows, so hand it the components directly (right=+x, up=+y, fwd=-z view space).
    s.offset[0] = eye_off.x;
    s.offset[1] = eye_off.y;
    s.offset[2] = -eye_off.z;

    // --- Per-eye projection (FH5 reverse-Z row-vector layout) -----------------------------------
    // The glm per-eye projection is column-major; ForzaTech's a7 is row-vector reverse-Z. We transpose
    // into a7's layout. Left disabled until calibrated against SimXR (parallax-only is correct meanwhile).
    {
        const glm::mat4 P = glm::transpose(view.projection[eye]);  // -> row-vector layout
        std::memcpy(s.proj, &P[0][0], 64);
        s.write_proj = false;   // TODO: enable after SimXR validate_stereo confirms frustum layout/sign
    }

    publish_stereo(s);
    (void)eye;
}

glm::mat4 Fh5Adapter::get_world_camera() const {
    // TODO: read the engine's committed camera-to-world for decoupled pitch / world-to-screen HUD aim.
    return glm::mat4{ 1.0f };
}

void Fh5Adapter::reproject_hud(float scale_x, float scale_y) {
    // TODO: ForzaTech HUD viewport reprojection (later milestone).
    (void)scale_x; (void)scale_y;
}

void Fh5Adapter::disable_incompatible_effects() {
    // TODO: drive FH5_DisableTAA -> engine TAA/letterbox toggle once the cvar/flag is located.
}

std::optional<std::string> Fh5Adapter::on_initialize() {
    return install_hooks() ? std::nullopt
                           : std::optional<std::string>{ "ForzaHorizon5: producer hook install failed" };
}

void Fh5Adapter::on_draw_ui() {
    if (!ImGui::CollapsingHeader(get_name().data())) {
        return;
    }
    m_ipd_scale->draw("IPD Scale");
    m_world_scale->draw("World Scale");
    m_disable_taa->draw("Disable TAA");
}

void Fh5Adapter::on_config_load(const utility::Config& cfg, bool set_defaults) {
    for (IModValue& opt : m_options) opt.config_load(cfg, set_defaults);
}

void Fh5Adapter::on_config_save(utility::Config& cfg) {
    for (IModValue& opt : m_options) opt.config_save(cfg);
}
