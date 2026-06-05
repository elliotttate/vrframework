#pragma once

// FH5 UPSTREAM camera-position writer (CCamDriver+0x320).
//
// WHY THIS EXISTS: the per-frame view producer (sub_140BB1EE0) is camera-relative, so TRANSLATING its
// camera-to-world argument does NOT move the rendered camera (geometry is submitted relative to the same
// position the view subtracts — it cancels; proven live, see Fh5CameraCbuffer.hpp). To move the camera
// UPSTREAM so shadows/culling/chevrons follow, we must write the ENGINE's OWN camera matrix one level
// higher: the active CCamDriver's camera-to-world pose at +0x320 (row-major: right=m[0..2], up=m[4..6],
// forward=m[8..10], position=m[12..14]) plus its inverse view-tail at +0x3E0.
//
// Mechanism (ported from the proven standalone freecam FH5CameraProbe/src/CamDriverMatrixFreecamDll.cpp,
// which moved the FH5 camera coherently): a background worker resolves the ForzaMultiCam singleton ONCE by
// scanning process memory for its vtable, then ~every 4 ms reads the active CCamDriver, applies the
// camera-relative offset published via the control file (fh5cb::ctl_up_* with tgt=driver), and writes the
// modified +0x320 pose + recomputed +0x3E0 view-tail back. The offset is applied to the engine's FRESH
// per-frame pose (no accumulation): the engine recomputes the base each frame as the car moves.

namespace fh5cam {

// Idempotent. Launches the background worker thread once (resolve-then-poll). Safe to call repeatedly;
// only the first call starts the thread. No input handling — the offset comes solely from the control
// file via fh5cb::ctl_up_* and is applied ONLY when fh5cb::ctl_up_tgt() == 5 ("driver").
void start();

} // namespace fh5cam
