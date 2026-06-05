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

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>

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
    // Per-eye HMD delta as a ROW-VECTOR 4x4 (FH5 a4 convention), applied Mp = Mul(delta, a4). Composing
    // rotation+translation as one matrix avoids the euler decomposition that bled pitch into roll. Stored
    // as the raw floats of the glm column-major eye-to-local transform, which IS the row-vector matrix
    // (column-major memory == row-major transpose == row-vector form). Identity at rest.
    float delta[16]{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

    // Signed per-eye IPD offset (FH5 world units). Applied in the producer hook ALONG THE ROTATED RIGHT
    // axis (Mp.row0) AFTER Mul(delta,a4) — exactly like the proven freecam (mr*b[0]). Baking it into the
    // delta row3 (and letting Mul mix it through the rotation) was why parallax cancelled.
    float ipd_units{ 0 };
    int   eye_idx{ 0 };   // 0=LEFT, 1=RIGHT — stamped so the D3D12 copy picks the eye the producer applied

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

// ---- proven DxgiProxy freecam math (4x4, column-vector A; post-multiply Mul(A, M)) --------------
struct Mat4 { float m[16]; };
inline Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0; for (int k = 0; k < 4; ++k) s += a.m[i*4+k] * b.m[k*4+j];
            r.m[i*4+j] = s;
        }
    return r;
}
// A = Rz(roll) * Rx(pitch) * Ry(yaw) — identical to the proven upstream freecam (no transpose).
inline Mat4 BuildA(float p, float y, float r) {
    auto rx = [](float a){ Mat4 m{{1,0,0,0, 0,cosf(a),-sinf(a),0, 0,sinf(a),cosf(a),0, 0,0,0,1}}; return m; };
    auto ry = [](float a){ Mat4 m{{cosf(a),0,sinf(a),0, 0,1,0,0, -sinf(a),0,cosf(a),0, 0,0,0,1}}; return m; };
    auto rz = [](float a){ Mat4 m{{cosf(a),-sinf(a),0,0, sinf(a),cosf(a),0,0, 0,0,1,0, 0,0,0,1}}; return m; };
    return Mul(rz(r), Mul(rx(p), ry(y)));
}

} // namespace

// The eye (0=LEFT,1=RIGHT) the producer last APPLIED to the main camera. D3D12Component reads this to
// copy the engine backbuffer into the matching eye swapchain — so the copied eye is always the eye the
// producer actually rendered, eliminating the parity desync that made both eyes identical.
std::atomic<int> g_fh5_applied_eye{ 0 };

// ---------------------------------------------------------------------------
// The 25-arg producer detour. Lifted from FH5CameraProbe/src/DxgiProxy.cpp (proven live), generalized
// from manual sliders to the per-eye StereoState the core publishes via apply_stereo.
// ---------------------------------------------------------------------------
using ProducerFn = void(__fastcall*)(void*, __int64, int, void*, double, void*, void*, float, float,
    void*, int, unsigned, __int64, void*, double, __int64, void*, void*, float, void*, void*,
    unsigned, unsigned, int, __int64);

static std::unique_ptr<FunctionHook> g_producer_hook;

// Diagnostics: producer call rate, main-camera gate passes, frames we actually injected.
static std::atomic<uint64_t> g_prodCalls{ 0 }, g_prodMainHits{ 0 }, g_engagedHits{ 0 };

static void __fastcall Hook_Producer(void* a1, __int64 a2, int a3, void* a4, double a5, void* a6,
    void* a7, float a8, float a9, void* a10, int a11, unsigned a12, __int64 a13, void* a14, double a15,
    __int64 a16, void* a17, void* a18, float a19, void* a20, void* a21, unsigned a22, unsigned a23,
    int a24, __int64 a25) {

    auto original = g_producer_hook->get_original<ProducerFn>();
    const uint64_t calls = g_prodCalls.fetch_add(1, std::memory_order_relaxed) + 1;

    // Main-camera identity gate. The gameplay view uses near~0.1; shadow/reflection passes use near~0.01
    // and UI uses near~0. The FAR plane varies by scene/draw-distance (seen 5000 in free-roam, 50000 at
    // the showcase intro), so gate on near~0.1 + a generous far floor (>2000) to exclude the near-0.01/
    // far-1000 shadow pass while catching the real camera regardless of its far value.
    const bool is_main = (a8 > 0.06f && a8 < 0.2f && a9 > 2000.0f);

    // Unconditional diagnostic: confirms the hook fires AT ALL and surfaces each camera's near/far so we
    // can see why is_main isn't matching (e.g. the menu/intro camera uses different near/far than gameplay).
    if ((calls % 500) == 1) {
        spdlog::info("[FH5] producer fired: calls={} mainHits={} | near(a8)={:.4f} far(a9)={:.1f} is_main={}",
            calls, g_prodMainHits.load(), a8, a9, is_main);
    }

    // Drive the VR FrameTimeline once per main-camera frame: ENGINE_FRAME_BEGIN advances eye parity,
    // WAIT_RENDER routes into VR::update_hmd_state -> push_stereo_to_adapter -> apply_stereo, which
    // updates g_stereo for THIS frame's eye BEFORE we snapshot it below. No-op until VR wires the
    // timeline callbacks (and they early-out until the OpenXR runtime is ready).
    if (is_main) {
        const uint64_t hits = g_prodMainHits.fetch_add(1, std::memory_order_relaxed) + 1;
        // The VR frame cadence (eye parity + apply_stereo) is driven ONCE per present from
        // VR::on_post_present — NOT here. The producer fires many times per frame, so reporting the
        // timeline here would scramble the L/R parity (-> both eyes identical). The producer's only job
        // is to APPLY the latched g_stereo below.
        if ((hits % 300) == 1) {
            StereoState dbg{}; const bool act = snapshot_stereo(dbg);
            spdlog::info("[FH5] main={} engaged={} active={} | dt=[{:.3f} {:.3f} {:.3f}] dr0=[{:.3f} {:.3f} {:.3f}]",
                hits, g_engagedHits.load(), act, dbg.delta[12], dbg.delta[13], dbg.delta[14],
                dbg.delta[0], dbg.delta[1], dbg.delta[2]);
        }
    }

    StereoState s{};
    const bool engaged = is_main && a4 != nullptr && snapshot_stereo(s);
    if (engaged) {
        g_engagedHits.fetch_add(1, std::memory_order_relaxed);
        g_fh5_applied_eye.store(s.eye_idx, std::memory_order_release);   // stamp eye for the D3D12 copy
    }

    float saved_view[16];  bool did_view = false;
    float saved_proj[16];  bool did_proj = false;

    if (engaged) {
        __try {
            // ---- a4: insert the per-eye HMD delta in front of the engine's camera-to-world:
            //          new_a4 = Mul(delta, a4). delta is identity at rest, so the unmoved camera is
            //          untouched; rotation + translation ride together with no axis decomposition. ----
            std::memcpy(saved_view, a4, 64);
            Mat4 M{}; std::memcpy(M.m, a4, 64);
            Mat4 D{}; std::memcpy(D.m, s.delta, 64);
            Mat4 Mp = Mul(D, M);
            // Per-eye IPD along the FINAL rotated RIGHT axis (Mp.row0), exactly like the proven freecam
            // (Mp.m[12] += mr*b[0]...). Applying it here — NOT baked into the delta — is what keeps the
            // left/right camera offset on the correct world axis so parallax doesn't cancel.
            if (s.ipd_units != 0.0f) {
                const float* b = Mp.m;
                Mp.m[12] += s.ipd_units * b[0];
                Mp.m[13] += s.ipd_units * b[1];
                Mp.m[14] += s.ipd_units * b[2];
            }
            std::memcpy(a4, Mp.m, 64);
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
        /*fallback RVA*/ 0xBB1EE0);   // FuncRelocation adds module_base() itself; pass the RVA, not absolute

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

    // IMPORTANT: StereoView::view[eye] is LABELED world->view but the core actually fills it with the
    // camera-to-world POSE (rotation_offset * hmd * eye_to_head) — see offline movement report. So it is
    // ALREADY the eye-to-local transform; inverting it (as before) flipped translation and bled axes.
    // Use it directly.
    const glm::mat4 G = view.view[eye];

    // Diagnostic (~1/s): pose vs inverse translation so we can confirm which tracks head motion correctly.
    {
        static std::atomic<uint32_t> dcount{ 0 };
        if ((dcount.fetch_add(1, std::memory_order_relaxed) % 90) == 0) {
            const glm::vec3 pose_t = glm::vec3(view.view[eye][3]);
            const glm::vec3 inv_t  = glm::vec3(glm::affineInverse(view.view[eye])[3]);
            const glm::vec3 hmd_t  = glm::vec3(view.hmd_transform[3]);
            spdlog::info("[FH5POSE] eye={} poseT=[{:.3f} {:.3f} {:.3f}] invT=[{:.3f} {:.3f} {:.3f}] hmdT=[{:.3f} {:.3f} {:.3f}]",
                eye, pose_t.x, pose_t.y, pose_t.z, inv_t.x, inv_t.y, inv_t.z, hmd_t.x, hmd_t.y, hmd_t.z);
        }
    }

    // Recenter against the first frame so the unmoved head -> identity delta (set_head_pose seats the
    // head at y=1.7; without this the camera would be lifted/displaced at rest). Grel rides BOTH the
    // rotation and the translation of the head's motion together, so there is no euler axis bleed.
    static glm::mat4 s_rest[2]; static bool s_rest_set[2]{ false, false };
    if (!s_rest_set[eye]) { s_rest[eye] = G; s_rest_set[eye] = true; }
    glm::mat4 Grel = glm::affineInverse(s_rest[eye]) * G;

    // --- Coordinate-basis fix (derived + numerically verified) -----------------------------------
    // OpenXR is (x-right, y-up, -z-forward); FH5's a4 local frame (per the proven freecam BuildA) is
    // (x-right, y-up, +z-forward). The change of basis is B = diag(1,1,-1) (negate forward). B is its
    // own inverse. Conjugating the head rotation by B maps head pitch->camera pitch(r0), yaw->yaw(r1),
    // roll->roll(r2); rotating translation by B maps right/up/forward correctly. We also SPLIT rotation
    // (applied about the head) from translation (recentered, re-added separately) so SimXR's floor-pivot
    // pitch does not swing the camera.
    constexpr glm::mat4 B{ 1.0f,0.0f,0.0f,0.0f,  0.0f,1.0f,0.0f,0.0f,  0.0f,0.0f,-1.0f,0.0f,  0.0f,0.0f,0.0f,1.0f };
    const glm::mat3 R_eng = glm::mat3(B) * glm::mat3(Grel) * glm::mat3(B);   // B^-1 * R * B

    // IPD magnitude is WORLD-SCALE: a4.row3 translation needs large values (the proven freecam needed
    // ~120 units to exit the cockpit), so 0.032 (metres) is sub-millimetre and yields ZERO disparity.
    // FH5_HALF_IPD_UNITS is the half-IPD in FH5 world units; calibrate against SimXR validate_stereo.
    const float ws = m_world_scale->value();
    constexpr float FH5_HALF_IPD_UNITS = 1.5f;
    const float half_ipd = FH5_HALF_IPD_UNITS * m_ipd_scale->value();
    const float ipd_sign = (view.current_render_eye == StereoView::Eye::LEFT) ? -1.0f : 1.0f;
    glm::vec3 t_eng = glm::mat3(B) * glm::vec3(Grel[3]) * ws;   // head translation in engine-local axes
    t_eng.x += ipd_sign * half_ipd;                            // IPD along engine-local right (+X)

    glm::mat4 Gfix{ R_eng };
    Gfix[3] = glm::vec4(t_eng, 1.0f);

    // Store raw column-major floats: this IS the row-vector delta for Mul(delta, a4) (column-major memory
    // == row-major transpose == row-vector form; the translation lands at [12..14] = FH5's a4.row3).
    std::memcpy(s.delta, &Gfix[0][0], 64);

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
    if (!install_hooks()) {
        return std::optional<std::string>{ "ForzaHorizon5: producer hook install failed" };
    }
    // Signal the framework that the engine seam is live. This flips m_engine_ready, which gates
    // is_ready(), the d3d-thread init pass (VR::on_initialize_d3d_thread -> OpenXR bringup) and the
    // per-frame stereo path. Without it the mod hooks the camera but never runs VR. (Framework.cpp:378.)
    if (g_framework != nullptr) {
        g_framework->enable_engine_thread();
        spdlog::info("[FH5] engine seam live -> enable_engine_thread()");
    }
    return std::nullopt;
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
