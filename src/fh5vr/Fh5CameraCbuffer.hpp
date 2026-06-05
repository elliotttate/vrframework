#pragma once

// FH5 downstream camera-constant-buffer stereo hook.
//
// WHY THIS EXISTS (the load-bearing discovery): FH5 renders camera-relative. The upstream view
// producer (sub_140BB1EE0) takes a4 = camera-to-world; ROTATING a4 rotates the rendered camera
// (culling-correct), but TRANSLATING a4.row3 (the world camera position) does NOT move the camera —
// it cancels, because world geometry is submitted relative to the same camera position the view
// subtracts. Proven live: dt=[0,0,300] produced zero on-screen movement; a 40-unit IPD produced zero
// stereo disparity. So per-eye translation (IPD) and 6-DOF head translation CANNOT be done upstream.
//
// The fix (see _agent_reports/fh5_stereo_cbuffer_spec.md): edit the already-derived 6912-byte camera
// constant buffer in place, AFTER camera-relative rebasing, applying a VIEW-SPACE lateral offset to the
// camera-relative view-projection (camRelVP@0x100). Because the geometry is already baked relative to the
// ORIGINAL camera, pre-translating the eye in view space re-projects that fixed geometry from a laterally
// offset viewpoint -> genuine depth-correct horizontal parallax that does NOT cancel.
//
// Mechanism (ported from the proven freecam FH5CameraProbe/src/DxgiProxy.cpp): hook the game device's
// CreateCommittedResource / CreatePlacedResource (to map upload rings) and CreateConstantBufferView (the
// per-frame trigger); when a 6912B CBV resolves to a slot whose content matches the main camera
// (near~0.1@0xA0, far~50000@0xA4, posW~1@0x8C, valid VIEW@0x000), transform VIEW/VP/camRelVP in place.

#include <windows.h>
#include <d3d12.h>

namespace fh5cb {

// Hook D3D12CreateDevice (call from dllmain once d3d12.dll is resident, BEFORE the framework/game create
// their devices) so the buffer-tracking hooks install at device creation and catch the camera ring's
// allocation. Without this the camera upload ring is created before any later install and is never tracked.
void install_createdevice_hook(HMODULE d3d12);

// Fallback installer for the device buffer-tracking + CBV-transform hooks (the PRIMARY install is the
// D3D12CreateDevice hook from install_createdevice_hook). Idempotent. The per-eye transform happens in the
// CreateConstantBufferView hook — no command-queue hook (a full ring scan at ExecuteCommandLists crashes).
void ensure_installed(ID3D12Device* device);

// Publish the per-eye camera offset (FH5 world units) the cbuffer hook applies, expressed in the camera's
// own VIEW-SPACE axes: x = right, y = up, z = forward. This is the per-eye IPD (lateral) plus optional
// 6-DOF head translation. `active` gates whether we transform at all (false = pass cbuffers through).
void set_eye_offset(float view_x, float view_y, float view_z, bool active);

// Diagnostics for the FH5VR.log heartbeat.
unsigned long long ring_writes();
unsigned long long buffers_tracked();
unsigned long long cam_hits();
unsigned long long cbv6912_count();

// Live-tuning values from the control file (E:\tmp\fh5vr_ctl.txt): half-IPD in FH5 units and the
// head-translation units-per-metre. apply_stereo reads these so IPD/scale can be swept without a rebuild.
float ctl_half_ipd();
float ctl_world_scale();

} // namespace fh5cb
