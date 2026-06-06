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
#include "Fh5MenuNav.hpp"
#include "Fh5UiScreen.hpp"

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
std::atomic<uint64_t> g_fh5_applied_eye_ms{ 0 };

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
static std::atomic<uint64_t> g_viewWrites{ 0 }, g_projWrites{ 0 };
// Camera pointer-capture diagnostics (defined here so the producer heartbeat can read them).
static std::atomic<uint64_t> g_camdriverCalls{ 0 }, g_bridgeCalls{ 0 };
// Most-recent gameplay-camera near/far (set under the is_main gate) for the menu navigator's scene class.
static std::atomic<float> g_lastNear{ 0.0f }, g_lastFar{ 0.0f };
// Recency timestamps (GetTickCount64 ms) of the last is_main frame in each far bucket. The producer renders
// SEVERAL main cameras per frame (live test: a far~5000 gameplay cam AND a far~50000 intro/cinematic cam
// alternate), so a single last_far flaps. The navigator instead classifies by RECENCY: showcase is "sticky"
// while ANY far>30000 frame is recent (intro running); world3d only once the far>30000 camera fully stops.
static std::atomic<uint64_t> g_lastShowcaseMs{ 0 }, g_lastWorldMs{ 0 };

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

    // Diagnostic: confirms the hook fires and surfaces each camera's near/far without flooding the log.
    static uint64_t s_last_producer_diag_ms = 0;
    const uint64_t producer_diag_now_ms = ::GetTickCount64();
    if (producer_diag_now_ms - s_last_producer_diag_ms >= 5000) {
        s_last_producer_diag_ms = producer_diag_now_ms;
        spdlog::info("[FH5] producer fired: calls={} mainHits={} | near(a8)={:.4f} far(a9)={:.1f} is_main={}",
            calls, g_prodMainHits.load(), a8, a9, is_main);
    }

    // Drive the VR FrameTimeline once per main-camera frame: ENGINE_FRAME_BEGIN advances eye parity,
    // WAIT_RENDER routes into VR::update_hmd_state -> push_stereo_to_adapter -> apply_stereo, which
    // updates g_stereo for THIS frame's eye BEFORE we snapshot it below. No-op until VR wires the
    // timeline callbacks (and they early-out until the OpenXR runtime is ready).
    if (is_main) {
        g_lastNear.store(a8, std::memory_order_relaxed);   // gameplay-camera planes for the nav scene class
        g_lastFar.store(a9, std::memory_order_relaxed);
        const uint64_t nowtick = GetTickCount64();          // recency-bucket the far plane (is_main => far>2000)
        if (a9 > 30000.0f) g_lastShowcaseMs.store(nowtick, std::memory_order_relaxed);
        else               g_lastWorldMs.store(nowtick, std::memory_order_relaxed);
        if (a4 != nullptr) {
            const float* pose_hint = reinterpret_cast<const float*>(a4);
            fh5cam::publish_pose_hint(pose_hint);
#if 0
            // Disabled while stabilizing startup: these are useful for finding the active camera object
            // without a process-wide scan, but probing every producer argument can perturb menu/driver
            // startup on the Empress/NVIDIA path before the gameplay camera is live.
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a1), pose_hint);
            fh5cam::publish_candidate_pointer(static_cast<uintptr_t>(a2), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a4), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a6), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a7), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a10), pose_hint);
            fh5cam::publish_candidate_pointer(static_cast<uintptr_t>(a13), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a14), pose_hint);
            fh5cam::publish_candidate_pointer(static_cast<uintptr_t>(a16), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a17), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a18), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a20), pose_hint);
            fh5cam::publish_candidate_pointer(reinterpret_cast<uintptr_t>(a21), pose_hint);
            fh5cam::publish_candidate_pointer(static_cast<uintptr_t>(a25), pose_hint);
#endif
        }
        const uint64_t hits = g_prodMainHits.fetch_add(1, std::memory_order_relaxed) + 1;
        // The VR frame cadence (eye parity + apply_stereo) is driven ONCE per present from
        // VR::on_post_present — NOT here. The producer fires many times per frame, so reporting the
        // timeline here would scramble the L/R parity (-> both eyes identical). The producer's only job
        // is to APPLY the latched g_stereo below.
        // Log on eye CHANGE (not a modulo, which aliases to one parity) so we can confirm L/R alternates.
        // Throttle this because the producer can fire dozens of times per frame; otherwise it hides the
        // camera-driver hook evidence we need during bring-up.
        static int s_last_logged_eye = -1;
        static uint64_t s_last_prod_log_ms = 0;
        StereoState dbg{}; const bool act = snapshot_stereo(dbg);
        const uint64_t now_ms = ::GetTickCount64();
        if (dbg.eye_idx != s_last_logged_eye && now_ms - s_last_prod_log_ms >= 1000) {
            s_last_logged_eye = dbg.eye_idx;
            s_last_prod_log_ms = now_ms;
            spdlog::info("[FH5] main={} eye={} | view writes={} proj writes={} | cbuf writes={} hits={} cbv6912={} buffers={} | camDriver captures={} bridge captures={}",
                hits, dbg.eye_idx,
                g_viewWrites.load(std::memory_order_relaxed),
                g_projWrites.load(std::memory_order_relaxed),
                fh5cb::ring_writes(),
                fh5cb::cam_hits(),
                fh5cb::cbv6912_count(),
                fh5cb::buffers_tracked(),
                g_camdriverCalls.load(std::memory_order_relaxed),
                g_bridgeCalls.load(std::memory_order_relaxed));
        }
    }

    StereoState s{};
    const bool engaged = is_main && a4 != nullptr && snapshot_stereo(s);
    if (engaged) {
        g_engagedHits.fetch_add(1, std::memory_order_relaxed);
        g_fh5_applied_eye.store(s.eye_idx, std::memory_order_release);   // stamp eye for the D3D12 copy
        g_fh5_applied_eye_ms.store(::GetTickCount64(), std::memory_order_release);
    }

    float saved_view[16];  bool did_view = false;
    float saved_proj[16];  bool did_proj = false;
    float saved_a17[4];    bool did_a17 = false;
    float saved_a18[4];    bool did_a18 = false;
    double saved_w15[3];   double* w15 = nullptr;   // producer a15 f64 world-cameraPos (poslane=proda15)
    double saved_w16[3];   double* w16 = nullptr;   // producer a16 f64 world-cameraPos (prev-frame pair)

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
            g_viewWrites.fetch_add(1, std::memory_order_relaxed);

            // ---- a15/a16: the TRUE upstream world-position lever (poslane=proda15). Empress RE: the producer
            //      reads the camera WORLD position as DOUBLE-precision from the pointers a15(bits)->f64[4] and
            //      a16->f64[4]; it splits those into the camera-relative rebasing origin (cameraPos@+0x80) that
            //      ALL geometry/culling/shadows are rebased against. a4.row3/a17/a18 are inert because they are
            //      post-rebasing. Shift the f64 world pos along the rotated camera basis (Mp rows: right/up/fwd);
            //      restore after the trampoline (engine re-derives the source next frame -> compose-on-top). ----
            if (fh5cb::ctl_pos_lane() == fh5cb::kPosLaneProducerA15) {
                float lx = 0.0f, ly = 0.0f, lz = 0.0f;
                if (fh5cam::current_local_offset(lx, ly, lz)) {
                    const float* b = Mp.m;
                    const double wx = (double)(lx * b[0] + ly * b[4] + lz * b[8]);
                    const double wy = (double)(lx * b[1] + ly * b[5] + lz * b[9]);
                    const double wz = (double)(lx * b[2] + ly * b[6] + lz * b[10]);
                    double* p15 = *reinterpret_cast<double**>(&a15);
                    if (p15 != nullptr) {
                        saved_w15[0] = p15[0]; saved_w15[1] = p15[1]; saved_w15[2] = p15[2];
                        p15[0] += wx; p15[1] += wy; p15[2] += wz;
                        w15 = p15;
                    }
                    double* p16 = reinterpret_cast<double*>(a16);
                    if (p16 != nullptr) {
                        saved_w16[0] = p16[0]; saved_w16[1] = p16[1]; saved_w16[2] = p16[2];
                        p16[0] += wx; p16[1] += wy; p16[2] += wz;
                        w16 = p16;
                    }
                }
            }

            // ---- a7: per-eye asymmetric projection (optional; parallax-only when disabled) ----
            if (s.write_proj && a7 != nullptr) {
                std::memcpy(saved_proj, a7, 64);
                std::memcpy(a7, s.proj, 64);
                did_proj = true;
                g_projWrites.fetch_add(1, std::memory_order_relaxed);
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
    if (w15)      { __try { w15[0]=saved_w15[0]; w15[1]=saved_w15[1]; w15[2]=saved_w15[2]; } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    if (w16)      { __try { w16[0]=saved_w16[0]; w16[1]=saved_w16[1]; w16[2]=saved_w16[2]; } __except (EXCEPTION_EXECUTE_HANDLER) {} }
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
// CCamDriver pose-publisher hook (sub_1406BE3A0, RVA 0x6BE3A0 on older builds). a1 = CCamDriver object;
// a1+0x320 is a row-major camera-to-world matrix that is useful for object identification and old
// diagnostics, but current Empress testing says it is not the authoritative translation input. The hook
// only captures the live object pointer; the current input-lane test is sub_1407A6300 / +0x540.
// ---------------------------------------------------------------------------
using CamDriverFn = __int64(__fastcall*)(__int64, __int64);
static std::unique_ptr<FunctionHook> g_camdriver_hook;

static __int64 __fastcall Hook_CamDriver(__int64 a1, __int64 a2) {
    auto original = g_camdriver_hook->get_original<CamDriverFn>();
    g_camdriverCalls.fetch_add(1, std::memory_order_relaxed);
    if (a1 != 0) {
        fh5cam::publish_driver(static_cast<uintptr_t>(a1));
    }
    return original(a1, a2);
}

// ForzaMultiCam state bridge (sub_140746BB0, RVA 0x746BB0). a1 is the bridge/wrapper object; [a1+0x198]
// is the live ForzaMultiCam object whose +0x5C8/+0x5D0 active-camera shared pointer gives the concrete
// camera object. This is the deterministic replacement for the old process-wide ForzaMultiCam vtable scan.
using BridgeFn = char(__fastcall*)(__int64);
static std::unique_ptr<FunctionHook> g_bridge_hook;

static char __fastcall Hook_Bridge(__int64 a1) {
    auto original = g_bridge_hook->get_original<BridgeFn>();
    g_bridgeCalls.fetch_add(1, std::memory_order_relaxed);
    if (a1 != 0) {
        __try {
            const uintptr_t multicam = *reinterpret_cast<const uintptr_t*>(a1 + 0x198);
            if (multicam != 0) {
                fh5cam::publish_multicam(multicam);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return original(a1);
}

// CCamDriver additive input fold (sub_1407A6300, Empress RVA 0x7A6300). This is the current best
// translation lever from Empress IDA: the function consumes `this+0x540` as a camera-space additive
// offset, derives `+0x550/+0x570`, then folds the result into the final camera matrix.
using Input540FoldFn = void(__fastcall*)(__int64);
static std::unique_ptr<FunctionHook> g_input540_fold_hook;

static void __fastcall Hook_Input540Fold(__int64 self) {
    if (self != 0) {
        fh5cam::on_input540_fold(static_cast<uintptr_t>(self));
    }
    auto original = g_input540_fold_hook->get_original<Input540FoldFn>();
    original(self);
}

// SEH-guarded byte compare (no C++ object unwinding here, so __try is legal — install_hooks holds
// unique_ptrs and cannot use __try directly). Verifies a function prologue before a raw-RVA inline hook.
static bool BytesMatchSEH(uintptr_t addr, const unsigned char* expected, size_t n) {
    if (addr == 0 || expected == nullptr || n == 0) return false;
    __try {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(addr);
        for (size_t i = 0; i < n; ++i) {
            if (p[i] != expected[i]) {
                return false;
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
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

    // CCamDriver pose publisher. Do not use an RVA fallback here: the known 0x6BE3A0 address belongs to a
    // different build than the live Empress 1.405 test exe, and a stale hook target is worse than no hook.
    const uintptr_t cd = memory::FuncRelocation(
        "fh5_ccamdriver_publish",
        "40 53 48 83 EC 20 48 81 C1 20 03 00 00 48 8B DA E8",
        /*fallback RVA*/ 0);
    if (cd != 0) {
        g_camdriver_hook = std::make_unique<FunctionHook>(Address{ cd }, &Hook_CamDriver);
        if (!g_camdriver_hook->create()) { g_camdriver_hook.reset(); spdlog::warn("[FH5] CCamDriver hook create failed"); }
        else spdlog::info("[FH5] CCamDriver pose hook installed @0x{:X}", cd);
    } else {
        spdlog::warn("[FH5] CCamDriver AOB not found");
    }

    // ForzaMultiCam state bridge. Do not use an RVA fallback here either; the bridge AOB is absent from the
    // live Empress 1.405 exe, so a fallback would hook unrelated code.
    const uintptr_t br = memory::FuncRelocation(
        "fh5_forzamulticam_state_bridge",
        "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B B9 88 01 00 00 48 8B D9 8B B1 90 01 00 00 48 C7 44 24 30 00 00 00 00 48 85 FF 74 09 48 8B 07 48 8B CF FF 50 10 44 8B C6 48 89 7C 24 30 48 8D",
        /*fallback RVA*/ 0);
    if (br != 0) {
        g_bridge_hook = std::make_unique<FunctionHook>(Address{ br }, &Hook_Bridge);
        if (!g_bridge_hook->create()) { g_bridge_hook.reset(); spdlog::warn("[FH5] ForzaMultiCam bridge hook create failed"); }
        else spdlog::info("[FH5] ForzaMultiCam bridge hook installed @0x{:X}", br);
    } else {
        spdlog::warn("[FH5] ForzaMultiCam bridge AOB not found");
    }

    // CCamDriver additive-fold sub_1407A6300 @ Empress RVA 0x7A6300 (no ASLR; base 0x140000000). Because
    // this is a raw-RVA target (no FuncRelocation AOB), VERIFY the function prologue before inline-hooking it:
    // a wrong target (build/base drift) would otherwise corrupt arbitrary code and crash. Prologue bytes are
    // from the Empress IDA RE (fh5_empress_camera_input_lanes_RE.md); the unique in-body anchor for scanners is
    // 0F 10 86 40 05 00 00 0F 58 86 30 05 00 00 at function+0x82 (movups xmm0,[r14+540h]; addps [r14+530h]).
    const uintptr_t input540 = memory::module_base() + 0x7A6300;
    static const unsigned char kFoldPrologue[] = {
        0x48,0x89,0x5C,0x24,0x08, 0x48,0x89,0x74,0x24,0x10, 0x48,0x89,0x7C,0x24,0x18,
        0x41,0x56, 0x48,0x83,0xEC,0x50, 0x4C,0x8B,0xF1
    };
    const bool input540_prologue_ok = BytesMatchSEH(input540, kFoldPrologue, sizeof(kFoldPrologue));
    if (input540_prologue_ok) {
        g_input540_fold_hook = std::make_unique<FunctionHook>(Address{ input540 }, &Hook_Input540Fold);
        if (!g_input540_fold_hook->create()) {
            g_input540_fold_hook.reset();
            spdlog::warn("[FH5] CCamDriver +0x540 input-fold hook create failed");
        } else {
            spdlog::info("[FH5] CCamDriver +0x540 input-fold hook installed @0x{:X} (prologue verified)", input540);
        }
    } else {
        spdlog::warn("[FH5] CCamDriver +0x540 input-fold prologue MISMATCH at 0x{:X}; NOT hooking (build/base drift?)",
                     input540);
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
    //   * HEAD TRANSLATION + per-eye IPD — published to the selected position lane. The current Empress
    //                      input-lane candidate is CCamDriver+0x540 (poslane=input540); +0x320 and clone
    //                      lanes are retained as diagnostics.
    //
    // Tracking source = the single HMD pose. The runtime now synthesizes hmd_transform from stage-space
    // xrLocateViews when xrLocateSpace(view, stage) omits translation, so we do not need to derive head
    // motion from per-eye view matrices. Using view[eye] here bakes eye-to-head into Hrel; pure pitch/roll
    // then creates fake translation and the two eyes drift apart.
    const glm::mat4 H = view.hmd_transform;

    // Recenter against the first-frame HMD rest pose so the unmoved head -> identity. The explicit IPD below
    // is the only per-eye difference.
    static glm::mat4 s_rest{ 1.0f };
    static bool s_rest_set = false;
    static int s_last_recenter_seq = 0;
    const int recenter_seq = fh5cb::ctl_recenter_seq();
    const bool do_recenter = !s_rest_set || (recenter_seq > 0 && recenter_seq != s_last_recenter_seq);
    if (do_recenter) {
        s_rest = H;
        s_rest_set = true;
        s_last_recenter_seq = recenter_seq;
        spdlog::info("[FH5POSE] recentered HMD rest pose seq={}", recenter_seq);
    }
    const glm::mat4 Hrel = glm::affineInverse(s_rest) * H;

    // Basis B = diag(1,1,-1): OpenXR (-z fwd) -> FH5 a4 (+z fwd). Conjugate the head rotation by B.
    constexpr glm::mat4 B{ 1.0f,0.0f,0.0f,0.0f,  0.0f,1.0f,0.0f,0.0f,  0.0f,0.0f,-1.0f,0.0f,  0.0f,0.0f,0.0f,1.0f };
    const glm::mat3 R_eng = glm::mat3(B) * glm::mat3(Hrel) * glm::mat3(B);   // head rotation in engine axes

    // Build the relative head rotation in ForzaTech's row-vector camera basis. Rotation stays on the proven
    // producer/driver path; translation/IPD is published as a camera-space vector and consumed by whichever
    // position lane is selected in the control file.
    glm::mat4 Gfix{ R_eng };
    Gfix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    s.ipd_units = 0.0f;
    s.eye_idx = eye;

    // Head translation in engine REST-frame axes (B relabels OpenXR->engine; recenter aligned rest with the
    // engine camera). The CCamDriver pose-writer path consumes metre-scale units; the live scale remains sweepable.
    const float ws = fh5cb::ctl_world_scale();
    const glm::vec3 head_t = glm::mat3(B) * glm::vec3(Hrel[3]) * ws;   // (right, up, forward) in engine units

    // Downstream cbuffer math used the CURRENT head-rotated camera axes. Keep that value only as a diagnostic
    // comparison; the active upstream writer below applies offsets through the source CCamDriver basis.
    const glm::vec3 head_t_local = glm::transpose(R_eng) * head_t;

    // SYMMETRIC stereo: share ONE head pose (rotation + translation) across both eyes, so the ONLY per-eye
    // difference is the symmetric ±IPD. Keep the left-eye latch under AFR so both eyes in the pair consume
    // the same sampled HMD pose.
    static glm::mat4 s_head_delta{ 1.0f };
    static glm::vec3 s_head_off_cbuf{ 0.0f };
    static glm::vec3 s_head_off_driver{ 0.0f };
    static bool s_head_set = false;
    if (do_recenter) {
        s_head_delta = glm::mat4{ 1.0f };
        s_head_off_cbuf = glm::vec3{ 0.0f };
        s_head_off_driver = glm::vec3{ 0.0f };
        s_head_set = false;
    }
    if (view.current_render_eye == StereoView::Eye::LEFT || !s_head_set) {
        s_head_delta = Gfix;
        s_head_off_cbuf = head_t_local;
        s_head_off_driver = head_t;
        s_head_set = true;
    }
    // Half-IPD is stored in metres, like OpenXR head translation. Convert it with the same live scale so
    // `ipd=0.032 scale=100` reproduces the earlier visible ~3.2 FH5-unit stereo offset, while scale=1 keeps
    // true metre-scale diagnostics available.
    const float ipd_sign = (view.current_render_eye == StereoView::Eye::LEFT) ? -1.0f : 1.0f;
    const float half_ipd = ipd_sign * fh5cb::ctl_half_ipd() * ws;
    const glm::vec3 cbuf_off = s_head_off_cbuf + glm::vec3(half_ipd, 0.0f, 0.0f);

    // Rotation path is live-selectable:
    //   rot=driver (default): write head rotation into the upstream CCamDriver pose so shadows/derived
    //                         camera systems see the same pose.
    //   rot=a4:              old bring-up path; rotate only the main producer argument.
    //   rot=off:             diagnostic.
    // Translation/IPD stays on the selected upstream lane unless poslane=downstream/off.
    float head_delta16[16]{};
    std::memcpy(head_delta16, &s_head_delta[0][0], sizeof(head_delta16));
    constexpr float identity16[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    const int requested_rot_mode = fh5cb::ctl_rotation_mode();
    const int pos_lane = fh5cb::ctl_pos_lane();
    // The +0x540 lane is a translation input only. Keep rot=driver as the public default, but route
    // rotation through the known-live producer hook while +0x540 owns upstream translation.
    const int rot_mode =
        ((pos_lane == fh5cb::kPosLaneInput540 || pos_lane == fh5cb::kPosLaneProducerA15) &&
         requested_rot_mode == 2) ? 1 : requested_rot_mode;
    const bool upstream_position =
        pos_lane != fh5cb::kPosLaneDownstream &&
        pos_lane != fh5cb::kPosLaneOff;
    const bool downstream_position = pos_lane == fh5cb::kPosLaneDownstream;
    const float* producer_delta = (rot_mode == 1) ? head_delta16 : identity16;
    const float* driver_delta   = (rot_mode == 2) ? head_delta16 : identity16;
    std::memcpy(s.delta, producer_delta, sizeof(s.delta));
    const glm::vec3 head_right_axis{ head_delta16[0], head_delta16[1], head_delta16[2] };
    const glm::vec3 driver_off_full = s_head_off_driver + head_right_axis * half_ipd;
    const glm::vec3 driver_off = upstream_position ? driver_off_full : glm::vec3{ 0.0f };
    const float delta9[9]{
        driver_delta[0], driver_delta[1], driver_delta[2],
        driver_delta[4], driver_delta[5], driver_delta[6],
        driver_delta[8], driver_delta[9], driver_delta[10],
    };
    fh5cam::publish_openxr_pose(driver_off.x, driver_off.y, driver_off.z, delta9, eye,
                                pos_lane != fh5cb::kPosLaneOff);

    // Downstream is now an explicit positive-control lane. It is not the final architecture, but it tells us
    // whether the current simulator offset would be visible if the final camera cbuffer were patched.
    fh5cb::set_eye_offset(cbuf_off.x, cbuf_off.y, cbuf_off.z, downstream_position);

    {   // ~1/s diagnostic: head delta + per-eye offset
        static std::atomic<uint32_t> dcount{ 0 };
        if ((dcount.fetch_add(1, std::memory_order_relaxed) % 90) == 0) {
            spdlog::info("[FH5POSE] eye={} headT(units)=[{:.3f} {:.3f} {:.3f}] driverOff=[{:.3f} {:.3f} {:.3f}] cbufOff=[{:.3f} {:.3f} {:.3f}] halfIPD={:.3f} rotMode={}->{} proj={} poslane={}",
                eye, head_t.x, head_t.y, head_t.z,
                driver_off.x, driver_off.y, driver_off.z,
                cbuf_off.x, cbuf_off.y, cbuf_off.z, half_ipd,
                requested_rot_mode, rot_mode, fh5cb::ctl_projection_enabled() ? 1 : 0,
                fh5cb::pos_lane_name(pos_lane));
        }
    }

    // --- Per-eye projection (FH5 reverse-Z row-vector layout) -----------------------------------
    // The glm per-eye projection is column-major; ForzaTech's a7 is row-vector reverse-Z. We transpose
    // into a7's layout. Left disabled until calibrated against SimXR (parallax-only is correct meanwhile).
    {
        const glm::mat4 P = glm::transpose(view.projection[eye]);  // -> row-vector layout
        std::memcpy(s.proj, &P[0][0], 64);
        s.write_proj = fh5cb::ctl_projection_enabled();
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
    // Start the menu navigator: publishes live flow state to E:\tmp\fh5_state.txt and injects controller
    // input from E:\tmp\fh5_nav.txt (the in-process, image-free path to free-roam). XInput is also hooked
    // from the dllmain bootstrap; start() retries the hook in case xinput loaded late.
    fh5nav::start();
    // Start the live UI screen detector (read-only heap scan for known UIPage controller vtables) so the
    // navigator gets a real `screen=` (e.g. CopterHud=free-roam HUD, Loading, PauseMenuTiled) — the breadcrumb
    // is crash-only and the active-page dispatcher can't be hooked statically.
    fh5ui::start();
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

// ---------------------------------------------------------------------------
// Producer diagnostics for the menu navigator (Fh5MenuNav reads these to publish flow state).
// ---------------------------------------------------------------------------
namespace fh5diag {
uint64_t producer_calls()     { return g_prodCalls.load(std::memory_order_relaxed); }
uint64_t producer_main_hits() { return g_prodMainHits.load(std::memory_order_relaxed); }
float    last_near()          { return g_lastNear.load(std::memory_order_relaxed); }
float    last_far()           { return g_lastFar.load(std::memory_order_relaxed); }
uint64_t last_showcase_ms()   { return g_lastShowcaseMs.load(std::memory_order_relaxed); }
uint64_t last_world_ms()      { return g_lastWorldMs.load(std::memory_order_relaxed); }
int      applied_eye()        { return g_fh5_applied_eye.load(std::memory_order_acquire); }
uint64_t applied_eye_ms()     { return g_fh5_applied_eye_ms.load(std::memory_order_acquire); }
uint64_t engaged_hits()       { return g_engagedHits.load(std::memory_order_relaxed); }
uint64_t view_writes()        { return g_viewWrites.load(std::memory_order_relaxed); }
uint64_t projection_writes()  { return g_projWrites.load(std::memory_order_relaxed); }
} // namespace fh5diag
