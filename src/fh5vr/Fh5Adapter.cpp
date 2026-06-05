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
#include "Fh5CameraCbuffer.hpp"
#include "Fh5CamDriver.hpp"

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
// CCamDriver pose-hook diagnostics (defined here so the producer heartbeat can read them).
static std::atomic<uint64_t> g_camdriverCalls{ 0 }, g_camdriverApplied{ 0 };

static void __fastcall Hook_Producer(void* a1, __int64 a2, int a3, void* a4, double a5, void* a6,
    void* a7, float a8, float a9, void* a10, int a11, unsigned a12, __int64 a13, void* a14, double a15,
    __int64 a16, void* a17, void* a18, float a19, void* a20, void* a21, unsigned a22, unsigned a23,
    int a24, __int64 a25) {

    auto original = g_producer_hook->get_original<ProducerFn>();
    const uint64_t calls = g_prodCalls.fetch_add(1, std::memory_order_relaxed) + 1;

    // Install the downstream camera-cbuffer hooks AS EARLY AS POSSIBLE (this hook fires during menu render,
    // before the level loads), so the camera upload ring's creation is tracked. Idempotent; stops once the
    // device + command queue are both available and hooked.
    static std::atomic<bool> s_cb_done{ false };
    if (!s_cb_done.load(std::memory_order_relaxed) && g_framework != nullptr) {
        auto& d3d12 = g_framework->get_d3d12_hook();
        if (d3d12 != nullptr && d3d12->get_device() != nullptr) {
            fh5cb::ensure_installed(d3d12->get_device());   // fallback; primary install is the CreateDevice hook
            s_cb_done.store(true, std::memory_order_relaxed);
        }
    }

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
        // Log on eye CHANGE (not a modulo, which aliases to one parity) so we can confirm L/R alternates.
        static int s_last_logged_eye = -1;
        StereoState dbg{}; const bool act = snapshot_stereo(dbg);
        if (dbg.eye_idx != s_last_logged_eye) {
            s_last_logged_eye = dbg.eye_idx;
            spdlog::info("[FH5] main={} eye={} | cbuf writes={} | camDriver calls={} applied={}",
                hits, dbg.eye_idx, fh5cb::ring_writes(),
                g_camdriverCalls.load(std::memory_order_relaxed), g_camdriverApplied.load(std::memory_order_relaxed));
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
    float saved_a17[4];    bool did_a17 = false;
    float saved_a18[4];    bool did_a18 = false;

    if (engaged) {
        __try {
            // ---- a4: insert the per-eye HMD delta in front of the engine's camera-to-world:
            //          new_a4 = Mul(delta, a4). delta is identity at rest, so the unmoved camera is
            //          untouched; rotation + translation ride together with no axis decomposition. ----
            std::memcpy(saved_view, a4, 64);
            Mat4 M{}; std::memcpy(M.m, a4, 64);
            Mat4 D{}; std::memcpy(D.m, s.delta, 64);
            Mat4 Mp = Mul(D, M);
            if (s.ipd_units != 0.0f) {
                const float* b = Mp.m;
                Mp.m[12] += s.ipd_units * b[0];
                Mp.m[13] += s.ipd_units * b[1];
                Mp.m[14] += s.ipd_units * b[2];
            }

            // ---- UPSTREAM camera-translation test: a constant camera-relative offset (control file) applied
            //      to a chosen argument, to find which is the real position lever (with shadows following).
            //      World-space delta along the rotated camera basis (Mp.row0=right, row1=up, row2=forward).
            const int   utgt = fh5cb::ctl_up_tgt();
            if (utgt != 0) {
                const float fwd = fh5cb::ctl_up_fwd(), strafe = fh5cb::ctl_up_strafe(), up = fh5cb::ctl_up_up();
                const float* b = Mp.m;
                const float wx = strafe * b[0] + up * b[4] + fwd * b[8];
                const float wy = strafe * b[1] + up * b[5] + fwd * b[9];
                const float wz = strafe * b[2] + up * b[6] + fwd * b[10];
                if (utgt == 1 || utgt == 4) { Mp.m[12] += wx; Mp.m[13] += wy; Mp.m[14] += wz; }
                if ((utgt == 2 || utgt == 4) && a17 != nullptr) {
                    float* c = reinterpret_cast<float*>(a17);
                    std::memcpy(saved_a17, c, 16); did_a17 = true;
                    c[0] += wx; c[1] += wy; c[2] += wz;
                }
                if ((utgt == 3 || utgt == 4) && a18 != nullptr) {
                    float* c = reinterpret_cast<float*>(a18);
                    std::memcpy(saved_a18, c, 16); did_a18 = true;
                    c[0] += wx; c[1] += wy; c[2] += wz;
                }
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
    if (did_a17)  { __try { std::memcpy(a17, saved_a17, 16); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    if (did_a18)  { __try { std::memcpy(a18, saved_a18, 16); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
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

// ---------------------------------------------------------------------------
// CCamDriver pose-publisher hook (sub_1406BE3A0, RVA 0x6BE3A0). a1 = CCamDriver object; a1+0x320 is the
// row-major camera-to-world matrix (right@m0..2, up@m4..6, fwd@m8..10, POSITION@m12..14) that feeds FH5's
// camera-relative rebasing — the TRUE upstream camera-position lever (the producer's a4/a17/a18 are
// post-rebasing and proven inert). Proven by the standalone freecam (FH5CameraProbe/src/
// CamDriverMatrixFreecamDll.cpp) which moves the rendered camera coherently by writing this exact lane.
// We add a camera-relative offset to the position, recompute the inverse view-tail at +0x3E0, let the
// original publish it, then restore the object so the engine's next frame starts from a clean base.
// ---------------------------------------------------------------------------
using CamDriverFn = __int64(__fastcall*)(__int64, __int64);
static std::unique_ptr<FunctionHook> g_camdriver_hook;

static __int64 __fastcall Hook_CamDriver(__int64 a1, __int64 a2) {
    auto original = g_camdriver_hook->get_original<CamDriverFn>();
    g_camdriverCalls.fetch_add(1, std::memory_order_relaxed);

    float saved320[16]; float saved3E0[16];
    bool did = false; bool did_vt = false;
    if (fh5cb::ctl_up_tgt() == 5 && a1 != 0) {
        __try {
            float* m = reinterpret_cast<float*>(a1 + 0x320);
            auto len3 = [](const float* v) { return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); };
            const float lr = len3(&m[0]), lu = len3(&m[4]), lf = len3(&m[8]);
            const bool ortho = lr > 0.9f && lr < 1.1f && lu > 0.9f && lu < 1.1f && lf > 0.9f && lf < 1.1f
                && std::isfinite(m[12]) && std::isfinite(m[13]) && std::isfinite(m[14]);
            if (ortho) {
                const float fwd = fh5cb::ctl_up_fwd(), strafe = fh5cb::ctl_up_strafe(), up = fh5cb::ctl_up_up();
                std::memcpy(saved320, m, 64);
                // camera-relative offset along the rotated basis (right/up/forward), into the position row.
                m[12] += strafe*m[0] + up*m[4] + fwd*m[8];
                m[13] += strafe*m[1] + up*m[5] + fwd*m[9];
                m[14] += strafe*m[2] + up*m[6] + fwd*m[10];
                did = true;
                // recompute the inverse view-tail at +0x3E0 (world->camera): translation = -dot(pos, axis).
                float* vt = reinterpret_cast<float*>(a1 + 0x3E0);
                if (std::isfinite(vt[12]) && std::isfinite(vt[13]) && std::isfinite(vt[14])) {
                    std::memcpy(saved3E0, vt, 64); did_vt = true;
                    const float px = m[12], py = m[13], pz = m[14];
                    vt[12] = -(px*m[0] + py*m[1] + pz*m[2]);
                    vt[13] = -(px*m[4] + py*m[5] + pz*m[6]);
                    vt[14] = -(px*m[8] + py*m[9] + pz*m[10]);
                }
                g_camdriverApplied.fetch_add(1, std::memory_order_relaxed);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { did = did_vt = false; }
    }

    const __int64 r = original(a1, a2);

    if (did)    { __try { std::memcpy(reinterpret_cast<void*>(a1 + 0x320), saved320, 64); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    if (did_vt) { __try { std::memcpy(reinterpret_cast<void*>(a1 + 0x3E0), saved3E0, 64); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    return r;
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
    const bool ok = g_producer_hook->create();

    // CCamDriver pose publisher (sub_1406BE3A0, RVA 0x6BE3A0): prologue `40 53 48 83 EC 20 48 81 C1 20 03
    // 00 00 48 8B DA E8` (the `48 81 C1 20 03 00 00` = add rcx,0x320 is the unique anchor). Upstream lever.
    const uintptr_t cd = memory::FuncRelocation(
        "fh5_ccamdriver_publish",
        "40 53 48 83 EC 20 48 81 C1 20 03 00 00 48 8B DA E8",
        /*fallback RVA*/ 0x6BE3A0);
    if (cd != 0) {
        g_camdriver_hook = std::make_unique<FunctionHook>(Address{ cd }, &Hook_CamDriver);
        if (!g_camdriver_hook->create()) { g_camdriver_hook.reset(); spdlog::warn("[FH5] CCamDriver hook create failed"); }
        else spdlog::info("[FH5] CCamDriver pose hook installed @0x{:X}", cd);
    } else {
        spdlog::warn("[FH5] CCamDriver AOB not found");
    }

    return ok;
}

void Fh5Adapter::apply_stereo(const StereoView& view) {
    const int eye = (int)view.current_render_eye;

    StereoState s{};
    s.active = true;

    // DECOUPLED stereo (fixes "left eye at a different angle" + 6-DOF translation):
    //   * HEAD ROTATION  — derived from the single HMD pose (hmd_transform), so BOTH eyes get the SAME
    //                      orientation. Applied to a4 in the producer (culling-correct). Using the per-eye
    //                      view[eye] before baked the eye offset + any per-eye canting into a4 -> the eyes
    //                      ended up at different angles. The IPD is a pure lateral offset, NOT a rotation.
    //   * HEAD TRANSLATION + per-eye IPD — applied DOWNSTREAM in view space (Fh5CameraCbuffer), because
    //                      a4.row3 (world camera position) cancels under FH5's camera-relative rendering.
    //
    // Tracking source = view[eye], NOT hmd_transform: SimXR's xrLocateSpace(view,local) does NOT track head
    // TRANSLATION (stays 0), but xrLocateViews (which fills view[eye]) does. The PER-EYE recenter below
    // subtracts the constant eye offset, so Hrel is the pure head delta (rotation + translation) and is the
    // SAME for both eyes (eye_to_head has no rotation) — that's what keeps the two eyes at the same angle.
    const glm::mat4 H = view.view[eye];

    // Recenter against this eye's first-frame rest pose so the unmoved head -> identity AND the constant eye
    // offset cancels. (set_head_pose seats the head at y=1.7; recentering keeps the camera at the engine's
    // normal position at rest.) The explicit IPD below is the ONLY per-eye difference.
    static glm::mat4 s_rest[2]; static bool s_rest_set[2]{ false, false };
    if (!s_rest_set[eye]) { s_rest[eye] = H; s_rest_set[eye] = true; }
    const glm::mat4 Hrel = glm::affineInverse(s_rest[eye]) * H;

    // Basis B = diag(1,1,-1): OpenXR (-z fwd) -> FH5 a4 (+z fwd). Conjugate the head rotation by B.
    constexpr glm::mat4 B{ 1.0f,0.0f,0.0f,0.0f,  0.0f,1.0f,0.0f,0.0f,  0.0f,0.0f,-1.0f,0.0f,  0.0f,0.0f,0.0f,1.0f };
    const glm::mat3 R_eng = glm::mat3(B) * glm::mat3(Hrel) * glm::mat3(B);   // head rotation in engine axes

    // a4 delta = rotation ONLY (no translation upstream — it cancels). Shared across eyes below.
    glm::mat4 Gfix{ R_eng };
    Gfix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    s.ipd_units = 0.0f;
    s.eye_idx = eye;

    // Head translation in engine REST-frame axes (B relabels OpenXR->engine; recenter aligned rest with the
    // engine camera). units/metre comes from the live control file (default 100) so it can be swept.
    const float ws = fh5cb::ctl_world_scale();
    const glm::vec3 head_t = glm::mat3(B) * glm::vec3(Hrel[3]) * ws;   // (right, up, forward) in engine units

    // The downstream hook converts the offset through the CURRENT (head-rotated) camera axes. The IPD must
    // ride those rotated axes (lateral to where you look), but the head translation is in the REST frame, so
    // pre-rotate it by R_eng^T to cancel the producer's rotation -> it lands in the rest/engine frame.
    const glm::vec3 head_t_local = glm::transpose(R_eng) * head_t;

    // SYMMETRIC stereo: share ONE head pose (rotation + translation) across both eyes, computed from the
    // LEFT eye, so the ONLY per-eye difference is the symmetric ±IPD. Deriving the head pose per-eye from
    // view[eye] leaks an asymmetric component (the eye-offset conjugation under head rotation, and any
    // per-eye recenter drift) -> the eyes drifted to different positions. This forces them symmetric.
    static glm::mat4 s_head_delta{ 1.0f }; static glm::vec3 s_head_off{ 0.0f }; static bool s_head_set = false;
    if (view.current_render_eye == StereoView::Eye::LEFT || !s_head_set) {
        s_head_delta = Gfix; s_head_off = head_t_local; s_head_set = true;
    }
    std::memcpy(s.delta, &s_head_delta[0][0], 64);   // shared rotation for the producer (both eyes identical)

    // Half-IPD in FH5 units, LIVE from the control file (sweep 0, 0.032, 0.10, 3.15 to find the right scale
    // without rebuilding). 0 -> both eyes identical (alignment baseline).
    const float ipd_sign = (view.current_render_eye == StereoView::Eye::LEFT) ? -1.0f : 1.0f;
    const float half_ipd = ipd_sign * fh5cb::ctl_half_ipd();
    const glm::vec3 off = s_head_off + glm::vec3(half_ipd, 0.0f, 0.0f);   // shared head + symmetric per-eye IPD
    fh5cb::set_eye_offset(off.x, off.y, off.z, /*active=*/true);

    {   // ~1/s diagnostic: head delta + per-eye offset
        static std::atomic<uint32_t> dcount{ 0 };
        if ((dcount.fetch_add(1, std::memory_order_relaxed) % 90) == 0) {
            spdlog::info("[FH5POSE] eye={} headT(units)=[{:.2f} {:.2f} {:.2f}] off=[{:.2f} {:.2f} {:.2f}] halfIPD={:.2f}",
                eye, head_t.x, head_t.y, head_t.z, off.x, off.y, off.z, half_ipd);
        }
    }

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
    // Start the UPSTREAM camera-pose writer (polls the active CCamDriver+0x320; gated to tgt=driver). This
    // is the proven lever that moves the rendered camera coherently (shadows/culling/chevrons follow).
    fh5cam::start();
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
