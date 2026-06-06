#pragma once

#include <cstdint>
#include <cstddef>

// FH5 UPSTREAM camera-position writer (CCamDriver+0x320).
//
// WHY THIS EXISTS: the per-frame view producer (sub_140BB1EE0) is camera-relative, so TRANSLATING its
// camera-to-world argument does NOT move the rendered camera (geometry is submitted relative to the same
// position the view subtracts — it cancels; proven live, see Fh5CameraCbuffer.hpp). The old +0x320 writer
// remains as a diagnostic, but the current Empress RE says true translation input is CCamDriver+0x540:
// a camera-space additive offset consumed by sub_1407A6300 before the engine derives +0x550/+0x320.
//
// Mechanism (ported from the proven standalone freecam FH5CameraProbe/src/CamDriverMatrixFreecamDll.cpp,
// which moved the FH5 camera coherently): camera hooks publish either the live CCamDriver pointer directly
// or the live ForzaMultiCam object from the state bridge when the build has a matching AOB. On builds where
// those AOBs are absent, the worker can resolve the active camera object by matching +0x320 against the
// proven view/projection producer's live pose hint. A background worker retains the older +0x320/clone
// diagnostic lanes; the current Empress translation test writes +0x540 inside the camera-build hook.

namespace fh5cam {

// Idempotent. Launches the background worker thread once. Safe to call repeatedly;
// only the first call starts the thread.
void start();

// Called by the deterministic sub_1406BE3A0 hook. `object` is the concrete CCamDriver pointer whose
// +0x320 matrix is being published by the engine this frame.
void publish_driver(uintptr_t object);

// Called by the deterministic sub_140746BB0 bridge hook. `object` is the ForzaMultiCam object; the worker
// reads its active camera slot at +0x5C8/+0x5D0 and validates the concrete camera matrix before writing.
void publish_multicam(uintptr_t object);

// Called by the proven view/projection producer hook for the main camera. This gives the worker a live
// camera-basis hint so it can resolve the active +0x320 camera object by shape when build-specific
// CCamDriver/ForzaMultiCam hooks are unavailable.
void publish_pose_hint(const float* matrix16);

// Called by the OpenXR adapter once per eye/frame. The offset is in the same camera-local units as the
// manual upstream test: +strafe = camera right, +up = camera up, +fwd = camera forward. `delta9_rowmajor`
// is the relative head rotation in the same row-vector basis: rows are right/up/forward.
void publish_openxr_pose(float strafe,
                         float up,
                         float fwd,
                         const float* delta9_rowmajor,
                         int eye,
                         bool active);

// Called by the sub_1407A6300 hook at function entry. If poslane=input540 is active, writes the current
// manual/OpenXR camera-space translation into `object + 0x540` immediately before the engine consumes it.
void on_input540_fold(uintptr_t object);

// Camera-LOCAL translation offset (manual tgt=driver + published OpenXR head/IPD pose), summed, ungated by
// pos_lane. Axes: x=right, y=up, z=forward (engine world units). Used by the producer a15/a16 f64 cameraPos
// hook (poslane=proda15). Returns false when there is no active, finite, non-trivial offset.
bool current_local_offset(float& strafe, float& up, float& fwd);

// Called by the CCamDriver +0x320 publisher hook (sub_1406BE3A0) with the live camera object. When
// poslane=proda15 and a head rotation is published, rotates the object's +0x320 camera-to-world BASIS in
// place (compose-on-top, anti-accumulation) BEFORE the publisher copies it downstream — so the main view
// AND the shadow cascades (which read +0x320, not the producer a4) follow head-look. Gated to a known
// camera vtable with an orthonormal-basis sanity check. Returns true if it wrote the pose.
bool apply_camdriver_head_rotation(uintptr_t object);

// Runtime camera-orientation probe (dumpcam / pokerot / pokerotvs control-file knobs). Called post-fold on
// the active CCamDriver to locate + live-rotate the orientation source the shadow cascades consume.
void probe_camera(uintptr_t object);

// SHADOW-COHERENT head-look. Called at the ENTRY of the camera pose getter sub_1407A9DD0 (which rebuilds the
// look-at basis from CCamDriver+0x540 every frame, feeding both the producer a4 and the shadow-cascade fit).
// Rotates the +0x540 look-direction by the published head delta so the whole frustum (view + cascades) turns
// with the head. Gated to poslane=proda15; anti-accumulated. Returns true if it wrote the lane.
bool rotate_aim_lookdir(uintptr_t object);

// Called by the producer hook with pointer-looking arguments. If an argument is already the active camera
// object, a shared-pointer control block, a ForzaMultiCam object, or a wrapper containing one, this captures
// the concrete camera object without process-wide scanning.
void publish_candidate_pointer(uintptr_t value, const float* matrix16);

// Best-effort current active-camera class name for the menu navigator's world3d disambiguation:
//   "CCamFollowLow"/"CCamFollowHigh"/"CCamHood"/"CCamBumperHigh"/"CCamDriver" = free-roam DRIVING views,
//   "CCamFollowExtended" = garage/menu extended-follow, "CCamFree*" = photo/free, "CCamDriver" = cockpit.
// Reads the active camera object resolved by the upstream writer (populated while it is active, i.e. during
// 3D scenes); writes "unknown" if not yet resolved. Always null-terminates within `cap`.
void active_camera_name(char* out, size_t cap);

} // namespace fh5cam
