// FH5 UPSTREAM camera-position writer — see Fh5CamDriver.hpp for the rationale.
//
// Ported from the proven standalone freecam FH5CameraProbe/src/CamDriverMatrixFreecamDll.cpp (resolve +
// poll + matrix-write loop that moved the FH5 camera coherently), with the freecam's WASD/mouse/ImGui input
// and FILE* CSV logging stripped. The camera-relative offset now comes ONLY from the existing control module
// (fh5cb::ctl_up_*), applied ONLY when fh5cb::ctl_up_tgt() == 5 ("driver"). Heartbeat goes to spdlog.

#include "Fh5CamDriver.hpp"
#include "Fh5CameraCbuffer.hpp"

#include <windows.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fh5cam {
namespace {

// ---------------------------------------------------------------------------
// IDA-derived constants (image base 0x140000000). Identical to the freecam.
// ---------------------------------------------------------------------------
constexpr uintptr_t kIdaImageBase = 0x140000000ull;
constexpr uintptr_t kForzaMultiCamRefcountIdaVa = 0x147053C50ull;  // ForzaMultiCam vtable (refcount slot)
constexpr uintptr_t kCCamDriverRefcountIdaVa = 0x1467F74C8ull;     // CCamDriver vtable (refcount slot)
constexpr uintptr_t kObjectStorageOffset = 0x10ull;               // control -> object adjustment
constexpr uintptr_t kActiveCameraSlotOffset = 0x5C8ull;           // multicam object -> active {object,control}
constexpr uintptr_t kCameraMatrixOffset = 0x320ull;               // CCamDriver -> camera-to-world matrix
constexpr uintptr_t kViewTailOffset = 0x360ull;                   // CCamDriver -> inverse view-tail (matrix+64; the PROVEN freecam writes here, NOT +0x3E0)
constexpr int kTgtDriver = 5;                                     // fh5cb::ctl_up_tgt() value for "driver"

// ---------------------------------------------------------------------------
// Resolved-once globals (the worker thread is the only writer/reader after start).
// ---------------------------------------------------------------------------
uintptr_t g_module_base = 0;
uintptr_t g_multicam_control = 0;
uintptr_t g_multicam_object = 0;
uintptr_t g_active_slot = 0;
uintptr_t g_driver_vtbl = 0;

std::atomic<bool> g_started{ false };

// ---------------------------------------------------------------------------
// Plain POD math types and helpers (no destructors — safe near __try scopes).
// ---------------------------------------------------------------------------
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Matrix4 {
    std::array<float, 16> m{};
};

struct Pose {
    Vec3 right{ 1.0f, 0.0f, 0.0f };
    Vec3 up{ 0.0f, 1.0f, 0.0f };
    Vec3 forward{ 0.0f, 0.0f, 1.0f };
    Vec3 position{};
};

// ---- SEH-guarded raw memory access (PODs only inside __try; legal) --------
template <typename T>
bool SafeRead(uintptr_t address, T& out) {
    if (!address) return false;
    __try {
        out = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::memset(&out, 0, sizeof(out));
        return false;
    }
}

bool SafeCopyIn(uintptr_t address, void* out, size_t size) {
    __try {
        std::memcpy(out, reinterpret_cast<const void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SafeCopyOut(uintptr_t address, const void* in, size_t size) {
    __try {
        std::memcpy(reinterpret_cast<void*>(address), in, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---- vector math ----------------------------------------------------------
float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }

bool IsFinite(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool LooksLikeRotationBasis(const Vec3& r, const Vec3& u, const Vec3& f) {
    if (!IsFinite(r) || !IsFinite(u) || !IsFinite(f)) return false;
    const float rl = Length(r), ul = Length(u), fl = Length(f);
    if (rl < 0.80f || rl > 1.20f || ul < 0.80f || ul > 1.20f || fl < 0.80f || fl > 1.20f) return false;
    return std::fabs(Dot(r, u)) <= 0.20f &&
           std::fabs(Dot(r, f)) <= 0.20f &&
           std::fabs(Dot(u, f)) <= 0.20f;
}

// Decode the +0x320 row-major camera-to-world matrix into a validated pose.
bool DecodePose(const Matrix4& matrix, Pose& pose) {
    for (float value : matrix.m) {
        if (!std::isfinite(value)) return false;
    }
    if (std::fabs(matrix.m[3]) > 0.25f ||
        std::fabs(matrix.m[7]) > 0.25f ||
        std::fabs(matrix.m[11]) > 0.25f ||
        std::fabs(matrix.m[15] - 1.0f) > 0.25f) {
        return false;
    }
    Pose out{};
    out.right = { matrix.m[0], matrix.m[1], matrix.m[2] };
    out.up = { matrix.m[4], matrix.m[5], matrix.m[6] };
    out.forward = { matrix.m[8], matrix.m[9], matrix.m[10] };
    out.position = { matrix.m[12], matrix.m[13], matrix.m[14] };
    if (!LooksLikeRotationBasis(out.right, out.up, out.forward) || !IsFinite(out.position)) return false;
    pose = out;
    return true;
}

// Inverse view-tail at +0x3E0: rotation = transpose of the basis; translation = -dot(pos, axis).
Matrix4 ViewTailFromPose(const Pose& pose) {
    Matrix4 matrix{};
    matrix.m[0] = pose.right.x;
    matrix.m[1] = pose.up.x;
    matrix.m[2] = pose.forward.x;
    matrix.m[3] = 0.0f;
    matrix.m[4] = pose.right.y;
    matrix.m[5] = pose.up.y;
    matrix.m[6] = pose.forward.y;
    matrix.m[7] = 0.0f;
    matrix.m[8] = pose.right.z;
    matrix.m[9] = pose.up.z;
    matrix.m[10] = pose.forward.z;
    matrix.m[11] = 0.0f;
    matrix.m[12] = -Dot(pose.position, pose.right);
    matrix.m[13] = -Dot(pose.position, pose.up);
    matrix.m[14] = -Dot(pose.position, pose.forward);
    matrix.m[15] = 1.0f;
    return matrix;
}

// ---------------------------------------------------------------------------
// ForzaMultiCam resolution: scan committed data memory for the fmc vtable value,
// then validate the candidate has a plausible active-camera slot (one-time cost).
// ---------------------------------------------------------------------------
bool IsReadableProtect(DWORD protect) {
    if ((protect & PAGE_GUARD) != 0) return false;
    const DWORD base = protect & 0xff;
    return base == PAGE_READONLY || base == PAGE_READWRITE || base == PAGE_WRITECOPY ||
           base == PAGE_EXECUTE_READ || base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool IsDataProtect(DWORD protect) {
    const DWORD base = protect & 0xff;
    return base == PAGE_READONLY || base == PAGE_READWRITE || base == PAGE_WRITECOPY;
}

bool AcceptMulticamCandidate(uintptr_t control) {
    const uintptr_t object = control + kObjectStorageOffset;
    const uintptr_t active_slot = object + kActiveCameraSlotOffset;
    uintptr_t active_object = 0;
    uintptr_t active_control = 0;
    uintptr_t active_vtbl = 0;
    if (!SafeRead(active_slot, active_object) ||
        !SafeRead(active_slot + sizeof(uintptr_t), active_control) ||
        !active_object || !active_control ||
        !SafeRead(active_control, active_vtbl)) {
        return false;
    }
    if (active_object != active_control + kObjectStorageOffset) return false;
    if (active_vtbl < g_module_base || active_vtbl >= g_module_base + 0xB000000ull) return false;
    g_multicam_control = control;
    g_multicam_object = object;
    g_active_slot = active_slot;
    return true;
}

uintptr_t ScanRangeForValidMulticam(uintptr_t target_value, uintptr_t start, uintptr_t end) {
    uintptr_t cursor = start;
    constexpr size_t kMaxChunk = 4 * 1024 * 1024;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi))) {
            cursor += 0x1000;
            continue;
        }
        const uintptr_t region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t region_end = region_base + mbi.RegionSize;
        const uintptr_t read_start = std::max(region_base, cursor);
        const uintptr_t read_end = std::min(region_end, end);
        if (mbi.State == MEM_COMMIT &&
            (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED) &&
            IsReadableProtect(mbi.Protect) &&
            IsDataProtect(mbi.Protect) &&
            read_start + sizeof(uint64_t) <= read_end) {
            uintptr_t chunk_start = read_start;
            while (chunk_start + sizeof(uint64_t) <= read_end) {
                const size_t chunk_size = static_cast<size_t>(std::min<uintptr_t>(read_end - chunk_start, kMaxChunk));
                std::vector<uint8_t> buffer(chunk_size);
                if (SafeCopyIn(chunk_start, buffer.data(), buffer.size())) {
                    for (size_t i = 0; i + sizeof(uint64_t) <= buffer.size(); i += sizeof(uint64_t)) {
                        uint64_t value = 0;
                        std::memcpy(&value, buffer.data() + i, sizeof(value));
                        if (value == target_value && AcceptMulticamCandidate(chunk_start + i)) {
                            return chunk_start + i;
                        }
                    }
                }
                chunk_start += chunk_size;
            }
        }
        cursor = region_end > cursor ? region_end : cursor + 0x1000;
    }
    return 0;
}

bool ResolveMulticam() {
    g_module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"ForzaHorizon5.exe"));
    if (!g_module_base) g_module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!g_module_base) return false;
    const uintptr_t multicam_vtbl = g_module_base + (kForzaMultiCamRefcountIdaVa - kIdaImageBase);
    g_driver_vtbl = g_module_base + (kCCamDriverRefcountIdaVa - kIdaImageBase);
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return ScanRangeForValidMulticam(multicam_vtbl,
                                     reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress),
                                     reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress)) != 0;
}

// Read the active CCamDriver via the multicam active slot; validate the control vtable.
bool ReadActiveDriver(uintptr_t& object, uintptr_t& control) {
    object = 0;
    control = 0;
    if (!SafeRead(g_active_slot, object) ||
        !SafeRead(g_active_slot + sizeof(uintptr_t), control) ||
        !object || !control) {
        return false;
    }
    uintptr_t control_vtbl = 0;
    if (!SafeRead(control, control_vtbl) || control_vtbl != g_driver_vtbl) return false;
    return object == control + kObjectStorageOffset;
}

uint64_t NowMs() {
    static LARGE_INTEGER freq{};
    static LARGE_INTEGER start{};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
    }
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>((now.QuadPart - start.QuadPart) * 1000 / freq.QuadPart);
}

// ---------------------------------------------------------------------------
// Worker thread: resolve once, then poll the active driver pose ~every 4 ms and
// apply the control-file offset (only when tgt == driver) to the FRESH engine pose.
// ---------------------------------------------------------------------------
DWORD WINAPI WorkerThread(void*) {
    spdlog::info("[FH5CAM] upstream CCamDriver position writer thread up (idle until tgt=driver)");

    // last_written guard: the engine recomputes +0x320 every frame as the car moves. We only apply our
    // offset to a FRESH engine pose — if the current matrix still equals what WE last wrote (engine hasn't
    // refreshed), skip to avoid double-applying the offset on top of our own output. Plain POD (no dtor).
    Matrix4 last_written{};
    bool have_last_written = false;
    bool resolved = false;

    uint64_t last_status_ms = 0;
    for (;;) {
        // IDLE unless the user explicitly engages the upstream test (tgt=driver). This avoids the heavy
        // full-process-memory ForzaMultiCam scan running during the game's startup/loading (which crashed
        // the GPU driver). The scan + polling only happen on demand.
        if (fh5cb::ctl_up_tgt() != kTgtDriver) {
            have_last_written = false;
            Sleep(100);
            continue;
        }
        if (!resolved) {
            if (!ResolveMulticam()) { Sleep(500); continue; }
            resolved = true;
            spdlog::info("[FH5CAM] resolved: module_base=0x{:X} multicam_object=0x{:X} active_slot=0x{:X}",
                         g_module_base, g_multicam_object, g_active_slot);
        }

        uintptr_t active_object = 0;
        uintptr_t active_control = 0;
        if (!ReadActiveDriver(active_object, active_control)) {
            have_last_written = false;
            Sleep(4);
            continue;
        }

        const uintptr_t matrix_address = active_object + kCameraMatrixOffset;
        const uintptr_t viewtail_address = active_object + kViewTailOffset;

        Matrix4 base{};
        Pose base_pose{};
        if (!SafeCopyIn(matrix_address, base.m.data(), sizeof(float) * base.m.size()) ||
            !DecodePose(base, base_pose)) {
            Sleep(4);
            continue;
        }

        // Read the offset from the existing control module. Apply ONLY when tgt == driver.
        const bool apply = (fh5cb::ctl_up_tgt() == kTgtDriver);
        const float off_strafe = fh5cb::ctl_up_strafe();
        const float off_up = fh5cb::ctl_up_up();
        const float off_fwd = fh5cb::ctl_up_fwd();

        bool applied = false;
        Pose out_pose = base_pose;
        if (apply) {
            // Skip if the engine hasn't refreshed since our last write (else we'd offset our own output).
            bool engine_refreshed = true;
            if (have_last_written) {
                engine_refreshed = std::memcmp(base.m.data(), last_written.m.data(),
                                               sizeof(float) * base.m.size()) != 0;
            }
            if (engine_refreshed) {
                // pos += strafe*right + up*up_axis + fwd*forward, using the matrix's OWN basis rows.
                out_pose.position.x = base_pose.position.x +
                    off_strafe * base_pose.right.x + off_up * base_pose.up.x + off_fwd * base_pose.forward.x;
                out_pose.position.y = base_pose.position.y +
                    off_strafe * base_pose.right.y + off_up * base_pose.up.y + off_fwd * base_pose.forward.y;
                out_pose.position.z = base_pose.position.z +
                    off_strafe * base_pose.right.z + off_up * base_pose.up.z + off_fwd * base_pose.forward.z;

                Matrix4 out = base;
                out.m[12] = out_pose.position.x;
                out.m[13] = out_pose.position.y;
                out.m[14] = out_pose.position.z;
                const Matrix4 view_tail = ViewTailFromPose(out_pose);

                SafeCopyOut(matrix_address, out.m.data(), sizeof(float) * out.m.size());
                SafeCopyOut(viewtail_address, view_tail.m.data(), sizeof(float) * view_tail.m.size());

                last_written = out;
                have_last_written = true;
                applied = true;
            }
        } else {
            have_last_written = false;   // not applying -> leave the engine pose untouched, reset the guard
        }

        const uint64_t now_ms = NowMs();
        if (now_ms - last_status_ms >= 1000) {
            last_status_ms = now_ms;
            spdlog::info("[FH5CAM] fmc=0x{:X} driver=0x{:X} pos=({:.3f},{:.3f},{:.3f}) "
                         "tgt={} off=({:.3f},{:.3f},{:.3f}) applied={}",
                         g_multicam_control, active_object,
                         out_pose.position.x, out_pose.position.y, out_pose.position.z,
                         fh5cb::ctl_up_tgt(), off_strafe, off_up, off_fwd, applied ? 1 : 0);
        }

        Sleep(4);
    }
    // unreachable
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void start() {
    if (g_started.exchange(true)) return;   // idempotent: launch the worker exactly once
    if (HANDLE t = CreateThread(nullptr, 0, &WorkerThread, nullptr, 0, nullptr)) {
        CloseHandle(t);
    } else {
        g_started.store(false);   // launch failed; allow a later retry
        spdlog::error("[FH5CAM] failed to create worker thread");
    }
}

} // namespace fh5cam
