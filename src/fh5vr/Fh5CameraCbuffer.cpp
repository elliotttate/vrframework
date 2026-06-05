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
// Phase 0). Lines: "ipd=0.032" (half-IPD in FH5 units), "scale=100" (head-trans
// units/metre), "mode=off|camrel|viewvp|all".
// ---------------------------------------------------------------------------
std::atomic<float> g_ctl_half_ipd{ 3.15f };
std::atomic<float> g_ctl_world_scale{ 100.0f };
std::atomic<int>   g_ctl_mode{ 3 };   // 0=off 1=camrel_only 2=view_vp_only 3=all
std::atomic<bool>  g_ctl_started{ false };

void poll_control_file() {
    FILE* f = nullptr;
    if (fopen_s(&f, "E:\\tmp\\fh5vr_ctl.txt", "rb") != 0 || f == nullptr) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        float v; int iv;
        if (sscanf_s(line, "ipd=%f", &v) == 1)        g_ctl_half_ipd.store(v, std::memory_order_relaxed);
        else if (sscanf_s(line, "scale=%f", &v) == 1) g_ctl_world_scale.store(v, std::memory_order_relaxed);
        else if (strncmp(line, "mode=", 5) == 0) {
            if      (strncmp(line + 5, "off", 3) == 0)    iv = 0;
            else if (strncmp(line + 5, "camrel", 6) == 0) iv = 1;
            else if (strncmp(line + 5, "viewvp", 6) == 0) iv = 2;
            else                                          iv = 3;
            g_ctl_mode.store(iv, std::memory_order_relaxed);
        }
    }
    fclose(f);
}

DWORD WINAPI ControlThread(void*) {
    int last_logged = -999;
    for (;;) {
        poll_control_file();
        const int m = g_ctl_mode.load(std::memory_order_relaxed);
        const int sig = m * 100000 + (int)(g_ctl_half_ipd.load(std::memory_order_relaxed) * 1000);
        if (sig != last_logged) {
            last_logged = sig;
            spdlog::info("[FH5CTL] half_ipd={:.3f} scale={:.1f} mode={}",
                         g_ctl_half_ipd.load(), g_ctl_world_scale.load(), m);
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
    const bool ok = g_hk_committed->create() && g_hk_placed->create() && g_hk_cbv->create();
    spdlog::info("[FH5CB] device buffer-tracking hooks {} (committed=vt27 placed=vt29 cbv=vt17)", ok ? "installed" : "FAILED");
    if (!ok) { g_hk_committed.reset(); g_hk_placed.reset(); g_hk_cbv.reset(); g_dev_done.store(false); }
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

void set_eye_offset(float view_x, float view_y, float view_z, bool active) {
    g_off_x.store(view_x, std::memory_order_relaxed);
    g_off_y.store(view_y, std::memory_order_relaxed);
    g_off_z.store(view_z, std::memory_order_relaxed);
    g_active.store(active, std::memory_order_relaxed);
    if (!g_ctl_started.exchange(true)) {   // start the live-tuning control-file poller once
        if (HANDLE t = CreateThread(nullptr, 0, &ControlThread, nullptr, 0, nullptr)) CloseHandle(t);
    }
}

float ctl_half_ipd()    { return g_ctl_half_ipd.load(std::memory_order_relaxed); }
float ctl_world_scale() { return g_ctl_world_scale.load(std::memory_order_relaxed); }

unsigned long long ring_writes()     { return g_ring_writes.load(std::memory_order_relaxed); }
unsigned long long buffers_tracked() { return g_buf_count.load(std::memory_order_relaxed); }
unsigned long long cam_hits()        { return g_cam_hits.load(std::memory_order_relaxed); }
unsigned long long cbv6912_count()   { return g_cbv6912.load(std::memory_order_relaxed); }

} // namespace fh5cb
