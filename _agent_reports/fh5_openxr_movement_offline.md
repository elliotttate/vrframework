# FH5 OpenXR movement / 6DOF issue - offline report

Scope: offline review only. FH5 was not launched or attached. No implementation files were edited.

User symptom:

- Moving head up tilts the screen left.
- Moving head down tilts the screen right.
- Forward/back/left/right translation does not work in OpenXR 6DOF.

## Ranked findings

### 1. `StereoView::view` is documented as world-to-view, but the core fills it with pose transforms

Likelihood: high.

Evidence:

- `E:\Github\vrframework\include\spi\StereoView.hpp:15` documents `view[2]` as `per-eye world->view`.
- `E:\Github\vrframework\src\mods\VR.cpp:278-293` builds `sv.view[eye] = caps.engine_to_vr_basis * m_rotation_offset * hmd * eye_to_head * caps.vr_to_engine_basis`.
- `hmd` comes from `VR::get_transform(0)` at `E:\Github\vrframework\src\mods\VR.cpp:194-204`, which constructs a matrix from `view_space_location.pose.orientation` and writes `pose.position` into `m[3]`.
- `eye_to_head` comes from `OpenXR::update_matrices()`, where `views[i].pose` is converted into a transform at `E:\Github\vrframework\src\vr\runtimes\OpenXR.cpp:274-278`.

OpenXR semantics:

- `xrLocateSpace(view_space, stage_space, ...)` gives the HMD/view pose in stage/local space.
- `xrLocateViews(... space = view_space ...)` gives per-eye poses relative to the view/head space.
- Those are pose transforms, not already inverted world-to-view camera matrices.

Why this fits the symptom:

If a pose transform is labeled as `world->view` and then consumed as if it were a view matrix, the adapter is likely to invert or compose it in the wrong space. Axis movement can show up as roll/pitch bleed, and translation can cancel or be applied in an unintended basis.

Smallest code probe:

Before publishing stereo, log the matrices in `VR::push_stereo_to_adapter()`:

```cpp
spdlog::info("[VRPOSE] hmd pos=[{:.3f} {:.3f} {:.3f}] view{} pos=[{:.3f} {:.3f} {:.3f}]",
    hmd[3].x, hmd[3].y, hmd[3].z, eye,
    sv.view[eye][3].x, sv.view[eye][3].y, sv.view[eye][3].z);
```

Then move the headset up/down/forward/right. If those positions change correctly but FH5 `delta[12..14]` does not, the bug is after `push_stereo_to_adapter()`.

### 2. FH5 adapter inverts `view.view[eye]` before deriving `delta`

Likelihood: high.

Evidence:

- `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:228-230` says `view.view[eye]` is `world->view`, then computes `const glm::mat4 G = glm::affineInverse(view.view[eye]);`.
- But as above, the core currently fills `view.view[eye]` from OpenXR pose transforms. If `view.view[eye]` is actually `local->eye/camera` pose, this inverse flips the direction and can convert expected translations/rotations into wrong signed axes.
- `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:235-244` recenters `G` and then scales/sets `Grel[3]`.
- `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:246-248` copies raw GLM floats into the FH5 row-vector delta.
- The producer hook logs `dbg.delta[12..14]` at `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:143-147` and applies `new_a4 = Mul(delta, a4)` at `:160-167`.

Smallest confirmation:

Temporarily compare both candidates in the log, without changing behavior:

```cpp
const glm::mat4 pose = view.view[eye];
const glm::mat4 inv  = glm::affineInverse(view.view[eye]);
spdlog::info("[FH5POSE] eye={} poseT=[{:.3f} {:.3f} {:.3f}] invT=[{:.3f} {:.3f} {:.3f}]",
    eye, pose[3].x, pose[3].y, pose[3].z, inv[3].x, inv[3].y, inv[3].z);
```

If moving head forward makes `pose[3].z` change in the expected direction and `delta[12..14]` does not, remove the inverse or rename/rebuild `StereoView::view` as a true view matrix.

Most likely fix direction:

Make the seam explicit:

- Either change `StereoView::view` to carry pose/camera-to-world transforms and have FH5 use it directly.
- Or keep `StereoView::view` as world-to-view and change `VR::push_stereo_to_adapter()` to actually store `glm::affineInverse(pose)` there.

Do not leave the core and adapter disagreeing about whether `view[eye]` is pose or view.

### 3. Translation is produced, but adapter ignores `hmd_transform` and recomputes from the ambiguous per-eye view

Likelihood: medium-high.

Evidence:

- `E:\Github\vrframework\src\mods\VR.cpp:278-279` sets `sv.hmd_transform = hmd`.
- `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:228-230` does not use `view.hmd_transform`; it derives `G` only from `view.view[eye]`.

Why this matters:

If eye pose or basis composition is wrong, the raw HMD transform could still be correct. Since the user reports head translation not working, the fastest discriminator is to log/use `hmd_transform[3]` separately from `view.view[eye][3]`.

Smallest confirmation:

Log in `Fh5Adapter::apply_stereo()`:

```cpp
spdlog::info("[FH5HMD] hmdT=[{:.3f} {:.3f} {:.3f}] viewT=[{:.3f} {:.3f} {:.3f}] deltaT=[{:.3f} {:.3f} {:.3f}]",
    view.hmd_transform[3].x, view.hmd_transform[3].y, view.hmd_transform[3].z,
    view.view[eye][3].x, view.view[eye][3].y, view.view[eye][3].z,
    s.delta[12], s.delta[13], s.delta[14]);
```

### 4. GLM quaternion storage is probably not the primary bug

Likelihood: low as primary cause.

Evidence:

- `E:\Github\vrframework\CMakeLists.txt:145-155` makes `GLM_FORCE_QUAT_DATA_XYZW` public on `vrframework`.
- `E:\Github\vrframework\fh5vr\CMakeLists.txt:23-29` links `FH5VR` against `vrframework`, so the public compile definition propagates to the adapter target.
- `OpenXR::update_matrices()` explicitly documents the reinterpret-cast assumption at `E:\Github\vrframework\src\vr\runtimes\OpenXR.cpp:274-278`.

This still deserves a diagnostic log, but it is less likely than the pose/view inversion mismatch.

## Best next action

Add a diagnostic-only log for one build:

```text
OpenXR view_space_location pose position/orientation
VR::get_transform(0) translation
sv.view[eye] translation
affineInverse(sv.view[eye]) translation
published delta[12..14]
producer-applied delta[12..14]
```

Move the headset in one axis at a time:

- up/down
- forward/back
- left/right
- yaw/pitch/roll separately

Expected result if the current hypothesis is right:

- Raw OpenXR/HMD translation changes correctly.
- Published `delta` changes on the wrong axis/sign, or rotation rows change when pure translation was expected.

The first implementation fix to try is to make `StereoView::view` semantics consistent: either pass pose/camera-to-world and stop inverting in `Fh5Adapter::apply_stereo()`, or pass a true world-to-view matrix from `VR::push_stereo_to_adapter()` and keep the adapter inverse.
