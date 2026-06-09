// FH5 UPSTREAM camera-position writer — see Fh5CamDriver.hpp for the rationale.
//
// Ported from the proven standalone freecam FH5CameraProbe/src/CamDriverMatrixFreecamDll.cpp (poll +
// matrix-write loop that moved the FH5 camera coherently), with the freecam's WASD/mouse/ImGui input
// and FILE* CSV logging stripped. The live camera pointer is published by deterministic engine hooks when
// the build has a matching AOB; on the Empress 1.405 test build those hooks are absent, so the worker falls
// back to resolving the active +0x320 camera object from the proven producer's live pose hint. The
// camera-relative offset comes from either the existing manual control module (fh5cb::ctl_up_* with
// tgt=driver) or the OpenXR adapter's head/IPD offset publisher. Heartbeat goes to spdlog.

#include "Fh5CamDriver.hpp"
#include "Fh5Adapter.hpp"
#include "Fh5CameraCbuffer.hpp"
#include "Mods.hpp"          // complete type for unique_ptr<Mods> in Framework: this TU references g_framework,
                             // so ~Framework -> ~unique_ptr<Mods> instantiates here and needs Mods defined.

#include <windows.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fh5cam {
void Tick1688DataHeadlook();   // 1.688 no-.text-hook DATA head-look (defined below; called from WorkerThread)
// RETAIL 1.688 head-look levers, defined below (after apply_angle_head_rotation_prewrite) and called from
// Tick1688DataHeadlook. Declared here (NOT in Fh5CamDriver.hpp) on purpose: keeping them out of the header
// avoids re-triggering a recompile of the other fh5vr TUs (Fh5Adapter/Fh5MenuNav) that include it.
bool apply_freelook_headlook_1688(uintptr_t cam);   // +0x5C4 yaw / +0x5F4 pitch / +0x5F8 roll (rot=freelook)
bool apply_ypr540_headlook_1688(uintptr_t cam);     // +0x540 CameraSpaceYPR input lane (rot=ypr540)
namespace {

// ---------------------------------------------------------------------------
// IDA-derived constants (image base 0x140000000).
// ---------------------------------------------------------------------------
constexpr uintptr_t kIdaImageBase = 0x140000000ull;
constexpr uintptr_t kForzaMultiCamRefcountIdaVa = 0x1465D4990ull;  // Empress std::_Ref_count_obj2<Camera::ForzaMultiCam>::vftable
constexpr uintptr_t kCCamDriverRefcountIdaVa = 0x145E1FF90ull;     // Empress std::_Ref_count_obj2<Camera::CCamDriver>::vftable
constexpr uintptr_t kObjectStorageOffset = 0x10ull;               // control -> object adjustment
constexpr uintptr_t kActiveCameraSlotOffset = 0x5C8ull;           // multicam object -> active {object,control}
constexpr uintptr_t kCameraMatrixOffset = 0x320ull;               // CCamDriver -> camera-to-world matrix
constexpr uintptr_t kViewTailOffset = 0x360ull;                   // CCamDriver -> inverse view-tail (matrix+64; the PROVEN freecam writes here, NOT +0x3E0)
constexpr uintptr_t kInputBaseOffset = 0x530ull;                  // CCamDriver -> engine camera-space base input
constexpr uintptr_t kInputOffsetLane = 0x540ull;                  // CCamDriver -> additive camera-space input lane
constexpr uintptr_t kInputDerivedOffset = 0x550ull;               // CCamDriver -> derived output from +0x530/+0x540
constexpr uintptr_t kInputDerivedTailOffset = 0x570ull;           // CCamDriver -> additional derived output row
constexpr uintptr_t kDoublePositionOffset = 0x550ull;             // legacy diagnostic only; current Empress RE says derived output
constexpr uintptr_t kCCamDriverVtableIdaVa = 0x145E3F290ull;       // Camera::CCamDriver concrete vftable
constexpr uintptr_t kCinematicGameCameraRefcountIdaVa = 0x145FA0440ull; // std::_Ref_count_obj2<Cinematics::CinematicGameCamera>::vftable
constexpr uintptr_t kCinematicMatrixOffset = 0x540ull;            // CinematicGameCamera -> camera-to-world matrix rows 0..3
constexpr int kTgtDriver = 5;                                     // fh5cb::ctl_up_tgt() value for "driver"
constexpr size_t kCameraVtableBytes = 0x430ull;
constexpr size_t kSlot550Float = 0x1B8ull;
constexpr size_t kSlot550Double = 0x1C0ull;
constexpr size_t kSlotUpdate328 = 0x328ull;
constexpr size_t kSlotUpdate330 = 0x330ull;

constexpr uintptr_t kKnownCameraRefcountVtableIdaVas[] = {
    0x145E1FF40ull, // std::_Ref_count_obj2<Camera::CCamFollowLow>
    0x145E1FF68ull, // std::_Ref_count_obj2<Camera::CCamFollowHigh>
    0x145E1FF90ull, // std::_Ref_count_obj2<Camera::CCamDriver>
    0x145E1FFB8ull, // std::_Ref_count_obj2<Camera::CCamHood>
    0x145E1FFE0ull, // std::_Ref_count_obj2<Camera::CCamBumperHigh>
    0x145E20008ull, // std::_Ref_count_obj2<Camera::CCamFree>
    0x145E20030ull, // std::_Ref_count_obj2<Camera::CCamFreeTargetCar>
    0x145E200D0ull, // std::_Ref_count_obj2<Camera::CCamFreeTrack>
    0x145E20148ull, // std::_Ref_count_obj2<Camera::CCamFollowExtended>
};

constexpr uintptr_t kKnownCameraVtableIdaVas[] = {
    0x145E3FFC0ull, // Camera::CCamFollowLow
    0x145E40308ull, // Camera::CCamFollowHigh
    0x145E3F290ull, // Camera::CCamDriver
    0x145E3EBF0ull, // Camera::CCamHood
    0x145E3E550ull, // Camera::CCamBumperHigh
    0x145E40D78ull, // Camera::CCamFree
    0x145E41208ull, // Camera::CCamFreeTargetCar
    0x145E415B8ull, // Camera::CCamFreeTrack
    0x145E40650ull, // Camera::CCamFollowExtended
};

constexpr uintptr_t kKnownMulticamVtableIdaVas[] = {
    0x1465D6808ull, // Camera::ForzaMultiCam concrete table, Empress
    0x1465D6BA0ull, // Camera::ForzaMultiCam concrete table variant, Empress
};

constexpr uintptr_t k1688KnownMulticamVtableIdaVas[] = {
    0x147050068ull, // Camera::ForzaMultiCam concrete table, Steam 1.688
    0x147050408ull, // Camera::ForzaMultiCam secondary table at object+0x530, Steam 1.688
};

// Retail 1.688 ForzaMultiCam refcount vftable (std::_Ref_count_obj2<Camera::ForzaMultiCam>) and concrete
// vtables. Camera-type concrete vtables are still not mapped on 1.688, so the active camera itself is validated
// structurally by its +0x320 basis after the owner ForzaMultiCam vtable matches. Build is fingerprinted by the
// 1.688 pose-writer prologue at RVA 0xF75430.
constexpr uintptr_t k1688ForzaMultiCamRefcountIdaVa = 0x147053C50ull;

// 1.688 pose-writer prologue (Fh5Adapter's fingerprint) — used here only to detect the retail build so the
// camera resolution/head-look relax to structural validation (no Empress vtable list on 1.688).
inline bool IsRetail1688Build() {
    static const int detected = [] {
        const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!base) return 0;
        static const unsigned char kPoseWriter[] = {
            0x48,0x8B,0xC4, 0x48,0x89,0x58,0x08, 0x48,0x89,0x70,0x10, 0x48,0x89,0x78,0x18,
            0x4C,0x89,0x70,0x20, 0x55, 0x48,0x8D,0x68,0xC8, 0x48,0x81,0xEC,0x30,0x01,0x00,0x00
        };
        __try {
            const auto* p = reinterpret_cast<const unsigned char*>(base + 0xF75430);
            for (size_t i = 0; i < sizeof(kPoseWriter); ++i) if (p[i] != kPoseWriter[i]) return 0;
            return 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }();
    return detected != 0;
}

// ---------------------------------------------------------------------------
// Resolved-once globals (the worker thread is the only writer/reader after start).
// ---------------------------------------------------------------------------
uintptr_t g_module_base = 0;
uintptr_t g_multicam_control = 0;
uintptr_t g_multicam_object = 0;
uintptr_t g_active_slot = 0;
uintptr_t g_driver_vtbl = 0;

std::atomic<bool> g_started{ false };
std::atomic<uintptr_t> g_driver_object{ 0 };
std::atomic<uintptr_t> g_shape_driver_object{ 0 };
std::atomic<uint64_t> g_driver_publish_count{ 0 };
std::atomic<uint64_t> g_arg_publish_count{ 0 };
std::atomic<uint64_t> g_shape_resolve_count{ 0 };
std::atomic<uintptr_t> g_multicam_published_object{ 0 };
std::atomic<uint64_t> g_multicam_publish_count{ 0 };
std::atomic<uint32_t> g_hint_gen{ 0 };
std::atomic<uint32_t> g_openxr_offset_gen{ 0 };
std::atomic<bool> g_openxr_offset_active{ false };
std::atomic<uint64_t> g_openxr_publish_count{ 0 };
std::atomic<uint64_t> g_openxr_last_publish_ms{ 0 };

float g_openxr_strafe = 0.0f;
float g_openxr_up = 0.0f;
float g_openxr_fwd = 0.0f;
int g_openxr_eye = 0;

// ---------------------------------------------------------------------------
// Plain POD math types and helpers (no destructors — safe near __try scopes).
// ---------------------------------------------------------------------------
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
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

struct CameraLayout {
    uintptr_t matrix_offset = kCameraMatrixOffset;
    uintptr_t view_tail_offset = kViewTailOffset;
    uintptr_t identity_vtable = 0;
    float class_priority = 0.0f;
    const char* name = "unknown";
    bool writes_view_tail = true;
};

struct CloneTarget {
    uintptr_t address = 0;
    bool view_tail = false;
    float pos_delta = 0.0f;
    float basis_delta = 0.0f;
};

struct ShadowVtableState {
    uintptr_t object = 0;
    uintptr_t original_vtable = 0;
    uintptr_t shadow_vtable = 0;
    size_t shadow_size = 0;
};

struct InputLaneSnapshot {
    Vec4 base530{};
    Vec4 offset540{};
    Vec4 derived550{};
    Vec4 derived560{};
    Vec4 derived570{};
    bool ok530 = false;
    bool ok540 = false;
    bool ok550 = false;
    bool ok560 = false;
    bool ok570 = false;
};

Pose g_pose_hint{};
Pose g_openxr_delta_pose{};

using Vec4GetterFn = Vec4*(__fastcall*)(void*, Vec4*);
using Double4GetterFn = double*(__fastcall*)(void*, double*);
using VoidSelfFn = void(__fastcall*)(void*);
using PtrSelfFn = void*(__fastcall*)(void*);

ShadowVtableState g_shadow{};
Vec4GetterFn g_original_550_float = nullptr;
Double4GetterFn g_original_550_double = nullptr;
VoidSelfFn g_original_update_328 = nullptr;
PtrSelfFn g_original_update_330 = nullptr;

std::atomic<uint64_t> g_shadow_install_count{ 0 };
std::atomic<uint64_t> g_shadow_restore_count{ 0 };
std::atomic<uint64_t> g_550_float_calls{ 0 };
std::atomic<uint64_t> g_550_float_modified{ 0 };
std::atomic<uint64_t> g_550_double_calls{ 0 };
std::atomic<uint64_t> g_550_double_modified{ 0 };
std::atomic<uint64_t> g_550_bad{ 0 };
std::atomic<uint64_t> g_update328_calls{ 0 };
std::atomic<uint64_t> g_update330_calls{ 0 };
std::atomic<uint64_t> g_update_matrix_applied{ 0 };
std::atomic<uint64_t> g_update_matrix_bad{ 0 };
std::atomic<float> g_550_base_x{ 0.0f }, g_550_base_y{ 0.0f }, g_550_base_z{ 0.0f }, g_550_base_w{ 0.0f };
std::atomic<float> g_550_off_x{ 0.0f }, g_550_off_y{ 0.0f }, g_550_off_z{ 0.0f };
std::atomic<float> g_550_out_x{ 0.0f }, g_550_out_y{ 0.0f }, g_550_out_z{ 0.0f }, g_550_out_w{ 0.0f };
std::atomic<uint64_t> g_input540_fold_calls{ 0 };
std::atomic<uint64_t> g_input540_writes{ 0 };
std::atomic<uint64_t> g_input540_bad{ 0 };
std::atomic<uint64_t> g_input540_last_log_ms{ 0 };
std::atomic<uintptr_t> g_input540_base_object{ 0 };
std::atomic<float> g_input540_base_x{ 0.0f };
std::atomic<float> g_input540_base_y{ 0.0f };
std::atomic<float> g_input540_base_z{ 0.0f };
std::atomic<float> g_input540_base_w{ 0.0f };
std::atomic<uint64_t> g_input540_gate_last_log_ms{ 0 };

float BasisScore(const Pose& a, const Pose& b);
float PositionDistance(const Vec3& a, const Vec3& b);
float BasisDistance(const Pose& a, const Pose& b);
float MatrixDistanceSum(const Matrix4& a, const Matrix4& b);
Vec3 Add(const Vec3& a, const Vec3& b);
Vec3 Scale(const Vec3& v, float s);
Vec3 TransformDirection(const Pose& base, const Vec3& local);
void Orthonormalize(Pose& pose);
Matrix4 MatrixFromPose(const Pose& pose);
bool SnapshotPoseHint(Pose& hint);
bool RestoreInput540Base(uintptr_t object);
bool EnsureModuleBase();
bool IsKnownCameraVtable(uintptr_t vtable);
bool IsKnownCameraRefcountVtable(uintptr_t vtable);
bool IsKnownMulticamVtable(uintptr_t vtable);
bool IsRetail1688MulticamVtable(uintptr_t vtable);
bool IsCinematicGameCameraRefcountVtable(uintptr_t vtable);
bool IsCCamDriverVtable(uintptr_t vtable);
bool LooksLikeCameraObjectHeader(uintptr_t object, uintptr_t* vtable_out = nullptr);
bool LooksLikeCameraControlHeader(uintptr_t control, uintptr_t* vtable_out = nullptr);
bool TryDecodeCameraPointer(uintptr_t value, uintptr_t& object);
bool TryDecodeRetail1688CameraPointer(uintptr_t value,
                                      const Pose& hint,
                                      uintptr_t& object,
                                      Pose& pose_out,
                                      float& basis_score_out,
                                      uintptr_t& vtable_out);
float CameraClassPriorityFromRefcountIda(uintptr_t ida_va);
float CameraClassPriorityFromVtable(uintptr_t vtable);
bool ReadActiveDriverFromMulticam(uintptr_t multicam_object, uintptr_t& object);
bool ResolveCameraLayout(uintptr_t object,
                         const Pose& hint,
                         Matrix4& matrix_out,
                         Pose& pose_out,
                         float& basis_score_out,
                         uintptr_t& identity_vtable_out,
                         CameraLayout& layout_out,
                         float min_basis = -3.0f);
std::vector<CloneTarget> ScanMatrixClones(uintptr_t exclude_address, const Matrix4& reference_matrix);
bool ViewTailLooksConsistent(uintptr_t viewtail_address, const Pose& pose);
bool DoublePositionLooksConsistent(uintptr_t object, const Pose& pose);
bool ReadDoublePositionRaw(uintptr_t object, double (&dpos)[4]);
bool WriteDoublePositionRaw(uintptr_t object, const Pose& pose);
bool ReadVec4f(uintptr_t address, Vec4& out);
bool ReadInputLaneSnapshot(uintptr_t object, InputLaneSnapshot& snap);
bool WriteInput540Offset(uintptr_t object, const Vec3& offset);
bool RestoreInput540Base(uintptr_t object);
bool ReadPoseAt(uintptr_t address, Pose& pose);
bool CameraWriterRequested();
bool SnapshotOpenXrPose(float& strafe, float& up, float& fwd, Pose& delta, int& eye);
Vec3 MapCameraOffsetToInput540(const Vec3& camera_offset);
bool BuildInput540Offset(Vec3& offset, bool& manual_active, bool& vr_tracking_active, int& eye);
uint64_t NowMs();
uintptr_t EffectiveObjectVtable(uintptr_t object, uintptr_t vtable);
bool EnsureDriverPositionGetterShadow(uintptr_t object);
void RestoreDriverPositionGetterShadow();

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
Vec3 Add(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vec3 Scale(const Vec3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }

Vec3 Normalize(const Vec3& v, const Vec3& fallback) {
    const float len = Length(v);
    if (len < 0.00001f || !std::isfinite(len)) return fallback;
    return Scale(v, 1.0f / len);
}

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

Vec3 TransformDirection(const Pose& base, const Vec3& local) {
    return Add(Add(Scale(base.right, local.x), Scale(base.up, local.y)), Scale(base.forward, local.z));
}

void Orthonormalize(Pose& pose) {
    pose.right = Normalize(pose.right, { 1.0f, 0.0f, 0.0f });
    pose.up = Add(pose.up, Scale(pose.right, -Dot(pose.up, pose.right)));
    pose.up = Normalize(pose.up, { 0.0f, 1.0f, 0.0f });
    pose.forward = Add(pose.forward, Scale(pose.right, -Dot(pose.forward, pose.right)));
    pose.forward = Add(pose.forward, Scale(pose.up, -Dot(pose.forward, pose.up)));
    pose.forward = Normalize(pose.forward, { 0.0f, 0.0f, 1.0f });
}

Matrix4 MatrixFromPose(const Pose& pose) {
    Matrix4 matrix{};
    matrix.m[0] = pose.right.x;
    matrix.m[1] = pose.right.y;
    matrix.m[2] = pose.right.z;
    matrix.m[3] = 0.0f;
    matrix.m[4] = pose.up.x;
    matrix.m[5] = pose.up.y;
    matrix.m[6] = pose.up.z;
    matrix.m[7] = 0.0f;
    matrix.m[8] = pose.forward.x;
    matrix.m[9] = pose.forward.y;
    matrix.m[10] = pose.forward.z;
    matrix.m[11] = 0.0f;
    matrix.m[12] = pose.position.x;
    matrix.m[13] = pose.position.y;
    matrix.m[14] = pose.position.z;
    matrix.m[15] = 1.0f;
    return matrix;
}

// Inverse view-tail at +0x360: rotation = transpose of the basis; translation = -dot(pos, axis).
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

bool IsWritableDataProtect(DWORD protect) {
    if ((protect & PAGE_GUARD) != 0) return false;
    const DWORD base = protect & 0xff;
    return base == PAGE_READWRITE || base == PAGE_WRITECOPY;
}

bool IsPlausibleUserPointer(uintptr_t value) {
    return value >= 0x10000ull && value < 0x0000800000000000ull;
}

bool AcceptMulticamCandidate(uintptr_t control) {
    const uintptr_t object = control + kObjectStorageOffset;
    const uintptr_t active_slot = object + kActiveCameraSlotOffset;
    uintptr_t active_object = 0;
    uintptr_t active_control = 0;
    uintptr_t multicam_vtbl = 0;
    uintptr_t decoded_active = 0;
    if (!SafeRead(active_slot, active_object) ||
        !SafeRead(active_slot + sizeof(uintptr_t), active_control) ||
        !active_object ||
        !SafeRead(object, multicam_vtbl)) {
        return false;
    }
    const bool retail1688 = IsRetail1688Build();
    if (retail1688) {
        if (!IsRetail1688MulticamVtable(multicam_vtbl)) return false;

        Pose decoded_pose{};
        float decoded_basis = -1000.0f;
        uintptr_t decoded_vtable = 0;
        Pose hint{};
        SnapshotPoseHint(hint);
        if (!TryDecodeRetail1688CameraPointer(active_object, hint, decoded_active, decoded_pose, decoded_basis, decoded_vtable) &&
            !TryDecodeRetail1688CameraPointer(active_control, hint, decoded_active, decoded_pose, decoded_basis, decoded_vtable)) {
            return false;
        }
    } else {
        if (!IsKnownMulticamVtable(multicam_vtbl)) return false;
        if (!TryDecodeCameraPointer(active_object, decoded_active) &&
            !TryDecodeCameraPointer(active_control, decoded_active)) {
            return false;
        }
    }
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
    if (kForzaMultiCamRefcountIdaVa == 0) return false;
    const uintptr_t refcount_ida = IsRetail1688Build() ? k1688ForzaMultiCamRefcountIdaVa : kForzaMultiCamRefcountIdaVa;
    const uintptr_t multicam_vtbl = g_module_base + (refcount_ida - kIdaImageBase);
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

bool ReadActiveDriverFromMulticam(uintptr_t multicam_object, uintptr_t& object) {
    object = 0;
    if (!multicam_object) return false;

    uintptr_t slot0 = 0;
    uintptr_t slot1 = 0;
    if (!SafeRead(multicam_object + kActiveCameraSlotOffset, slot0) ||
        !SafeRead(multicam_object + kActiveCameraSlotOffset + sizeof(uintptr_t), slot1) ||
        !slot0) {
        return false;
    }

    // Seen layouts:
    //   1. slot0 = camera object, slot1 = shared_ptr control block.
    //   2. slot0 = shared_ptr control block, slot1 is transient/unused.
    // Resolve either shape, then let ValidateCameraObject decode +0x320 as the final guard.
    if (TryDecodeCameraPointer(slot0, object)) {
        return true;
    }
    if (TryDecodeCameraPointer(slot1, object)) {
        return true;
    }
    if (slot1 && slot0 == slot1 + kObjectStorageOffset &&
        LooksLikeCameraObjectHeader(slot0)) {
        object = slot0;
        return true;
    }

    return false;
}

bool ResolveCameraLayout(uintptr_t object,
                         const Pose& hint,
                         Matrix4& matrix_out,
                         Pose& pose_out,
                         float& basis_score_out,
                         uintptr_t& identity_vtable_out,
                         CameraLayout& layout_out,
                         float min_basis) {
    matrix_out = {};
    pose_out = {};
    basis_score_out = -1000.0f;
    identity_vtable_out = 0;
    layout_out = {};
    if (object < 0x10000ull) return false;

    uintptr_t object_vtable = 0;
    if (SafeRead(object, object_vtable)) {
        object_vtable = EffectiveObjectVtable(object, object_vtable);
    }
    if (IsKnownCameraVtable(object_vtable)) {
        Matrix4 matrix{};
        Pose pose{};
        if (!SafeCopyIn(object + kCameraMatrixOffset, matrix.m.data(), sizeof(float) * matrix.m.size()) ||
            !DecodePose(matrix, pose)) {
            return false;
        }

        const float basis_score = BasisScore(pose, hint);
        if (basis_score < min_basis) return false;

        CameraLayout layout{};
        layout.matrix_offset = kCameraMatrixOffset;
        layout.view_tail_offset = kViewTailOffset;
        layout.identity_vtable = object_vtable;
        layout.class_priority = CameraClassPriorityFromVtable(object_vtable);
        layout.name = "ccam";
        layout.writes_view_tail = true;

        matrix_out = matrix;
        pose_out = pose;
        basis_score_out = basis_score;
        identity_vtable_out = object_vtable;
        layout_out = layout;
        return true;
    }

    uintptr_t control_vtable = 0;
    if (object > kObjectStorageOffset &&
        SafeRead(object - kObjectStorageOffset, control_vtable) &&
        IsCinematicGameCameraRefcountVtable(control_vtable)) {
        Matrix4 matrix{};
        Pose pose{};
        if (!SafeCopyIn(object + kCinematicMatrixOffset, matrix.m.data(), sizeof(float) * matrix.m.size()) ||
            !DecodePose(matrix, pose)) {
            return false;
        }

        const float basis_score = BasisScore(pose, hint);
        if (basis_score < min_basis) return false;

        CameraLayout layout{};
        layout.matrix_offset = kCinematicMatrixOffset;
        layout.view_tail_offset = 0;
        layout.identity_vtable = control_vtable;
        layout.class_priority = CameraClassPriorityFromVtable(control_vtable);
        layout.name = "cinematic";
        layout.writes_view_tail = false;

        matrix_out = matrix;
        pose_out = pose;
        basis_score_out = basis_score;
        identity_vtable_out = control_vtable;
        layout_out = layout;
        return true;
    }

    return false;
}

bool ResolveRetail1688CameraLayout(uintptr_t object,
                                   const Pose& hint,
                                   Matrix4& matrix_out,
                                   Pose& pose_out,
                                   float& basis_score_out,
                                   uintptr_t& identity_vtable_out,
                                   CameraLayout& layout_out,
                                   float min_basis) {
    matrix_out = {};
    pose_out = {};
    basis_score_out = -1000.0f;
    identity_vtable_out = 0;
    layout_out = {};
    if (!IsRetail1688Build() || object < 0x10000ull) return false;

    Matrix4 matrix{};
    Pose pose{};
    if (!SafeCopyIn(object + kCameraMatrixOffset, matrix.m.data(), sizeof(float) * matrix.m.size()) ||
        !DecodePose(matrix, pose)) {
        return false;
    }

    const float basis_score = BasisScore(pose, hint);
    if (basis_score < min_basis) return false;

    float discr = 0.0f;
    if (SafeRead(object + 0x31C, discr) &&
        std::isfinite(discr) &&
        std::fabs(discr - 1.0f) > 0.25f) {
        return false;
    }

    uintptr_t object_vtable = 0;
    SafeRead(object, object_vtable);
    object_vtable = EffectiveObjectVtable(object, object_vtable);

    CameraLayout layout{};
    layout.matrix_offset = kCameraMatrixOffset;
    layout.view_tail_offset = kViewTailOffset;
    layout.identity_vtable = object_vtable;
    layout.class_priority = 2.50f;
    layout.name = "retail1688_struct";
    layout.writes_view_tail = true;

    matrix_out = matrix;
    pose_out = pose;
    basis_score_out = basis_score;
    identity_vtable_out = object_vtable;
    layout_out = layout;
    return true;
}

bool ValidateCameraObject(uintptr_t object,
                          const Pose& hint,
                          Pose& pose_out,
                          float& basis_score_out,
                          uintptr_t& vtable_out,
                          float min_basis = -3.0f) {
    Matrix4 matrix{};
    CameraLayout layout{};
    return ResolveCameraLayout(object, hint, matrix, pose_out, basis_score_out, vtable_out, layout, min_basis);
}

bool ValidateRetail1688CameraObject(uintptr_t object,
                                    const Pose& hint,
                                    Pose& pose_out,
                                    float& basis_score_out,
                                    uintptr_t& vtable_out,
                                    float min_basis = -3.0f) {
    Matrix4 matrix{};
    CameraLayout layout{};
    return ResolveRetail1688CameraLayout(object, hint, matrix, pose_out, basis_score_out, vtable_out, layout, min_basis);
}

bool TryDecodeRetail1688CameraPointer(uintptr_t value,
                                      const Pose& hint,
                                      uintptr_t& object,
                                      Pose& pose_out,
                                      float& basis_score_out,
                                      uintptr_t& vtable_out) {
    object = 0;
    pose_out = {};
    basis_score_out = -1000.0f;
    vtable_out = 0;
    if (!IsRetail1688Build() || !IsPlausibleUserPointer(value)) return false;

    const uintptr_t candidates[] = {
        value,
        value + kObjectStorageOffset,
        value > kObjectStorageOffset ? value - kObjectStorageOffset : 0,
    };
    for (uintptr_t candidate : candidates) {
        if (!IsPlausibleUserPointer(candidate)) continue;
        Pose pose{};
        float basis_score = -1000.0f;
        uintptr_t vtable = 0;
        if (ValidateRetail1688CameraObject(candidate, hint, pose, basis_score, vtable)) {
            object = candidate;
            pose_out = pose;
            basis_score_out = basis_score;
            vtable_out = vtable;
            return true;
        }
    }
    return false;
}

bool ReadActiveRetail1688CameraFromMulticam(uintptr_t multicam_object,
                                            const Pose& hint,
                                            uintptr_t& object,
                                            Pose& pose_out,
                                            float& basis_score_out,
                                            uintptr_t& vtable_out) {
    object = 0;
    pose_out = {};
    basis_score_out = -1000.0f;
    vtable_out = 0;
    if (!IsRetail1688Build() || multicam_object < 0x10000ull) return false;

    uintptr_t slot0 = 0;
    uintptr_t slot1 = 0;
    if (!SafeRead(multicam_object + kActiveCameraSlotOffset, slot0) ||
        !SafeRead(multicam_object + kActiveCameraSlotOffset + sizeof(uintptr_t), slot1)) {
        return false;
    }

    if (TryDecodeRetail1688CameraPointer(slot0, hint, object, pose_out, basis_score_out, vtable_out)) return true;
    if (TryDecodeRetail1688CameraPointer(slot1, hint, object, pose_out, basis_score_out, vtable_out)) return true;
    return false;
}

bool AcceptScannedMulticamObject(uintptr_t object,
                                 const Pose& hint,
                                 uintptr_t& active_out,
                                 Pose& active_pose_out,
                                 float& basis_score_out,
                                 uintptr_t& multicam_vtable_out,
                                 uintptr_t& active_vtable_out) {
    active_out = 0;
    active_pose_out = {};
    basis_score_out = -1000.0f;
    multicam_vtable_out = 0;
    active_vtable_out = 0;

    if (object < 0x10000ull) return false;
    if (!SafeRead(object, multicam_vtable_out)) return false;

    if (IsRetail1688Build()) {
        if (!IsRetail1688MulticamVtable(multicam_vtable_out)) return false;

        uintptr_t active = 0;
        if (!ReadActiveRetail1688CameraFromMulticam(object,
                                                    hint,
                                                    active,
                                                    active_pose_out,
                                                    basis_score_out,
                                                    active_vtable_out)) {
            return false;
        }
        active_out = active;
        g_multicam_object = object;
        g_active_slot = object + kActiveCameraSlotOffset;
        return true;
    }

    if (!IsKnownMulticamVtable(multicam_vtable_out)) return false;

    uintptr_t active = 0;
    if (!ReadActiveDriverFromMulticam(object, active)) return false;
    if (!ValidateCameraObject(active, hint, active_pose_out, basis_score_out, active_vtable_out)) return false;

    active_out = active;
    g_multicam_object = object;
    g_active_slot = object + kActiveCameraSlotOffset;
    return true;
}

bool TryPublishCameraObject(uintptr_t object, const Pose& hint, const char* source) {
    Pose pose{};
    float basis_score = -1000.0f;
    uintptr_t vtable = 0;
    if (!ValidateCameraObject(object, hint, pose, basis_score, vtable)) return false;

    const uintptr_t previous = g_driver_object.exchange(object, std::memory_order_acq_rel);
    g_shape_driver_object.store(0, std::memory_order_release);
    if (previous != object) {
        g_arg_publish_count.fetch_add(1, std::memory_order_relaxed);
        spdlog::info("[FH5CAM] producer arg captured camera object=0x{:X} source={} vtable=0x{:X} basis={:.3f} pos=({:.3f},{:.3f},{:.3f})",
                     object, source, vtable, basis_score,
                     pose.position.x, pose.position.y, pose.position.z);
    }
    return true;
}

bool TryPublishMulticamObject(uintptr_t object, const Pose& hint, const char* source) {
    if (object < 0x10000ull) return false;
    uintptr_t vtable = 0;
    if (!SafeRead(object, vtable) || !IsKnownMulticamVtable(vtable)) return false;

    uintptr_t active = 0;
    if (!ReadActiveDriverFromMulticam(object, active)) return false;
    if (TryPublishCameraObject(active, hint, source)) {
        g_multicam_published_object.store(object, std::memory_order_release);
        g_multicam_publish_count.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool TryPublishPointerValue(uintptr_t value, const Pose& hint, const char* source) {
    if (value < 0x10000ull) return false;

    if (TryPublishCameraObject(value, hint, source)) return true;
    if (TryPublishCameraObject(value + kObjectStorageOffset, hint, source)) return true;
    if (value > kCameraMatrixOffset && TryPublishCameraObject(value - kCameraMatrixOffset, hint, source)) return true;
    if (TryPublishMulticamObject(value, hint, source)) return true;

    uintptr_t deref = 0;
    if (SafeRead(value, deref)) {
        if (TryPublishCameraObject(deref, hint, source)) return true;
        if (TryPublishCameraObject(deref + kObjectStorageOffset, hint, source)) return true;
        if (TryPublishMulticamObject(deref, hint, source)) return true;
    }

    uintptr_t wrapped_multicam = 0;
    if (SafeRead(value + 0x198ull, wrapped_multicam) &&
        TryPublishMulticamObject(wrapped_multicam, hint, source)) {
        return true;
    }

    return false;
}

bool ResolveDriverByRefcountScan(const Pose& hint, uintptr_t& object_out) {
    object_out = 0;
    if (!EnsureModuleBase()) return false;

    struct ScanTarget {
        uintptr_t value = 0;
        uintptr_t ida_va = 0;
    };
    std::vector<ScanTarget> targets{};
    targets.reserve(sizeof(kKnownCameraRefcountVtableIdaVas) / sizeof(kKnownCameraRefcountVtableIdaVas[0]) + 1);
    for (uintptr_t ida_va : kKnownCameraRefcountVtableIdaVas) {
        if (ida_va == 0) continue;
        targets.push_back({ g_module_base + (ida_va - kIdaImageBase), ida_va });
    }
    targets.push_back({
        g_module_base + (kCinematicGameCameraRefcountIdaVa - kIdaImageBase),
        kCinematicGameCameraRefcountIdaVa
    });
    if (targets.empty()) return false;

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    uintptr_t cursor = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    const uintptr_t end = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
    constexpr size_t kMaxChunk = 4 * 1024 * 1024;

    const uint64_t start_ms = NowMs();
    uintptr_t best_object = 0;
    uintptr_t best_control = 0;
    uintptr_t best_refcount_ida = 0;
    uintptr_t best_object_vtable = 0;
    Pose best_pose{};
    float best_score = -1000.0f;
    size_t qwords_checked = 0;
    size_t refcount_hits = 0;
    size_t validated_hits = 0;
    size_t vtable_rejects = 0;
    size_t matrix_rejects = 0;
    size_t basis_rejects = 0;

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
            mbi.Type == MEM_PRIVATE &&
            IsReadableProtect(mbi.Protect) &&
            IsWritableDataProtect(mbi.Protect) &&
            read_start + sizeof(uint64_t) <= read_end) {
            uintptr_t chunk_start = read_start;
            while (chunk_start + sizeof(uint64_t) <= read_end) {
                const size_t chunk_size = static_cast<size_t>(std::min<uintptr_t>(read_end - chunk_start, kMaxChunk));
                std::vector<uint8_t> buffer(chunk_size);
                if (SafeCopyIn(chunk_start, buffer.data(), buffer.size())) {
                    for (size_t i = 0; i + sizeof(uint64_t) <= buffer.size(); i += sizeof(uint64_t)) {
                        uint64_t value = 0;
                        std::memcpy(&value, buffer.data() + i, sizeof(value));
                        ++qwords_checked;

                        uintptr_t matched_ida = 0;
                        for (const ScanTarget& target : targets) {
                            if (value == target.value) {
                                matched_ida = target.ida_va;
                                break;
                            }
                        }
                        if (matched_ida == 0) continue;

                        ++refcount_hits;
                        const uintptr_t control = chunk_start + i;
                        const uintptr_t object = control + kObjectStorageOffset;
                        Matrix4 matrix{};
                        Pose pose{};
                        float basis_score = -1000.0f;
                        uintptr_t identity_vtable = 0;
                        CameraLayout layout{};
                        if (!ResolveCameraLayout(object, hint, matrix, pose, basis_score, identity_vtable, layout, -3.0f)) {
                            ++vtable_rejects;
                            continue;
                        }

                        ++validated_hits;
                        const float class_priority = CameraClassPriorityFromRefcountIda(matched_ida);
                        float score = class_priority + (basis_score * 0.25f) + 0.75f; // exact camera/refcount vtable match
                        if (layout.writes_view_tail && ViewTailLooksConsistent(object + layout.view_tail_offset, pose)) score += 0.25f;
                        if (DoublePositionLooksConsistent(object, pose)) score += 0.50f;

                        if (score > best_score) {
                            best_score = score;
                            best_object = object;
                            best_control = control;
                            best_refcount_ida = matched_ida;
                            best_object_vtable = identity_vtable;
                            best_pose = pose;
                        }

                        if (score >= 4.50f) {
                            object_out = object;
                            spdlog::info("[FH5CAM] refcount resolver early hit object=0x{:X} control=0x{:X} refcount_ida=0x{:X} "
                                         "vtable=0x{:X} layout={} score={:.3f} class={:.2f} basis={:.3f} pos=({:.3f},{:.3f},{:.3f}) qwords={} hits={} validated={} ms={}",
                                         object, control, matched_ida, identity_vtable, layout.name, score, class_priority, basis_score,
                                         pose.position.x, pose.position.y, pose.position.z,
                                         qwords_checked, refcount_hits, validated_hits,
                                         NowMs() - start_ms);
                            return true;
                        }
                    }
                }
                chunk_start += chunk_size;
            }
        }
        cursor = region_end > cursor ? region_end : cursor + 0x1000;
    }

    if (best_object != 0) {
        object_out = best_object;
        spdlog::info("[FH5CAM] refcount resolver hit object=0x{:X} control=0x{:X} refcount_ida=0x{:X} "
                     "vtable=0x{:X} score={:.3f} class={:.2f} pos=({:.3f},{:.3f},{:.3f}) qwords={} hits={} validated={} "
                     "rejects(vtbl={},matrix={},basis={}) ms={}",
                     best_object, best_control, best_refcount_ida, best_object_vtable, best_score,
                     CameraClassPriorityFromRefcountIda(best_refcount_ida),
                     best_pose.position.x, best_pose.position.y, best_pose.position.z,
                     qwords_checked, refcount_hits, validated_hits,
                     vtable_rejects, matrix_rejects, basis_rejects,
                     NowMs() - start_ms);
        return true;
    }

    spdlog::info("[FH5CAM] refcount resolver miss qwords={} hits={} validated={} rejects(vtbl={},matrix={},basis={}) ms={}",
                 qwords_checked, refcount_hits, validated_hits,
                 vtable_rejects, matrix_rejects, basis_rejects, NowMs() - start_ms);
    return false;
}

bool ResolveMulticamByScan(const Pose& hint, uintptr_t& multicam_out, uintptr_t& active_out) {
    multicam_out = 0;
    active_out = 0;
    if (!EnsureModuleBase()) return false;

    constexpr size_t kKnownMulticamCount =
        sizeof(kKnownMulticamVtableIdaVas) / sizeof(kKnownMulticamVtableIdaVas[0]);
    struct ScanTarget {
        uintptr_t value = 0;
        uintptr_t ida_va = 0;
        bool hit_is_control = false;
        const char* label = "";
    };
    std::vector<ScanTarget> targets{};
    targets.reserve(kKnownMulticamCount + 1);
    const bool retail1688 = IsRetail1688Build();
    const uintptr_t refcount_ida = retail1688 ? k1688ForzaMultiCamRefcountIdaVa : kForzaMultiCamRefcountIdaVa;
    if (refcount_ida != 0) {
        targets.push_back({
            g_module_base + (refcount_ida - kIdaImageBase),
            refcount_ida,
            true,
            retail1688 ? "retail_refcount" : "refcount"
        });
    }
    if (!retail1688) {
        for (size_t i = 0; i < kKnownMulticamCount; ++i) {
            if (kKnownMulticamVtableIdaVas[i] == 0) continue;
            targets.push_back({
                g_module_base + (kKnownMulticamVtableIdaVas[i] - kIdaImageBase),
                kKnownMulticamVtableIdaVas[i],
                false,
                "object"
            });
        }
    }
    if (targets.empty()) return false;

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    constexpr size_t kMaxChunk = 4 * 1024 * 1024;

    struct ScanRegion {
        uintptr_t start = 0;
        uintptr_t end = 0;
        size_t size = 0;
    };
    std::vector<ScanRegion> regions{};
    uintptr_t cursor = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    const uintptr_t end = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
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
            mbi.Type == MEM_PRIVATE &&
            IsReadableProtect(mbi.Protect) &&
            IsWritableDataProtect(mbi.Protect) &&
            read_start + sizeof(uint64_t) <= read_end) {
            regions.push_back({ read_start, read_end, static_cast<size_t>(read_end - read_start) });
        }
        cursor = region_end > cursor ? region_end : cursor + 0x1000;
    }

    std::sort(regions.begin(), regions.end(), [](const ScanRegion& a, const ScanRegion& b) {
        const bool a_high = a.start >= 0x100000000ull;
        const bool b_high = b.start >= 0x100000000ull;
        if (a_high != b_high) return a_high;
        if (a.size != b.size) return a.size > b.size;
        return a.start > b.start;
    });

    size_t qwords = 0;
    size_t hits = 0;
    size_t candidates = 0;
    const uint64_t scan_start_ms = NowMs();
    uint64_t last_progress_ms = scan_start_ms;
    spdlog::info("[FH5CAM] ForzaMultiCam scan start targets={} first=0x{:X} regions={}",
                 targets.size(), targets.empty() ? 0 : targets[0].value, regions.size());

    for (const ScanRegion& region : regions) {
        uintptr_t chunk_start = region.start;
        while (chunk_start + sizeof(uint64_t) <= region.end) {
                const size_t chunk_size = static_cast<size_t>(std::min<uintptr_t>(region.end - chunk_start, kMaxChunk));
                std::vector<uint8_t> buffer(chunk_size);
                if (SafeCopyIn(chunk_start, buffer.data(), buffer.size())) {
                    for (size_t i = 0; i + sizeof(uint64_t) <= buffer.size(); i += sizeof(uint64_t)) {
                        uint64_t value = 0;
                        std::memcpy(&value, buffer.data() + i, sizeof(value));
                        ++qwords;

                        for (const ScanTarget& target : targets) {
                            if (value != target.value) continue;

                            ++hits;
                            const uintptr_t hit_address = chunk_start + i;
                            const uintptr_t multicam_object =
                                target.hit_is_control ? hit_address + kObjectStorageOffset : hit_address;
                            ++candidates;

                            Pose active_pose{};
                            float basis_score = -1000.0f;
                            uintptr_t multicam_vtable = 0;
                            uintptr_t active_vtable = 0;
                            uintptr_t active = 0;
                            if (AcceptScannedMulticamObject(multicam_object,
                                                            hint,
                                                            active,
                                                            active_pose,
                                                            basis_score,
                                                            multicam_vtable,
                                                            active_vtable)) {
                                g_multicam_control = target.hit_is_control ? hit_address : 0;
                                g_multicam_published_object.store(multicam_object, std::memory_order_release);
                                g_multicam_publish_count.fetch_add(1, std::memory_order_relaxed);
                                multicam_out = multicam_object;
                                active_out = active;
                                spdlog::info("[FH5CAM] resolved ForzaMultiCam object=0x{:X} active=0x{:X} target={} ida=0x{:X} hit=0x{:X} fmc_vtbl=0x{:X} active_vtbl=0x{:X} basis={:.3f} pos=({:.3f},{:.3f},{:.3f}) qwords={} hits={} candidates={}",
                                             multicam_object,
                                             active,
                                             target.label,
                                             target.ida_va,
                                             hit_address,
                                             multicam_vtable,
                                             active_vtable,
                                             basis_score,
                                             active_pose.position.x,
                                             active_pose.position.y,
                                             active_pose.position.z,
                                             qwords,
                                             hits,
                                             candidates);
                                return true;
                            }

                            if (candidates <= 32) {
                                uintptr_t slot_object = 0;
                                uintptr_t slot_control = 0;
                                uintptr_t slot_control_vtable = 0;
                                uintptr_t fmc_vtable_diag = 0;
                                uintptr_t active_vtable_diag = 0;
                                SafeRead(multicam_object, fmc_vtable_diag);
                                SafeRead(multicam_object + kActiveCameraSlotOffset, slot_object);
                                SafeRead(multicam_object + kActiveCameraSlotOffset + sizeof(uintptr_t), slot_control);
                                if (IsPlausibleUserPointer(slot_control)) SafeRead(slot_control, slot_control_vtable);
                                if (IsPlausibleUserPointer(slot_object)) SafeRead(slot_object, active_vtable_diag);

                                Pose diag_pose{};
                                float diag_basis = -1000.0f;
                                uintptr_t diag_active_vtable = 0;
                                const bool retail1688 = IsRetail1688Build();
                                const bool camera_ok = retail1688 ?
                                    ValidateRetail1688CameraObject(slot_object,
                                                                   hint,
                                                                   diag_pose,
                                                                   diag_basis,
                                                                   diag_active_vtable) :
                                    ValidateCameraObject(slot_object,
                                                         hint,
                                                         diag_pose,
                                                         diag_basis,
                                                         diag_active_vtable);
                                spdlog::info("[FH5CAM] ForzaMultiCam candidate rejected target={} ida=0x{:X} hit=0x{:X} object=0x{:X} fmc_vtbl=0x{:X} fmc_known={} slot_obj=0x{:X} slot_ctrl=0x{:X} slot_ctrl_vtbl=0x{:X} relation={} active_vtbl=0x{:X} active_known={} camera_ok={} basis={:.3f}",
                                             target.label,
                                             target.ida_va,
                                             hit_address,
                                             multicam_object,
                                             fmc_vtable_diag,
                                             (retail1688 ? IsRetail1688MulticamVtable(fmc_vtable_diag) : IsKnownMulticamVtable(fmc_vtable_diag)) ? 1 : 0,
                                             slot_object,
                                             slot_control,
                                             slot_control_vtable,
                                             (slot_control && slot_object == slot_control + kObjectStorageOffset) ? 1 : 0,
                                             active_vtable_diag,
                                             (retail1688 ? (active_vtable_diag != 0) : IsKnownCameraVtable(active_vtable_diag)) ? 1 : 0,
                                             camera_ok ? 1 : 0,
                                             diag_basis);
                            }
                        }
                    }
                }
                chunk_start += chunk_size;

                const uint64_t now_ms = NowMs();
                if (now_ms - last_progress_ms >= 1000) {
                    last_progress_ms = now_ms;
                    spdlog::info("[FH5CAM] ForzaMultiCam scan progress cursor=0x{:X} region=0x{:X}-0x{:X} elapsed_ms={} qwords={} hits={} candidates={}",
                                 chunk_start,
                                 region.start,
                                 region.end,
                                 now_ms - scan_start_ms,
                                 qwords,
                                 hits,
                                 candidates);
                }
            }
    }

    spdlog::info("[FH5CAM] ForzaMultiCam scan miss elapsed_ms={} qwords={} hits={} candidates={}",
                 NowMs() - scan_start_ms, qwords, hits, candidates);
    return false;
}

bool SnapshotPoseHint(Pose& hint) {
    for (int spin = 0; spin < 8; ++spin) {
        const uint32_t g0 = g_hint_gen.load(std::memory_order_acquire);
        if (g0 == 0 || (g0 & 1u)) continue;
        hint = g_pose_hint;
        const uint32_t g1 = g_hint_gen.load(std::memory_order_acquire);
        if (g0 == g1) return true;
    }
    return false;
}

bool CameraWriterRequested() {
    if (fh5cb::ctl_pos_lane() == fh5cb::kPosLaneInput540) return true;
    if (fh5cb::ctl_up_tgt() == kTgtDriver) return true;

    float strafe = 0.0f;
    float up = 0.0f;
    float fwd = 0.0f;
    Pose delta{};
    int eye = 0;
    constexpr float kOffsetActiveEpsilon = 0.001f;
    constexpr float kRotationActiveEpsilon = 0.0005f;
    return SnapshotOpenXrPose(strafe, up, fwd, delta, eye) &&
           (std::fabs(strafe) > kOffsetActiveEpsilon ||
            std::fabs(up) > kOffsetActiveEpsilon ||
            std::fabs(fwd) > kOffsetActiveEpsilon ||
            BasisDistance(delta, Pose{}) > kRotationActiveEpsilon);
}

bool SnapshotOpenXrPose(float& strafe, float& up, float& fwd, Pose& delta, int& eye) {
    for (int spin = 0; spin < 8; ++spin) {
        const uint32_t g0 = g_openxr_offset_gen.load(std::memory_order_acquire);
        if (g0 == 0 || (g0 & 1u)) continue;

        const bool active = g_openxr_offset_active.load(std::memory_order_acquire);
        const uint64_t published_ms = g_openxr_last_publish_ms.load(std::memory_order_acquire);
        const float s = g_openxr_strafe;
        const float u = g_openxr_up;
        const float f = g_openxr_fwd;
        const Pose d = g_openxr_delta_pose;
        const int e = g_openxr_eye;

        const uint32_t g1 = g_openxr_offset_gen.load(std::memory_order_acquire);
        if (g0 != g1) continue;

        constexpr uint64_t kOpenXrPoseStaleMs = 500;
        if (!active || published_ms == 0 || NowMs() - published_ms > kOpenXrPoseStaleMs) {
            return false;
        }

        strafe = s;
        up = u;
        fwd = f;
        delta = d;
        eye = e;
        return true;
    }
    return false;
}

Vec3 MapCameraOffsetToInput540(const Vec3& camera_offset) {
    // sub_1407A6300 consumes +0x540 as camera-space XYZ before the engine's Z-flip transform.
    // OpenXR/FH5 public controls use +fwd as "move camera forward", which maps to lane -Z.
    return { camera_offset.x, camera_offset.y, -camera_offset.z };
}

bool GameplayCameraRecentForInput540() {
    const uint64_t now = GetTickCount64();
    const uint64_t world = fh5diag::last_world_ms();
    const uint64_t showcase = fh5diag::last_showcase_ms();
    constexpr uint64_t kInput540GameplayRecencyMs = 150;

    const bool world_recent =
        world != 0 && now >= world && (now - world) <= kInput540GameplayRecencyMs;
    const bool showcase_recent =
        showcase != 0 && now >= showcase && (now - showcase) <= kInput540GameplayRecencyMs;
    return world_recent || showcase_recent;
}

void RestoreInput540AndForget(uintptr_t object) {
    if (object != 0) {
        RestoreInput540Base(object);
    }
    g_input540_base_object.store(0, std::memory_order_release);
}

void LogInput540GateIfNeeded() {
    const uint64_t now_ms = NowMs();
    uint64_t last_ms = g_input540_gate_last_log_ms.load(std::memory_order_relaxed);
    if (now_ms >= last_ms && now_ms - last_ms >= 1000 &&
        g_input540_gate_last_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
        spdlog::info("[FH5IN540] gated off outside gameplay camera: world_age_ms={} showcase_age_ms={}",
                     fh5diag::last_world_ms() ? (GetTickCount64() - fh5diag::last_world_ms()) : 0,
                     fh5diag::last_showcase_ms() ? (GetTickCount64() - fh5diag::last_showcase_ms()) : 0);
    }
}

bool BuildInput540Offset(Vec3& offset, bool& manual_active, bool& vr_tracking_active, int& eye) {
    Vec3 camera_offset{};
    manual_active = false;
    vr_tracking_active = false;
    eye = 0;

    if (fh5cb::ctl_pos_lane() != fh5cb::kPosLaneInput540) {
        return false;
    }
    if (!GameplayCameraRecentForInput540()) {
        return false;
    }

    if (fh5cb::ctl_up_tgt() == kTgtDriver) {
        camera_offset.x += fh5cb::ctl_up_strafe();
        camera_offset.y += fh5cb::ctl_up_up();
        camera_offset.z += fh5cb::ctl_up_fwd();
        manual_active = true;
    }

    float strafe = 0.0f;
    float up = 0.0f;
    float fwd = 0.0f;
    Pose delta{};
    if (SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) {
        camera_offset.x += strafe;
        camera_offset.y += up;
        camera_offset.z += fwd;
        vr_tracking_active = true;
    }

    if (!IsFinite(camera_offset)) {
        g_input540_bad.fetch_add(1, std::memory_order_relaxed);
        offset = {};
        return false;
    }

    offset = MapCameraOffsetToInput540(camera_offset);
    // For +0x540 we must keep writing even at neutral pose; otherwise a prior nonzero
    // headset offset remains latched in the engine input lane.
    return true;
}

Vec3 CurrentOpenXrLocalOffset(bool& active, int& eye) {
    active = false;
    eye = 0;

    float strafe = 0.0f;
    float up = 0.0f;
    float fwd = 0.0f;
    Pose delta{};
    if (!SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) {
        return {};
    }
    if (!std::isfinite(strafe) || !std::isfinite(up) || !std::isfinite(fwd)) {
        g_550_bad.fetch_add(1, std::memory_order_relaxed);
        return {};
    }

    constexpr float kOffsetActiveEpsilon = 0.001f;
    if (std::fabs(strafe) <= kOffsetActiveEpsilon &&
        std::fabs(up) <= kOffsetActiveEpsilon &&
        std::fabs(fwd) <= kOffsetActiveEpsilon) {
        return {};
    }

    active = true;
    return { strafe, up, fwd };
}

bool SaneGetter550Vector(const Vec4& v) {
    constexpr float kMaxReasonableAbs = 10000000.0f;
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.w) &&
           std::fabs(v.x) < kMaxReasonableAbs &&
           std::fabs(v.y) < kMaxReasonableAbs &&
           std::fabs(v.z) < kMaxReasonableAbs &&
           std::fabs(v.w) < kMaxReasonableAbs;
}

bool ReadGetter550Source(uintptr_t object, Vec4& base) {
    double raw[4]{};
    if (!SafeCopyIn(object + kDoublePositionOffset, raw, sizeof(raw))) {
        return false;
    }

    base = {
        static_cast<float>(raw[0]),
        static_cast<float>(raw[1]),
        static_cast<float>(raw[2]),
        static_cast<float>(raw[3])
    };
    return SaneGetter550Vector(base);
}

Vec4 ApplyOpenXrGetterOffset(uintptr_t object, Vec4 base, bool& modified, int& eye) {
    modified = false;
    if (object == 0 || object != g_shadow.object) {
        eye = 0;
        return base;
    }

    bool active = false;
    const Vec3 local_offset = CurrentOpenXrLocalOffset(active, eye);
    if (!active) return base;

    // +0x550 is the engine-owned camera-space translation lane. The standalone
    // freecam adds XYZ directly here; transforming it through +0x320 turns a
    // local head offset into a mismatched world/cull origin and blanks geometry.
    base.x += local_offset.x;
    base.y += local_offset.y;
    base.z += local_offset.z;
    modified = true;
    return base;
}

void RecordGetter550Sample(const Vec4& base, const Vec4& result) {
    g_550_base_x.store(base.x, std::memory_order_relaxed);
    g_550_base_y.store(base.y, std::memory_order_relaxed);
    g_550_base_z.store(base.z, std::memory_order_relaxed);
    g_550_base_w.store(base.w, std::memory_order_relaxed);
    g_550_off_x.store(result.x - base.x, std::memory_order_relaxed);
    g_550_off_y.store(result.y - base.y, std::memory_order_relaxed);
    g_550_off_z.store(result.z - base.z, std::memory_order_relaxed);
    g_550_out_x.store(result.x, std::memory_order_relaxed);
    g_550_out_y.store(result.y, std::memory_order_relaxed);
    g_550_out_z.store(result.z, std::memory_order_relaxed);
    g_550_out_w.store(result.w, std::memory_order_relaxed);
}

Vec4* __fastcall Hook550FloatVec4(void* self, Vec4* out) {
    g_550_float_calls.fetch_add(1, std::memory_order_relaxed);
    if (out == nullptr) return out;

    const uintptr_t object = reinterpret_cast<uintptr_t>(self);
    Vec4 base{};
    Vec4* ret = out;
    bool have_base = false;
    if (g_original_550_float != nullptr) {
        __try {
            ret = g_original_550_float(self, out);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ret = out;
        }
        if (SafeCopyIn(reinterpret_cast<uintptr_t>(out), &base, sizeof(base)) &&
            SaneGetter550Vector(base)) {
            have_base = true;
        }
    }
    if (!have_base && ReadGetter550Source(object, base)) {
        have_base = true;
    }
    if (!have_base) {
        g_550_bad.fetch_add(1, std::memory_order_relaxed);
        return ret;
    }

    bool modified = false;
    int eye = 0;
    const Vec4 result = ApplyOpenXrGetterOffset(object, base, modified, eye);
    if (modified) {
        g_550_float_modified.fetch_add(1, std::memory_order_relaxed);
    }
    RecordGetter550Sample(base, result);
    SafeCopyOut(reinterpret_cast<uintptr_t>(out), &result, sizeof(result));
    return ret;
}

double* __fastcall Hook550DoubleVec4(void* self, double* out) {
    g_550_double_calls.fetch_add(1, std::memory_order_relaxed);
    if (out == nullptr) return out;

    const uintptr_t object = reinterpret_cast<uintptr_t>(self);
    Vec4 base{};
    double* ret = out;
    bool have_base = false;
    if (g_original_550_double != nullptr) {
        __try {
            ret = g_original_550_double(self, out);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ret = out;
        }
        double raw[4]{};
        if (SafeCopyIn(reinterpret_cast<uintptr_t>(out), raw, sizeof(raw))) {
            base = {
                static_cast<float>(raw[0]),
                static_cast<float>(raw[1]),
                static_cast<float>(raw[2]),
                static_cast<float>(raw[3])
            };
            have_base = SaneGetter550Vector(base);
        }
    }
    if (!have_base && ReadGetter550Source(object, base)) {
        have_base = true;
    }
    if (!have_base) {
        g_550_bad.fetch_add(1, std::memory_order_relaxed);
        return ret;
    }

    bool modified = false;
    int eye = 0;
    const Vec4 result = ApplyOpenXrGetterOffset(object, base, modified, eye);
    if (modified) {
        g_550_double_modified.fetch_add(1, std::memory_order_relaxed);
    }
    RecordGetter550Sample(base, result);

    const double out_doubles[4]{
        static_cast<double>(result.x),
        static_cast<double>(result.y),
        static_cast<double>(result.z),
        static_cast<double>(result.w)
    };
    SafeCopyOut(reinterpret_cast<uintptr_t>(out), out_doubles, sizeof(out_doubles));
    return ret;
}

bool ApplyOpenXrUpdatePose(uintptr_t object) {
    if (object == 0 || object != g_shadow.object) return false;

    float strafe = 0.0f;
    float up = 0.0f;
    float fwd = 0.0f;
    Pose delta{};
    int eye = 0;
    if (!SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) {
        return false;
    }

    constexpr float kOffsetActiveEpsilon = 0.001f;
    constexpr float kRotationActiveEpsilon = 0.0005f;
    const bool has_translation =
        std::fabs(strafe) > kOffsetActiveEpsilon ||
        std::fabs(up) > kOffsetActiveEpsilon ||
        std::fabs(fwd) > kOffsetActiveEpsilon;
    const bool has_rotation = BasisDistance(delta, Pose{}) > kRotationActiveEpsilon;
    if (!has_translation && !has_rotation) {
        return false;
    }
    if (!std::isfinite(strafe) || !std::isfinite(up) || !std::isfinite(fwd)) {
        g_update_matrix_bad.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    Matrix4 base{};
    if (!SafeCopyIn(object + kCameraMatrixOffset, &base, sizeof(base))) {
        g_update_matrix_bad.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    Pose source{};
    if (!DecodePose(base, source)) {
        g_update_matrix_bad.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    Pose relative{};
    if (has_rotation) {
        relative.right = delta.right;
        relative.up = delta.up;
        relative.forward = delta.forward;
    }
    relative.position = { strafe, up, fwd };

    Pose out_pose{};
    out_pose.right = TransformDirection(source, relative.right);
    out_pose.up = TransformDirection(source, relative.up);
    out_pose.forward = TransformDirection(source, relative.forward);
    out_pose.position = Add(source.position, TransformDirection(source, relative.position));
    Orthonormalize(out_pose);

    const Matrix4 camera_matrix = MatrixFromPose(out_pose);
    if (!SafeCopyOut(object + kCameraMatrixOffset, &camera_matrix, sizeof(camera_matrix))) {
        g_update_matrix_bad.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const Matrix4 view_tail = ViewTailFromPose(out_pose);
    SafeCopyOut(object + kViewTailOffset, &view_tail, sizeof(view_tail));

    g_update_matrix_applied.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void __fastcall HookUpdate328(void* self) {
    g_update328_calls.fetch_add(1, std::memory_order_relaxed);
    if (g_original_update_328 != nullptr) {
        g_original_update_328(self);
    }
    ApplyOpenXrUpdatePose(reinterpret_cast<uintptr_t>(self));
}

void* __fastcall HookUpdate330(void* self) {
    g_update330_calls.fetch_add(1, std::memory_order_relaxed);
    void* result = nullptr;
    if (g_original_update_330 != nullptr) {
        result = g_original_update_330(self);
    }
    ApplyOpenXrUpdatePose(reinterpret_cast<uintptr_t>(self));
    return result;
}

void RestoreDriverPositionGetterShadow() {
    if (g_shadow.object != 0 && g_shadow.original_vtable != 0) {
        uintptr_t current = 0;
        if (SafeRead(g_shadow.object, current) && current == g_shadow.shadow_vtable) {
            SafeCopyOut(g_shadow.object, &g_shadow.original_vtable, sizeof(g_shadow.original_vtable));
            g_shadow_restore_count.fetch_add(1, std::memory_order_relaxed);
            spdlog::info("[FH5CAM] restored CCamDriver getter shadow object=0x{:X}", g_shadow.object);
        }
    }

    if (g_shadow.shadow_vtable != 0) {
        VirtualFree(reinterpret_cast<void*>(g_shadow.shadow_vtable), 0, MEM_RELEASE);
    }
    g_shadow = {};
    g_original_550_float = nullptr;
    g_original_550_double = nullptr;
    g_original_update_328 = nullptr;
    g_original_update_330 = nullptr;
}

bool EnsureDriverPositionGetterShadow(uintptr_t object) {
    if (object == 0) return false;
    if (g_shadow.object == object && g_shadow.shadow_vtable != 0) return true;

    RestoreDriverPositionGetterShadow();

    uintptr_t original_vtable = 0;
    if (!SafeRead(object, original_vtable) || !IsCCamDriverVtable(original_vtable)) {
        return false;
    }

    void* shadow = VirtualAlloc(nullptr, kCameraVtableBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (shadow == nullptr) {
        spdlog::warn("[FH5CAM] failed to allocate CCamDriver getter shadow object=0x{:X} err={}", object, GetLastError());
        return false;
    }
    if (!SafeCopyIn(original_vtable, shadow, kCameraVtableBytes)) {
        VirtualFree(shadow, 0, MEM_RELEASE);
        spdlog::warn("[FH5CAM] failed to copy CCamDriver vtable object=0x{:X}", object);
        return false;
    }

    auto* entries = reinterpret_cast<uintptr_t*>(shadow);
    g_original_550_float = reinterpret_cast<Vec4GetterFn>(entries[kSlot550Float / sizeof(uintptr_t)]);
    g_original_550_double = reinterpret_cast<Double4GetterFn>(entries[kSlot550Double / sizeof(uintptr_t)]);
    g_original_update_328 = reinterpret_cast<VoidSelfFn>(entries[kSlotUpdate328 / sizeof(uintptr_t)]);
    g_original_update_330 = reinterpret_cast<PtrSelfFn>(entries[kSlotUpdate330 / sizeof(uintptr_t)]);
    entries[kSlot550Float / sizeof(uintptr_t)] = reinterpret_cast<uintptr_t>(&Hook550FloatVec4);
    entries[kSlot550Double / sizeof(uintptr_t)] = reinterpret_cast<uintptr_t>(&Hook550DoubleVec4);

    DWORD old_protect = 0;
    VirtualProtect(shadow, kCameraVtableBytes, PAGE_READONLY, &old_protect);
    const uintptr_t shadow_vtable = reinterpret_cast<uintptr_t>(shadow);
    if (!SafeCopyOut(object, &shadow_vtable, sizeof(shadow_vtable))) {
        VirtualFree(shadow, 0, MEM_RELEASE);
        g_original_550_float = nullptr;
        g_original_550_double = nullptr;
        g_original_update_328 = nullptr;
        g_original_update_330 = nullptr;
        spdlog::warn("[FH5CAM] failed to install CCamDriver getter shadow object=0x{:X}", object);
        return false;
    }

    g_shadow = { object, original_vtable, shadow_vtable, kCameraVtableBytes };
    g_shadow_install_count.fetch_add(1, std::memory_order_relaxed);
    spdlog::info("[FH5CAM] installed CCamDriver getter shadow object=0x{:X} original_vtable=0x{:X} shadow_vtable=0x{:X}",
                 object, original_vtable, shadow_vtable);
    return true;
}

float BasisScore(const Pose& a, const Pose& b) {
    return Dot(a.right, b.right) + Dot(a.up, b.up) + Dot(a.forward, b.forward);
}

float Distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float PositionDistance(const Vec3& a, const Vec3& b) {
    return Distance(a, b);
}

float BasisDistance(const Pose& a, const Pose& b) {
    return std::fabs(a.right.x - b.right.x) + std::fabs(a.right.y - b.right.y) + std::fabs(a.right.z - b.right.z) +
           std::fabs(a.up.x - b.up.x) + std::fabs(a.up.y - b.up.y) + std::fabs(a.up.z - b.up.z) +
           std::fabs(a.forward.x - b.forward.x) + std::fabs(a.forward.y - b.forward.y) + std::fabs(a.forward.z - b.forward.z);
}

float MatrixDistanceSum(const Matrix4& a, const Matrix4& b) {
    float total = 0.0f;
    for (size_t i = 0; i < a.m.size(); ++i) {
        if (!std::isfinite(a.m[i]) || !std::isfinite(b.m[i])) return 1000000.0f;
        total += std::fabs(a.m[i] - b.m[i]);
    }
    return total;
}

float AbsDiff(float a, float b) {
    return std::fabs(a - b);
}

bool EnsureModuleBase() {
    if (g_module_base) return true;
    g_module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"ForzaHorizon5.exe"));
    if (!g_module_base) g_module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    return g_module_base != 0;
}

uintptr_t EffectiveObjectVtable(uintptr_t object, uintptr_t vtable) {
    if (object != 0 &&
        object == g_shadow.object &&
        vtable == g_shadow.shadow_vtable &&
        g_shadow.original_vtable != 0) {
        return g_shadow.original_vtable;
    }
    return vtable;
}

bool MatchesKnownVtable(uintptr_t vtable, const uintptr_t* ida_vas, size_t count) {
    if (!EnsureModuleBase() || vtable == 0) return false;
    for (size_t i = 0; i < count; ++i) {
        if (ida_vas[i] == 0) continue;
        if (vtable == g_module_base + (ida_vas[i] - kIdaImageBase)) return true;
    }
    return false;
}

bool IsKnownCameraVtable(uintptr_t vtable) {
    return MatchesKnownVtable(vtable, kKnownCameraVtableIdaVas,
                              sizeof(kKnownCameraVtableIdaVas) / sizeof(kKnownCameraVtableIdaVas[0]));
}

bool IsKnownCameraRefcountVtable(uintptr_t vtable) {
    return MatchesKnownVtable(vtable, kKnownCameraRefcountVtableIdaVas,
                              sizeof(kKnownCameraRefcountVtableIdaVas) / sizeof(kKnownCameraRefcountVtableIdaVas[0]));
}

bool IsKnownMulticamVtable(uintptr_t vtable) {
    return MatchesKnownVtable(vtable, kKnownMulticamVtableIdaVas,
                              sizeof(kKnownMulticamVtableIdaVas) / sizeof(kKnownMulticamVtableIdaVas[0]));
}

bool IsRetail1688MulticamVtable(uintptr_t vtable) {
    return MatchesKnownVtable(vtable, k1688KnownMulticamVtableIdaVas,
                              sizeof(k1688KnownMulticamVtableIdaVas) / sizeof(k1688KnownMulticamVtableIdaVas[0]));
}

bool IsCinematicGameCameraRefcountVtable(uintptr_t vtable) {
    if (!EnsureModuleBase() || vtable == 0) return false;
    return vtable == g_module_base + (kCinematicGameCameraRefcountIdaVa - kIdaImageBase);
}

bool IsCCamDriverVtable(uintptr_t vtable) {
    if (!EnsureModuleBase() || vtable == 0) return false;
    return vtable == g_module_base + (kCCamDriverVtableIdaVa - kIdaImageBase);
}

bool LooksLikeCameraObjectHeader(uintptr_t object, uintptr_t* vtable_out) {
    if (vtable_out) *vtable_out = 0;
    if (object < 0x10000ull) return false;
    uintptr_t vtable = 0;
    if (!SafeRead(object, vtable)) return false;
    vtable = EffectiveObjectVtable(object, vtable);
    if (!IsKnownCameraVtable(vtable)) return false;
    if (vtable_out) *vtable_out = vtable;
    return true;
}

bool LooksLikeCameraControlHeader(uintptr_t control, uintptr_t* vtable_out) {
    if (vtable_out) *vtable_out = 0;
    if (control < 0x10000ull) return false;
    uintptr_t vtable = 0;
    if (!SafeRead(control, vtable) ||
        (!IsKnownCameraRefcountVtable(vtable) && !IsCinematicGameCameraRefcountVtable(vtable))) {
        return false;
    }
    if (vtable_out) *vtable_out = vtable;
    return true;
}

bool TryDecodeCameraPointer(uintptr_t value, uintptr_t& object) {
    object = 0;
    if (value < 0x10000ull) return false;

    if (LooksLikeCameraObjectHeader(value)) {
        object = value;
        return true;
    }

    uintptr_t control_vtable = 0;
    if (LooksLikeCameraControlHeader(value, &control_vtable)) {
        const uintptr_t embedded_object = value + kObjectStorageOffset;
        if (LooksLikeCameraObjectHeader(embedded_object) ||
            IsCinematicGameCameraRefcountVtable(control_vtable)) {
            object = embedded_object;
            return true;
        }
    }

    return false;
}

float CameraClassPriorityFromRefcountIda(uintptr_t ida_va) {
    switch (ida_va) {
    case 0x145E1FF40ull: return 4.00f; // CCamFollowLow: normal third-person driving view
    case 0x145E1FF68ull: return 3.80f; // CCamFollowHigh
    case 0x145E1FF90ull: return 3.40f; // CCamDriver
    case 0x145E1FFB8ull: return 3.20f; // CCamHood
    case 0x145E1FFE0ull: return 3.10f; // CCamBumperHigh
    case 0x145E20148ull: return 0.40f; // CCamFollowExtended: menu/garage-like extended follow camera
    case 0x145E20008ull: return 0.20f; // CCamFree
    case 0x145E20030ull: return 0.20f; // CCamFreeTargetCar
    case 0x145E200D0ull: return 0.20f; // CCamFreeTrack
    case kCinematicGameCameraRefcountIdaVa: return 3.60f; // CinematicGameCamera
    default: return 0.0f;
    }
}

float CameraClassPriorityFromVtable(uintptr_t vtable) {
    if (!EnsureModuleBase()) return 0.0f;
    const auto runtime = [](uintptr_t ida_va) -> uintptr_t {
        return g_module_base + (ida_va - kIdaImageBase);
    };
    if (vtable == runtime(0x145E3FFC0ull)) return 4.00f; // CCamFollowLow
    if (vtable == runtime(0x145E40308ull)) return 3.80f; // CCamFollowHigh
    if (vtable == runtime(0x145E3F290ull)) return 3.40f; // CCamDriver
    if (vtable == runtime(0x145E3EBF0ull)) return 3.20f; // CCamHood
    if (vtable == runtime(0x145E3E550ull)) return 3.10f; // CCamBumperHigh
    if (vtable == runtime(0x145E40650ull)) return 0.40f; // CCamFollowExtended
    if (vtable == runtime(0x145E40D78ull)) return 0.20f; // CCamFree
    if (vtable == runtime(0x145E41208ull)) return 0.20f; // CCamFreeTargetCar
    if (vtable == runtime(0x145E415B8ull)) return 0.20f; // CCamFreeTrack
    if (vtable == runtime(kCinematicGameCameraRefcountIdaVa)) return 3.60f; // CinematicGameCamera active camera path
    return 0.0f;
}

bool ViewTailLooksConsistent(uintptr_t viewtail_address, const Pose& pose) {
    Matrix4 view_tail{};
    if (!SafeCopyIn(viewtail_address, view_tail.m.data(), sizeof(float) * view_tail.m.size())) {
        return false;
    }
    const Matrix4 expected = ViewTailFromPose(pose);
    if (AbsDiff(view_tail.m[15], 1.0f) > 0.25f) return false;
    return AbsDiff(view_tail.m[12], expected.m[12]) < 8.0f &&
           AbsDiff(view_tail.m[13], expected.m[13]) < 8.0f &&
           AbsDiff(view_tail.m[14], expected.m[14]) < 8.0f;
}

bool DoublePositionLooksConsistent(uintptr_t object, const Pose& pose) {
    double dpos[4]{};
    if (!ReadDoublePositionRaw(object, dpos)) {
        return false;
    }
    return std::isfinite(dpos[0]) && std::isfinite(dpos[1]) && std::isfinite(dpos[2]) &&
           std::fabs(dpos[0] - static_cast<double>(pose.position.x)) < 2.0 &&
           std::fabs(dpos[1] - static_cast<double>(pose.position.y)) < 2.0 &&
           std::fabs(dpos[2] - static_cast<double>(pose.position.z)) < 2.0;
}

bool ReadDoublePositionRaw(uintptr_t object, double (&dpos)[4]) {
    dpos[0] = dpos[1] = dpos[2] = dpos[3] = 0.0;
    return object >= 0x10000ull &&
           SafeCopyIn(object + kDoublePositionOffset, dpos, sizeof(dpos)) &&
           std::isfinite(dpos[0]) &&
           std::isfinite(dpos[1]) &&
           std::isfinite(dpos[2]);
}

bool WriteDoublePositionRaw(uintptr_t object, const Pose& pose) {
    double dpos[4]{};
    if (!ReadDoublePositionRaw(object, dpos)) {
        dpos[3] = 1.0;
    }
    dpos[0] = static_cast<double>(pose.position.x);
    dpos[1] = static_cast<double>(pose.position.y);
    dpos[2] = static_cast<double>(pose.position.z);
    if (!std::isfinite(dpos[3])) dpos[3] = 1.0;
    return SafeCopyOut(object + kDoublePositionOffset, dpos, sizeof(dpos));
}

bool ReadVec4f(uintptr_t address, Vec4& out) {
    out = {};
    float raw[4]{};
    if (!SafeCopyIn(address, raw, sizeof(raw))) {
        return false;
    }
    out = { raw[0], raw[1], raw[2], raw[3] };
    return std::isfinite(out.x) &&
           std::isfinite(out.y) &&
           std::isfinite(out.z) &&
           std::isfinite(out.w);
}

bool ReadInputLaneSnapshot(uintptr_t object, InputLaneSnapshot& snap) {
    snap = {};
    if (object < 0x10000ull) return false;
    snap.ok530 = ReadVec4f(object + kInputBaseOffset, snap.base530);
    snap.ok540 = ReadVec4f(object + kInputOffsetLane, snap.offset540);
    snap.ok550 = ReadVec4f(object + kInputDerivedOffset, snap.derived550);
    snap.ok560 = ReadVec4f(object + kInputDerivedOffset + 0x10ull, snap.derived560);
    snap.ok570 = ReadVec4f(object + kInputDerivedTailOffset, snap.derived570);
    return snap.ok530 || snap.ok540 || snap.ok550 || snap.ok560 || snap.ok570;
}

bool WriteInput540Offset(uintptr_t object, const Vec3& offset) {
    if (object < 0x10000ull || !IsFinite(offset)) return false;

    Vec4 base{};
    if (g_input540_base_object.load(std::memory_order_acquire) != object) {
        if (!ReadVec4f(object + kInputOffsetLane, base)) {
            base = {};
        }
        g_input540_base_x.store(base.x, std::memory_order_relaxed);
        g_input540_base_y.store(base.y, std::memory_order_relaxed);
        g_input540_base_z.store(base.z, std::memory_order_relaxed);
        g_input540_base_w.store(base.w, std::memory_order_relaxed);
        g_input540_base_object.store(object, std::memory_order_release);
    } else {
        base = {
            g_input540_base_x.load(std::memory_order_relaxed),
            g_input540_base_y.load(std::memory_order_relaxed),
            g_input540_base_z.load(std::memory_order_relaxed),
            g_input540_base_w.load(std::memory_order_relaxed)
        };
    }

    const Vec4 lane{ base.x + offset.x, base.y + offset.y, base.z + offset.z, base.w };
    return SafeCopyOut(object + kInputOffsetLane, &lane, sizeof(lane));
}

bool RestoreInput540Base(uintptr_t object) {
    if (object < 0x10000ull ||
        g_input540_base_object.load(std::memory_order_acquire) != object) {
        return false;
    }

    const Vec4 base{
        g_input540_base_x.load(std::memory_order_relaxed),
        g_input540_base_y.load(std::memory_order_relaxed),
        g_input540_base_z.load(std::memory_order_relaxed),
        g_input540_base_w.load(std::memory_order_relaxed)
    };
    return SafeCopyOut(object + kInputOffsetLane, &base, sizeof(base));
}

bool ReadPoseAt(uintptr_t address, Pose& pose) {
    pose = {};
    Matrix4 matrix{};
    return address >= 0x10000ull &&
           SafeCopyIn(address, matrix.m.data(), sizeof(float) * matrix.m.size()) &&
           DecodePose(matrix, pose);
}

bool MatrixLooksLikePose(const Matrix4& matrix, const Pose& pose, float max_position_delta, float max_basis_delta) {
    Pose decoded{};
    if (!DecodePose(matrix, decoded)) return false;
    return PositionDistance(decoded.position, pose.position) <= max_position_delta &&
           BasisDistance(decoded, pose) <= max_basis_delta;
}

std::vector<CloneTarget> ScanMatrixClones(uintptr_t exclude_address, const Matrix4& reference_matrix) {
    Pose reference{};
    std::vector<CloneTarget> clones;
    if (!DecodePose(reference_matrix, reference)) return clones;

    uintptr_t scan_start = exclude_address & ~0xFFFFFFFFull;
    uintptr_t scan_end = scan_start + 0x100000000ull;
    uintptr_t cursor = scan_start;
    constexpr size_t kMaxChunk = 16 * 1024 * 1024;
    while (cursor < scan_end && clones.size() < 256) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi))) {
            cursor += 0x1000;
            continue;
        }

        const uintptr_t region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t region_end = region_base + mbi.RegionSize;
        const uintptr_t read_start = std::max(region_base, cursor);
        const uintptr_t read_end = std::min(region_end, scan_end);
        if (mbi.State == MEM_COMMIT &&
            (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED) &&
            IsReadableProtect(mbi.Protect) &&
            IsDataProtect(mbi.Protect) &&
            read_start + sizeof(float) * 16 <= read_end) {
            uintptr_t chunk_start = read_start;
            while (chunk_start + sizeof(float) * 16 <= read_end && clones.size() < 256) {
                const size_t chunk_size = static_cast<size_t>(std::min<uintptr_t>(read_end - chunk_start, kMaxChunk));
                std::vector<uint8_t> buffer(chunk_size);
                if (SafeCopyIn(chunk_start, buffer.data(), buffer.size())) {
                    for (size_t i = 0; i + sizeof(float) * 16 <= buffer.size() && clones.size() < 256; i += 16) {
                        const uintptr_t address = chunk_start + i;
                        if (address == exclude_address) continue;

                        Matrix4 candidate_matrix{};
                        std::memcpy(candidate_matrix.m.data(), buffer.data() + i, sizeof(float) * candidate_matrix.m.size());

                        Pose candidate{};
                        if (!DecodePose(candidate_matrix, candidate)) continue;

                        const float pos_delta = PositionDistance(candidate.position, reference.position);
                        const float basis_delta = BasisDistance(candidate, reference);
                        if (pos_delta <= 75.0f && basis_delta <= 2.0f) {
                            bool view_tail = false;
                            if (i + 0xC0 + sizeof(float) * 16 <= buffer.size()) {
                                Matrix4 tail_matrix{};
                                std::memcpy(tail_matrix.m.data(), buffer.data() + i + 0xC0, sizeof(float) * tail_matrix.m.size());
                                view_tail = MatrixDistanceSum(tail_matrix, ViewTailFromPose(candidate)) <= 0.25f;
                            }
                            clones.push_back({ address, view_tail, pos_delta, basis_delta });
                        }
                    }
                }
                chunk_start += chunk_size;
            }
        }
        cursor = region_end > cursor ? region_end : cursor + 0x1000;
    }

    std::sort(clones.begin(), clones.end(), [](const CloneTarget& a, const CloneTarget& b) {
        if (std::fabs(a.pos_delta - b.pos_delta) > 0.001f) return a.pos_delta < b.pos_delta;
        return a.basis_delta < b.basis_delta;
    });
    return clones;
}

bool ResolveDriverByShape(const Pose& hint, uintptr_t& object_out) {
    object_out = 0;
    if (!g_module_base) {
        g_module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"ForzaHorizon5.exe"));
        if (!g_module_base) g_module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    }
    if (!g_module_base) return false;

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    uintptr_t cursor = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    const uintptr_t end = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
    constexpr size_t kMaxChunk = 4 * 1024 * 1024;

    uintptr_t best_object = 0;
    float best_score = -1000.0f;
    size_t checked = 0;
    size_t basis_matches = 0;
    size_t shape_matches = 0;

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
            mbi.Type == MEM_PRIVATE &&
            IsReadableProtect(mbi.Protect) &&
            IsWritableDataProtect(mbi.Protect) &&
            read_start + sizeof(float) * 16 <= read_end) {
            uintptr_t chunk_start = read_start;
            while (chunk_start + sizeof(float) * 16 <= read_end) {
                const size_t chunk_size = static_cast<size_t>(std::min<uintptr_t>(read_end - chunk_start, kMaxChunk));
                std::vector<uint8_t> buffer(chunk_size);
                if (SafeCopyIn(chunk_start, buffer.data(), buffer.size())) {
                    for (size_t i = 0; i + sizeof(float) * 16 <= buffer.size(); i += 16) {
                        float m0 = 0.0f, m3 = 0.0f, m7 = 0.0f, m11 = 0.0f, m15 = 0.0f;
                        std::memcpy(&m0, buffer.data() + i, sizeof(float));
                        if (AbsDiff(m0, hint.right.x) > 0.08f) continue;
                        std::memcpy(&m3, buffer.data() + i + sizeof(float) * 3, sizeof(float));
                        std::memcpy(&m7, buffer.data() + i + sizeof(float) * 7, sizeof(float));
                        std::memcpy(&m11, buffer.data() + i + sizeof(float) * 11, sizeof(float));
                        std::memcpy(&m15, buffer.data() + i + sizeof(float) * 15, sizeof(float));
                        if (AbsDiff(m3, 0.0f) > 0.10f || AbsDiff(m7, 0.0f) > 0.10f ||
                            AbsDiff(m11, 0.0f) > 0.10f || AbsDiff(m15, 1.0f) > 0.10f) {
                            continue;
                        }

                        ++checked;
                        Matrix4 matrix{};
                        std::memcpy(matrix.m.data(), buffer.data() + i, sizeof(float) * matrix.m.size());
                        Pose pose{};
                        if (!DecodePose(matrix, pose)) continue;
                        const float basis_score = BasisScore(pose, hint);
                        if (basis_score < -3.0f) continue;
                        ++basis_matches;

                        const uintptr_t matrix_address = chunk_start + i;
                        if (matrix_address < kCameraMatrixOffset) continue;
                        const uintptr_t object = matrix_address - kCameraMatrixOffset;
                        uintptr_t vtable = 0;
                        if (!SafeRead(object, vtable)) continue;
                        vtable = EffectiveObjectVtable(object, vtable);
                        if (!IsKnownCameraVtable(vtable)) continue;

                        const float class_priority = CameraClassPriorityFromVtable(vtable);
                        float score = class_priority + (basis_score * 0.25f) + 0.75f; // exact camera vtable match
                        if (ViewTailLooksConsistent(matrix_address + kViewTailOffset - kCameraMatrixOffset, pose)) score += 0.25f;
                        if (DoublePositionLooksConsistent(object, pose)) score += 0.50f;
                        ++shape_matches;

                        if (score >= 4.50f) {
                            object_out = object;
                            spdlog::info("[FH5CAM] shape resolver early hit object=0x{:X} vtable=0x{:X} score={:.3f} class={:.2f} basis={:.3f} checked={} basis_count={} shape={}",
                                         object, vtable, score, class_priority, basis_score, checked, basis_matches, shape_matches);
                            return true;
                        }

                        if (score > best_score) {
                            best_score = score;
                            best_object = object;
                        }
                    }
                }
                chunk_start += chunk_size;
            }
        }
        cursor = region_end > cursor ? region_end : cursor + 0x1000;
    }

    if (best_object) {
        object_out = best_object;
        spdlog::info("[FH5CAM] shape resolver hit object=0x{:X} score={:.3f} checked={} basis={} shape={}",
                     best_object, best_score, checked, basis_matches, shape_matches);
        return true;
    }
    spdlog::info("[FH5CAM] shape resolver miss checked={} basis={} shape={}", checked, basis_matches, shape_matches);
    return false;
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

bool WriteCameraPose(uintptr_t object, const CameraLayout& layout, const Matrix4& camera_matrix, const Pose& pose) {
    const bool wrote_pose = SafeCopyOut(object + layout.matrix_offset,
                                        camera_matrix.m.data(),
                                        sizeof(float) * camera_matrix.m.size());
    if (!layout.writes_view_tail) {
        return wrote_pose;
    }
    const Matrix4 view_tail = ViewTailFromPose(pose);
    const bool wrote_tail = SafeCopyOut(object + layout.view_tail_offset,
                                        view_tail.m.data(),
                                        sizeof(float) * view_tail.m.size());
    return wrote_pose && wrote_tail;
}

void WriteCameraClones(const std::vector<CloneTarget>& clones, const Matrix4& camera_matrix, const Pose& pose) {
    if (clones.empty()) return;
    const Matrix4 view_tail = ViewTailFromPose(pose);
    for (const CloneTarget& clone : clones) {
        SafeCopyOut(clone.address, camera_matrix.m.data(), sizeof(float) * camera_matrix.m.size());
        if (clone.view_tail) {
            SafeCopyOut(clone.address + 0xC0, view_tail.m.data(), sizeof(float) * view_tail.m.size());
        }
    }
}

bool WriteCameraCloneByIndex(const std::vector<CloneTarget>& clones, int idx, const Matrix4& camera_matrix, const Pose& pose) {
    if (idx < 0 || static_cast<size_t>(idx) >= clones.size()) return false;
    const CloneTarget& clone = clones[static_cast<size_t>(idx)];
    const bool wrote_pose = SafeCopyOut(clone.address,
                                        camera_matrix.m.data(),
                                        sizeof(float) * camera_matrix.m.size());
    bool wrote_tail = true;
    if (clone.view_tail) {
        const Matrix4 view_tail = ViewTailFromPose(pose);
        wrote_tail = SafeCopyOut(clone.address + 0xC0,
                                 view_tail.m.data(),
                                 sizeof(float) * view_tail.m.size());
    }
    return wrote_pose && wrote_tail;
}

bool WriteFirstViewTailClone(const std::vector<CloneTarget>& clones, const Matrix4& camera_matrix, const Pose& pose) {
    for (size_t i = 0; i < clones.size(); ++i) {
        if (clones[i].view_tail) {
            return WriteCameraCloneByIndex(clones, static_cast<int>(i), camera_matrix, pose);
        }
    }
    return false;
}

bool HasViewTailClone(const std::vector<CloneTarget>& clones) {
    for (const CloneTarget& clone : clones) {
        if (clone.view_tail) return true;
    }
    return false;
}

bool WriteCameraPoseTargets(uintptr_t object,
                            const CameraLayout& layout,
                            const Matrix4& camera_matrix,
                            const Pose& pose,
                            const std::vector<CloneTarget>& clones,
                            int pos_lane,
                            bool write_clones) {
    bool wrote_primary = false;
    bool wrote = false;
    switch (pos_lane) {
    case fh5cb::kPosLaneViewTail:
        if (HasViewTailClone(clones)) {
            wrote_primary = WriteCameraPose(object, layout, camera_matrix, pose);
            wrote = wrote_primary && WriteFirstViewTailClone(clones, camera_matrix, pose);
        }
        break;
    case fh5cb::kPosLaneCcam320:
        wrote_primary = WriteCameraPose(object, layout, camera_matrix, pose);
        wrote = wrote_primary;
        break;
    case fh5cb::kPosLaneCcam320D550:
        wrote_primary = WriteCameraPose(object, layout, camera_matrix, pose);
        wrote = wrote_primary && WriteDoublePositionRaw(object, pose);
        break;
    case fh5cb::kPosLaneClone0:
        wrote = WriteCameraCloneByIndex(clones, 0, camera_matrix, pose);
        break;
    case fh5cb::kPosLaneClone1:
        wrote = WriteCameraCloneByIndex(clones, 1, camera_matrix, pose);
        break;
    case fh5cb::kPosLaneClone2:
        wrote = WriteCameraCloneByIndex(clones, 2, camera_matrix, pose);
        break;
    case fh5cb::kPosLaneDownstream:
    case fh5cb::kPosLaneOff:
    case fh5cb::kPosLaneInput540:
    case fh5cb::kPosLaneProducerA15:   // producer hook owns the f64 cameraPos write; worker stays out
        wrote = false;
        break;
    default:
        wrote_primary = WriteCameraPose(object, layout, camera_matrix, pose);
        wrote = wrote_primary;
        break;
    }
    if (wrote_primary && write_clones) {
        WriteCameraClones(clones, camera_matrix, pose);
    }
    return wrote;
}

void FormatCloneSummary(char* dst, size_t cap, const std::vector<CloneTarget>& clones, size_t idx) {
    if (dst == nullptr || cap == 0) return;
    dst[0] = '\0';
    if (idx >= clones.size()) {
        _snprintf_s(dst, cap, _TRUNCATE, "-");
        return;
    }

    const CloneTarget& clone = clones[idx];
    Pose pose{};
    if (ReadPoseAt(clone.address, pose)) {
        _snprintf_s(dst, cap, _TRUNCATE,
                    "0x%llX:vt=%d:pd=%.2f:bd=%.2f:pos=(%.2f,%.2f,%.2f)",
                    static_cast<unsigned long long>(clone.address),
                    clone.view_tail ? 1 : 0,
                    clone.pos_delta,
                    clone.basis_delta,
                    pose.position.x,
                    pose.position.y,
                    pose.position.z);
    } else {
        _snprintf_s(dst, cap, _TRUNCATE,
                    "0x%llX:vt=%d:pd=%.2f:bd=%.2f:unreadable",
                    static_cast<unsigned long long>(clone.address),
                    clone.view_tail ? 1 : 0,
                    clone.pos_delta,
                    clone.basis_delta);
    }
}

void LogLaneReadback(uintptr_t object,
                     const CameraLayout& layout,
                     const Pose& source_pose,
                     const Pose& out_pose,
                     const std::vector<CloneTarget>& clones,
                     int pos_lane,
                     bool applied) {
    Pose readback_pose{};
    const bool rb_ok = ReadPoseAt(object + layout.matrix_offset, readback_pose);
    double dpos[4]{};
    const bool d_ok = ReadDoublePositionRaw(object, dpos);
    InputLaneSnapshot input{};
    const bool input_ok = ReadInputLaneSnapshot(object, input);

    char c0[160], c1[160], c2[160];
    FormatCloneSummary(c0, sizeof(c0), clones, 0);
    FormatCloneSummary(c1, sizeof(c1), clones, 1);
    FormatCloneSummary(c2, sizeof(c2), clones, 2);

    spdlog::info("[FH5LANE] lane={} applied={} object=0x{:X} matrix=0x{:X} "
                 "src=({:.3f},{:.3f},{:.3f}) out=({:.3f},{:.3f},{:.3f}) "
                 "rb_ok={} rb=({:.3f},{:.3f},{:.3f}) d550_ok={} d550=({:.3f},{:.3f},{:.3f},{:.3f}) "
                 "in_ok={} f530=({:.3f},{:.3f},{:.3f},{:.3f}) f540=({:.3f},{:.3f},{:.3f},{:.3f}) "
                 "f550=({:.3f},{:.3f},{:.3f},{:.3f}) f570=({:.3f},{:.3f},{:.3f},{:.3f}) "
                 "clones={} c0={} c1={} c2={}",
                 fh5cb::pos_lane_name(pos_lane),
                 applied ? 1 : 0,
                 object,
                 object + layout.matrix_offset,
                 source_pose.position.x, source_pose.position.y, source_pose.position.z,
                 out_pose.position.x, out_pose.position.y, out_pose.position.z,
                 rb_ok ? 1 : 0,
                 readback_pose.position.x, readback_pose.position.y, readback_pose.position.z,
                 d_ok ? 1 : 0,
                 dpos[0], dpos[1], dpos[2], dpos[3],
                 input_ok ? 1 : 0,
                 input.base530.x, input.base530.y, input.base530.z, input.base530.w,
                 input.offset540.x, input.offset540.y, input.offset540.z, input.offset540.w,
                 input.derived550.x, input.derived550.y, input.derived550.z, input.derived550.w,
                 input.derived570.x, input.derived570.y, input.derived570.z, input.derived570.w,
                 clones.size(), c0, c1, c2);
}

// ---------------------------------------------------------------------------
// Worker thread: poll the latest hook-published/resolved active driver pose ~every 4 ms and apply the
// manual or OpenXR camera-local offset to the fresh engine pose.
// ---------------------------------------------------------------------------
DWORD WINAPI WorkerThread(void*) {
    spdlog::info("[FH5CAM] upstream CCamDriver position writer thread up (hook-published driver/FMC pointer; idle until manual tgt=driver or OpenXR offset)");

    // Keep the last clean source pose so writes are idempotent: if the engine has not refreshed +0x320
    // since our previous write, recompute from the stored source instead of accumulating offset on offset.
    Matrix4 last_written{};
    Matrix4 last_source{};
    CameraLayout last_layout{};
    bool have_last_written = false;
    bool have_last_source = false;
    bool have_last_layout = false;

    uint64_t last_status_ms = 0;
    uint64_t last_wait_ms = 0;
    uint64_t last_refcount_scan_ms = 0;
    uint64_t last_multicam_scan_ms = 0;
    bool tried_refcount_scan = false;
    bool tried_multicam_scan = false;
    uintptr_t last_active_object = 0;
    uint64_t last_active_change_ms = 0;
    bool last_writer_active = false;
    int last_pos_lane = fh5cb::ctl_pos_lane();
    std::vector<CloneTarget> camera_clones;
    bool have_scanned_clones = false;
    for (;;) {
        // Retail 1.688: NO inline .text camera hooks (anti-tamper kills them), so head-look runs entirely from
        // this worker via DATA writes — a hook-free ForzaMultiCam vtable scan + structural camera validation +
        // a +0x320 compose. The Empress flow below relies on hook-published pointers and per-build vtables that
        // don't exist on retail, so skip it.
        if (IsRetail1688Build()) {
            Tick1688DataHeadlook();
            Sleep(3);
            continue;
        }
        const int tgt = fh5cb::ctl_up_tgt();
        const int current_pos_lane = fh5cb::ctl_pos_lane();
        if (last_pos_lane == fh5cb::kPosLaneInput540 &&
            current_pos_lane != fh5cb::kPosLaneInput540 &&
            last_active_object != 0) {
            RestoreInput540Base(last_active_object);
            g_input540_base_object.store(0, std::memory_order_release);
        }
        last_pos_lane = current_pos_lane;

        float vr_strafe = 0.0f;
        float vr_up = 0.0f;
        float vr_fwd = 0.0f;
        Pose vr_delta{};
        int vr_eye = 0;
        const bool manual_active = (tgt == kTgtDriver);
        const bool vr_tracking_active = SnapshotOpenXrPose(vr_strafe, vr_up, vr_fwd, vr_delta, vr_eye);
        constexpr float kOffsetActiveEpsilon = 0.001f;
        constexpr float kRotationActiveEpsilon = 0.0005f;
        const float vr_rotation_delta = BasisDistance(vr_delta, Pose{});
        const bool vr_active =
            vr_tracking_active &&
            (std::fabs(vr_strafe) > kOffsetActiveEpsilon ||
             std::fabs(vr_up) > kOffsetActiveEpsilon ||
             std::fabs(vr_fwd) > kOffsetActiveEpsilon ||
             vr_rotation_delta > kRotationActiveEpsilon);
        const bool input540_lane = current_pos_lane == fh5cb::kPosLaneInput540;
        const bool input540_gameplay = !input540_lane || GameplayCameraRecentForInput540();
        if (input540_lane && !input540_gameplay && last_active_object != 0) {
            RestoreInput540AndForget(last_active_object);
            have_last_written = false;
            have_last_source = false;
            have_last_layout = false;
            LogInput540GateIfNeeded();
        }
        const bool writer_active = (manual_active || vr_active || input540_lane) && input540_gameplay;

        // IDLE for writes unless the user engages the manual upstream test or OpenXR publishes a live head/IPD
        // offset. Still keep resolving/classifying the active camera so menu automation can distinguish
        // garage/showroom/free-roam without requiring camera writes.
        if (!writer_active) {
            if (last_writer_active && have_last_written && last_active_object) {
                spdlog::info("[FH5CAM] writer disabled; leaving engine-owned camera pose to refresh object=0x{:X}",
                             last_active_object);
            }
            RestoreDriverPositionGetterShadow();
            have_last_written = false;
            have_last_source = false;
            have_last_layout = false;
            camera_clones.clear();
            have_scanned_clones = false;
            last_writer_active = false;
        } else if (!last_writer_active) {
            const uintptr_t shape_object = g_shape_driver_object.load(std::memory_order_acquire);
            if (shape_object != 0 && shape_object == g_driver_object.load(std::memory_order_acquire)) {
                g_driver_object.store(0, std::memory_order_release);
                g_shape_driver_object.store(0, std::memory_order_release);
            }
            have_last_written = false;
            have_last_source = false;
            have_last_layout = false;
            last_active_object = 0;
            tried_refcount_scan = false;
            tried_multicam_scan = false;
            last_refcount_scan_ms = 0;
            last_multicam_scan_ms = 0;
            last_active_change_ms = 0;
            camera_clones.clear();
            have_scanned_clones = false;
        }
        last_writer_active = writer_active;

        uintptr_t active_from_fmc = 0;
        uintptr_t multicam_object = g_multicam_published_object.load(std::memory_order_acquire);
        const bool have_fmc_active = ReadActiveDriverFromMulticam(multicam_object, active_from_fmc);
        uintptr_t active_object = g_driver_object.load(std::memory_order_acquire);
        uintptr_t object_to_use =
            have_fmc_active ? active_from_fmc :
            (active_object ? active_object : 0);

        if (!object_to_use) {
            Pose hint{};
            // SCANS DISABLED 2026-06-06: ResolveMulticamByScan/RefcountScan/ByShape walked transient/guard
            // pages during load and tripped NVIDIA's overlay VEH into a 0xc00000fd crash. The active camera
            // is now captured (validated) from the per-frame sub_1407A1AC0 hook via capture_active_camera(),
            // which publishes g_driver_object. While no camera is published yet (early load) the worker just
            // idles below — no scanning, no first-chance AV. See [[fh5-nvidia-veh-crash]].
            if (false && SnapshotPoseHint(hint)) {
                const uint64_t now_ms = NowMs();
                if (!object_to_use && (!tried_multicam_scan || now_ms - last_multicam_scan_ms >= 5000)) {
                    tried_multicam_scan = true;
                    last_multicam_scan_ms = now_ms;
                    uintptr_t scanned_multicam = 0;
                    uintptr_t scanned_active = 0;
                    if (ResolveMulticamByScan(hint, scanned_multicam, scanned_active)) {
                        multicam_object = scanned_multicam;
                        active_from_fmc = scanned_active;
                        object_to_use = scanned_active;
                    }
                }

                if (!object_to_use && (!tried_refcount_scan || now_ms - last_refcount_scan_ms >= 10000)) {
                    tried_refcount_scan = true;
                    last_refcount_scan_ms = now_ms;
                    if (ResolveDriverByRefcountScan(hint, object_to_use)) {
                        g_driver_object.store(object_to_use, std::memory_order_release);
                        g_shape_driver_object.store(0, std::memory_order_release);
                        active_object = object_to_use;
                    }
                }

                if (!object_to_use && writer_active && ResolveDriverByShape(hint, object_to_use)) {
                    g_driver_object.store(object_to_use, std::memory_order_release);
                    g_shape_driver_object.store(object_to_use, std::memory_order_release);
                    g_shape_resolve_count.fetch_add(1, std::memory_order_relaxed);
                    active_object = object_to_use;
                }
            }
        }

        if (!object_to_use) {
            have_last_written = false;
            have_last_source = false;
            have_last_layout = false;
            const uint64_t now_ms = NowMs();
            if (now_ms - last_wait_ms >= 1000) {
                last_wait_ms = now_ms;
                spdlog::info("[FH5CAM] waiting for camera pointer (driver_published={} arg_published={} shape_resolved={} fmc=0x{:X} fmc_published={})",
                             g_driver_publish_count.load(std::memory_order_relaxed),
                             g_arg_publish_count.load(std::memory_order_relaxed),
                             g_shape_resolve_count.load(std::memory_order_relaxed),
                             multicam_object,
                             g_multicam_publish_count.load(std::memory_order_relaxed));
            }
            Sleep(50);
            continue;
        }
        if (object_to_use != last_active_object) {
            if (g_shadow.object != 0 && g_shadow.object != object_to_use) {
                RestoreDriverPositionGetterShadow();
            }
            last_active_object = object_to_use;
            last_active_change_ms = NowMs();
            have_last_written = false;
            have_last_source = false;
            have_last_layout = false;
            camera_clones.clear();
            have_scanned_clones = false;
            const char* source =
                have_fmc_active ? "multicam" :
                active_object ? (object_to_use == g_shape_driver_object.load(std::memory_order_acquire) ? "shape" : "driver") :
                (active_from_fmc ? "multicam" : "unknown");
            spdlog::info("[FH5CAM] captured active camera object=0x{:X} source={} driver_published={} arg_published={} shape_resolved={} fmc=0x{:X} fmc_published={}",
                         object_to_use,
                         source,
                         g_driver_publish_count.load(std::memory_order_relaxed),
                         g_arg_publish_count.load(std::memory_order_relaxed),
                         g_shape_resolve_count.load(std::memory_order_relaxed),
                         multicam_object,
                         g_multicam_publish_count.load(std::memory_order_relaxed));
        }

        Matrix4 base{};
        Pose base_pose{};
        Pose hint{};
        if (!SnapshotPoseHint(hint)) {
            Sleep(4);
            continue;
        }
        float basis_score = -1000.0f;
        uintptr_t identity_vtable = 0;
        CameraLayout layout{};
        if (!ResolveCameraLayout(object_to_use, hint, base, base_pose, basis_score, identity_vtable, layout, -3.0f)) {
            have_last_written = false;
            have_last_source = false;
            have_last_layout = false;
            Sleep(20);
            continue;
        }

        // Runtime orientation probe from the WORKER (fires reliably even when the camera-update fold is quiet,
        // e.g. parked): dumpcam scans this camera + its view-source for the orthonormal orientation matrix;
        // pokerot/pokerotvs live-rotate a chosen offset. The fold-hook path is preferred for the final fix
        // (synchronous, fires per-frame while driving), but the worker is what lets us FIND the source now.
        probe_camera(object_to_use);
        // Head-look rotation is applied at the getter sub_1407A9DD0 POST-original (Hook_AimGetter ->
        // apply_camdriver_head_rotation on +0x320), i.e. right after the getter rebuilds the orientation,
        // beating the per-frame clobber. NOT from the worker (async writes lose to the getter's rebuild).

        const float class_priority = layout.class_priority;
        if (!writer_active) {
            const uint64_t now_ms = NowMs();
            if (now_ms - last_status_ms >= 2000) {
                last_status_ms = now_ms;
                spdlog::info("[FH5CAM] classified active camera=0x{:X} vtable=0x{:X} layout={} class={:.2f} basis={:.3f} (writer idle)",
                             object_to_use,
                             identity_vtable,
                             layout.name,
                             class_priority,
                             basis_score);
            }
            Sleep(100);
            continue;
        }

        const bool openxr_writable_camera =
            vr_active &&
            !manual_active &&
            layout.writes_view_tail &&
            class_priority >= 3.0f &&
            basis_score >= -0.5f;
        const bool openxr_direct_candidate = openxr_writable_camera;
        const uint64_t prewrite_now_ms = NowMs();
        const bool camera_stable =
            last_active_change_ms != 0 &&
            prewrite_now_ms >= last_active_change_ms &&
            (prewrite_now_ms - last_active_change_ms) >= 750;
        if (!camera_stable || class_priority < 3.0f || (!openxr_direct_candidate && basis_score < 1.0f)) {
            have_last_written = false;
            have_last_source = false;
            if (prewrite_now_ms - last_status_ms >= 1000) {
                last_status_ms = prewrite_now_ms;
                spdlog::info("[FH5CAM] skip write camera=0x{:X} vtable=0x{:X} layout={} matrix=0x{:X} class={:.2f} basis={:.3f} stable={} age_ms={} vr={} manual={}",
                             object_to_use,
                             identity_vtable,
                             layout.name,
                             layout.matrix_offset,
                             class_priority,
                             basis_score,
                             camera_stable ? 1 : 0,
                             last_active_change_ms ? (prewrite_now_ms - last_active_change_ms) : 0,
                             vr_active ? 1 : 0,
                             manual_active ? 1 : 0);
            }
            Sleep(10);
            continue;
        }

        const uintptr_t shape_object = g_shape_driver_object.load(std::memory_order_acquire);
        if (shape_object != 0 && shape_object == object_to_use) {
            if (basis_score < -3.0f) {
                spdlog::warn("[FH5CAM] shape-resolved camera went stale object=0x{:X} basis={:.3f}; clearing",
                             object_to_use, basis_score);
                if (g_driver_object.load(std::memory_order_acquire) == object_to_use) {
                    g_driver_object.store(0, std::memory_order_release);
                }
                g_shape_driver_object.store(0, std::memory_order_release);
                have_last_written = false;
                have_last_source = false;
                have_last_layout = false;
                last_active_object = 0;
                Sleep(50);
                continue;
            }
        }

        if (vr_active && !manual_active && !openxr_writable_camera) {
            RestoreDriverPositionGetterShadow();
            have_last_written = false;
            have_last_source = false;
            have_last_layout = false;
            camera_clones.clear();
            have_scanned_clones = false;
            if (prewrite_now_ms - last_status_ms >= 1000) {
                last_status_ms = prewrite_now_ms;
                spdlog::info("[FH5CAM] skip OpenXR write camera=0x{:X} vtable=0x{:X} layout={} class={:.2f} basis={:.3f}; waiting for writable driving camera",
                             object_to_use,
                             identity_vtable,
                             layout.name,
                             class_priority,
                             basis_score);
            }
            Sleep(20);
            continue;
        }

        if (vr_active && !manual_active) {
            // The +0x550 getter is not a stable live 6DOF lever: even a constant 0.3m
            // head offset can flip the cockpit into the sky/culling view. Keep it disabled;
            // poslane=input540 now tests the Empress additive input path.
            RestoreDriverPositionGetterShadow();
        }

        RestoreDriverPositionGetterShadow();

        if (!have_scanned_clones) {
            have_scanned_clones = true;
            camera_clones.clear();
            if (layout.writes_view_tail) {
                const uintptr_t matrix_address = object_to_use + layout.matrix_offset;
                camera_clones = ScanMatrixClones(matrix_address, base);
                size_t clone_tails = 0;
                for (const CloneTarget& clone : camera_clones) {
                    if (clone.view_tail) ++clone_tails;
                }
                spdlog::info("[FH5CAM] matrix clone scan object=0x{:X} matrix=0x{:X} clones={} view_tails={}",
                             object_to_use,
                             matrix_address,
                             camera_clones.size(),
                             clone_tails);
            }
        }

        float off_strafe = 0.0f;
        float off_up = 0.0f;
        float off_fwd = 0.0f;
        if (manual_active) {
            off_strafe += fh5cb::ctl_up_strafe();
            off_up += fh5cb::ctl_up_up();
            off_fwd += fh5cb::ctl_up_fwd();
        }
        if (vr_active) {
            off_strafe += vr_strafe;
            off_up += vr_up;
            off_fwd += vr_fwd;
        }
        const bool invalid_offset =
            !std::isfinite(off_strafe) ||
            !std::isfinite(off_up) ||
            !std::isfinite(off_fwd);
        if (invalid_offset) {
            have_last_written = false;
            have_last_source = false;
            have_last_layout = false;
            Sleep(10);
            continue;
        }
        const bool clamped = false;

        Matrix4 source = base;
        Pose source_pose = base_pose;
        bool reused_source = false;
        Pose last_written_pose{};
        if (have_last_written && have_last_source &&
            DecodePose(last_written, last_written_pose) &&
            MatrixLooksLikePose(base, last_written_pose, 0.25f, 0.05f)) {
            Pose decoded_source{};
            if (DecodePose(last_source, decoded_source)) {
                source = last_source;
                source_pose = decoded_source;
                reused_source = true;
            } else {
                have_last_written = false;
                have_last_source = false;
                have_last_layout = false;
            }
        }
        if (!reused_source) {
            last_source = base;
            last_layout = layout;
            have_last_source = true;
            have_last_layout = true;
        }

        bool applied = false;
        Pose relative_pose{};
        if (vr_active && vr_rotation_delta > kRotationActiveEpsilon) {
            relative_pose.right = vr_delta.right;
            relative_pose.up = vr_delta.up;
            relative_pose.forward = vr_delta.forward;
        }
        relative_pose.position = { off_strafe, off_up, off_fwd };

        Pose out_pose{};
        out_pose.right = TransformDirection(source_pose, relative_pose.right);
        out_pose.up = TransformDirection(source_pose, relative_pose.up);
        out_pose.forward = TransformDirection(source_pose, relative_pose.forward);
        out_pose.position = Add(source_pose.position, TransformDirection(source_pose, relative_pose.position));
        Orthonormalize(out_pose);

        Matrix4 out = MatrixFromPose(out_pose);

        const int pos_lane = current_pos_lane;
        // The lane selector is deliberately explicit. Clone writes and raw +0x550 mirroring are diagnostics
        // until visible movement proves which field FH5 actually consumes for this camera mode.
        const bool write_clones = manual_active && !vr_active && pos_lane == fh5cb::kPosLaneCcam320;
        if (pos_lane == fh5cb::kPosLaneInput540) {
            const Vec3 input_offset = MapCameraOffsetToInput540({ off_strafe, off_up, off_fwd });
            applied = WriteInput540Offset(object_to_use, input_offset);
            if (applied) {
                g_input540_writes.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_input540_bad.fetch_add(1, std::memory_order_relaxed);
            }
            have_last_written = false;
        } else if (WriteCameraPoseTargets(object_to_use, layout, out, out_pose, camera_clones, pos_lane, write_clones)) {
            last_written = out;
            have_last_written = pos_lane == fh5cb::kPosLaneCcam320 ||
                                pos_lane == fh5cb::kPosLaneCcam320D550 ||
                                pos_lane == fh5cb::kPosLaneViewTail;
            applied = true;
        }

        const uint64_t now_ms = NowMs();
        if (now_ms - last_status_ms >= 1000) {
            last_status_ms = now_ms;
            LogLaneReadback(object_to_use, layout, source_pose, out_pose, camera_clones, pos_lane, applied);
            spdlog::info("[FH5CAM] camera=0x{:X} driver_published={} arg_published={} shape_resolved={} fmc_published={} openxr_published={} pos=({:.3f},{:.3f},{:.3f}) "
                         "tgt={} manual={} vr={} eye={} off=({:.3f},{:.3f},{:.3f}) rot_delta={:.4f} applied={} source={} layout={} matrix=0x{:X} class={:.2f} basis={:.3f} clones={} clamped={} poslane={}",
                         object_to_use,
                         g_driver_publish_count.load(std::memory_order_relaxed),
                         g_arg_publish_count.load(std::memory_order_relaxed),
                         g_shape_resolve_count.load(std::memory_order_relaxed),
                         g_multicam_publish_count.load(std::memory_order_relaxed),
                         g_openxr_publish_count.load(std::memory_order_relaxed),
                         out_pose.position.x, out_pose.position.y, out_pose.position.z,
                         tgt, manual_active ? 1 : 0, vr_active ? 1 : 0, vr_eye,
                         off_strafe, off_up, off_fwd, BasisDistance(relative_pose, Pose{}),
                         applied ? 1 : 0, reused_source ? "cached" : "engine",
                         layout.name, layout.matrix_offset, class_priority, basis_score,
                         write_clones ? camera_clones.size() : 0, clamped ? 1 : 0,
                         fh5cb::pos_lane_name(pos_lane));
        }

        Sleep(1);
    }
    // unreachable
}

// ---------------------------------------------------------------------------
// PAGE_GUARD tracer for the RENDERED camera's per-frame +0x90 angle writer (ported from
// FH5CameraProbe/src/PageGuardTracerDll.cpp, which found sub_1407A1AC0 this way). Finds the upstream
// head-look injection point (the function that writes g_driver_object+0x90 each frame). PAGE_GUARD — NOT a
// HW data-bp (that crashes FH5). Our front VEH (AddVectoredExceptionHandler First=1) CONSUMES the guard
// fault (EXCEPTION_CONTINUE_EXECUTION) so NVIDIA's overlay VEH never sees it. Gated to rot=driver; logs the
// distinct writer RVAs to FH5VR.log ([FH5PGTRACE]). Hand the RVA to RE to decompile.
// ---------------------------------------------------------------------------
// DIAGNOSTIC first-chance exception logger. Registered FIRST. CRITICAL SAFETY: it ONLY inspects faults whose
// faulting RIP lies inside OUR OWN module (FH5VR.dll). For ALL other faults -- especially the Empress de-DRM's
// (EMP.dll+0x21A1C reads 0x76350000 in a tight exception-VM loop, sometimes storming hundreds/ms) -- it returns
// EXCEPTION_CONTINUE_SEARCH IMMEDIATELY after a cheap range check, doing NO logging/module-lookup. That is the
// whole point: a VEH that does heavy work on the de-DRM's exceptions is exactly what amplifies the storm into a
// crash (the documented failure mode). So we observe only OUR hook faults (the actionable ones) and never touch
// the de-DRM/driver/overlay exceptions. No thread_local (faults on pre-existing threads); depth-capped.
PVOID g_diag_veh = nullptr;
std::atomic<uint64_t> g_diag_veh_count{ 0 };
std::atomic<int>      g_diag_veh_depth{ 0 };
std::atomic<uint64_t> g_diag_veh_last_ms{ 0 };
uintptr_t             g_self_base = 0, g_self_end = 0;   // FH5VR.dll image range (set in InstallDiagVeh)

LONG CALLBACK DiagVeh(EXCEPTION_POINTERS* ep) {
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    // Only hard memory faults; ignore C++ EH (0xE06D7363), guard-page (PgVeh owns it), DBG/control codes, etc.
    if (code != 0xC0000005u /*AV*/ && code != 0xC0000006u /*in-page*/ && code != 0xC00000FDu /*stack overflow*/)
        return EXCEPTION_CONTINUE_SEARCH;
    // SELF-ONLY GATE (cheap, FIRST): ignore every fault whose RIP is outside FH5VR.dll. The de-DRM faults
    // constantly as normal operation -- doing ANY work on those (esp. during a storm) destabilises it. Bail now.
    const uintptr_t rip = (uintptr_t)ep->ContextRecord->Rip;
    if (!g_self_base || rip < g_self_base || rip >= g_self_end) return EXCEPTION_CONTINUE_SEARCH;
    if (g_diag_veh_depth.fetch_add(1, std::memory_order_relaxed) >= 16) {   // runaway backstop (no thread_local)
        g_diag_veh_depth.fetch_sub(1, std::memory_order_relaxed);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const uint64_t n = g_diag_veh_count.fetch_add(1, std::memory_order_relaxed);
    bool do_log = n < 80;                                                   // always log the first 80...
    if (!do_log) {                                                         // ...then at most ~1/sec
        const uint64_t now = ::GetTickCount64();
        uint64_t last = g_diag_veh_last_ms.load(std::memory_order_relaxed);
        if (now - last >= 1000 && g_diag_veh_last_ms.compare_exchange_strong(last, now, std::memory_order_relaxed))
            do_log = true;
    }
    if (do_log) {
        const ULONG np = ep->ExceptionRecord->NumberParameters;
        const int       rw = np >= 1 ? (int)ep->ExceptionRecord->ExceptionInformation[0] : -1;
        const uintptr_t fa = np >= 2 ? (uintptr_t)ep->ExceptionRecord->ExceptionInformation[1] : 0;
        char modname[64] = "?"; uintptr_t rva = 0;
        HMODULE mod = nullptr;
        if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 (LPCSTR)rip, &mod) && mod) {
            char full[MAX_PATH] = {0};
            if (::GetModuleFileNameA(mod, full, MAX_PATH)) {
                const char* b = strrchr(full, '\\');
                strncpy_s(modname, sizeof(modname), b ? b + 1 : full, _TRUNCATE);
            }
            rva = rip - (uintptr_t)mod;
        }
        spdlog::warn("[FH5VEH] first-chance code=0x{:X} rip=0x{:X} mod={}+0x{:X} access={} faultAddr=0x{:X} n={}",
                     (uint32_t)code, rip, modname, rva,
                     rw == 0 ? "READ" : (rw == 1 ? "WRITE" : (rw == 8 ? "EXEC" : "?")), fa, n);
    }
    g_diag_veh_depth.fetch_sub(1, std::memory_order_relaxed);
    return EXCEPTION_CONTINUE_SEARCH;                                       // pure observer -> behavior unchanged
}

void InstallDiagVeh() {
    if (g_diag_veh) return;
    // Resolve OUR module's image range so DiagVeh can cheaply ignore all non-FH5VR.dll faults (esp. the de-DRM's).
    HMODULE self = nullptr;
    if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             (LPCSTR)&InstallDiagVeh, &self) && self) {
        g_self_base = (uintptr_t)self;
        auto* dos = (IMAGE_DOS_HEADER*)self;
        auto* nt  = (IMAGE_NT_HEADERS*)(g_self_base + dos->e_lfanew);
        g_self_end = g_self_base + nt->OptionalHeader.SizeOfImage;
    }
    g_diag_veh = AddVectoredExceptionHandler(1, &DiagVeh);   // FIRST, but self-only -> never touches de-DRM faults
    spdlog::info("[FH5VEH] diagnostic logger installed self=0x{:X}..0x{:X} ({})",
                 g_self_base, g_self_end, g_diag_veh ? "ok" : "FAILED");
}

PVOID g_pg_veh = nullptr;
std::atomic<uintptr_t> g_pg_page{ 0 };      // guarded page base (0 = disarmed/consumed)
std::atomic<int> g_pg_kind{ -1 };           // -1 none, 0 read, 1 write, 8 exec
std::atomic<uintptr_t> g_pg_rip{ 0 }, g_pg_addr{ 0 };

LONG CALLBACK PgVeh(EXCEPTION_POINTERS* ep) {
    constexpr DWORD kGuard = 0x80000001;    // STATUS_GUARD_PAGE_VIOLATION
    if (ep->ExceptionRecord->ExceptionCode != kGuard) return EXCEPTION_CONTINUE_SEARCH;
    const uintptr_t fa = static_cast<uintptr_t>(ep->ExceptionRecord->ExceptionInformation[1]);
    const uintptr_t pg = g_pg_page.load(std::memory_order_relaxed);
    if (!pg || (fa & ~static_cast<uintptr_t>(0xFFF)) != pg) return EXCEPTION_CONTINUE_SEARCH;  // not our page
    g_pg_kind.store(static_cast<int>(ep->ExceptionRecord->ExceptionInformation[0]));
    g_pg_rip.store(static_cast<uintptr_t>(ep->ContextRecord->Rip));
    g_pg_addr.store(fa);
    g_pg_page.store(0);   // consume; guard auto-cleared -> re-exec succeeds; tracer re-arms
    return EXCEPTION_CONTINUE_EXECUTION;
}

DWORD WINAPI PgTracerThread(void*) {
    g_pg_veh = AddVectoredExceptionHandler(1, &PgVeh);   // FIRST -> before NVIDIA's overlay VEH
    if (!EnsureModuleBase()) return 0;
    uintptr_t seenW[8]{}; int nW = 0; uintptr_t seenR[8]{}; int nR = 0;
    auto have = [](uintptr_t* a, int n, uintptr_t v) { for (int i = 0; i < n; ++i) if (a[i] == v) return true; return false; };
    int arms = 0; uint64_t lastStatus = 0;
    while (arms < 6000 && nW < 6) {
        if (fh5cb::ctl_rotation_mode() != 2) { Sleep(150); continue; }   // only trace in rot=driver
        const uintptr_t cam = g_driver_object.load(std::memory_order_acquire);
        if (cam < 0x10000ull) { Sleep(50); continue; }
        const uintptr_t watch = cam + 0x90;
        const uintptr_t page = watch & ~static_cast<uintptr_t>(0xFFF);
        g_pg_kind.store(-1);
        g_pg_page.store(page);
        DWORD old = 0;
        if (!VirtualProtect(reinterpret_cast<LPVOID>(page), 0x1000, PAGE_READWRITE | PAGE_GUARD, &old)) {
            g_pg_page.store(0); ++arms; Sleep(2); continue;
        }
        int k = -1;
        for (int i = 0; i < 40; ++i) { Sleep(2); k = g_pg_kind.load(); if (k >= 0) break; }
        ++arms;
        if (k < 0) {   // no fault in window: lift the guard we set
            const uintptr_t pg = g_pg_page.exchange(0);
            if (pg) { DWORD o2 = 0; VirtualProtect(reinterpret_cast<LPVOID>(pg), 0x1000, PAGE_READWRITE, &o2); }
            continue;
        }
        const uintptr_t rip = g_pg_rip.load(), fa = g_pg_addr.load();
        const bool in90 = (fa >= watch && fa < watch + 12);   // [+0x90,+0x9C) = the Euler angle triple
        const uintptr_t rrva = (rip >= g_module_base && rip < g_module_base + 0x10000000ull) ? rip - g_module_base : rip;
        if (k == 1 && in90) {
            if (!have(seenW, nW, rrva) && nW < 8) { seenW[nW++] = rrva;
                spdlog::info("[FH5PGTRACE] +0x90 WRITER rva=0x{:X} cam=0x{:X} faddr_off=+0x{:X} (writers={})",
                             rrva, cam, static_cast<unsigned>(fa - cam), nW);
            }
        } else if (k == 0 && in90) {
            if (!have(seenR, nR, rrva) && nR < 8) seenR[nR++] = rrva;
        }
        const uint64_t now = NowMs();
        if (now - lastStatus >= 2000) { lastStatus = now;
            spdlog::info("[FH5PGTRACE] status arms={} +0x90_writers={} +0x90_readers={}", arms, nW, nR); }
    }
    spdlog::info("[FH5PGTRACE] DONE +0x90_writers={} (arms={}). RVAs above are the per-frame angle setter.", nW, arms);
    if (g_pg_veh) { RemoveVectoredExceptionHandler(g_pg_veh); g_pg_veh = nullptr; }
    return 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void start() {
    if (g_started.exchange(true)) return;   // idempotent: launch the worker exactly once
    InstallDiagVeh();   // FIRST: log every first-chance fault (incl. our hooks' stale-ptr AVs during loading)
    // PAGE_GUARD tracer for the rendered camera's +0x90 writer (gated to rot=driver inside the thread).
    if (HANDLE pg = CreateThread(nullptr, 0, &PgTracerThread, nullptr, 0, nullptr)) { CloseHandle(pg); }
    if (HANDLE t = CreateThread(nullptr, 0, &WorkerThread, nullptr, 0, nullptr)) {
        CloseHandle(t);
    } else {
        g_started.store(false);   // launch failed; allow a later retry
        spdlog::error("[FH5CAM] failed to create worker thread");
    }
}

void publish_driver(uintptr_t object) {
    if (object == 0) return;
    g_shape_driver_object.store(0, std::memory_order_release);
    g_driver_object.store(object, std::memory_order_release);
    g_driver_publish_count.fetch_add(1, std::memory_order_relaxed);
}

// AV-SAFE camera capture from the per-frame +0x320 writer hook (sub_1407A1AC0), whose a1 IS the live
// camera. This REPLACES the worker's memory SCAN (ResolveMulticamByScan/RefcountScan/ByShape), which walked
// transient/guard pages during "waiting for camera pointer" mid-load and raised a first-chance AV that
// NVIDIA's overlay VEH turned into a 0xc00000fd stack-overflow crash ([[fh5-nvidia-veh-crash]]). `cam` was
// just dereferenced by the engine (it read cam+0x48/0x60 and wrote cam+0x320), so cam+0 and cam+0x320 are
// mapped — no arbitrary-memory fault. Validate (in-module vtable + known camera vtable + orthonormal +0x320)
// then publish g_driver_object.
void capture_active_camera(uintptr_t cam) {
    if (cam < 0x10000ull) return;
    if (g_driver_object.load(std::memory_order_acquire) == cam) return;   // already current; skip re-validate
    uintptr_t vt = 0;
    if (!SafeRead(cam, vt)) return;
    vt = EffectiveObjectVtable(cam, vt);
    if (!IsKnownCameraVtable(vt)) return;
    Matrix4 m{};
    if (!SafeCopyIn(cam + kCameraMatrixOffset, m.m.data(), sizeof(float) * m.m.size())) return;
    Pose p{};
    if (!DecodePose(m, p)) return;
    g_shape_driver_object.store(0, std::memory_order_release);
    g_driver_object.store(cam, std::memory_order_release);
    g_driver_publish_count.fetch_add(1, std::memory_order_relaxed);
}

bool current_local_offset(float& strafe, float& up, float& fwd) {
    strafe = up = fwd = 0.0f;
    bool active = false;
    if (fh5cb::ctl_up_tgt() == kTgtDriver) {
        strafe += fh5cb::ctl_up_strafe();
        up     += fh5cb::ctl_up_up();
        fwd    += fh5cb::ctl_up_fwd();
        active = true;
    }
    float s = 0.0f, u = 0.0f, f = 0.0f;
    Pose delta{};
    int eye = 0;
    if (SnapshotOpenXrPose(s, u, f, delta, eye)) {
        strafe += s; up += u; fwd += f;
        active = true;
    }
    if (!std::isfinite(strafe) || !std::isfinite(up) || !std::isfinite(fwd)) {
        strafe = up = fwd = 0.0f;
        return false;
    }
    constexpr float kEps = 1e-4f;
    return active && (std::fabs(strafe) > kEps || std::fabs(up) > kEps || std::fabs(fwd) > kEps);
}

// Anti-accumulation cache for the +0x320 rotation. The engine re-derives +0x320 from the car each frame;
// we detect that refresh by comparing to our last write, so we compose head rotation onto a CLEAN base
// rather than onto an already-rotated matrix (which would spin a parked camera).
static uintptr_t g_camrot_obj = 0;
static Matrix4 g_camrot_base{};
static Matrix4 g_camrot_written{};

static bool MatricesClose16(const Matrix4& a, const Matrix4& b, float eps) {
    for (size_t i = 0; i < a.m.size(); ++i) {
        if (std::fabs(a.m[i] - b.m[i]) > eps) return false;
    }
    return true;
}

// Row-vector compose r = d * m (same convention as the proven producer a4 rotation Mul(delta, a4)).
static Matrix4 MulRowVector(const Matrix4& d, const Matrix4& m) {
    Matrix4 r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += d.m[i * 4 + k] * m.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    }
    return r;
}

static int Retail1688EffectivePosLane() {
    const int lane = fh5cb::ctl_pos_lane();
    // Steam retail has no producer hook path. Preserve the Empress default globally, but keep direct retail
    // launches from silently selecting a lane this worker cannot consume.
    return lane == fh5cb::kPosLaneProducerA15 ? fh5cb::kPosLaneCamSrc : lane;
}

bool apply_camdriver_head_rotation(uintptr_t object) {
    if (object < 0x10000ull) return false;
    // Mode 2 ONLY (rot=driver). In mode 3 (rot=angle) the head-look is injected at the orientation angles
    // cam+0x90 instead (apply_angle_head_rotation_prewrite); rotating the +0x320 MATRIX here too would
    // double-apply. In mode 1 (rot=a4) the producer rotates a4 directly and this path is left untouched.
    if (fh5cb::ctl_rotation_mode() != 2) return false;
    if (fh5cb::ctl_pos_lane() != fh5cb::kPosLaneProducerA15) return false;

    // Head rotation (row-vector 3x3, rows = right/up/forward) published by apply_stereo via the driver
    // delta; identity at rest. If VR is not engaged, leave the engine pose untouched.
    float strafe = 0.0f, up = 0.0f, fwd = 0.0f;
    Pose delta{};
    int eye = 0;
    if (!SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) return false;

    // Gate: a known camera object with an orthonormal +0x320 basis (the publisher can fire for more than
    // the active camera). DecodePose enforces orthonormal basis + cam-to-world layout.
    uintptr_t vtable = 0;
    if (!SafeRead(object, vtable)) return false;
    vtable = EffectiveObjectVtable(object, vtable);
    if (!IsKnownCameraVtable(vtable)) return false;

    Matrix4 cur{};
    if (!SafeCopyIn(object + kCameraMatrixOffset, cur.m.data(), sizeof(float) * cur.m.size())) return false;
    Pose decoded{};
    if (!DecodePose(cur, decoded)) return false;

    // Refresh detection -> clean base (no accumulation).
    if (g_camrot_obj != object) {
        g_camrot_obj = object;
        g_camrot_base = cur;
    } else if (!MatricesClose16(cur, g_camrot_written, 1e-3f)) {
        g_camrot_base = cur;   // engine wrote a fresh pose this frame
    }   // else: engine did not refresh -> reuse g_camrot_base

    Matrix4 rot{};
    rot.m[0]  = delta.right.x;   rot.m[1]  = delta.right.y;   rot.m[2]  = delta.right.z;   rot.m[3]  = 0.0f;
    rot.m[4]  = delta.up.x;      rot.m[5]  = delta.up.y;      rot.m[6]  = delta.up.z;      rot.m[7]  = 0.0f;
    rot.m[8]  = delta.forward.x; rot.m[9]  = delta.forward.y; rot.m[10] = delta.forward.z; rot.m[11] = 0.0f;
    rot.m[12] = 0.0f;            rot.m[13] = 0.0f;            rot.m[14] = 0.0f;            rot.m[15] = 1.0f;

    // Compose head rotation in front of the camera-to-world basis (keeps the position row 3), then commit
    // BEFORE the publisher copies the pose downstream so the shadow cascades fit to the rotated frustum.
    const Matrix4 out = MulRowVector(rot, g_camrot_base);
    if (!SafeCopyOut(object + kCameraMatrixOffset, out.m.data(), sizeof(float) * out.m.size())) return false;
    g_camrot_written = out;
    // DO NOT write +0x360. Raw disassembly of the +0x320 writer sub_1407A1AC0 (user, 2026-06-06) proves
    // `+0x320` = the 4x4 cam-to-world (vtbl[0x60]()), and `+0x360` is a SCALAR (movss [rbx+0x360],xmm0 from
    // vtbl[0xA0] — an FOV/scale), NOT an inverse view-tail. The old 64-byte ViewTailFromPose write here
    // clobbered the FOV scalar + 60 bytes past it -> the "skyward"/washed-out/black render. The matrix at
    // +0x320 is self-contained; rotating it (above) is sufficient. (kViewTailOffset/+0x360 belief was wrong.)
    {   // ~1/s diagnostic: confirms the getter fires + +0x320 is being rotated (calls climb while driving).
        static uint64_t s_last = 0;
        static uint64_t s_calls = 0;
        ++s_calls;
        const uint64_t now = NowMs();
        if (now - s_last >= 1000) {
            s_last = now;
            const float rotmag = std::fabs(rot.m[0] - 1.0f) + std::fabs(rot.m[5] - 1.0f) + std::fabs(rot.m[10] - 1.0f);
            spdlog::info("[FH5CAMROT] obj=0x{:X} calls={} rotMag={:.3f} baseFwd=({:.3f},{:.3f},{:.3f}) outFwd=({:.3f},{:.3f},{:.3f})",
                         object, s_calls, rotmag,
                         g_camrot_base.m[8], g_camrot_base.m[9], g_camrot_base.m[10],
                         out.m[8], out.m[9], out.m[10]);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// 1.688 NO-.text-HOOK DATA head-look. Retail FH5's integrity check CRCs .text and kills the process when it
// sees our inline (safetyhook) prologue patches; the present/device hooks + DATA writes are invisible to it
// (bisect-proven). So on 1.688 we install NO camera .text hooks; instead this runs from the WORKER thread:
//   (a) resolve the ForzaMultiCam hook-free via the vtable scan (k1688ForzaMultiCamRefcountIdaVa),
//   (b) read the active camera structurally (+0x5C8 active slot; validate by +0x320 orthonormality, NOT by
//       the per-build concrete vtables, which are unmapped on 1.688),
//   (c) compose the published VR head rotation onto the camera's +0x320 cam-to-world matrix (DATA write).
// Tradeoff: the worker write races the engine's per-frame +0x320 rebuild (may jitter) — the proven inline
// path can't be used on retail. rot=off disables. Empress is unaffected (IsRetail1688Build()==false).
// ---------------------------------------------------------------------------
namespace {
uintptr_t g_dc_multicam = 0;
uintptr_t g_dc_camera = 0;
uint64_t  g_dc_last_scan_ms = 0;
uint64_t  g_dc_invalid_since_ms = 0;
uintptr_t g_dc_base_obj = 0;
Matrix4   g_dc_base{};
Matrix4   g_dc_written{};

bool Cam320IsOrthonormal(uintptr_t cam, Matrix4& cur_out) {
    if (cam < 0x10000ull) return false;
    Matrix4 cur{};
    if (!SafeCopyIn(cam + kCameraMatrixOffset, cur.m.data(), sizeof(float) * cur.m.size())) return false;
    Pose decoded{};
    if (!DecodePose(cur, decoded)) return false;
    cur_out = cur;
    return true;
}

// Cheap re-validation of a cached camera object: the active-camera gate float at +0x31C ~ 1.0 AND an
// orthonormal +0x320 cam-to-world. A freed/reused object won't pass both, so this guards against staleness.
bool Cam1688Valid(uintptr_t cam) {
    if (cam < 0x10000ull) return false;
    float discr = 0.0f;
    if (!SafeRead(cam + 0x31C, discr) || std::fabs(discr - 1.0f) > 1.0e-2f) return false;
    Matrix4 m{};
    return Cam320IsOrthonormal(cam, m);
}

// Resolve the active gameplay camera only through the retail ForzaMultiCam owner. The previous loose
// camera-shaped heap scan was useful for RE, but it can lock the wrong transient object and burns hundreds of
// MB of reads after a miss. Retail production should fail closed and wait for a valid ForzaMultiCam refcount hit.
uint64_t g_dc_scan_count = 0;
uintptr_t ScanForCamera1688() {
    ++g_dc_scan_count;
    Pose hint{};
    SnapshotPoseHint(hint);

    uintptr_t multicam = 0;
    uintptr_t active = 0;
    if (ResolveMulticamByScan(hint, multicam, active)) {
        g_dc_multicam = multicam;
        spdlog::info("[FH5DATACAM] camera scan#{}: ForzaMultiCam active camera multicam=0x{:X} active=0x{:X}",
                     g_dc_scan_count, multicam, active);
        return active;
    }

    spdlog::warn("[FH5DATACAM] camera scan#{}: ForzaMultiCam active-camera resolve failed; no loose heap fallback on retail",
                 g_dc_scan_count);
    return 0;
}
} // namespace

void Tick1688DataHeadlook() {
    if (!IsRetail1688Build()) return;

    // (a) Lock the active camera, then cache it. STABILITY: the region scan memcpy's up to ~768MB; running it
    // repeatedly during FH5's fragile boot/load phase hung the game at ~0fps (only ~10 presents in 43 min). So:
    //   * never scan during the first 130s of mod uptime (the retail launcher needs ~100s to reach free-roam),
    //   * scan at most every 12s while uncached,
    //   * once locked, NEVER re-scan unless Cam1688Valid fails continuously for >30s (handles a real scene
    //     camera move without re-scanning on transient gate flicker).
    static uint64_t s_worker_start_ms = 0;
    const uint64_t now = NowMs();
    if (s_worker_start_ms == 0) s_worker_start_ms = now;

    if (g_dc_camera != 0) {
        if (Cam1688Valid(g_dc_camera)) { g_dc_invalid_since_ms = 0; }
        else {
            if (g_dc_invalid_since_ms == 0) g_dc_invalid_since_ms = now;
            if (now - g_dc_invalid_since_ms > 30000) g_dc_camera = 0;   // sustained loss -> allow re-scan
            else return;                                               // transient flicker -> skip this tick
        }
    }
    if (g_dc_camera == 0) {
        if (now - s_worker_start_ms < 130000) return;  // skip the fragile boot/load window entirely
        if (now - g_dc_last_scan_ms < 12000) return;
        g_dc_last_scan_ms = now;
        const uintptr_t found = ScanForCamera1688();
        if (found == 0) return;            // no active gameplay camera yet
        g_dc_camera = found;
        g_dc_invalid_since_ms = 0;
        spdlog::info("[FH5DATACAM] active camera LOCKED 0x{:X}", g_dc_camera);
    }
    const uintptr_t cam = g_dc_camera;
    const int rotmode = fh5cb::ctl_rotation_mode();
    const int requested_poslane = fh5cb::ctl_pos_lane();
    const int poslane = Retail1688EffectivePosLane();

    // (c) Retail no-.text-hook 6DOF. The +0x540 "YPR" lane proved visually positional on Steam, so the
    // shipping retail path writes the active camera's +0x320 matrix directly:
    //   * rows 0-2: head-rotated basis for look
    //   * row 3: physical/manual camera-space offset for position
    // It composes onto the engine-refreshed base every tick, so there is no accumulation.
    bool angle_applied = false, fl_applied = false, ypr_applied = false;
    bool matrix_applied = false, pos_applied = false;
    if (rotmode == 4) {
        fl_applied = apply_freelook_headlook_1688(cam);
    }

    float s = 0, u = 0, f = 0; Pose delta{}; int eye = 0;
    const bool pose_ok_now = SnapshotOpenXrPose(s, u, f, delta, eye);
    Matrix4 cur{};
    if (pose_ok_now && Cam320IsOrthonormal(cam, cur)) {
        if (g_dc_base_obj != cam) {
            g_dc_base_obj = cam;
            g_dc_base = cur;
        } else if (!MatricesClose16(cur, g_dc_written, 1e-3f)) {
            g_dc_base = cur;
        }

        Matrix4 out = g_dc_base;
        if (rotmode == 2 || rotmode == 3 || rotmode == 5) {
            Matrix4 rot{};
            rot.m[0]=delta.right.x;   rot.m[1]=delta.right.y;   rot.m[2]=delta.right.z;   rot.m[3]=0.0f;
            rot.m[4]=delta.up.x;      rot.m[5]=delta.up.y;      rot.m[6]=delta.up.z;      rot.m[7]=0.0f;
            rot.m[8]=delta.forward.x; rot.m[9]=delta.forward.y; rot.m[10]=delta.forward.z;rot.m[11]=0.0f;
            rot.m[12]=0.0f;           rot.m[13]=0.0f;           rot.m[14]=0.0f;           rot.m[15]=1.0f;
            out = MulRowVector(rot, g_dc_base);
            matrix_applied = true;
        }

        float lx = 0.0f, ly = 0.0f, lz = 0.0f;
        if (poslane == fh5cb::kPosLaneCamSrc && current_local_offset(lx, ly, lz)) {
            const float* b = out.m.data();
            out.m[12] = g_dc_base.m[12] + lx * b[0] + ly * b[4] + lz * b[8];
            out.m[13] = g_dc_base.m[13] + lx * b[1] + ly * b[5] + lz * b[9];
            out.m[14] = g_dc_base.m[14] + lx * b[2] + ly * b[6] + lz * b[10];
            pos_applied = true;
        }

        if ((matrix_applied || pos_applied) &&
            SafeCopyOut(cam + kCameraMatrixOffset, out.m.data(), sizeof(float) * out.m.size())) {
            g_dc_written = out;
            if (rotmode == 3) angle_applied = matrix_applied;
            if (rotmode == 5) ypr_applied = matrix_applied;
        }
    }

    {   // ~1/s heartbeat with GATE diagnostics (why apply_angle fails: rot mode / poslane / stale pose / non-angle +0x90)
        static uint64_t sl = 0; const uint64_t now = NowMs();
        if (now - sl >= 1000) { sl = now;
            float p_s=0,p_u=0,p_f=0; Pose p_d{}; int p_e=0;
            const bool poseOk = SnapshotOpenXrPose(p_s,p_u,p_f,p_d,p_e);
            float a90=0,a94=0,a98=0; SafeRead(cam+0x90,a90); SafeRead(cam+0x94,a94); SafeRead(cam+0x98,a98);
            float hy = 0.0f, hp = 0.0f, hr = 0.0f;
            if (poseOk) {
                float fy = p_d.forward.y;
                if (fy > 1.0f) fy = 1.0f;
                if (fy < -1.0f) fy = -1.0f;
                hy = std::atan2(p_d.forward.x, p_d.forward.z);
                hp = std::asin(fy);
                hr = std::atan2(p_d.right.y, p_d.up.y);
            }
            spdlog::info("[FH5DATACAM] cam=0x{:X} applied(ang/fl/ypr/mat/pos)={}/{}/{}/{}/{} | rotMode={} poslane={}=>{} poseOk={} head(y,p,r)=({:.3f},{:.3f},{:.3f}) row3=({:.3f},{:.3f},{:.3f}) | cam90/94/98=({:.3f},{:.3f},{:.3f})",
                         cam, angle_applied?1:0, fl_applied?1:0, ypr_applied?1:0,
                         matrix_applied?1:0, pos_applied?1:0,
                         rotmode, requested_poslane, poslane, poseOk ? 1 : 0,
                         hy, hp, hr,
                         g_dc_written.m[12], g_dc_written.m[13], g_dc_written.m[14],
                         a90, a94, a98);
            // Read-only dump of the native cockpit free-look + CameraSpaceYPR lever fields, so we can SEE which
            // are plausible angle fields on this retail camera object (small radians / a tag dword) vs garbage —
            // this picks the working lever without a rebuild (just flip rot= in the ctl file).
            uint32_t l540[4] = {0,0,0,0}; SafeCopyIn(cam + 0x540, l540, sizeof(l540));
            float f544=0,f548=0,f54c=0;
            std::memcpy(&f544,&l540[1],4); std::memcpy(&f548,&l540[2],4); std::memcpy(&f54c,&l540[3],4);
            float a5C4=0,a5F4=0,a5F8=0; SafeRead(cam+0x5C4,a5C4); SafeRead(cam+0x5F4,a5F4); SafeRead(cam+0x5F8,a5F8);
            spdlog::info("[FH5LEVER] cam=0x{:X} ypr540[tag=0x{:08X} f544={:.4f} f548={:.4f} f54c={:.4f}] freelook[5C4={:.4f} 5F4={:.4f} 5F8={:.4f}]",
                         cam, l540[0], f544, f548, f54c, a5C4, a5F4, a5F8);
        }
    }
}

// Runtime camera-orientation probe. Called post-fold on the active CCamDriver. Two jobs:
//  (1) dumpcam: scan the CCamDriver AND its nested view-source object (*(this+0x48)) for orthonormal 4x4s,
//      logging each offset + its basisScore vs the live producer a4. The offset whose basisScore≈3 is the
//      camera orientation the producer (and the shadow-cascade fit) consume — the lever we must rotate.
//  (2) pokerot/pokerotvs: rotate the orthonormal 4x4 at a control-file-chosen offset (on the cam, or on the
//      view-source) by the published head delta — so we can sweep candidate orientation sources LIVE and
//      watch whether the view + shadows follow, without rebuilding.
void probe_camera(uintptr_t object) {
    if (object < 0x10000ull) return;

    if (fh5cb::ctl_dumpcam()) {
        static uint64_t s_last_dump_ms = 0;
        const uint64_t now = NowMs();
        if (now - s_last_dump_ms >= 1000) {
            s_last_dump_ms = now;
            Pose hint{};
            const bool have_hint = SnapshotPoseHint(hint);
            uintptr_t vs = 0;
            SafeRead(object + 0x48, vs);
            spdlog::info("[FH5DUMP] object=0x{:X} vs=0x{:X} a4hint_ok={} a4.fwd=({:.3f},{:.3f},{:.3f})",
                         object, vs, have_hint ? 1 : 0, hint.forward.x, hint.forward.y, hint.forward.z);
            const auto scan = [&](uintptr_t base, const char* tag) {
                if (base < 0x10000ull) return;
                for (uintptr_t off = 0; off <= 0x720; off += 4) {
                    Matrix4 m{};
                    if (!SafeCopyIn(base + off, m.m.data(), sizeof(float) * m.m.size())) break;
                    Pose p{};
                    if (!DecodePose(m, p)) continue;
                    const float bs = have_hint ? BasisScore(p, hint) : 0.0f;
                    spdlog::info("[FH5DUMP]   {} +0x{:X} basisScore={:.3f} fwd=({:.3f},{:.3f},{:.3f}) pos=({:.2f},{:.2f},{:.2f})",
                                 tag, off, bs, p.forward.x, p.forward.y, p.forward.z,
                                 p.position.x, p.position.y, p.position.z);
                }
            };
            scan(object, "cam");
            scan(vs, "vs ");
        }
    }

    const int pr = fh5cb::ctl_pokerot();
    const int prvs = fh5cb::ctl_pokerotvs();
    if (pr < 0 && prvs < 0) return;

    float strafe = 0.0f, up = 0.0f, fwd = 0.0f;
    Pose delta{};
    int eye = 0;
    if (!SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) return;

    Matrix4 rot{};
    rot.m[0]  = delta.right.x;   rot.m[1]  = delta.right.y;   rot.m[2]  = delta.right.z;   rot.m[3]  = 0.0f;
    rot.m[4]  = delta.up.x;      rot.m[5]  = delta.up.y;      rot.m[6]  = delta.up.z;      rot.m[7]  = 0.0f;
    rot.m[8]  = delta.forward.x; rot.m[9]  = delta.forward.y; rot.m[10] = delta.forward.z; rot.m[11] = 0.0f;
    rot.m[12] = 0.0f;            rot.m[13] = 0.0f;            rot.m[14] = 0.0f;            rot.m[15] = 1.0f;

    const auto pokerot = [&](uintptr_t base, int off) {
        if (base < 0x10000ull || off < 0) return;
        Matrix4 m{};
        if (!SafeCopyIn(base + static_cast<uintptr_t>(off), m.m.data(), sizeof(float) * m.m.size())) return;
        Pose p{};
        if (!DecodePose(m, p)) return;   // only rotate a genuine orthonormal basis
        const Matrix4 out = MulRowVector(rot, m);
        SafeCopyOut(base + static_cast<uintptr_t>(off), out.m.data(), sizeof(float) * out.m.size());
    };
    if (pr >= 0) pokerot(object, pr);
    if (prvs >= 0) {
        uintptr_t vs = 0;
        if (SafeRead(object + 0x48, vs)) pokerot(vs, prvs);
    }
}

// Anti-accumulation cache for the +0x540 look-direction lane (the LOOK-AT camera's orientation input).
static uintptr_t g_aim_obj = 0;
static Vec4 g_aim_base{};
static Vec4 g_aim_written{};

// SHADOW-COHERENT head-look. FH5's gameplay camera is a LOOK-AT camera: orientation is derived each frame
// from the look-direction lane at CCamDriver+0x540 (aim = +0x530 eye + +0x540 lookdir; forward ∝ +0x540).
// The getter sub_1407A9DD0 rebuilds the basis from it EVERY frame (fires parked too, unlike the fold), and
// the shadow cascades derive from that same basis — so ROTATING the look direction here (before the getter
// reads it) makes the main view AND cascades follow head-look. Rotate the +0x540 vector by the head delta
// (anti-accumulation so a static look dir doesn't spin). Translating +0x540 was inert; rotating it steers.
bool rotate_aim_lookdir(uintptr_t object) {
    if (object < 0x10000ull) return false;
    if (fh5cb::ctl_pos_lane() != fh5cb::kPosLaneProducerA15) return false;

    float strafe = 0.0f, up = 0.0f, fwd = 0.0f;
    Pose delta{};
    int eye = 0;
    if (!SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) return false;

    Vec4 cur{};
    if (!SafeCopyIn(object + kInputOffsetLane, &cur, sizeof(cur))) return false;
    if (!std::isfinite(cur.x) || !std::isfinite(cur.y) || !std::isfinite(cur.z)) return false;

    Vec4 base{};
    if (g_aim_obj != object) {
        g_aim_obj = object;
        base = cur;
        g_aim_base = cur;
    } else if (std::fabs(cur.x - g_aim_written.x) < 1e-3f &&
               std::fabs(cur.y - g_aim_written.y) < 1e-3f &&
               std::fabs(cur.z - g_aim_written.z) < 1e-3f) {
        base = g_aim_base;   // engine didn't refresh the look dir -> reuse the clean base (no spin)
    } else {
        base = cur;
        g_aim_base = cur;    // engine wrote a fresh look dir this frame
    }

    // Rotate the look-direction vector by the head delta R (row-vector 3x3; rows = right/up/forward).
    // new = R * base. Convention verified live; flip R if the yaw sense is reversed.
    const Vec4 out{
        delta.right.x   * base.x + delta.right.y   * base.y + delta.right.z   * base.z,
        delta.up.x      * base.x + delta.up.y      * base.y + delta.up.z      * base.z,
        delta.forward.x * base.x + delta.forward.y * base.y + delta.forward.z * base.z,
        cur.w
    };
    if (!SafeCopyOut(object + kInputOffsetLane, &out, sizeof(out))) return false;
    g_aim_written = out;
    return true;
}

// Anti-accumulation cache for the CMultiCam world basis (S+0x650).
static uintptr_t g_mc_obj = 0;
static Matrix4 g_mc_base{};
static Matrix4 g_mc_written{};

// SHADOW-COHERENT head-look (the real lever, RE fh5_empress_viewsource_orientation_RE.md ROUND-2).
// The builder slot[0x68] ignores +0x540 — the world camera basis lives in the CMultiCam view-source
// S=*(CCamDriver+0x48): row0/right @S+0x650, row1/up @+0x660, row2/forward @+0x670, row3/pos @+0x680
// (row-vector, row-major), rebuilt each frame from the car. S+0x600 is the row handed to the producer a4.
// The shadow cascades fit from that same basis. Rotating the 3x3 by the head delta here (getter entry, after
// the rebuild, before a4 copy) turns the view AND the cascades. pos (row3) preserved (translation lever owns
// it). Anti-accumulated so a static basis doesn't spin.
bool rotate_multicam_basis(uintptr_t ccam) {
    // Bail diagnostic (~1/s): pinpoints WHERE this returns early so we can see if the getter fires and
    // which gate trips. Logs the S+0x650 basis vectors + S vtable so we can confirm the Lane-A source.
    static uint64_t s_dbg_ms = 0;
    const uint64_t dbg_now = NowMs();
    const bool dbg = (dbg_now - s_dbg_ms >= 1000);
    auto bail = [&](const char* why) -> bool {
        if (dbg) { s_dbg_ms = dbg_now; spdlog::info("[FH5MCDBG] bail={} ccam=0x{:X} poslane={}", why, ccam, fh5cb::ctl_pos_lane()); }
        return false;
    };

    if (ccam < 0x10000ull) return bail("ccam");
    if (fh5cb::ctl_pos_lane() != fh5cb::kPosLaneProducerA15) return bail("poslane");

    uintptr_t S = 0;
    if (!SafeRead(ccam + 0x48, S) || S < 0x10000ull) return bail("S_read");

    // GATE (mandatory, AV-safe): S must be a real object whose vtable lies in the FH5 module before we
    // dereference S+0x650. NVIDIA's overlay VEH turns ANY first-chance AV into a fatal stack overflow
    // BEFORE our SEH __except runs (see [[fh5-nvidia-veh-crash]]), so we must NOT fault — validate the
    // vtable pointer range first. The byte-verified RE (fh5_empress_upstream_pose_writer_RE.md) says the
    // active gameplay view-source S is a CMultiCam (vtable 0x145E1E558); accept any in-module vtable +
    // the orthonormal-basis check below as the offset-free discriminator.
    uintptr_t svt = 0;
    if (!SafeRead(S, svt) || svt < kIdaImageBase || svt >= 0x148000000ull) return bail("svt");

    // One-shot layout probe per distinct camera object: dump vtables + which candidate offsets hold a
    // genuine orthonormal basis, so we can see WHICH camera/offset feeds the producer a4.
    {
        static uintptr_t seen[12]{}; static int seen_n = 0; bool known = false;
        for (int i = 0; i < seen_n; ++i) if (seen[i] == ccam) { known = true; break; }
        if (!known && seen_n < 12) {
            seen[seen_n++] = ccam;
            auto orthoAt = [](uintptr_t addr) -> int {
                Matrix4 m{};
                if (!SafeCopyIn(addr, m.m.data(), sizeof(float) * m.m.size())) return -1;
                const Vec3 a{ m.m[0], m.m[1], m.m[2] }, b{ m.m[4], m.m[5], m.m[6] }, c{ m.m[8], m.m[9], m.m[10] };
                return LooksLikeRotationBasis(a, b, c) ? 1 : 0;
            };
            uintptr_t camvt = 0, svt = 0; SafeRead(ccam, camvt); SafeRead(S, svt);
            auto row0 = [](uintptr_t addr, float* out) { Matrix4 m{}; if (SafeCopyIn(addr, m.m.data(), sizeof(float)*m.m.size())) { out[0]=m.m[0]; out[1]=m.m[1]; out[2]=m.m[2]; } else { out[0]=out[1]=out[2]=0; } };
            float r320[3]{}, r550[3]{}; row0(ccam + 0x320, r320); row0(ccam + 0x550, r550);
            spdlog::info("[FH5MCPROBE] cam=0x{:X} camvt=0x{:X} ortho@550={} ortho@320={} | S=0x{:X} svt=0x{:X} ortho@S650={} ortho@S600={} | @320(0x{:X})=({:.3f},{:.3f},{:.3f}) @550(0x{:X})=({:.3f},{:.3f},{:.3f})",
                         ccam, camvt, orthoAt(ccam + 0x550), orthoAt(ccam + 0x320),
                         S, svt, orthoAt(S + 0x650), orthoAt(S + 0x600),
                         ccam + 0x320, r320[0], r320[1], r320[2], ccam + 0x550, r550[0], r550[1], r550[2]);
        }
    }

    float strafe = 0.0f, up = 0.0f, fwd = 0.0f;
    Pose delta{};
    int eye = 0;
    if (!SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) return bail("snapshot");

    Matrix4 M{};   // S+0x650: right(row0)/up(row1)/forward(row2)/pos(row3)
    if (!SafeCopyIn(S + 0x650, M.m.data(), sizeof(float) * M.m.size())) return bail("readbasis");
    const Vec3 r0{ M.m[0], M.m[1], M.m[2] };
    const Vec3 r1{ M.m[4], M.m[5], M.m[6] };
    const Vec3 r2{ M.m[8], M.m[9], M.m[10] };
    if (dbg) {
        uintptr_t svt = 0; SafeRead(S, svt);
        s_dbg_ms = dbg_now;
        spdlog::info("[FH5MCDBG] reached-basis ccam=0x{:X} S=0x{:X} Svt=0x{:X} r0=({:.3f},{:.3f},{:.3f}) r1=({:.3f},{:.3f},{:.3f}) r2=({:.3f},{:.3f},{:.3f})",
                     ccam, S, svt, r0.x, r0.y, r0.z, r1.x, r1.y, r1.z, r2.x, r2.y, r2.z);
    }
    if (!LooksLikeRotationBasis(r0, r1, r2)) return bail("notbasis");   // only touch a genuine orthonormal basis

    Matrix4 base{};
    if (g_mc_obj != S) {
        g_mc_obj = S;
        base = M;
        g_mc_base = M;
    } else if (MatricesClose16(M, g_mc_written, 1e-3f)) {
        base = g_mc_base;   // engine didn't rebuild this frame -> reuse clean base
    } else {
        base = M;
        g_mc_base = M;       // fresh car-derived basis
    }

    Matrix4 R{};
    R.m[0]  = delta.right.x;   R.m[1]  = delta.right.y;   R.m[2]  = delta.right.z;   R.m[3]  = 0.0f;
    R.m[4]  = delta.up.x;      R.m[5]  = delta.up.y;      R.m[6]  = delta.up.z;      R.m[7]  = 0.0f;
    R.m[8]  = delta.forward.x; R.m[9]  = delta.forward.y; R.m[10] = delta.forward.z; R.m[11] = 0.0f;
    R.m[12] = 0.0f;            R.m[13] = 0.0f;            R.m[14] = 0.0f;            R.m[15] = 1.0f;

    const Matrix4 Mp = MulRowVector(R, base);   // rotate basis rows 0-2; row3 (pos) preserved
    if (!SafeCopyOut(S + 0x650, Mp.m.data(), sizeof(float) * Mp.m.size())) return false;
    g_mc_written = Mp;

    // Re-do sub_14060B390's transpose-gather from the ROTATED basis so the per-frame getter's CMultiCam
    // builder (slot 0x68 returns *(S+0x600)) and the a4-assembler read the rotated rows -> producer a4 (and
    // the cascade fit, which consumes the same view basis) follow head-look. Engine formula
    // (fh5_empress_upstream_pose_writer_RE.md §3): S+0x600 = (M[2],M[6],M[10]); S+0x610 = (M[1],M[5],M[9])
    // where M = the S+0x650 row-major basis. These are basis COLUMNS (they rotate with the basis), so
    // recomputing from Mp keeps S+0x600/0x610 consistent with the rotated S+0x650. Preserve the w lanes.
    {
        Vec4 c600{}, c610{};
        SafeCopyIn(S + 0x600, &c600, sizeof(c600));
        SafeCopyIn(S + 0x610, &c610, sizeof(c610));
        const float g600[4]{ Mp.m[2], Mp.m[6], Mp.m[10], c600.w };
        const float g610[4]{ Mp.m[1], Mp.m[5], Mp.m[9],  c610.w };
        SafeCopyOut(S + 0x600, g600, sizeof(g600));
        SafeCopyOut(S + 0x610, g610, sizeof(g610));
    }

    {   // ~1/s verify log: forward row of S+0x650 base vs rotated; confirms the Lane-A source is being turned
        static uint64_t s_last = 0;
        const uint64_t now = NowMs();
        if (now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5MCROT] S=0x{:X} svt=0x{:X} base_fwd=({:.3f},{:.3f},{:.3f}) rot_fwd=({:.3f},{:.3f},{:.3f}) rotApplied=1",
                         S, svt, base.m[8], base.m[9], base.m[10], Mp.m[8], Mp.m[9], Mp.m[10]);
        }
    }
    return true;
}

// UPSTREAM head-look injection at the camera-state interpolator fh5_cam_LerpCameraStateStruct
// (RVA 0xC7F270). Per the user's dataflow RE: this lerp writes the camera's Euler angle triple at
// +0x90 (Y/yaw), +0x94 (X/pitch), +0x98 (Z/roll) every frame from the SOURCE state (a2); then
// sub_1407A1AC0 READS cam+0x90 to BUILD cam+0x320 (-> producer a4). So this is UPSTREAM of +0x320 and
// (if the cull/cascade fit snapshots after the lerp) upstream of the cull too — the single point that can
// make view + culling + shadows all follow head-look. We set cam+0x90/94/98 = source+0x90/94/98 + head
// Euler (fresh source as the base => no accumulation, no lerp damping/lag). a1/a2 were just dereferenced by
// the original lerp at these exact offsets, so reading/writing them is AV-safe.
bool apply_lerp_angle_head_rotation(uintptr_t cam, uintptr_t src) {
    static uint64_t s_dbg = 0;
    const uint64_t dnow = NowMs();
    const bool dbg = (dnow - s_dbg >= 1000);
    auto bail = [&](const char* why) -> bool {
        if (dbg) { s_dbg = dnow; spdlog::info("[FH5LERPDBG] bail={} cam=0x{:X} poslane={}", why, cam, fh5cb::ctl_pos_lane()); }
        return false;
    };
    if (cam < 0x10000ull || src < 0x10000ull) return bail("ptr");
    if (fh5cb::ctl_pos_lane() != fh5cb::kPosLaneProducerA15) return bail("poslane");

    float strafe = 0.0f, up = 0.0f, fwd = 0.0f;
    Pose delta{};
    int eye = 0;
    if (!SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) return bail("snapshot");

    // Head Euler from the published delta basis (right/up/forward). Identity basis -> 0,0,0.
    const Vec3 f = delta.forward, r = delta.right, u = delta.up;
    float fy = f.y; if (fy > 1.0f) fy = 1.0f; if (fy < -1.0f) fy = -1.0f;
    const float yaw   = std::atan2(f.x, f.z);   // around up   -> cam+0x90
    const float pitch = std::asin(fy);          // around right-> cam+0x94
    const float roll  = std::atan2(r.y, u.y);   // around fwd  -> cam+0x98
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(roll)) return bail("nan");
    if (std::fabs(yaw) < 1e-4f && std::fabs(pitch) < 1e-4f && std::fabs(roll) < 1e-4f) return bail("identity");

    float s90 = 0.0f, s94 = 0.0f, s98 = 0.0f;
    if (!SafeRead(src + 0x90, s90) || !SafeRead(src + 0x94, s94) || !SafeRead(src + 0x98, s98)) return bail("read");
    if (!std::isfinite(s90) || !std::isfinite(s94) || !std::isfinite(s98)) return bail("srcnan");
    if (std::fabs(s90) > 1000.0f || std::fabs(s94) > 1000.0f || std::fabs(s98) > 1000.0f) {
        if (dbg) { s_dbg = dnow; spdlog::info("[FH5LERPDBG] bail=range cam=0x{:X} s90={:.2f} s94={:.2f} s98={:.2f}", cam, s90, s94, s98); }
        return false;
    }

    const float o90 = s90 + yaw, o94 = s94 + pitch, o98 = s98 + roll;
    SafeCopyOut(cam + 0x90, &o90, sizeof(o90));
    SafeCopyOut(cam + 0x94, &o94, sizeof(o94));
    SafeCopyOut(cam + 0x98, &o98, sizeof(o98));
    {   // ~1/s
        static uint64_t s_last = 0;
        const uint64_t now = NowMs();
        if (now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5LERP] cam=0x{:X} base(90,94,98)=({:.3f},{:.3f},{:.3f}) head(yaw,pit,rol)=({:.3f},{:.3f},{:.3f})",
                         cam, s90, s94, s98, yaw, pitch, roll);
        }
    }
    return true;
}

bool apply_active_camdriver_head_rotation() {
    uintptr_t obj = g_driver_object.load(std::memory_order_acquire);
    if (obj < 0x10000ull) obj = g_shape_driver_object.load(std::memory_order_acquire);
    if (obj < 0x10000ull) return false;
    return apply_camdriver_head_rotation(obj);
}

// Anti-accumulation cache for the cam+0x320 ROW-3 position shift (poslane=camsrc). The engine rewrites the
// camera world position each frame (sub_1407A1AC0 builds cam+0x320 row3 from the f64 accumulators); we
// detect that refresh by comparing the live row3 to our last write, so we compose the head translation onto
// a CLEAN base (no drift on a parked camera, no double-apply on multi-call frames).
static uintptr_t g_camsrc_obj = 0;
static double    g_camsrc_base[3]    = { 0.0, 0.0, 0.0 };
static double    g_camsrc_written[3] = { 0.0, 0.0, 0.0 };

bool apply_camsrc_translation_postwrite(uintptr_t cam) {
    if (cam < 0x10000ull) return false;
    if (fh5cb::ctl_pos_lane() != fh5cb::kPosLaneCamSrc) return false;

    // Camera-local head translation (x=right, y=up, z=forward), summed manual + OpenXR head; false at rest.
    float lx = 0.0f, ly = 0.0f, lz = 0.0f;
    if (!current_local_offset(lx, ly, lz)) return false;

    // The cam-to-world 4x4 the original just wrote (rows 0-2 = head-rotated basis if rot=angle ran PRE-orig;
    // row 3 = world position). Project the local offset onto the basis rows -> world delta (row-vector:
    // worldvec = localvec * M_rot), identical to the producer a15/a16 shift so the view and the cascade move
    // by the SAME world vector.
    Matrix4 M{};
    if (!SafeCopyIn(cam + kCameraMatrixOffset, M.m.data(), sizeof(float) * M.m.size())) return false;
    const float* b = M.m.data();
    for (int i = 0; i < 12; ++i) { if (!std::isfinite(b[i])) return false; }
    const double wx = (double)(lx * b[0] + ly * b[4] + lz * b[8]);
    const double wy = (double)(lx * b[1] + ly * b[5] + lz * b[9]);
    const double wz = (double)(lx * b[2] + ly * b[6] + lz * b[10]);

    const double cur[3] = { (double)b[12], (double)b[13], (double)b[14] };
    if (!std::isfinite(cur[0]) || !std::isfinite(cur[1]) || !std::isfinite(cur[2])) return false;

    // Refresh detection -> clean base (no accumulation across re-entries within a frame).
    constexpr double kPosEps = 1e-3;
    const bool refreshed =
        (g_camsrc_obj != cam) ||
        std::fabs(cur[0] - g_camsrc_written[0]) > kPosEps ||
        std::fabs(cur[1] - g_camsrc_written[1]) > kPosEps ||
        std::fabs(cur[2] - g_camsrc_written[2]) > kPosEps;
    if (refreshed) {
        g_camsrc_obj = cam;
        g_camsrc_base[0] = cur[0]; g_camsrc_base[1] = cur[1]; g_camsrc_base[2] = cur[2];
    }

    const float o12 = (float)(g_camsrc_base[0] + wx);
    const float o13 = (float)(g_camsrc_base[1] + wy);
    const float o14 = (float)(g_camsrc_base[2] + wz);
    if (!SafeCopyOut(cam + kCameraMatrixOffset + 0x30, &o12, sizeof(o12))) return false;
    SafeCopyOut(cam + kCameraMatrixOffset + 0x34, &o13, sizeof(o13));
    SafeCopyOut(cam + kCameraMatrixOffset + 0x38, &o14, sizeof(o14));
    g_camsrc_written[0] = (double)o12; g_camsrc_written[1] = (double)o13; g_camsrc_written[2] = (double)o14;
    {   // ~1/s
        static uint64_t s_last = 0;
        const uint64_t now = NowMs();
        if (now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5CAMSRC] cam=0x{:X} local=({:.3f},{:.3f},{:.3f}) worldDelta=({:.3f},{:.3f},{:.3f}) base=({:.1f},{:.1f},{:.1f}) out=({:.1f},{:.1f},{:.1f})",
                         cam, lx, ly, lz, wx, wy, wz,
                         g_camsrc_base[0], g_camsrc_base[1], g_camsrc_base[2], o12, o13, o14);
        }
    }
    return true;
}

// Anti-accumulation cache for the +0x90 Euler-angle injection (rot=angle / mode 3). The engine rewrites the
// base orientation angles each frame; we detect that refresh by comparing the live angles to our last write,
// so we compose head-look onto a CLEAN base (no spin on a parked camera, no double-apply if sub_1407A1AC0
// fires more than once per frame for the same camera between engine refreshes).
static uintptr_t g_angrot_obj = 0;
static float     g_angrot_base[3]    = { 0.0f, 0.0f, 0.0f };
static float     g_angrot_written[3] = { 0.0f, 0.0f, 0.0f };

bool apply_angle_head_rotation_prewrite(uintptr_t cam) {
    if (cam < 0x10000ull) return false;
    if (fh5cb::ctl_rotation_mode() != 3) return false;                       // rot=angle only
    {   // proda15 (rotation only) or camsrc (rotation + cam+0x320-row3 translation)
        const int pl = fh5cb::ctl_pos_lane();
        if (pl != fh5cb::kPosLaneProducerA15 && pl != fh5cb::kPosLaneCamSrc) return false;
    }

    float strafe = 0.0f, up = 0.0f, fwd = 0.0f;
    Pose delta{};
    int eye = 0;
    if (!SnapshotOpenXrPose(strafe, up, fwd, delta, eye)) return false;      // no fresh VR head pose -> engine owns the angles

    // Head Euler from the published delta basis (rows = right/up/forward). Identity basis -> (0,0,0), so at
    // head-center this re-writes the engine base unchanged (idempotent). RADIANS, used raw (the rendered
    // camera's +0x90/94/98 are radians fed straight to the Rodrigues build in sub_1407A1AC0).
    const Vec3 f = delta.forward, r = delta.right, u = delta.up;
    float fy = f.y; if (fy > 1.0f) fy = 1.0f; if (fy < -1.0f) fy = -1.0f;
    const float yaw   = std::atan2(f.x, f.z);   // around up    -> cam+0x90
    const float pitch = std::asin(fy);          // around right -> cam+0x94
    const float roll  = std::atan2(r.y, u.y);   // around fwd   -> cam+0x98
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(roll)) return false;

    float c90 = 0.0f, c94 = 0.0f, c98 = 0.0f;
    if (!SafeRead(cam + 0x90, c90) || !SafeRead(cam + 0x94, c94) || !SafeRead(cam + 0x98, c98)) return false;
    if (!std::isfinite(c90) || !std::isfinite(c94) || !std::isfinite(c98)) return false;
    if (std::fabs(c90) > 1000.0f || std::fabs(c94) > 1000.0f || std::fabs(c98) > 1000.0f) return false;  // not the radian-angle camera

    // Refresh detection -> clean base (no accumulation across re-entries within a frame).
    constexpr float kAngEps = 1e-4f;
    const bool refreshed =
        (g_angrot_obj != cam) ||
        std::fabs(c90 - g_angrot_written[0]) > kAngEps ||
        std::fabs(c94 - g_angrot_written[1]) > kAngEps ||
        std::fabs(c98 - g_angrot_written[2]) > kAngEps;
    if (refreshed) {
        g_angrot_obj = cam;
        g_angrot_base[0] = c90; g_angrot_base[1] = c94; g_angrot_base[2] = c98;
    }   // else: live angles still equal our last write -> engine did not refresh -> reuse g_angrot_base

    const float o90 = g_angrot_base[0] + yaw;
    const float o94 = g_angrot_base[1] + pitch;
    const float o98 = g_angrot_base[2] + roll;
    if (!SafeCopyOut(cam + 0x90, &o90, sizeof(o90))) return false;
    SafeCopyOut(cam + 0x94, &o94, sizeof(o94));
    SafeCopyOut(cam + 0x98, &o98, sizeof(o98));
    g_angrot_written[0] = o90; g_angrot_written[1] = o94; g_angrot_written[2] = o98;
    {   // ~1/s: confirm the rendered-camera angles are being head-rotated upstream of the +0x320 build.
        static uint64_t s_last = 0;
        const uint64_t now = NowMs();
        if (now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5ANGLE] cam=0x{:X} base(90,94,98)=({:.4f},{:.4f},{:.4f}) head(yaw,pit,rol)=({:.4f},{:.4f},{:.4f}) out=({:.4f},{:.4f},{:.4f})",
                         cam, g_angrot_base[0], g_angrot_base[1], g_angrot_base[2], yaw, pitch, roll, o90, o94, o98);
        }
    }
    return true;
}

// Head Euler (RADIANS) from the published OpenXR head-delta basis (rows = right/up/forward). Identity basis ->
// (0,0,0), so head-center is idempotent. Shared by the retail free-look / YPR levers below. Returns false if
// there's no fresh VR head pose (engine then owns the angles) or the result isn't finite.
static bool head_euler_radians(float& yaw, float& pitch, float& roll) {
    float s = 0, u = 0, f = 0; Pose delta{}; int eye = 0;
    if (!SnapshotOpenXrPose(s, u, f, delta, eye)) return false;
    const Vec3 ff = delta.forward, rr = delta.right, uu = delta.up;
    float fy = ff.y; if (fy > 1.0f) fy = 1.0f; if (fy < -1.0f) fy = -1.0f;
    yaw   = std::atan2(ff.x, ff.z);   // around up
    pitch = std::asin(fy);            // around right
    roll  = std::atan2(rr.y, uu.y);   // around forward
    return std::isfinite(yaw) && std::isfinite(pitch) && std::isfinite(roll);
}

// RETAIL 1.688 head-look lever A: the engine's NATIVE cockpit free-look angles. +0x5C4 (yaw) and +0x5F4
// (pitch) are each clamped by the engine to ±0.785 rad (±45°) — confirming they're the look yaw/pitch; +0x5F8
// is the adjacent roll float (CamDriverVtableOffsetDll probe, Empress). The engine consumes these during its
// own per-frame camera build, so view + cull + cascades stay coherent (no late output-matrix clobber). We add
// the head delta on top of the engine's resting base, anti-accumulate (refresh-detect a clean base), restore
// on neutral, and per-field SKIP any field whose captured base isn't a plausible angle (never corrupt a
// pointer/handle). Written from the worker; if async timing is clobbered the heartbeat shows base==written and
// the view won't follow -> escalate to the cloned-vtable detour.
static uintptr_t g_fl1688_obj = 0;
static float     g_fl1688_base[3]    = { 0.0f, 0.0f, 0.0f };
static float     g_fl1688_written[3] = { 0.0f, 0.0f, 0.0f };
static bool      g_fl1688_have = false;
static bool      g_fl1688_skip[3] = { false, false, false };

bool apply_freelook_headlook_1688(uintptr_t cam) {
    if (cam < 0x10000ull) return false;
    if (fh5cb::ctl_rotation_mode() != 4) return false;

    float yaw = 0, pitch = 0, roll = 0;
    if (!head_euler_radians(yaw, pitch, roll)) return false;
    const float head[3] = { yaw, pitch, roll };
    const uintptr_t off[3] = { 0x5C4ull, 0x5F4ull, 0x5F8ull };
    constexpr float kMaxSaneAngle = 6.5f;

    float live[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; ++i) {
        if (!SafeRead(cam + off[i], live[i]) || !std::isfinite(live[i])) return false;
    }
    bool refreshed = (g_fl1688_obj != cam) || !g_fl1688_have;
    if (!refreshed) {
        for (int i = 0; i < 3; ++i)
            if (std::fabs(live[i] - g_fl1688_written[i]) > 1e-4f) { refreshed = true; break; }
    }
    if (refreshed) {
        g_fl1688_obj = cam; g_fl1688_have = true;
        for (int i = 0; i < 3; ++i) {
            g_fl1688_base[i] = live[i];
            g_fl1688_skip[i] = std::fabs(live[i]) > kMaxSaneAngle;   // not a float angle -> never write it
        }
    }

    const bool neutral = std::fabs(yaw) < 1e-5f && std::fabs(pitch) < 1e-5f && std::fabs(roll) < 1e-5f;
    bool wrote = false;
    for (int i = 0; i < 3; ++i) {
        if (g_fl1688_skip[i]) continue;
        const float v = neutral ? g_fl1688_base[i] : (g_fl1688_base[i] + head[i]);
        if (std::isfinite(v) && SafeCopyOut(cam + off[i], &v, sizeof(v))) { g_fl1688_written[i] = v; wrote = true; }
    }
    {   // ~1/s
        static uint64_t s_last = 0; const uint64_t now = NowMs();
        if (now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5FREELK] cam=0x{:X} base(5C4,5F4,5F8)=({:.4f},{:.4f},{:.4f}) skip=({},{},{}) head(y,p,r)=({:.4f},{:.4f},{:.4f}) wrote={}",
                         cam, g_fl1688_base[0], g_fl1688_base[1], g_fl1688_base[2],
                         g_fl1688_skip[0]?1:0, g_fl1688_skip[1]?1:0, g_fl1688_skip[2]?1:0,
                         yaw, pitch, roll, wrote?1:0);
        }
    }
    return wrote && !neutral;
}

// RETAIL 1.688 head-look lever B: the +0x540 CameraSpaceYPR input lane (tag dword + 3 floats). This mirrors the
// "head-tracking active" form the engine itself writes (sub_14084CC90 CameraSpaceYPRSet): tag=0 (explicitly
// set/active), +0x544=pitch, +0x548=yaw, +0x54C=roll. The engine consumes it during the camera build, so the
// view/cull/projection stay internally consistent. We capture the resting lane ONCE per object (the tuned
// baseline head offset) and add the head delta on top; restore the raw base on neutral.
static uintptr_t g_ypr540_obj = 0;
static uint32_t  g_ypr540_base_raw[4] = { 0, 0, 0, 0 };
static bool      g_ypr540_have = false;
static bool      g_ypr540_applied = false;
static float     ypr540_base_f(int i) { float v = 0.0f; std::memcpy(&v, &g_ypr540_base_raw[i], sizeof(v)); return v; }

bool apply_ypr540_headlook_1688(uintptr_t cam) {
    if (cam < 0x10000ull) return false;
    if (fh5cb::ctl_rotation_mode() != 5) return false;

    float yaw = 0, pitch = 0, roll = 0;
    if (!head_euler_radians(yaw, pitch, roll)) return false;

    if (g_ypr540_obj != cam || !g_ypr540_have) {
        uint32_t raw[4] = { 0, 0, 0, 0 };
        if (!SafeCopyIn(cam + 0x540, raw, sizeof(raw))) return false;
        std::memcpy(g_ypr540_base_raw, raw, sizeof(raw));
        g_ypr540_obj = cam; g_ypr540_have = true; g_ypr540_applied = false;
    }

    const bool neutral = std::fabs(yaw) < 1e-5f && std::fabs(pitch) < 1e-5f && std::fabs(roll) < 1e-5f;
    if (neutral) {
        if (g_ypr540_applied) {
            SafeCopyOut(cam + 0x540, g_ypr540_base_raw, sizeof(g_ypr540_base_raw));
            g_ypr540_applied = false;
        }
        return false;
    }

    const float f544 = ypr540_base_f(1) + pitch;
    const float f548 = ypr540_base_f(2) + yaw;
    const float f54c = ypr540_base_f(3) + roll;
    if (!std::isfinite(f544) || !std::isfinite(f548) || !std::isfinite(f54c)) return false;
    uint32_t lane[4];
    lane[0] = 0u;   // tag: 0 == explicitly set / active
    std::memcpy(&lane[1], &f544, sizeof(float));
    std::memcpy(&lane[2], &f548, sizeof(float));
    std::memcpy(&lane[3], &f54c, sizeof(float));
    const bool wrote = SafeCopyOut(cam + 0x540, lane, sizeof(lane));
    if (wrote) g_ypr540_applied = true;
    {   // ~1/s
        static uint64_t s_last = 0; const uint64_t now = NowMs();
        if (now - s_last >= 1000) {
            s_last = now;
            spdlog::info("[FH5YPR540] cam=0x{:X} baseTag=0x{:08X} base=({:.4f},{:.4f},{:.4f}) head(y,p,r)=({:.4f},{:.4f},{:.4f}) out=({:.4f},{:.4f},{:.4f}) wrote={}",
                         cam, g_ypr540_base_raw[0], ypr540_base_f(1), ypr540_base_f(2), ypr540_base_f(3),
                         yaw, pitch, roll, f544, f548, f54c, wrote?1:0);
        }
    }
    return wrote;
}

void on_input540_fold(uintptr_t object) {
    g_input540_fold_calls.fetch_add(1, std::memory_order_relaxed);
    if (object == 0 || fh5cb::ctl_pos_lane() != fh5cb::kPosLaneInput540) {
        return;
    }

    if (!GameplayCameraRecentForInput540()) {
        RestoreInput540AndForget(object);
        LogInput540GateIfNeeded();
        return;
    }

    const uintptr_t previous = g_driver_object.exchange(object, std::memory_order_acq_rel);
    g_shape_driver_object.store(0, std::memory_order_release);
    if (previous != object) {
        g_driver_publish_count.fetch_add(1, std::memory_order_relaxed);
    }

    Vec3 offset{};
    bool manual_active = false;
    bool vr_tracking_active = false;
    int eye = 0;
    if (!BuildInput540Offset(offset, manual_active, vr_tracking_active, eye)) {
        return;
    }

    InputLaneSnapshot before{};
    ReadInputLaneSnapshot(object, before);
    const bool wrote = WriteInput540Offset(object, offset);
    if (wrote) {
        g_input540_writes.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_input540_bad.fetch_add(1, std::memory_order_relaxed);
    }

    const uint64_t now_ms = NowMs();
    uint64_t last_ms = g_input540_last_log_ms.load(std::memory_order_relaxed);
    if (now_ms >= last_ms && now_ms - last_ms >= 1000 &&
        g_input540_last_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
        InputLaneSnapshot after{};
        ReadInputLaneSnapshot(object, after);
        uintptr_t vtable = 0;
        SafeRead(object, vtable);
        spdlog::info("[FH5IN540] object=0x{:X} vtable=0x{:X} wrote={} manual={} vr={} eye={} "
                     "off=({:.3f},{:.3f},{:.3f}) before540=({:.3f},{:.3f},{:.3f},{:.3f}) "
                     "after540=({:.3f},{:.3f},{:.3f},{:.3f}) f530=({:.3f},{:.3f},{:.3f},{:.3f}) "
                     "f550=({:.3f},{:.3f},{:.3f},{:.3f}) f570=({:.3f},{:.3f},{:.3f},{:.3f}) "
                     "calls={} writes={} bad={}",
                     object,
                     vtable,
                     wrote ? 1 : 0,
                     manual_active ? 1 : 0,
                     vr_tracking_active ? 1 : 0,
                     eye,
                     offset.x, offset.y, offset.z,
                     before.offset540.x, before.offset540.y, before.offset540.z, before.offset540.w,
                     after.offset540.x, after.offset540.y, after.offset540.z, after.offset540.w,
                     after.base530.x, after.base530.y, after.base530.z, after.base530.w,
                     after.derived550.x, after.derived550.y, after.derived550.z, after.derived550.w,
                     after.derived570.x, after.derived570.y, after.derived570.z, after.derived570.w,
                     g_input540_fold_calls.load(std::memory_order_relaxed),
                     g_input540_writes.load(std::memory_order_relaxed),
                     g_input540_bad.load(std::memory_order_relaxed));
    }
}

void publish_multicam(uintptr_t object) {
    if (object == 0) return;
    g_multicam_published_object.store(object, std::memory_order_release);
    g_multicam_publish_count.fetch_add(1, std::memory_order_relaxed);
}

void publish_pose_hint(const float* matrix16) {
    if (matrix16 == nullptr) return;
    Matrix4 matrix{};
    __try {
        std::memcpy(matrix.m.data(), matrix16, sizeof(float) * matrix.m.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    Pose pose{};
    if (!DecodePose(matrix, pose)) return;
    g_hint_gen.fetch_add(1, std::memory_order_acq_rel);
    g_pose_hint = pose;
    g_hint_gen.fetch_add(1, std::memory_order_release);
}

void publish_openxr_pose(float strafe,
                         float up,
                         float fwd,
                         const float* delta9_rowmajor,
                         int eye,
                         bool active) {
    Pose delta{};
    if (delta9_rowmajor != nullptr) {
        __try {
            delta.right = { delta9_rowmajor[0], delta9_rowmajor[1], delta9_rowmajor[2] };
            delta.up = { delta9_rowmajor[3], delta9_rowmajor[4], delta9_rowmajor[5] };
            delta.forward = { delta9_rowmajor[6], delta9_rowmajor[7], delta9_rowmajor[8] };
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            active = false;
        }
    }

    if (!std::isfinite(strafe) || !std::isfinite(up) || !std::isfinite(fwd)) {
        active = false;
        strafe = 0.0f;
        up = 0.0f;
        fwd = 0.0f;
    }
    if (!LooksLikeRotationBasis(delta.right, delta.up, delta.forward)) {
        active = false;
        delta = {};
    }
    Orthonormalize(delta);

    g_openxr_offset_gen.fetch_add(1, std::memory_order_acq_rel);
    g_openxr_last_publish_ms.store(NowMs(), std::memory_order_release);
    g_openxr_strafe = active ? strafe : 0.0f;
    g_openxr_up = active ? up : 0.0f;
    g_openxr_fwd = active ? fwd : 0.0f;
    g_openxr_delta_pose = active ? delta : Pose{};
    g_openxr_eye = eye;
    g_openxr_offset_active.store(active, std::memory_order_release);
    g_openxr_offset_gen.fetch_add(1, std::memory_order_release);

    if (active) {
        g_openxr_publish_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void publish_candidate_pointer(uintptr_t value, const float* matrix16) {
    if (value < 0x10000ull || matrix16 == nullptr) return;

    Matrix4 matrix{};
    __try {
        std::memcpy(matrix.m.data(), matrix16, sizeof(float) * matrix.m.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    Pose hint{};
    if (!DecodePose(matrix, hint)) return;

    // If a non-shape source already produced a camera object, keep the hot producer hook cheap.
    const uintptr_t current = g_driver_object.load(std::memory_order_acquire);
    if (current != 0 && g_shape_driver_object.load(std::memory_order_acquire) == 0) return;

    TryPublishPointerValue(value, hint, "producer_arg");
}

void active_camera_name(char* out, size_t cap) {
    if (out == nullptr || cap == 0) return;
    auto setout = [&](const char* s) { size_t i = 0; for (; s[i] && i + 1 < cap; ++i) out[i] = s[i]; out[i] = '\0'; };
    setout("unknown");

    // The active camera object the upstream writer resolved (stored on resolve, BEFORE the write gate, so it
    // reflects the true active camera class even when the writer skips writing a non-CCamDriver under VR).
    uintptr_t obj = g_driver_object.load(std::memory_order_acquire);
    if (obj == 0) {
        const uintptr_t mc = g_multicam_published_object.load(std::memory_order_acquire);
        if (mc != 0) ReadActiveDriverFromMulticam(mc, obj);
    }
    if (obj == 0) return;

    uintptr_t vtbl = 0;
    if (!SafeRead(obj, vtbl) || !EnsureModuleBase()) return;
    vtbl = EffectiveObjectVtable(obj, vtbl);

    struct VtableName { uintptr_t ida_va; const char* name; };
    static const VtableName kTbl[] = {
        { 0x145E3FFC0ull, "CCamFollowLow" },      { 0x145E40308ull, "CCamFollowHigh" },
        { 0x145E3F290ull, "CCamDriver" },         { 0x145E3EBF0ull, "CCamHood" },
        { 0x145E3E550ull, "CCamBumperHigh" },     { 0x145E40D78ull, "CCamFree" },
        { 0x145E41208ull, "CCamFreeTargetCar" },  { 0x145E415B8ull, "CCamFreeTrack" },
        { 0x145E40650ull, "CCamFollowExtended" },
        // Cinematic intro/scripted-drive camera (concrete vtable; its refcount table 0x145FA0440 =
        // kCinematicGameCameraRefcountIdaVa). Live diag: the post-launch "world3d far=5000" scene is this
        // cinematic, NOT player free-roam — so the navigator must SKIP it, not treat it as ready.
        { 0x145F9E840ull, "CinematicGameCamera" },
    };
    for (const VtableName& e : kTbl) {
        if (vtbl == g_module_base + (e.ida_va - kIdaImageBase)) { setout(e.name); return; }
    }
    setout("other");
}

} // namespace fh5cam
