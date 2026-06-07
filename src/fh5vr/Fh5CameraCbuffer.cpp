// FH5 downstream camera-constant-buffer stereo hook — see Fh5CameraCbuffer.hpp for the rationale.
//
// Ported from the proven freecam FH5CameraProbe/src/DxgiProxy.cpp (resource-ring tracking + CBV-triggered
// in-place transform), with the freecam's rotation build swapped for the per-eye VIEW-SPACE IPD/translation
// shift specified in _agent_reports/fh5_stereo_cbuffer_spec.md §4. Uses the framework's safetyhook-backed
// FunctionHook to inline-hook the game device's CreateCommittedResource/CreatePlacedResource/
// CreateConstantBufferView (the probe used MinHook; same vtable indices 27/29/17).

#include "Fh5CameraCbuffer.hpp"

#include <utility/Hooks.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fh5cb {
namespace {

// ---------------------------------------------------------------------------
// Per-eye offset published by the adapter (apply_stereo), read by Hook_CBV.
// View-space axes (x=right, y=up, z=forward), FH5 world units. Slowly-varying;
// a torn read for one frame is harmless, so plain atomics (no seqlock needed).
// ---------------------------------------------------------------------------
std::atomic<float> g_off_x{ 0.0f }, g_off_y{ 0.0f }, g_off_z{ 0.0f };
std::atomic<bool>  g_active{ false };

// ---------------------------------------------------------------------------
// Live-tuning control file: E:\tmp\fh5vr_ctl.txt, polled by a worker thread, so
// IPD / world-scale / transform-mode can be swept WITHOUT a rebuild (the audit's
// Phase 0). Lines: "ipd=0.032" (half-IPD metres), "scale=100" (FH5 camera-lane
// units/metre), "mode=off|camrel|viewvp|all", "poslane=input540|ccam320|viewtail|clone0|...".
// ---------------------------------------------------------------------------
std::atomic<float> g_ctl_half_ipd{ 0.0f };
std::atomic<float> g_ctl_world_scale{ 1.0f };
std::atomic<int>   g_ctl_mode{ 3 };   // 0=off 1=camrel_only 2=view_vp_only 3=all
std::atomic<int>   g_ctl_rot_mode{ 2 }; // 0=off 1=producer a4 2=CCamDriver+0x320
std::atomic<bool>  g_ctl_projection{ true };
std::atomic<int>   g_ctl_pos_lane{ kPosLaneProducerA15 };   // Empress RE: producer a15/a16 f64 cameraPos is the lever (+0x540 proved inert live)
std::atomic<bool>  g_ctl_started{ false };
std::atomic<int>   g_ctl_recenter_seq{ 0 };
// --- runtime camera-orientation probe (find/rotate the view-source orientation that shadow cascades read) ---
std::atomic<int>   g_ctl_pokerot{ -1 };     // rotate the orthonormal 4x4 at active_cam + offset by head delta (-1=off)
std::atomic<int>   g_ctl_pokerotvs{ -1 };   // rotate the orthonormal 4x4 at (*(active_cam+0x48)) + offset (-1=off)
std::atomic<bool>  g_ctl_dumpcam{ false };  // scan active_cam + view-source for orthonormal matrices; log offsets + basisScore
std::atomic<bool>  g_ctl_hud_quad{ false }; // submit the UI/HUD as a head-locked OpenXR quad layer (hudquad=on)
std::atomic<bool>  g_ctl_hud_opaque{ true }; // hudopaque=on -> opaque quad (no source-alpha blend); for backbuffer validation
std::atomic<float> g_ctl_hud_w{ 1.6f };      // quad width in metres (height derives from texture aspect)
std::atomic<float> g_ctl_hud_x{ 0.0f };      // quad centre offset (metres) in view space: +x right
std::atomic<float> g_ctl_hud_y{ 0.0f };      // +y up
std::atomic<float> g_ctl_hud_z{ -1.8f };     // -z forward (distance in front of the head)
std::atomic<bool>  g_ctl_ui_redirect{ false }; // uiredirect=on -> redirect FH5's backbuffer UI draws to a separate RT (Case B)

// UPSTREAM camera-translation test: a constant camera-relative offset applied IN THE PRODUCER to a chosen
// argument, to find which lever actually moves the rendered camera (with shadows/derived data following).
std::atomic<float> g_ctl_fwd{ 0.0f }, g_ctl_strafe{ 0.0f }, g_ctl_up{ 0.0f };
std::atomic<int>   g_ctl_tgt{ 0 };    // 0=off 1=a4.row3 2=a17 3=a18 4=all

void poll_control_file() {
    FILE* f = nullptr;
    if (fopen_s(&f, "E:\\tmp\\fh5vr_ctl.txt", "rb") != 0 || f == nullptr) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        float v; int iv;
        if (sscanf_s(line, "ipd=%f", &v) == 1)          g_ctl_half_ipd.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "scale=%f", &v) == 1)   g_ctl_world_scale.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "recenter=%d", &iv) == 1) g_ctl_recenter_seq.store(iv, std::memory_order_relaxed);
        else if (sscanf_s(line, "fwd=%f", &v) == 1)     g_ctl_fwd.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "strafe=%f", &v) == 1)  g_ctl_strafe.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "up=%f", &v) == 1)      g_ctl_up.store(v, std::memory_order_relaxed);
        else if (strncmp(line, "mode=", 5) == 0) {
            if      (strncmp(line + 5, "off", 3) == 0)    iv = 0;
            else if (strncmp(line + 5, "camrel", 6) == 0) iv = 1;
            else if (strncmp(line + 5, "viewvp", 6) == 0) iv = 2;
            else                                          iv = 3;
            g_ctl_mode.store(iv, std::memory_order_relaxed);
        }
        else if (strncmp(line, "rot=", 4) == 0) {
            if      (strncmp(line + 4, "off", 3)    == 0) iv = 0;
            else if (strncmp(line + 4, "a4", 2)     == 0) iv = 1;
            else if (strncmp(line + 4, "driver", 6) == 0) iv = 2;
            else if (strncmp(line + 4, "angle", 5)  == 0) iv = 3;   // inject head Euler at cam+0x90/94/98 (upstream of view+cull+shadow)
            else                                          iv = 2;
            g_ctl_rot_mode.store(iv, std::memory_order_relaxed);
        }
        else if (strncmp(line, "proj=", 5) == 0) {
            const bool enabled =
                strncmp(line + 5, "on", 2) == 0 ||
                strncmp(line + 5, "1", 1) == 0 ||
                strncmp(line + 5, "true", 4) == 0;
            g_ctl_projection.store(enabled, std::memory_order_relaxed);
        }
        else if (strncmp(line, "hudquad=", 8) == 0) {
            const bool enabled =
                strncmp(line + 8, "on", 2) == 0 ||
                strncmp(line + 8, "1", 1) == 0 ||
                strncmp(line + 8, "true", 4) == 0;
            g_ctl_hud_quad.store(enabled, std::memory_order_relaxed);
        }
        else if (strncmp(line, "hudopaque=", 10) == 0) {
            const bool enabled =
                strncmp(line + 10, "on", 2) == 0 ||
                strncmp(line + 10, "1", 1) == 0 ||
                strncmp(line + 10, "true", 4) == 0;
            g_ctl_hud_opaque.store(enabled, std::memory_order_relaxed);
        }
        else if (strncmp(line, "uiredirect=", 11) == 0) {
            const bool en = strncmp(line+11,"on",2)==0 || strncmp(line+11,"1",1)==0 || strncmp(line+11,"true",4)==0;
            g_ctl_ui_redirect.store(en, std::memory_order_relaxed);
        }
        else if (sscanf_s(line, "hudw=%f", &v) == 1)    g_ctl_hud_w.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "hudx=%f", &v) == 1)    g_ctl_hud_x.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "hudy=%f", &v) == 1)    g_ctl_hud_y.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "hudz=%f", &v) == 1)    g_ctl_hud_z.store(v, std::memory_order_relaxed);
        else if (strncmp(line, "poslane=", 8) == 0) {
            if      (strncmp(line + 8, "camsrc", 6)         == 0) iv = kPosLaneCamSrc;
            else if (strncmp(line + 8, "proda15", 7)        == 0) iv = kPosLaneProducerA15;
            else if (strncmp(line + 8, "a15", 3)            == 0) iv = kPosLaneProducerA15;
            else if (strncmp(line + 8, "input540", 8)       == 0) iv = kPosLaneInput540;
            else if (strncmp(line + 8, "ccam540", 7)        == 0) iv = kPosLaneInput540;
            else if (strncmp(line + 8, "viewtail", 8)       == 0) iv = kPosLaneViewTail;
            else if (strncmp(line + 8, "ccam320_viewtail", 16) == 0) iv = kPosLaneViewTail;
            else if (strncmp(line + 8, "ccam320_d550", 13) == 0) iv = kPosLaneCcam320D550;
            else if (strncmp(line + 8, "ccam320", 7)       == 0) iv = kPosLaneCcam320;
            else if (strncmp(line + 8, "clone0", 6)        == 0) iv = kPosLaneClone0;
            else if (strncmp(line + 8, "clone1", 6)        == 0) iv = kPosLaneClone1;
            else if (strncmp(line + 8, "clone2", 6)        == 0) iv = kPosLaneClone2;
            else if (strncmp(line + 8, "downstream", 10)   == 0) iv = kPosLaneDownstream;
            else if (strncmp(line + 8, "off", 3)           == 0) iv = kPosLaneOff;
            else                                                   iv = kPosLaneCcam320;
            g_ctl_pos_lane.store(iv, std::memory_order_relaxed);
        }
        else if (strncmp(line, "tgt=", 4) == 0) {
            if      (strncmp(line + 4, "off", 3)    == 0) iv = 0;
            else if (strncmp(line + 4, "a4",  2)    == 0) iv = 1;
            else if (strncmp(line + 4, "a17", 3)    == 0) iv = 2;
            else if (strncmp(line + 4, "a18", 3)    == 0) iv = 3;
            else if (strncmp(line + 4, "driver", 6) == 0) iv = 5;   // CCamDriver+0x320 (upstream pose)
            else                                          iv = 4;   // all
            g_ctl_tgt.store(iv, std::memory_order_relaxed);
        }
        else {
            unsigned uv = 0;
            if      (sscanf_s(line, "pokerotvs=%x", &uv) == 1) g_ctl_pokerotvs.store((int)uv, std::memory_order_relaxed);
            else if (sscanf_s(line, "pokerot=%x", &uv) == 1)   g_ctl_pokerot.store((int)uv, std::memory_order_relaxed);
            else if (sscanf_s(line, "dumpcam=%u", &uv) == 1)   g_ctl_dumpcam.store(uv != 0, std::memory_order_relaxed);
        }
    }
    fclose(f);
}

DWORD WINAPI ControlThread(void*) {
    int last_logged = -999;
    for (;;) {
        poll_control_file();
        const int m = g_ctl_mode.load(std::memory_order_relaxed);
        const int tgt = g_ctl_tgt.load(std::memory_order_relaxed);
        const int rot = g_ctl_rot_mode.load(std::memory_order_relaxed);
        const bool proj = g_ctl_projection.load(std::memory_order_relaxed);
        const int poslane = g_ctl_pos_lane.load(std::memory_order_relaxed);
        const int sig = m * 100000 + tgt * 10000
            + rot * 1000 + (proj ? 500 : 0) + poslane
            + (int)(g_ctl_half_ipd.load(std::memory_order_relaxed) * 100)
            + (int)(g_ctl_fwd.load(std::memory_order_relaxed) + g_ctl_strafe.load(std::memory_order_relaxed));
        if (sig != last_logged) {
            last_logged = sig;
            spdlog::info("[FH5CTL] ipd={:.3f} scale={:.1f} mode={} rot={} proj={} poslane={} | UPSTREAM tgt={} fwd={:.1f} strafe={:.1f} up={:.1f}",
                         g_ctl_half_ipd.load(), g_ctl_world_scale.load(), m, rot, proj ? 1 : 0,
                         pos_lane_name(poslane), tgt,
                         g_ctl_fwd.load(), g_ctl_strafe.load(), g_ctl_up.load());
        }
        Sleep(300);
    }
}

// ---- math (identical helpers to DxgiProxy.cpp) ----------------------------
struct Mat4 { float m[16]; };
inline Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0; for (int k = 0; k < 4; ++k) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    return r;
}
// Rigid inverse of a column-vector world->view matrix (translation in m[3],m[7],m[11]).
inline Mat4 InvRigid(const Mat4& V) {
    Mat4 r{};
    r.m[0] = V.m[0]; r.m[1] = V.m[4]; r.m[2] = V.m[8];
    r.m[4] = V.m[1]; r.m[5] = V.m[5]; r.m[6] = V.m[9];
    r.m[8] = V.m[2]; r.m[9] = V.m[6]; r.m[10] = V.m[10];
    const float tx = V.m[3], ty = V.m[7], tz = V.m[11];
    r.m[3]  = -(r.m[0] * tx + r.m[1] * ty + r.m[2] * tz);
    r.m[7]  = -(r.m[4] * tx + r.m[5] * ty + r.m[6] * tz);
    r.m[11] = -(r.m[8] * tx + r.m[9] * ty + r.m[10] * tz);
    r.m[15] = 1;
    return r;
}

inline float F32(const uint8_t* p) { float v = 0; std::memcpy(&v, p, 4); return v; }

// Same VIEW validator the proven freecam used: finite, last ROW (m12..15) == (0,0,0,1), orthonormal rows.
inline bool LooksView(const float* m) {
    for (int i = 0; i < 16; ++i) if (!std::isfinite(m[i])) return false;
    if (std::fabs(m[12]) > 0.01f || std::fabs(m[13]) > 0.01f || std::fabs(m[14]) > 0.01f || std::fabs(m[15] - 1) > 0.01f) return false;
    for (int r = 0; r < 3; ++r) {
        float l = std::sqrt(m[r * 4] * m[r * 4] + m[r * 4 + 1] * m[r * 4 + 1] + m[r * 4 + 2] * m[r * 4 + 2]);
        if (l < 0.9f || l > 1.1f) return false;
    }
    return true;
}
inline bool Finite16(const float* f) { for (int i = 0; i < 16; ++i) if (!std::isfinite(f[i])) return false; return true; }

// Main-camera identity gate on the 6912B block: near~0.1@0xA0, posW~1@0x8C, valid VIEW@0. The FAR plane
// varies by scene — 5000 in free-roam, 50000 at the showcase intro (matches the producer gate a9>2000) —
// so gate far on a generous floor, NOT the showcase-only [45000,55000] (which silently rejected free-roam).
inline bool LooksMainCameraCb(const uint8_t* p) {
    const float* view = reinterpret_cast<const float*>(p);
    const float* vp = reinterpret_cast<const float*>(p + 64);
    const float* camRelVp = reinterpret_cast<const float*>(p + 256);
    const float nr = F32(p + 160), fr = F32(p + 164), posW = F32(p + 140);
    return nr > 0.08f && nr < 0.2f && fr > 2000.0f && fr < 60000.0f &&
           std::fabs(posW - 1.0f) < 0.01f && LooksView(view) && Finite16(vp) && Finite16(camRelVp);
}

// ---------------------------------------------------------------------------
// The per-eye transform. VIEW@0x000 (col-vector world->view), VP@0x040 and
// camRelVP@0x100 (row-vector world/camRel->clip). The eye is shifted by the
// published VIEW-SPACE offset; geometry is already camera-relative so this does
// not cancel (spec §4.4). Returns true if applied.
// ---------------------------------------------------------------------------
bool TransformStereo(uint8_t* p) {
    const float dx = g_off_x.load(std::memory_order_relaxed);
    const float dy = g_off_y.load(std::memory_order_relaxed);
    const float dz = g_off_z.load(std::memory_order_relaxed);

    Mat4 V{}, P{}, R{};
    std::memcpy(V.m, p, 64);
    if (!LooksView(V.m)) return false;
    std::memcpy(P.m, p + 64, 64);    // VP @ 0x040
    std::memcpy(R.m, p + 256, 64);   // camRelVP @ 0x100

    // Camera axes in world (rows of V^-1 = view->world rotation): right/up/forward.
    Mat4 Vinv = InvRigid(V);
    const float rx = Vinv.m[0], ry = Vinv.m[1], rz = Vinv.m[2];   // camera +X (right) in world
    const float ux = Vinv.m[4], uy = Vinv.m[5], uz = Vinv.m[6];   // camera +Y (up) in world
    const float fx = Vinv.m[8], fy = Vinv.m[9], fz = Vinv.m[10];  // camera +Z (forward) in world

    // World-space displacement of the eye for the requested view-space offset (camera axes * offset).
    const float wdx = dx * rx + dy * ux + dz * fx;
    const float wdy = dx * ry + dy * uy + dz * fy;
    const float wdz = dx * rz + dy * uz + dz * fz;

    // These matrices are COLUMN-VECTOR (clip/view = M * pos_col; translation in COLUMN 3 = m[3],m[7],m[11];
    // last ROW = (0,0,0,1)). Proven by the freecam's InvRigid reading tx=m[3],ty=m[7],tz=m[11] and LooksView
    // requiring m[12..14]=0. To move the camera by +wd (world), POST-multiply by a column-vector translation
    // that pre-shifts the input position by -wd: Mnew = M * Tcol(-wd). (Pre-multiplying a ROW-vector Trow was
    // the bug -- it had zero render effect because the convention was wrong.) The camera-relative geometry
    // fed to camRelVP is FIXED for the frame, so shifting the eye here does NOT cancel -> real parallax.
    Mat4 Tcol{ {1,0,0,-wdx,  0,1,0,-wdy,  0,0,1,-wdz,  0,0,0,1} };   // column-vector translate by -wd

    // Mode gate (live via the control file): isolate which field moves which surface.
    const int mode = g_ctl_mode.load(std::memory_order_relaxed);
    if (mode == 0) return false;                      // off
    if (mode == 1 || mode == 3) {                     // camrel_only / all -> world parallax lever
        Mat4 camRelVPnew = Mul(R, Tcol);
        std::memcpy(p + 256,   camRelVPnew.m, 64);
        std::memcpy(p + 0xC40, camRelVPnew.m, 64);    // camRelVP exact duplicate @ 0xC40 (spec §1.2)
    }
    if (mode == 2 || mode == 3) {                     // view_vp_only / all -> car/cockpit/world-space path
        Mat4 Vnew  = Mul(V, Tcol);                    // VIEW @ 0x000 (world->view)
        Mat4 VPnew = Mul(P, Tcol);                    // VP   @ 0x040 (world->clip)
        std::memcpy(p,       Vnew.m,  64);
        std::memcpy(p + 64,  VPnew.m, 64);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Upload-ring tracking: map every CPU-visible BUFFER once (persistent), resolve
// a CBV's GPU-VA to a CPU pointer by linear scan (newest first).
// ---------------------------------------------------------------------------
struct BufRec { D3D12_GPU_VIRTUAL_ADDRESS va; UINT64 size; uint8_t* cpu; };
std::mutex            g_buf_mtx;
std::vector<BufRec>   g_buffers;
std::atomic<uint64_t> g_buf_count{ 0 };

void TrackBuffer(ID3D12Resource* res, const D3D12_RESOURCE_DESC* desc, bool cpuVisible) {
    if (!res || !desc || desc->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || !cpuVisible) return;
    D3D12_GPU_VIRTUAL_ADDRESS va = res->GetGPUVirtualAddress();
    if (!va) return;
    void* cpu = nullptr;
    if (FAILED(res->Map(0, nullptr, &cpu)) || !cpu) return;   // persistent map; never Unmap
    std::lock_guard<std::mutex> lk(g_buf_mtx);
    g_buffers.push_back({ va, desc->Width, reinterpret_cast<uint8_t*>(cpu) });
    g_buf_count.store(g_buffers.size(), std::memory_order_relaxed);
}

uint8_t* ResolveVA(D3D12_GPU_VIRTUAL_ADDRESS va, UINT need) {
    if (!va || !need) return nullptr;
    std::lock_guard<std::mutex> lk(g_buf_mtx);
    for (size_t i = g_buffers.size(); i-- > 0;) {
        const BufRec& b = g_buffers[i];
        if (!b.cpu || va < b.va || need > b.size) continue;
        const UINT64 off = static_cast<UINT64>(va - b.va);
        if (off <= b.size - need) return b.cpu + off;
    }
    return nullptr;
}

// Refill guard: transform a physical slot at most once per engine refill (else repeated CBV creations on
// the same slot in one frame re-apply the shift -> doubled disparity / degenerate). Map slot -> our hash.
std::mutex g_slot_mtx;
std::unordered_map<uintptr_t, uint64_t> g_slot_hash;
uint64_t HashCam(const uint8_t* p) { uint64_t h = 14695981039346656037ull; for (int i = 0; i < 320; ++i) h = (h ^ p[i]) * 1099511628211ull; return h; }
bool SlotDone(uintptr_t a, uint64_t h) { std::lock_guard<std::mutex> lk(g_slot_mtx); auto it = g_slot_hash.find(a); return it != g_slot_hash.end() && it->second == h; }
void SlotMark(uintptr_t a, uint64_t h) { std::lock_guard<std::mutex> lk(g_slot_mtx); g_slot_hash[a] = h; }

std::atomic<uint64_t> g_ring_writes{ 0 }, g_cam_hits{ 0 };

// SEH-guarded transform of one resolved slot (the mapped ptr can dangle if its ring was freed). No C++
// objects with destructors live in THIS frame (the helpers it calls own their own frames), so __try is legal.
bool TransformSlotSEH(uint8_t* p) {
    __try {
        if (!LooksMainCameraCb(p)) return false;
        g_cam_hits.fetch_add(1, std::memory_order_relaxed);
        if (!g_active.load(std::memory_order_relaxed)) return false;
        uint64_t h = HashCam(p);
        if (SlotDone(reinterpret_cast<uintptr_t>(p), h)) return false;   // already transformed this refill
        if (TransformStereo(p)) {
            SlotMark(reinterpret_cast<uintptr_t>(p), HashCam(p));
            g_ring_writes.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

// ---------------------------------------------------------------------------
// Device vtable hooks (indices 27/29/17, same as the proven probe).
// ---------------------------------------------------------------------------
using FnCommitted = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_HEAP_PROPERTIES*,
    D3D12_HEAP_FLAGS, const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*, REFIID, void**);
using FnPlaced = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Heap*, UINT64,
    const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*, REFIID, void**);
using FnCBV = void(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_CONSTANT_BUFFER_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);

std::unique_ptr<FunctionHook> g_hk_committed, g_hk_placed, g_hk_cbv;
std::atomic<uint64_t> g_cbv6912{ 0 };   // count of 6912B CBVs seen by Hook_CBV

HRESULT STDMETHODCALLTYPE Hook_Committed(ID3D12Device* self, const D3D12_HEAP_PROPERTIES* hp,
    D3D12_HEAP_FLAGS flags, const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES state,
    const D3D12_CLEAR_VALUE* clr, REFIID riid, void** ppv) {
    auto orig = g_hk_committed->get_original<FnCommitted>();
    HRESULT hr = orig(self, hp, flags, desc, state, clr, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv && hp &&
        (hp->Type == D3D12_HEAP_TYPE_UPLOAD ||
         (hp->Type == D3D12_HEAP_TYPE_CUSTOM && hp->CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE)))
        TrackBuffer(reinterpret_cast<ID3D12Resource*>(*ppv), desc, true);
    return hr;
}

HRESULT STDMETHODCALLTYPE Hook_Placed(ID3D12Device* self, ID3D12Heap* heap, UINT64 off,
    const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clr,
    REFIID riid, void** ppv) {
    auto orig = g_hk_placed->get_original<FnPlaced>();
    HRESULT hr = orig(self, heap, off, desc, state, clr, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv && desc && desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        auto* res = reinterpret_cast<ID3D12Resource*>(*ppv);
        D3D12_HEAP_PROPERTIES php{}; D3D12_HEAP_FLAGS hf{};
        bool cpuVisible = SUCCEEDED(res->GetHeapProperties(&php, &hf)) &&
            (php.Type == D3D12_HEAP_TYPE_UPLOAD ||
             (php.Type == D3D12_HEAP_TYPE_CUSTOM && php.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE));
        TrackBuffer(res, desc, cpuVisible);
    }
    return hr;
}

void STDMETHODCALLTYPE Hook_CBV(ID3D12Device* self, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    auto orig = g_hk_cbv->get_original<FnCBV>();
    orig(self, desc, handle);
    // The main-camera block is 6912 bytes, 256-aligned location. Resolve to CPU and transform in place
    // BEFORE the engine records draws that read it (CBV creation precedes the binding/draw).
    if (desc && desc->SizeInBytes == 6912 && (desc->BufferLocation & 0xFF) == 0) {
        g_cbv6912.fetch_add(1, std::memory_order_relaxed);
        uint8_t* cpu = ResolveVA(desc->BufferLocation, desc->SizeInBytes);
        if (cpu) TransformSlotSEH(cpu);
    }
}

// NOTE: an ExecuteCommandLists ring-scan was tried and REMOVED — scanning all persistently-mapped upload
// buffers crashes FH5 with STATUS_IN_PAGE_ERROR (a tracked resource is freed/decommitted and the dangling
// mapped pointer faults, or a remapped page write corrupts another allocation). The probe hit the same
// wall. The safe, proven path is the bounded per-CBV transform in Hook_CBV above: the camera cbuffer lives
// in a moving upload-ring slot allocated FRESH each frame, so the engine creates a fresh 6912B CBV for it
// every frame -> Hook_CBV resolves the exact LIVE slot (no scan, no dangling) and transforms it in place.

// ---------------------------------------------------------------------------
// UI-draw redirect (Case B): track RTV->resource so we can tell when a command list binds the backbuffer.
// CreateRenderTargetView is hooked EARLY (with the other device hooks) so the game's backbuffer RTVs —
// created at swapchain setup, before ui_redirect_install — are captured. Map updated; backbuffer handles
// are resolved in ui_redirect_install once the swapchain is known.
// ---------------------------------------------------------------------------
using FnRTV = void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_RENDER_TARGET_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
std::unique_ptr<FunctionHook> g_hk_rtv;
std::mutex g_rtv_mtx;
std::unordered_map<size_t, ID3D12Resource*> g_rtv_map;          // rtvHandle.ptr -> resource (raw; compare-only)
std::unordered_set<size_t> g_uir_bb_rtvs;                       // subset whose resource is a swapchain backbuffer
std::vector<ID3D12Resource*> g_uir_bb_resources;               // swapchain backbuffer ptrs (identity; set in install)

// Helpers split out so the SEH (__try) hook bodies contain no C++ unwinding objects (MSVC C2712).
void RecordRtv(size_t handle, ID3D12Resource* res) {
    std::scoped_lock lk(g_rtv_mtx);
    g_rtv_map[handle] = res;                   // store ptr identity only (never dereferenced)
    for (auto* b : g_uir_bb_resources)         // tag backbuffer RTVs created after install too
        if (b == res) { g_uir_bb_rtvs.insert(handle); break; }
}
bool IsBackbufferRtv(size_t handle) {
    std::scoped_lock lk(g_rtv_mtx);
    return g_uir_bb_rtvs.count(handle) != 0;
}

void STDMETHODCALLTYPE Hook_RTV(ID3D12Device* self, ID3D12Resource* res,
    const D3D12_RENDER_TARGET_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    auto orig = g_hk_rtv->get_original<FnRTV>();
    orig(self, res, desc, handle);
    __try {
        RecordRtv((size_t)handle.ptr, res);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

std::atomic<bool> g_dev_done{ false };

// Install the device buffer-tracking hooks (committed/placed/cbv). Called as early as possible — from the
// D3D12CreateDevice hook — so the camera upload ring's creation is tracked. Idempotent. The vtable function
// addresses are shared across all ID3D12Device instances of the same runtime, so hooking once catches all.
void install_device_hooks(ID3D12Device* device) {
    if (!device || g_dev_done.exchange(true)) return;
    void** vt = *reinterpret_cast<void***>(device);
    g_hk_committed = std::make_unique<FunctionHook>(Address{ vt[27] }, &Hook_Committed);
    g_hk_placed    = std::make_unique<FunctionHook>(Address{ vt[29] }, &Hook_Placed);
    g_hk_cbv       = std::make_unique<FunctionHook>(Address{ vt[17] }, &Hook_CBV);
    g_hk_rtv       = std::make_unique<FunctionHook>(Address{ vt[20] }, &Hook_RTV);  // CreateRenderTargetView
    const bool ok = g_hk_committed->create() && g_hk_placed->create() && g_hk_cbv->create() && g_hk_rtv->create();
    spdlog::info("[FH5CB] device buffer-tracking hooks {} (committed=vt27 placed=vt29 cbv=vt17 rtv=vt20)", ok ? "installed" : "FAILED");
    if (!ok) { g_hk_committed.reset(); g_hk_placed.reset(); g_hk_cbv.reset(); g_hk_rtv.reset(); g_dev_done.store(false); }
}

// D3D12CreateDevice detour — catches the game device at creation (before the camera ring is allocated) and
// installs the buffer-tracking hooks. Mirrors FH5CameraProbe/src/DxgiProxy.cpp:557.
using FnCreateDevice = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
std::unique_ptr<FunctionHook> g_hk_createdev;

HRESULT WINAPI Hook_CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL fl, REFIID riid, void** ppDevice) {
    auto orig = g_hk_createdev->get_original<FnCreateDevice>();
    HRESULT hr = orig(adapter, fl, riid, ppDevice);
    if (SUCCEEDED(hr) && ppDevice && *ppDevice) {
        ID3D12Device* dev = nullptr;
        if (SUCCEEDED(reinterpret_cast<IUnknown*>(*ppDevice)->QueryInterface(IID_PPV_ARGS(&dev))) && dev) {
            install_device_hooks(dev);
            dev->Release();
        }
    }
    return hr;
}

// ---- UI-draw redirect: OMSetRenderTargets + DrawIndexedInstanced hooks + UI render target -----------
// Strategy (draw-level, scene-robust): OMSetRenderTargets RECORDS the per-command-list binding (is the
// backbuffer bound? + its RTV/DSV handles). DrawIndexedInstanced then redirects ONLY large backbuffer
// draws (the UI: idx>=30) to the UI RT, restoring the backbuffer binding afterwards — so the tiny 3-index
// fullscreen WORLD COMPOSITE stays on the backbuffer (eyes=clean world) while the HUD/menus land on the UI
// RT (quad). Index count, not bind order, is the discriminator (the bind-order heuristic mis-split scenes).
using FnOMSetRT = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT,
    const D3D12_CPU_DESCRIPTOR_HANDLE*, BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE*);
using FnDrawIdx = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, INT, UINT);
std::unique_ptr<FunctionHook> g_hk_omsetrt, g_hk_drawidx;
std::atomic<bool> g_uir_installed{ false };
ID3D12Resource*       g_uir_ui_rt = nullptr;      // UI-only RT (alpha); process-lifetime singleton
ID3D12DescriptorHeap* g_uir_rtv_heap = nullptr;
D3D12_CPU_DESCRIPTOR_HANDLE g_uir_ui_rtv{};
std::atomic<bool>     g_uir_rt_valid{ false };
std::atomic<bool>     g_uir_cleared{ false };     // UI RT cleared this frame
std::atomic<uint64_t> g_uir_redirects{ 0 };       // total UI draws redirected (diagnostic)
constexpr UINT kUiIdxThreshold = 12;              // composite=3 (kept); UI draws idx>=30 (redirected)

struct ListBinding { bool bb_bound{false}; D3D12_CPU_DESCRIPTOR_HANDLE bb_rtv{}; bool has_dsv{false}; D3D12_CPU_DESCRIPTOR_HANDLE dsv{}; };
std::mutex g_uir_state_mtx;
std::unordered_map<ID3D12GraphicsCommandList*, ListBinding> g_uir_list_state;

void RecordListBinding(ID3D12GraphicsCommandList* list, bool bb, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                       bool has_dsv, D3D12_CPU_DESCRIPTOR_HANDLE dsv) {
    std::scoped_lock lk(g_uir_state_mtx);
    ListBinding& s = g_uir_list_state[list];
    s.bb_bound = bb; s.bb_rtv = rtv; s.has_dsv = has_dsv; s.dsv = dsv;
}
bool GetListBinding(ID3D12GraphicsCommandList* list, ListBinding& out) {
    std::scoped_lock lk(g_uir_state_mtx);
    auto it = g_uir_list_state.find(list);
    if (it == g_uir_list_state.end()) return false;
    out = it->second; return true;
}

void STDMETHODCALLTYPE Hook_OMSetRT(ID3D12GraphicsCommandList* self, UINT num,
    const D3D12_CPU_DESCRIPTOR_HANDLE* rts, BOOL single, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
    auto orig = g_hk_omsetrt->get_original<FnOMSetRT>();
    orig(self, num, rts, single, dsv);   // bind normally; we only RECORD here
    __try {
        if (g_uir_rt_valid.load(std::memory_order_relaxed)) {
            const bool bb = (num >= 1 && rts && IsBackbufferRtv((size_t)rts[0].ptr));
            RecordListBinding(self, bb, (num >= 1 && rts) ? rts[0] : D3D12_CPU_DESCRIPTOR_HANDLE{},
                              dsv != nullptr, dsv ? *dsv : D3D12_CPU_DESCRIPTOR_HANDLE{});
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

std::atomic<uint32_t> g_uir_bbdraw_count{ 0 };   // diagnostic: backbuffer-bound DrawIndexed calls / frame
std::atomic<uint32_t> g_uir_bbdraw_maxidx{ 0 };  // diagnostic: largest idxCount among them

void STDMETHODCALLTYPE Hook_DrawIdx(ID3D12GraphicsCommandList* self, UINT idxCount, UINT instCount,
    UINT startIdx, INT baseVtx, UINT startInst) {
    auto orig = g_hk_drawidx->get_original<FnDrawIdx>();
    bool redirected = false;
    __try {
        if (g_ctl_ui_redirect.load(std::memory_order_relaxed) && g_uir_rt_valid.load(std::memory_order_relaxed)) {
            ListBinding s;
            const bool bb = GetListBinding(self, s) && s.bb_bound;
            if (bb) {   // diagnostic: characterize backbuffer-bound draw sizes
                g_uir_bbdraw_count.fetch_add(1, std::memory_order_relaxed);
                uint32_t cur = g_uir_bbdraw_maxidx.load(std::memory_order_relaxed);
                while (idxCount > cur && !g_uir_bbdraw_maxidx.compare_exchange_weak(cur, idxCount, std::memory_order_relaxed)) {}
            }
            if (bb && idxCount > kUiIdxThreshold) {   // a large draw into the backbuffer == UI
                if (!g_uir_cleared.exchange(true, std::memory_order_relaxed)) {
                    const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    self->ClearRenderTargetView(g_uir_ui_rtv, zero, 0, nullptr);
                }
                auto omset = g_hk_omsetrt->get_original<FnOMSetRT>();
                omset(self, 1, &g_uir_ui_rtv, FALSE, s.has_dsv ? &s.dsv : nullptr);   // -> UI RT
                orig(self, idxCount, instCount, startIdx, baseVtx, startInst);        // draw the UI element
                omset(self, 1, &s.bb_rtv, FALSE, s.has_dsv ? &s.dsv : nullptr);       // restore backbuffer
                g_uir_redirects.fetch_add(1, std::memory_order_relaxed);
                redirected = true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { redirected = false; }
    if (!redirected) orig(self, idxCount, instCount, startIdx, baseVtx, startInst);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void install_createdevice_hook(HMODULE d3d12) {
    static std::atomic<bool> done{ false };
    if (!d3d12 || done.exchange(true)) return;
    auto* p = reinterpret_cast<void*>(GetProcAddress(d3d12, "D3D12CreateDevice"));
    if (!p) { spdlog::warn("[FH5CB] D3D12CreateDevice export not found"); done.store(false); return; }
    g_hk_createdev = std::make_unique<FunctionHook>(Address{ p }, &Hook_CreateDevice);
    if (g_hk_createdev->create()) spdlog::info("[FH5CB] D3D12CreateDevice hooked @0x{:X}", reinterpret_cast<uintptr_t>(p));
    else { g_hk_createdev.reset(); done.store(false); spdlog::warn("[FH5CB] D3D12CreateDevice hook FAILED"); }
}

void ensure_installed(ID3D12Device* device) {
    // Fallback: if the CreateDevice hook missed (e.g. installed late), still install the device hooks now.
    // The transform happens in Hook_CBV (no command-queue hook needed).
    install_device_hooks(device);
}

void ui_redirect_install(ID3D12Device* device, IDXGISwapChain* swapchain) {
    if (!device || !swapchain || g_uir_installed.exchange(true)) return;

    // Resolve which already-recorded RTV handles point to swapchain backbuffers (the game creates these at
    // swapchain setup; the early CreateRenderTargetView hook captured them into g_rtv_map).
    DXGI_SWAP_CHAIN_DESC scd{};
    if (FAILED(swapchain->GetDesc(&scd))) { g_uir_installed.store(false); return; }
    std::vector<ID3D12Resource*> bbs;
    for (UINT i = 0; i < scd.BufferCount && i < 8; ++i) {
        ID3D12Resource* b = nullptr;
        if (SUCCEEDED(swapchain->GetBuffer(i, IID_PPV_ARGS(&b))) && b) { bbs.push_back(b); b->Release(); } // identity only
    }
    {
        std::scoped_lock lk(g_rtv_mtx);
        g_uir_bb_resources = bbs;            // remember backbuffer ptrs so Hook_RTV tags future RTVs too
        g_uir_bb_rtvs.clear();
        for (auto& kv : g_rtv_map)
            for (auto* b : bbs) if (kv.second == b) { g_uir_bb_rtvs.insert(kv.first); break; }
    }

    // Create the UI-only render target (backbuffer-sized RGBA8) + its RTV.
    const UINT w = scd.BufferDesc.Width  ? scd.BufferDesc.Width  : 1152;
    const UINT h = scd.BufferDesc.Height ? scd.BufferDesc.Height : 864;
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = w; rd.Height = h;
    rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rd.SampleDesc.Count = 1; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE cv{}; cv.Format = rd.Format;   // transparent black
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, IID_PPV_ARGS(&g_uir_ui_rt))) || !g_uir_ui_rt) {
        spdlog::warn("[FH5UIR] UI RT create failed"); return;
    }
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
    if (FAILED(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g_uir_rtv_heap))) || !g_uir_rtv_heap) {
        spdlog::warn("[FH5UIR] RTV heap create failed"); return;
    }
    g_uir_ui_rtv = g_uir_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(g_uir_ui_rt, nullptr, g_uir_ui_rtv);

    // Hook OMSetRenderTargets (ID3D12GraphicsCommandList vtable index 46) via a throwaway list's vtable
    // (shared across all lists from this device, so this catches FH5's UI-recording list too).
    ID3D12CommandAllocator* alloc = nullptr; ID3D12GraphicsCommandList* cl = nullptr;
    if (SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))) && alloc &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&cl))) && cl) {
        void** cvt = *reinterpret_cast<void***>(cl);
        g_hk_omsetrt = std::make_unique<FunctionHook>(Address{ cvt[46] }, &Hook_OMSetRT);     // OMSetRenderTargets
        g_hk_drawidx = std::make_unique<FunctionHook>(Address{ cvt[13] }, &Hook_DrawIdx);     // DrawIndexedInstanced
        if (!g_hk_omsetrt->create()) { g_hk_omsetrt.reset(); spdlog::warn("[FH5UIR] OMSetRenderTargets hook FAILED"); }
        if (!g_hk_drawidx->create()) { g_hk_drawidx.reset(); spdlog::warn("[FH5UIR] DrawIndexedInstanced hook FAILED"); }
    }
    if (cl) cl->Release();
    if (alloc) alloc->Release();

    g_uir_rt_valid.store(g_hk_omsetrt != nullptr && g_hk_drawidx != nullptr, std::memory_order_release);
    spdlog::info("[FH5UIR] installed: UI RT {}x{} bbRTVs={} omsetrt={} drawidx={}",
                 w, h, g_uir_bb_rtvs.size(), g_hk_omsetrt ? 1 : 0, g_hk_drawidx ? 1 : 0);
}

void ui_redirect_on_present() {
    static uint64_t s_last_log = 0;
    static uint64_t s_last_redirects = 0;
    g_uir_cleared.store(false, std::memory_order_relaxed);   // re-clear the UI RT next frame
    const uint64_t now = GetTickCount64();
    const uint32_t bbdraws = g_uir_bbdraw_count.exchange(0, std::memory_order_relaxed);
    const uint32_t maxidx = g_uir_bbdraw_maxidx.exchange(0, std::memory_order_relaxed);
    if (g_ctl_ui_redirect.load(std::memory_order_relaxed) && now - s_last_log >= 1000) {
        s_last_log = now;
        const uint64_t tot = g_uir_redirects.load(std::memory_order_relaxed);
        spdlog::info("[FH5UIR] frame: bbDraws={} maxIdx={} redirected~{}/frame (total {})",
                     bbdraws, maxidx, tot - s_last_redirects, tot);
        s_last_redirects = tot;
    }
}

ID3D12Resource* ui_redirect_target() {
    return (g_ctl_ui_redirect.load(std::memory_order_relaxed) && g_uir_rt_valid.load(std::memory_order_relaxed)
            && g_uir_redirects.load(std::memory_order_relaxed) > 0) ? g_uir_ui_rt : nullptr;
}

bool ui_redirect_active() { return ui_redirect_target() != nullptr; }

void set_eye_offset(float view_x, float view_y, float view_z, bool active) {
    g_off_x.store(view_x, std::memory_order_relaxed);
    g_off_y.store(view_y, std::memory_order_relaxed);
    g_off_z.store(view_z, std::memory_order_relaxed);
    g_active.store(active, std::memory_order_relaxed);
    if (!g_ctl_started.exchange(true)) {   // start the live-tuning control-file poller once
        if (HANDLE t = CreateThread(nullptr, 0, &ControlThread, nullptr, 0, nullptr)) {
            CloseHandle(t);
            spdlog::info("[FH5CTL] control-file poller started");
        } else {
            g_ctl_started.store(false);
            spdlog::warn("[FH5CTL] failed to start control-file poller");
        }
    }
}

float ctl_half_ipd()    { return g_ctl_half_ipd.load(std::memory_order_relaxed); }
float ctl_world_scale() { return g_ctl_world_scale.load(std::memory_order_relaxed); }
int   ctl_recenter_seq(){ return g_ctl_recenter_seq.load(std::memory_order_relaxed); }
int   ctl_rotation_mode(){ return g_ctl_rot_mode.load(std::memory_order_relaxed); }
bool  ctl_projection_enabled(){ return g_ctl_projection.load(std::memory_order_relaxed); }
int   ctl_pos_lane()    { return g_ctl_pos_lane.load(std::memory_order_relaxed); }
int   ctl_pokerot()     { return g_ctl_pokerot.load(std::memory_order_relaxed); }
int   ctl_pokerotvs()   { return g_ctl_pokerotvs.load(std::memory_order_relaxed); }
bool  ctl_dumpcam()     { return g_ctl_dumpcam.load(std::memory_order_relaxed); }
bool  ctl_hud_quad()    { return g_ctl_hud_quad.load(std::memory_order_relaxed); }
bool  ctl_hud_opaque()  { return g_ctl_hud_opaque.load(std::memory_order_relaxed); }
float ctl_hud_w()       { return g_ctl_hud_w.load(std::memory_order_relaxed); }
float ctl_hud_x()       { return g_ctl_hud_x.load(std::memory_order_relaxed); }
float ctl_hud_y()       { return g_ctl_hud_y.load(std::memory_order_relaxed); }
float ctl_hud_z()       { return g_ctl_hud_z.load(std::memory_order_relaxed); }
bool  ctl_ui_redirect() { return g_ctl_ui_redirect.load(std::memory_order_relaxed); }
const char* pos_lane_name(int lane) {
    switch (lane) {
    case kPosLaneCcam320: return "ccam320";
    case kPosLaneCcam320D550: return "ccam320_d550";
    case kPosLaneClone0: return "clone0";
    case kPosLaneClone1: return "clone1";
    case kPosLaneClone2: return "clone2";
    case kPosLaneDownstream: return "downstream";
    case kPosLaneOff: return "off";
    case kPosLaneViewTail: return "viewtail";
    case kPosLaneInput540: return "input540";
    case kPosLaneProducerA15: return "proda15";
    case kPosLaneCamSrc: return "camsrc";
    default: return "unknown";
    }
}
float ctl_up_fwd()      { return g_ctl_fwd.load(std::memory_order_relaxed); }
float ctl_up_strafe()   { return g_ctl_strafe.load(std::memory_order_relaxed); }
float ctl_up_up()       { return g_ctl_up.load(std::memory_order_relaxed); }
int   ctl_up_tgt()      { return g_ctl_tgt.load(std::memory_order_relaxed); }

unsigned long long ring_writes()     { return g_ring_writes.load(std::memory_order_relaxed); }
unsigned long long buffers_tracked() { return g_buf_count.load(std::memory_order_relaxed); }
unsigned long long cam_hits()        { return g_cam_hits.load(std::memory_order_relaxed); }
unsigned long long cbv6912_count()   { return g_cbv6912.load(std::memory_order_relaxed); }

} // namespace fh5cb
