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
#include <cstdio>
#include <cstring>
#include <string>

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

bool producer_stack_probe_enabled() {
    static const bool enabled = [] {
        char value[16]{};
        const DWORD n = ::GetEnvironmentVariableA("FH5_PRODUCER_STACK_PROBE", value, sizeof(value));
        return n > 0 && value[0] != '0' && value[0] != 'f' && value[0] != 'F';
    }();
    return enabled;
}

void maybe_log_producer_stack(uint64_t main_hits, float near_plane, float far_plane) {
    if (!producer_stack_probe_enabled()) return;

    static std::atomic<uint32_t> s_emitted{ 0 };
    static std::atomic<uint64_t> s_last_ms{ 0 };

    const uint32_t emitted = s_emitted.load(std::memory_order_relaxed);
    if (emitted >= 24) return;

    const uint64_t now = ::GetTickCount64();
    uint64_t last = s_last_ms.load(std::memory_order_relaxed);
    if (now - last < 1000) return;
    if (!s_last_ms.compare_exchange_strong(last, now, std::memory_order_relaxed)) return;

    const uint32_t index = s_emitted.fetch_add(1, std::memory_order_relaxed);
    if (index >= 24) return;

    void* frames[32]{};
    const USHORT count = ::CaptureStackBackTrace(
        0, static_cast<DWORD>(sizeof(frames) / sizeof(frames[0])), frames, nullptr);
    const uintptr_t base = memory::module_base();
    const uintptr_t end = base + memory::module_size();

    std::string trace;
    trace.reserve(512);
    for (USHORT i = 0; i < count; ++i) {
        const uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);
        char part[64]{};
        if (base != 0 && addr >= base && addr < end) {
            std::snprintf(part, sizeof(part), "%s+0x%llX", trace.empty() ? "" : " ",
                static_cast<unsigned long long>(addr - base));
        } else {
            std::snprintf(part, sizeof(part), "%s%p", trace.empty() ? "" : " ", frames[i]);
        }
        trace += part;
    }

    spdlog::info("[FH5] producer stack probe #{} mainHits={} near={:.4f} far={:.1f} frames={}",
        index + 1, main_hits, near_plane, far_plane, trace);
}

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
            // NOTE: the old [FH5A4]/[FH5A6] diagnostic here dereferenced a15 (bits-as-pointer) and a6 inside a
            // __try — but NVIDIA's overlay VEH turns any first-chance AV fatal BEFORE our __except runs (see
            // [[fh5-nvidia-veh-crash]]). At camera transitions a15/a6 are transiently non-pointer, so that
            // deref was a residual crash source. Removed (its findings: a4.row2 == +0x320 fwd; a6 is a scaled
            // non-orthonormal transform; view ~ transpose(a4) + a4*a6).
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
        maybe_log_producer_stack(hits, a8, a9);
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
            // proda15 AND camsrc both shift the producer a15/a16 f64 cameraPos (moves view + instance-cull).
            // camsrc ADDITIONALLY shifts cam+0x320 row3 in the pose-writer hook so the shadow cascades follow.
            if (fh5cb::ctl_pos_lane() == fh5cb::kPosLaneProducerA15 ||
                fh5cb::ctl_pos_lane() == fh5cb::kPosLaneCamSrc) {
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
        // SHADOW-COHERENT head rotation: rotate the UPSTREAM CCamDriver +0x320 camera-to-world basis BEFORE
        // this publisher copies the pose downstream. The shadow-cascade fitter reads this same +0x320 pose
        // (NOT the producer's a4 — see fh5_empress_shadow_cascade_RE.md), so rotating it here makes the main
        // view AND the cascades/culling follow head-look. Gated to poslane=proda15; the producer a4 rotation
        // is left at identity in that mode (apply_stereo). Translation stays on the producer a15/a16 f64 lever.
        fh5cam::apply_camdriver_head_rotation(static_cast<uintptr_t>(a1));
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
    // NOTE: the camera-orientation probe (dumpcam/pokerot) is driven from the Fh5CamDriver WORKER, not here,
    // because this fold only fires while the camera is actively updating (quiet when parked). The worker
    // resolves the active camera every loop, so it can scan/rotate even in the parked showcase test scene.
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

// Camera pose getter sub_1407A9DD0 (Empress RVA 0x7A9DD0): rebuilds the look-at basis from CCamDriver+0x540
// every frame (feeds the producer a4 AND the shadow-cascade fit). We rotate the +0x540 look-direction on
// ENTRY (before it is consumed) so head-look turns the whole frustum — shadow-coherent. a1(rcx)=CCamDriver.
using AimGetterFn = char(__fastcall*)(void*, __int64, unsigned char);
static std::unique_ptr<FunctionHook> g_aim_getter_hook;

static char __fastcall Hook_AimGetter(void* a1, __int64 a2, unsigned char a3) {
    // SHADOW-COHERENT head-look — LANE A (the real lever). RE (fh5_empress_upstream_pose_writer_RE.md)
    // proved the producer's a4 AND the shadow cascades derive from the CMultiCam world basis at S+0x650
    // (S=*(camera+0x48)), transpose-gathered into S+0x600 — NOT the +0x320 "Lane B" renderer snapshot the
    // old freecam poked (which is why rotating +0x320 post-getter never moved the producer view). The
    // world-basis producer sub_14060B390 rebuilds S+0x650 each frame BEFORE this getter; we rotate it (and
    // re-sync the S+0x600 gather row the builder returns) HERE, BEFORE the getter copies the basis into a4
    // (+0x550), so a4 AND the cascade fit both follow head-look. Anti-accumulated (compose-on-top of the
    // freshly-rebuilt base); gated to poslane=proda15 + a live CMultiCam view-source with an orthonormal
    // basis. Producer a4 stays identity in this mode (rot=driver -> rot_mode 2 in apply_stereo).
    // NOTE: rotating +0x320 / S+0x650 HERE is TOO LATE — the producer a4 is copied from +0x320's BASE value
    // before this getter fires (proven: [FH5A4] a4==+0x320 baseFwd, not the rotated outFwd). The rotation now
    // lives in the per-frame +0x320 WRITER hook (sub_1407A1AC0).
    //
    // LANE-A DISABLED (2026-06-06): the report's Lane A (a4 <- +0x550 <- S+0x600 <- S+0x650) is a RUNTIME
    // NO-OP on this build. Live probe of EVERY camera class (cockpit/hood/bumper/chase) showed +0x550 and
    // S+0x650 are junk while +0x320 holds the orthonormal basis (= producer a4); [FH5MCROT] fired 0 times.
    // So rotate_multicam_basis never found a clean basis — it only ever READ S+0x650 (an AV risk: while
    // driving, S churns and S+0x650 (0x650 bytes into a transient object) can be unmapped -> first-chance AV
    // -> NVIDIA overlay VEH stack overflow, see [[fh5-nvidia-veh-crash]]). Removed the call entirely. The live
    // rotation path is rot=a4 (producer hook rotates the a4 arg directly — clean view, world-anchored shadows).
    auto original = g_aim_getter_hook->get_original<AimGetterFn>();
    return original(a1, a2, a3);
}

// a4-assembler sub_1407AC2D0 (RVA 0x7AC2D0, CCamDriver vtable slot 0x370): per-frame, this=CCamDriver,
// this+0x48 = CMultiCam view-source S; it builds a4 rows this+0x550 from S->vtbl[104] (= *(S+0x600)). The
// getter sub_1407A9DD0 fires for a non-gameplay AGGREGATE camera (its +0x48 is not a CMultiCam) on this
// build, so the a4-assembler is the path that produces the GAMEPLAY camera's a4. Rotate the Lane-A source
// (S+0x650 / S+0x600) PRE-trampoline here too, so the gameplay a4 + cascades follow head-look.
using A4AsmFn = void*(__fastcall*)(void*, void*, void*, void*);
static std::unique_ptr<FunctionHook> g_a4asm_hook;
static void* __fastcall Hook_A4Assembler(void* a1, void* a2, void* a3, void* a4) {
    // LANE-A DISABLED (2026-06-06): same as the getter — Lane A (+0x550/S+0x650) is a runtime no-op (a4 is
    // Lane B / +0x320 on every camera live; [FH5MCROT]=0), and reading S+0x650 while driving AV'd. Removed.
    auto original = g_a4asm_hook->get_original<A4AsmFn>();
    return original(a1, a2, a3, a4);
}

// Per-frame +0x320 WRITER sub_1407A2726 (RVA 0x7A2726): the camera function that contains the
// `movaps [rbp+0x320]` store (0x7A2C2B) — a candidate per-frame writer of the camera-to-world basis the
// producer a4 + shadow cascades derive from. Rotate the RESOLVED active camera's +0x320 POST-original
// (i.e. right after this writes the base pose), BEFORE the render-view pass copies it into a4 / fits the
// cascades. If a4 then follows ([FH5A4] forward == rotated outFwd), this is the upstream lever.
using PoseWriterFn = void*(__fastcall*)(void*, void*, void*, void*);
static std::unique_ptr<FunctionHook> g_posewriter_hook;
static std::atomic<uint64_t> g_posewriter_calls{ 0 };
static void* __fastcall Hook_PoseWriter(void* a1, void* a2, void* a3, void* a4) {
    auto original = g_posewriter_hook->get_original<PoseWriterFn>();
    // SHADOW-COHERENT head-look (rot=angle / mode 3) — the CAMERA_VR_FIX_GUIDE convergence-point fix. a1
    // (rcx/rbx) IS the rendered CAMERA (raw disasm of sub_1407A1AC0 @0x7A1AC0). The original reads the
    // camera's orientation Euler angles cam+0x90/94/98 (RADIANS) and rebuilds cam+0x320 (the 4x4
    // cam-to-world) from them via Rodrigues. We add the head Euler to +0x90/94/98 HERE, PRE-original, so the
    // original rebuilds +0x320 ALREADY head-rotated. The engine then copies +0x320 into the producer a4 (the
    // view) AND the render-thread cull/shadow cascade fit reads the SAME orientation (NOT +0x320) — so view,
    // culling, and shadows all rotate together from this single upstream injection (unlike the OLD +0x320
    // matrix post-rotate, which moved the view but the cull/cascade fit read an upstream snapshot -> washout).
    // Self-gated inside the call (mode 3 + poslane=proda15 + fresh VR pose); no-op in modes 0/1/2 and at rest.
    bool angle_injected = false;
    if (a1 != nullptr) {
        angle_injected = fh5cam::apply_angle_head_rotation_prewrite(reinterpret_cast<uintptr_t>(a1));
    }
    void* r = original(a1, a2, a3, a4);
    // SHADOW-COHERENT 6DOF translation (poslane=camsrc): POST-original, shift the camera world position in
    // cam+0x320 ROW 3 along the (head-rotated) basis. The original just wrote row3 from its f64 accumulators;
    // the shadow-cascade fit reads cam+0x320 live on the render thread, so shifting row3 here rebases the
    // cascades coherently — while the producer a15/a16 f64 shift (Hook_Producer, also gated to camsrc) moves
    // the view + instance-cull by the SAME world delta. No-op outside poslane=camsrc.
    bool camsrc_shifted = false;
    if (a1 != nullptr) {
        camsrc_shifted = fh5cam::apply_camsrc_translation_postwrite(reinterpret_cast<uintptr_t>(a1));
    }
    // Capture the live camera (a1) for the worker — replaces the crash-prone memory scan. AV-safe: a1 was
    // just dereferenced by the original writer, and capture_active_camera validates (now against the freshly
    // written, head-rotated +0x320, which stays orthonormal).
    if (a1 != nullptr) {
        fh5cam::capture_active_camera(reinterpret_cast<uintptr_t>(a1));
    }
    (void)camsrc_shifted;
    const uint64_t n = g_posewriter_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    {   // ~1/s
        static uint64_t s_last = 0;
        const uint64_t now = ::GetTickCount64();
        if (now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5POSEWR] calls={} a1=0x{:X} angleInj={} (camera captured; no scan)",
                         n, reinterpret_cast<uintptr_t>(a1), angle_injected ? 1 : 0);
        }
    }
    return r;
}

// UPSTREAM head-look injection: fh5_cam_LerpCameraStateStruct (RVA 0xC7F270), the per-frame camera-state
// interpolator. Signature __fastcall(a1 = dest CAMERA, a2 = source state, a3 = lerp factor). It lerps the
// camera's Euler angle triple into a1+0x90/0x94/0x98 from a2; sub_1407A1AC0 then reads a1+0x90 to build
// a1+0x320 (-> producer a4). We add the head Euler POST-original (a1+0x90 = a2+0x90 + headYaw, etc.), which
// is upstream of the +0x320 build AND — if the cull/cascade fit snapshots after this lerp — the cull too.
using LerpCamFn = __int64(__fastcall*)(__int64, __int64, float);
static std::unique_ptr<FunctionHook> g_lerpcam_hook;
static __int64 __fastcall Hook_LerpCameraState(__int64 a1, __int64 a2, float a3) {
    // INJECTION DISABLED: this lerp's a1 is a camera STATE STRUCT whose +0x90 holds non-angle data
    // (observed base ~500/100/100), and it's transition-only. The real per-frame angle setter is
    // sub_140DC9770 (Hook_FollowCamAngles). Keep this hook installed for diagnostics only.
    auto original = g_lerpcam_hook->get_original<LerpCamFn>();
    return original(a1, a2, a3);
}

// PER-FRAME camera-follow orientation setter sub_140DC9770 (RVA 0xDC9770; user RE 2026-06-06). a1(rcx) =
// CAMERA; writes camera+0x90/0x94/0x98 (Euler angle triple) EVERY frame from its a3 input. This is upstream
// of sub_1407A1AC0 (reads +0x90 -> builds +0x320 -> producer a4) AND the cull/cascade fit — the candidate
// single point that makes view + culling + shadows all follow head-look. We add the head Euler POST-original
// (base = the freshly-written cam+0x90, so apply_lerp_angle_head_rotation(a1,a1) = base + headEuler); the
// setter rewrites the base each frame, so no accumulation. Call-counter confirms it's steady-state (climbs
// every frame like [FH5POSEWR]) vs transition-only (stays ~0).
using FollowCamFn = void*(__fastcall*)(void*, void*, void*, void*);
static std::unique_ptr<FunctionHook> g_followcam_hook;
static std::atomic<uint64_t> g_followcam_calls{ 0 };
static void* __fastcall Hook_FollowCamAngles(void* a1, void* a2, void* a3, void* a4) {
    auto original = g_followcam_hook->get_original<FollowCamFn>();
    void* r = original(a1, a2, a3, a4);
    const uint64_t n = g_followcam_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    // INJECTION DISABLED (2026-06-06): sub_140DC9770 is the RIG/template camera (base angles 96/58/44, NOT
    // the rendered camera's radians — confirmed [FH5LERP]). Harmless no-op while the published head delta was
    // identity (rot=a4), but under rot=angle (mode 3) the delta is non-identity, so apply_lerp_angle_head_rotation
    // started READING+WRITING this transient rig object every off-center frame — a first-chance-AV risk that
    // NVIDIA's overlay VEH escalates to a crash ([[fh5-nvidia-veh-crash]]). The rendered camera is handled by
    // apply_angle_head_rotation_prewrite in the sub_1407A1AC0 hook; this decoy is no longer needed.
    const bool applied = false;
    {   // ~1/s: confirm steady-state (calls climb) + injection applied
        static uint64_t s_last = 0;
        const uint64_t now = ::GetTickCount64();
        if (now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5FOLLOWCAM] calls={} a1=0x{:X} applied={}", n, reinterpret_cast<uintptr_t>(a1), applied ? 1 : 0);
        }
    }
    return r;
}

// UIRenderer (D3D12UIRenderer) per-frame RENDER ENTRY = vtable slot 54 @ EA 0x14181FCB0 (RVA 0x181FCB0).
// RE: analysis/rtti_map.json — UIRenderer vtable base 0x145F8F498 (RVA 0x5F8F498), slot 54 ptr at +0x1B0
// (RVA 0x5F8F648); FH3 dev build confirms UIRenderer::GetRenderTarget (the class owns its RT, == vf54's
// *(this+64)). Hooking vf54 brackets the WHOLE AVUI pass: while it executes, the engine records the
// HUD/menu draws on this thread, so Fh5CameraCbuffer's command-list draw hooks redirect every backbuffer
// draw made during the bracket to the UI RT for the OpenXR quad (scene-independent — covers HUD AND menus,
// unlike the composite-PSO heuristic). Body is trivial (enter/leave bracket + a counter) -> no faulting
// deref -> AV-safe (won't trip the NVIDIA overlay VEH, see [[fh5-nvidia-veh-crash]]). The redirect itself is
// still gated by uiredirect=on, so this hook is a no-op for the rendered image when the redirect is off.
using UIRendererRenderFn = void(__fastcall*)(void*, void*, void*);
static std::unique_ptr<FunctionHook> g_uir_render_hook;
static std::atomic<uint64_t> g_uir_render_calls{ 0 };
static void __fastcall Hook_UIRendererRender(void* a1, void* a2, void* a3) {
    fh5cb::enter_ui_pass();
    const uint64_t n = g_uir_render_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    g_uir_render_hook->get_original<UIRendererRenderFn>()(a1, a2, a3);
    fh5cb::leave_ui_pass();
    {   // Heartbeat + optional RT probe. The probe (probe_ui_renderer_rt) does SEH-guarded memory scans that
        // generate FIRST-CHANCE AVs near the Empress de-DRM -> can trip the NVIDIA overlay VEH ([[fh5-nvidia-veh-crash]]),
        // and uiredirect=30 does NOT need it (it uses the bracket, not the probed RT). So gate the probe behind
        // FH5VR_UI_RT_PROBE=1; keep the lightweight heartbeat always.
        static uint64_t s_last = 0;
        static const bool probe_enabled = ::GetEnvironmentVariableA("FH5VR_UI_RT_PROBE", nullptr, 0) > 0;
        const uint64_t now = ::GetTickCount64();
        if (n <= 12 || now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5UIR] UIRenderer vf54 calls={} a1=0x{:X}", n, reinterpret_cast<uintptr_t>(a1));
            if (probe_enabled) fh5cb::probe_ui_renderer_rt(a1);   // locate FH5's HUD RT -> [FH5UIRT]
        }
    }
}

using OverlayVf14Fn = void(__fastcall*)(void*, char);
using OverlayVf17Fn = void(__fastcall*)(void*, void*);
using OverlayVf19Fn = void(__fastcall*)(void*, void*);
using OverlayVf24Fn = uint64_t(__fastcall*)(void*, uint32_t, float, float);
using OverlayTextureBindFn = void(__fastcall*)(void*, void*, void*);
using OverlayPsBindingVf1Fn = char(__fastcall*)(void*);
using OverlayImmediateFlushFn = void(__fastcall*)(void*);
using OverlayNativeTargetBindFn = void(__fastcall*)(void*, void*, char);

static std::unique_ptr<FunctionHook> g_overlay_vf14_hook;
static std::unique_ptr<FunctionHook> g_overlay_vf17_hook;
static std::unique_ptr<FunctionHook> g_overlay_vf19_hook;
static std::unique_ptr<FunctionHook> g_overlay_vf24_hook;
static std::unique_ptr<FunctionHook> g_overlay_texbind_hook;
static std::unique_ptr<FunctionHook> g_overlay_psbind_hook;
static std::unique_ptr<FunctionHook> g_overlay_flush_hook;
static std::unique_ptr<FunctionHook> g_overlay_target_bind_hook;

static void __fastcall Hook_OverlayVf14(void* renderer, char surface_sized) {
    fh5cb::record_overlay_viewport_setup(renderer, surface_sized != 0);
    g_overlay_vf14_hook->get_original<OverlayVf14Fn>()(renderer, surface_sized);
    fh5cb::record_overlay_viewport_setup(renderer, surface_sized != 0);
}

static void __fastcall Hook_OverlayVf17(void* renderer, void* item) {
    fh5cb::enter_overlay_renderer12_draw(renderer, 0x17000000u);
    g_overlay_vf17_hook->get_original<OverlayVf17Fn>()(renderer, item);
    fh5cb::leave_overlay_renderer12_draw();
}

static void __fastcall Hook_OverlayVf19(void* renderer, void* item) {
    fh5cb::enter_overlay_renderer12_draw(renderer, 0x19000000u);
    g_overlay_vf19_hook->get_original<OverlayVf19Fn>()(renderer, item);
    fh5cb::leave_overlay_renderer12_draw();
}

static uint64_t __fastcall Hook_OverlayVf24(void* renderer, uint32_t layer, float x, float y) {
    fh5cb::enter_overlay_renderer12_draw(renderer, layer);
    const uint64_t r = g_overlay_vf24_hook->get_original<OverlayVf24Fn>()(renderer, layer, x, y);
    fh5cb::leave_overlay_renderer12_draw();
    return r;
}

static void __fastcall Hook_OverlayTextureBind(void* renderer, void* texture_lock, void* render_context) {
    g_overlay_texbind_hook->get_original<OverlayTextureBindFn>()(renderer, texture_lock, render_context);
    fh5cb::record_overlay_texture_bind(renderer, texture_lock, render_context);
}

static char __fastcall Hook_OverlayPsBindingVf1(void* ps) {
    const char r = g_overlay_psbind_hook->get_original<OverlayPsBindingVf1Fn>()(ps);
    fh5cb::record_overlay_ps_binding(ps);
    return r;
}

static void __fastcall Hook_OverlayImmediateFlush(void* state) {
    fh5cb::enter_overlay_immediate_flush(state);
    g_overlay_flush_hook->get_original<OverlayImmediateFlushFn>()(state);
    fh5cb::leave_overlay_immediate_flush();
}

static void __fastcall Hook_OverlayNativeTargetBind(void* render_context, void* target_object, char bind_mode) {
    g_overlay_target_bind_hook->get_original<OverlayNativeTargetBindFn>()(render_context, target_object, bind_mode);
    fh5cb::record_overlay_native_target_bind(render_context, target_object, bind_mode != 0);
}

// SEH-guarded pointer read (verify a vtable slot before hooking; no C++ unwinding objects -> __try legal).
static bool ReadPtrSEH(uintptr_t addr, uintptr_t& out) {
    if (addr == 0) return false;
    __try { out = *reinterpret_cast<const uintptr_t*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
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

    // CCamDriver +0x320 publisher sub_1406BE3A0 (Empress RVA 0x6BE3A0; confirmed live in the .i64 by
    // fh5_empress_shadow_cascade_RE.md). The Release build's signature scan misses this .text region (the
    // producer only installed via its fallback RVA), so resolve by RVA + a prologue sanity-check. Defining
    // insn: `add rcx,0x320; mov rbx,rdx`. Hooking it lets us rotate the upstream pose so the shadow cascades
    // (which read +0x320, not a4) follow head-look.
    const uintptr_t cd = memory::module_base() + 0x6BE3A0;
    static const unsigned char kCamDriverPrologue[] = {
        0x40,0x53, 0x48,0x83,0xEC,0x20, 0x48,0x81,0xC1,0x20,0x03,0x00,0x00, 0x48,0x8B,0xDA
    };
    if (BytesMatchSEH(cd, kCamDriverPrologue, sizeof(kCamDriverPrologue))) {
        g_camdriver_hook = std::make_unique<FunctionHook>(Address{ cd }, &Hook_CamDriver);
        if (!g_camdriver_hook->create()) { g_camdriver_hook.reset(); spdlog::warn("[FH5] CCamDriver hook create failed"); }
        else spdlog::info("[FH5] CCamDriver +0x320 publisher hook installed @0x{:X} (prologue verified)", cd);
    } else {
        spdlog::warn("[FH5] CCamDriver +0x320 publisher prologue MISMATCH at 0x{:X}; not hooking (build/base drift?)", cd);
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

    // Camera pose getter sub_1407A9DD0 (RVA 0x7A9DD0) — the per-frame look-at basis rebuild. Hooking its
    // entry lets us rotate the +0x540 look-direction so the main view AND shadow cascades follow head-look.
    // Raw-RVA target -> verify the prologue before inline-hooking (no ASLR; base 0x140000000).
    const uintptr_t aim = memory::module_base() + 0x7A9DD0;
    static const unsigned char kAimGetterPrologue[] = {
        0x48,0x8B,0xC4, 0x48,0x89,0x58,0x18, 0x48,0x89,0x70,0x20, 0x55,
        0x48,0x8D,0x68,0xA1, 0x48,0x81,0xEC,0xB0,0x00,0x00,0x00
    };
    if (BytesMatchSEH(aim, kAimGetterPrologue, sizeof(kAimGetterPrologue))) {
        g_aim_getter_hook = std::make_unique<FunctionHook>(Address{ aim }, &Hook_AimGetter);
        if (!g_aim_getter_hook->create()) {
            g_aim_getter_hook.reset();
            spdlog::warn("[FH5] camera pose getter hook create failed");
        } else {
            spdlog::info("[FH5] camera pose getter hook installed @0x{:X} (prologue verified)", aim);
        }
    } else {
        spdlog::warn("[FH5] camera pose getter prologue MISMATCH at 0x{:X}; NOT hooking (build/base drift?)", aim);
    }

    // a4-assembler sub_1407AC2D0 (RVA 0x7AC2D0) — builds the GAMEPLAY camera's a4 (+0x550) from the CMultiCam
    // view-source. Prologue: push rbx; sub rsp,0x50; mov rbx,rcx; call ... (rel32 wildcarded -> verify first 10).
    const uintptr_t a4asm = memory::module_base() + 0x7AC2D0;
    static const unsigned char kA4AsmPrologue[] = {
        0x40,0x53, 0x48,0x83,0xEC,0x50, 0x48,0x8B,0xD9, 0xE8
    };
    // DISABLED (2026-06-06): Lane A is a runtime no-op (a4 ← +0x320 on every camera live), so this hook has
    // no purpose; not installing it removes its per-frame S+0x650 read (an AV source while driving).
    if (false && BytesMatchSEH(a4asm, kA4AsmPrologue, sizeof(kA4AsmPrologue))) {
        g_a4asm_hook = std::make_unique<FunctionHook>(Address{ a4asm }, &Hook_A4Assembler);
        if (!g_a4asm_hook->create()) {
            g_a4asm_hook.reset();
            spdlog::warn("[FH5] a4-assembler hook create failed");
        } else {
            spdlog::info("[FH5] a4-assembler hook installed @0x{:X} (prologue verified)", a4asm);
        }
    } else {
        spdlog::warn("[FH5] a4-assembler prologue MISMATCH at 0x{:X}; NOT hooking (build/base drift?)", a4asm);
    }

    // Per-frame +0x320 WRITER sub_1407A1AC0 (RVA 0x7A1AC0) — FOUND via x64dbg HW-write-bp on the live
    // active-camera +0x320: this function commits the full camera-to-world 4x4 to camera+0x320 (movups
    // [rdi],xmm0 @0x7A1DE7 etc., rdi=cam+0x320, rbx/rcx=camera) on the camera-update worker thread, BEFORE
    // the render pass copies +0x320 into the producer a4 and fits the shadow cascades. It's the only +0x320
    // writer the bp caught for the active camera, and has a clean entry prologue. Rotate the active camera's
    // +0x320 POST-original here so a4 AND the cascades follow head-look.
    const uintptr_t posewriter = memory::module_base() + 0x7A1AC0;
    static const unsigned char kPoseWriterPrologue[] = {
        0x48,0x8B,0xC4, 0x48,0x89,0x58,0x08, 0x48,0x89,0x70,0x10, 0x48,0x89,0x78,0x18,
        0x4C,0x89,0x70,0x20, 0x55, 0x48,0x8D,0x68,0xC8, 0x48,0x81,0xEC,0x30,0x01,0x00,0x00
    };
    if (BytesMatchSEH(posewriter, kPoseWriterPrologue, sizeof(kPoseWriterPrologue))) {
        g_posewriter_hook = std::make_unique<FunctionHook>(Address{ posewriter }, &Hook_PoseWriter);
        if (!g_posewriter_hook->create()) {
            g_posewriter_hook.reset();
            spdlog::warn("[FH5] +0x320 pose-writer hook create failed");
        } else {
            spdlog::info("[FH5] +0x320 pose-writer hook installed @0x{:X} (sub_1407A1AC0, prologue verified)", posewriter);
        }
    } else {
        spdlog::warn("[FH5] +0x320 pose-writer prologue MISMATCH at 0x{:X}; NOT hooking (build/base drift?)", posewriter);
    }

    // UPSTREAM head-look lever: fh5_cam_LerpCameraStateStruct (RVA 0xC7F270, named in the decompile — a
    // verified function start, so no byte gate needed). Hooking it lets us add the head Euler to the camera
    // angle triple +0x90/0x94/0x98 upstream of the +0x320 build and (hopefully) the cull/cascade fit.
    const uintptr_t lerpcam = memory::module_base() + 0xC7F270;
    g_lerpcam_hook = std::make_unique<FunctionHook>(Address{ lerpcam }, &Hook_LerpCameraState);
    if (!g_lerpcam_hook->create()) {
        g_lerpcam_hook.reset();
        spdlog::warn("[FH5] LerpCameraStateStruct hook create failed @0x{:X}", lerpcam);
    } else {
        spdlog::info("[FH5] LerpCameraStateStruct hook installed @0x{:X} (fh5_cam_LerpCameraStateStruct)", lerpcam);
    }

    // PER-FRAME camera-follow angle setter sub_140DC9770 (RVA 0xDC9770) — THE upstream injection point.
    const uintptr_t followcam = memory::module_base() + 0xDC9770;
    g_followcam_hook = std::make_unique<FunctionHook>(Address{ followcam }, &Hook_FollowCamAngles);
    if (!g_followcam_hook->create()) {
        g_followcam_hook.reset();
        spdlog::warn("[FH5] follow-cam angle hook create failed @0x{:X}", followcam);
    } else {
        spdlog::info("[FH5] follow-cam angle hook installed @0x{:X} (sub_140DC9770)", followcam);
    }

    // UIRenderer (D3D12UIRenderer) render-entry vf54 — a diagnostic UI-pass bracket. This path can be called
    // thousands of times during startup/menu rendering, so keep it opt-in while the stable HUD quad path uses
    // the lower-level command-list/resource tracking instead.
    {
        // vf54 UI-pass bracket. REQUIRED for the engine-seam UI redirect (uiredirect=30 / "seam"): it marks the
        // UIRenderer render pass so the command-list draw hooks know which draws are UI. enter/leave are trivial
        // atomic counters (no faulting deref -> AV-safe). Installed BY DEFAULT now; set FH5VR_UI_RENDERER_HOOK=0
        // to disable. The redirect itself is still gated by uiredirect, so this is a no-op image-wise when off.
        char uirhk[8]{};
        const bool disabled = ::GetEnvironmentVariableA("FH5VR_UI_RENDERER_HOOK", uirhk, sizeof(uirhk)) > 0 && uirhk[0] == '0';
        if (!disabled) {
            const uintptr_t base = memory::module_base();
            const uintptr_t vtbl_slot54 = base + 0x5F8F648;   // &UIRenderer::vftable[54]
            const uintptr_t expected    = base + 0x181FCB0;   // UIRenderer__vf54
            uintptr_t slot = 0;
            if (ReadPtrSEH(vtbl_slot54, slot) && slot == expected) {
                g_uir_render_hook = std::make_unique<FunctionHook>(Address{ slot }, &Hook_UIRendererRender);
                if (!g_uir_render_hook->create()) {
                    g_uir_render_hook.reset();
                    spdlog::warn("[FH5UIR] UIRenderer vf54 render-entry hook create FAILED @0x{:X}", slot);
                } else {
                    spdlog::info("[FH5UIR] UIRenderer vf54 render-entry hook installed @0x{:X} (vtable slot 54 verified; UI-pass bracket ON for uiredirect=30)", slot);
                }
            } else {
                spdlog::warn("[FH5UIR] UIRenderer vf54 vtable mismatch (slot=0x{:X} expected=0x{:X}) — UI-pass bracket OFF; uiredirect=30 cannot catch UI", slot, expected);
            }
        } else {
            spdlog::info("[FH5UIR] UIRenderer vf54 render-entry hook DISABLED (FH5VR_UI_RENDERER_HOOK=0)");
        }
    }

    // OverlayRenderer12 exact UI source path. The resource-binding hook is the complete capture point because
    // both sub_140E15C90 and the inline textured rectangle path update OverlayRendererPSParameters before this
    // binding function resolves textureObject+0x10 to the SRV descriptor.
    {
        const uintptr_t base = memory::module_base();
        const uintptr_t overlay_vtbl = base + 0x5EB1CF0; // vftbl_OverlayRenderer12
        struct OverlaySlot { const char* name; uintptr_t slot_rva; uintptr_t fn_rva; std::unique_ptr<FunctionHook>* hook; void* detour; };
        OverlaySlot slots[] = {
            {"vf14", 14u * sizeof(uintptr_t), 0xDEE440, &g_overlay_vf14_hook, (void*)&Hook_OverlayVf14},
            {"vf17", 17u * sizeof(uintptr_t), 0xE0C4D0, &g_overlay_vf17_hook, (void*)&Hook_OverlayVf17},
            {"vf19", 19u * sizeof(uintptr_t), 0xE0B290, &g_overlay_vf19_hook, (void*)&Hook_OverlayVf19},
            {"vf24", 24u * sizeof(uintptr_t), 0xE08D40, &g_overlay_vf24_hook, (void*)&Hook_OverlayVf24},
        };
        for (const auto& s : slots) {
            uintptr_t slot = 0;
            const uintptr_t expected = base + s.fn_rva;
            if (ReadPtrSEH(overlay_vtbl + s.slot_rva, slot) && slot == expected) {
                *s.hook = std::make_unique<FunctionHook>(Address{ slot }, reinterpret_cast<uintptr_t>(s.detour));
                if (!(*s.hook)->create()) {
                    s.hook->reset();
                    spdlog::warn("[FH5OVERLAY] OverlayRenderer12 {} hook create FAILED @0x{:X}", s.name, slot);
                } else {
                    spdlog::info("[FH5OVERLAY] OverlayRenderer12 {} hook installed @0x{:X}", s.name, slot);
                }
            } else {
                spdlog::warn("[FH5OVERLAY] OverlayRenderer12 {} vtable mismatch (slot=0x{:X} expected=0x{:X})",
                             s.name, slot, expected);
            }
        }

        const uintptr_t texbind = base + 0xE15C90;
        g_overlay_texbind_hook = std::make_unique<FunctionHook>(Address{ texbind }, &Hook_OverlayTextureBind);
        if (!g_overlay_texbind_hook->create()) {
            g_overlay_texbind_hook.reset();
            spdlog::warn("[FH5OVERLAY] sub_140E15C90 texture-bind hook create FAILED @0x{:X}", texbind);
        } else {
            spdlog::info("[FH5OVERLAY] sub_140E15C90 texture-bind hook installed @0x{:X}", texbind);
        }

        const uintptr_t psbind = base + 0xEA55D0;
        g_overlay_psbind_hook = std::make_unique<FunctionHook>(Address{ psbind }, &Hook_OverlayPsBindingVf1);
        if (!g_overlay_psbind_hook->create()) {
            g_overlay_psbind_hook.reset();
            spdlog::warn("[FH5OVERLAY] OverlayRendererPSParameters vf1 hook create FAILED @0x{:X}", psbind);
        } else {
            spdlog::info("[FH5OVERLAY] OverlayRendererPSParameters vf1 hook installed @0x{:X}", psbind);
        }

        // This is a very hot render-target bind body and is only useful for the discarded mode-29
        // native-target probe. Leaving it installed during normal HUD quad runs produces tens of
        // thousands of calls per second and has reproduced the NVIDIA stack-overflow crash bucket.
        // Keep it as an explicit diagnostic, not part of the default overlay capture chain.
        if (::GetEnvironmentVariableA("FH5VR_OVERLAY_TARGET_BIND_HOOK", nullptr, 0) > 0) {
            const uintptr_t targetbind = base + 0x9AD3E0; // sub_1409AD3E0 render-target bind body.
            g_overlay_target_bind_hook = std::make_unique<FunctionHook>(Address{ targetbind }, &Hook_OverlayNativeTargetBind);
            if (!g_overlay_target_bind_hook->create()) {
                g_overlay_target_bind_hook.reset();
                spdlog::warn("[FH5OVERLAYTARGET] sub_1409AD3E0 target-bind hook create FAILED @0x{:X}", targetbind);
            } else {
                spdlog::info("[FH5OVERLAYTARGET] sub_1409AD3E0 target-bind hook installed @0x{:X}", targetbind);
            }
        } else {
            spdlog::info("[FH5OVERLAYTARGET] sub_1409AD3E0 target-bind hook skipped (set FH5VR_OVERLAY_TARGET_BIND_HOOK=1 for diagnostics)");
        }

        const uintptr_t flush = base + 0xDF5910;
        static constexpr unsigned char kFlushPrologue[] = {
            0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
            0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48,
        };
        if (BytesMatchSEH(flush, kFlushPrologue, sizeof(kFlushPrologue))) {
            g_overlay_flush_hook = std::make_unique<FunctionHook>(Address{ flush }, &Hook_OverlayImmediateFlush);
            if (!g_overlay_flush_hook->create()) {
                g_overlay_flush_hook.reset();
                spdlog::warn("[FH5OVERLAY] sub_140DF5910 immediate-flush hook create FAILED @0x{:X}", flush);
            } else {
                spdlog::info("[FH5OVERLAY] sub_140DF5910 immediate-flush hook installed @0x{:X} (Empress prologue verified)", flush);
            }
        } else {
            spdlog::warn("[FH5OVERLAY] sub_140DF5910 immediate-flush prologue mismatch @0x{:X}; hook skipped", flush);
        }
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
    //   rot=angle:           SHADOW-COHERENT (CAMERA_VR_FIX_GUIDE) — inject the head Euler at the rendered
    //                        camera's orientation angles cam+0x90/94/98 (in the sub_1407A1AC0 hook), upstream
    //                        of the +0x320 view build AND the render-thread cull/shadow cascade fit, so all
    //                        three rotate together.
    //   rot=driver:          rotate the upstream CCamDriver +0x320 MATRIX (moves the view; cull/cascade fit
    //                        reads an upstream snapshot -> shadows NOT coherent — superseded by rot=angle).
    //   rot=a4:              bring-up path; rotate only the main producer argument (view-only, shadows follow).
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
    // input540 has no upstream rotation hook -> keep its rotation on the producer a4 (rot_mode 1).
    // proda15 routes rotation to the CCamDriver +0x320 publisher hook (rot_mode 2 -> driver delta published),
    // so the producer a4 stays identity and the shadow cascades (reading +0x320) follow head-look.
    const int rot_mode =
        (pos_lane == fh5cb::kPosLaneInput540 && requested_rot_mode == 2) ? 1 : requested_rot_mode;
    const bool upstream_position =
        pos_lane != fh5cb::kPosLaneDownstream &&
        pos_lane != fh5cb::kPosLaneOff;
    const bool downstream_position = pos_lane == fh5cb::kPosLaneDownstream;
    const float* producer_delta = (rot_mode == 1) ? head_delta16 : identity16;
    // driver_delta carries the head rotation for mode 2 (CCamDriver +0x320 matrix) AND mode 3 (cam+0x90
    // Euler angles) — both consume the published delta basis via SnapshotOpenXrPose. The two appliers
    // self-gate (apply_camdriver_head_rotation -> mode 2, apply_angle_head_rotation_prewrite -> mode 3), so
    // only one acts; the producer a4 stays identity in both (the view rotates via the rebuilt +0x320/a4).
    const float* driver_delta   = (rot_mode == 2 || rot_mode == 3) ? head_delta16 : identity16;
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
