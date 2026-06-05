# FH5 Synced Eye Rendering Deep Dive

Date: 2026-06-05

Scope: offline/read-only investigation. No FH5 launch, no RenderDoc/IDA live work, no file edits outside this report.

## Executive conclusion

FH5/vrframework is currently AFR, not synchronized sequential stereo. It opens one OpenXR frame across a left/right pair, but the two eye images are still produced by two consecutive FH5 engine presents. That means the OpenXR submit cadence is mostly workable, but the source images are not guaranteed to share the same simulation state.

The shortest credible path is:

1. Fix the current AFR path so each eye is internally coherent: upstream pose/projection, correct copy-eye cadence, per-eye temporal history or TAA disable.
2. Add a paired-AFR camera-state hold: snapshot the upstream camera base on left, reuse that same base for right, and apply only the opposite eye offset/projection.
3. In parallel, search for a ForzaTech render-entry/viewport-draw equivalent. If found, that is the only path to true UEVRJ-style synchronized sequential: render eye A, force a second world draw for eye B before the next world tick, then submit both.

I do not see evidence that split-screen, command-list replay, or present-time cbuffer/backbuffer manipulation can solve synced eye rendering by themselves.

## Agent results

Three FH5-side agents converged on the same answer:

- Native split-screen/two-main-camera support is not proven. FH5 has multi-camera vocabulary and many camera-dependent render passes, but `ForzaMultiCam` appears to select/aggregate one active camera rather than schedule two main gameplay views.
- Present/backbuffer/CBV-level work is too late for true sync. It can transport an image and prove stereo math, but it cannot make culling, shadows, chevrons, motion vectors, or TAA derive from the correct eye.
- Command-list replay is a poor production route. There is no safe replayable closed-command-list boundary, and previous ECL/upload-ring scanning was already unstable.

The UEVRJ comparison was also checked directly from source while a dedicated UEVRJ subagent was running. The useful part of UEVRJ is not the OpenXR copy code alone; it is that Unreal exposes named engine-level render seams that UEVR can call.

## Current FH5 cadence

FH5 declares AFR:

- `EngineCaps::Submission` already has `SEQUENTIAL` and `AFR` modes at `E:\Github\vrframework\include\spi\EngineCaps.hpp:13`.
- `Fh5Adapter::capabilities()` sets `caps.submission = EngineCaps::Submission::AFR` at `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:261`.

The core drives one engine present per eye:

- `VR::on_present()` opens the OpenXR frame on the left-eye present, updates poses/matrices, calls `begin_frame()`, then copies the finished engine backbuffer at `E:\Github\vrframework\src\mods\VR.cpp:129`.
- `VR::on_post_present()` increments `m_render_frame_count`, latches the next eye, and pushes the next per-eye stereo state to the FH5 adapter at `E:\Github\vrframework\src\mods\VR.cpp:146`.
- `D3D12Component::on_frame()` chooses the eye from `vr->get_current_render_eye()`, copies the current backbuffer, and only calls `end_frame()` on the right-eye frame at `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:153`.

This is correct AFR submission behavior, but it is not synchronized sequential rendering. The world can advance between the left and right source images.

## Current FH5 stereo split

The current adapter still has a split upstream/downstream implementation:

- `Hook_Producer` applies latched stereo to the main camera producer, but it explicitly does not drive parity because the producer fires many times per frame (`E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:162`).
- `apply_stereo()` builds shared head rotation but intentionally keeps translation/IPD downstream: comments at `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:367` say head translation and IPD are applied in `Fh5CameraCbuffer`.
- `s.write_proj` is still disabled at `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:433`.
- `Fh5CameraCbuffer::TransformStereo()` patches `VIEW`, `VP`, and `camRelVP` in the 6912-byte camera cbuffer at `E:\Github\vrframework\src\fh5vr\Fh5CameraCbuffer.cpp:140`.

That is why the image can show stereo parallax while derived systems remain wrong. The CBV path modifies final shader inputs after the engine has already derived culling, shadow views, world-to-screen chevrons, motion vectors, and TAA history.

There is now also a `CCamDriver` upstream test hook:

- `Hook_CamDriver` targets `sub_1406BE3A0` / `CCamDriver+0x320` at `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:277`.
- It only applies when the control file target is `tgt=5` (`fh5cb::ctl_up_tgt() == 5`) at `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:294`.

That hook is the right kind of surface for paired-AFR and eventual upstream pose. It is not yet wired as the default stereo pose path.

## UEVRJ synchronized sequential mechanics

UEVRJ documents three modes:

- Native Stereo is fully synchronized and uses Unreal's real stereo rendering pipeline.
- Synchronized Sequential renders two frames sequentially on the same engine tick, with no world-time advance between them.
- AFR renders separate eyes on separate advancing frames and is explicitly not synchronized.

Source evidence:

- Mode names are declared at `E:\Github\UEVRJ\src\mods\VR.hpp:1164`.
- README describes synchronized sequential as same engine tick/no world-time advance at `E:\Github\UEVRJ\README.md:77`.
- `is_using_afr()` includes both alternating and synchronized modes, while `is_using_synchronized_afr()` distinguishes the synchronized version at `E:\Github\UEVRJ\src\mods\VR.hpp:484`.

The core synchronized trick is engine-level:

- UEVRJ hooks `UGameViewportClient::Draw`, discovers/hooks `FViewport::Draw`, and finds/uses `UGameEngine::Tick` context at `E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:16280`.
- After the normal viewport draw, if synchronized AFR is active, it enqueues a second `FViewport::Draw(viewport, true)` before the next world tick at `E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:16351`.
- The comment states the model directly: force a world draw at the start of the next engine tick before the world ticks again, so both views are drawn in sync with no world-time advance (`E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:16403`).
- It then chooses `Skip Tick` or `Skip Draw` to prevent the extra draw from being followed by another normal update/draw pair (`E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:16423`).
- The actual skip points are the engine tick hook (`E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:6103`) and the viewport draw hook (`E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:15898`). This is important for FH5: UEVRJ is not freezing D3D12; it is suppressing an Unreal update/draw at engine level.

UEVRJ can do this because Unreal exposes stereo hooks:

- It locates/hooks `GetDesiredNumberOfViews` and `GetViewPassForIndex` at `E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:15138`.
- It maps view indices to `eSSP_PRIMARY` / `eSSP_SECONDARY` at `E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:20017`.
- It applies per-eye pose in `CalculateStereoViewOffset` at `E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:19264`.
- It applies per-eye projection in `CalculateStereoProjectionMatrix` at `E:\Github\UEVRJ\src\mods\vr\FFakeStereoRenderingHook.cpp:19651`.
- In synchronized sequential, `CalculateStereoViewOffset` and `CalculateStereoProjectionMatrix` choose the eye by frame parity (`g_frame_count % 2`), not by Unreal's native two-view index alone. That is the same conceptual phase source FH5 currently uses, but UEVRJ pairs it with the second engine draw before time advances.

UEVRJ's OpenXR path is then just transport/submission:

- `OpenXR::synchronize_frame()` calls `xrWaitFrame`, stores one predicted frame state, and marks `frame_synced` at `E:\Github\UEVRJ\src\mods\vr\runtimes\OpenXR.cpp:920`.
- `D3D12Component` copies left/right source images into AFR/per-eye or native stereo array swapchains at `E:\Github\UEVRJ\src\mods\vr\D3D12Component.cpp:3224` and `E:\Github\UEVRJ\src\mods\vr\D3D12Component.cpp:3369`.
- `OpenXR::end_frame()` submits both projection views in one `xrEndFrame` at `E:\Github\UEVRJ\src\mods\vr\runtimes\OpenXR.cpp:2991`.
- The D3D12 submitter explicitly classifies `is_same_frame`, `is_afr`, `is_left_eye_frame`, and `is_right_eye_frame` before choosing copy behavior (`E:\Github\UEVRJ\src\mods\vr\D3D12Component.cpp:2564`). That distinction matters for FH5 because AFR, synchronized sequential, double-wide, and native-array sources are different transport shapes.
- UEVRJ seeds the AFR pair with the same OpenXR frame state in `synchronize_frame()` (`E:\Github\UEVRJ\src\mods\vr\runtimes\OpenXR.cpp:1068`) and its `VR::update_hmd_state()` path reuses/updates pose state for AFR/synchronized frames (`E:\Github\UEVRJ\src\mods\VR.cpp:5537`). That is the runtime side of keeping the eye pair tied to one predicted display time.

Native stereo is separate from synchronized sequential:

- UEVRJ disables the native-stereo fix while AFR/sequential is active (`E:\Github\UEVRJ\src\mods\VR.hpp:624`).
- Native stereo submits a same-frame double-wide or native array source, splitting left/right boxes or array slices (`E:\Github\UEVRJ\src\mods\vr\D3D12Component.cpp:3370`).
- Synchronized sequential still behaves like AFR at the source/copy layer, but its second source image is generated by an extra engine world draw before simulation advances.

Portable lesson: FH5 needs the ForzaTech equivalent of Unreal's viewport draw / world tick split. Copying UEVRJ's D3D12 submit layer will not create a second eye. UEVRJ succeeds because it asks the engine to render the world a second time before time advances.

## Split-screen / multi-camera possibility

The offline evidence says "possible residue, not proven path."

Evidence found by the FH5 split-screen agent:

- `splitscreenlaunch` and `splitscreen` strings exist in decompile/string artifacts.
- Camera file data contains `FollowCam\SplitScreen` and `SplitscreenCamCockpitOffsetX/Y/Z`.
- FH5 has multiple camera types and many auxiliary camera-like passes: shadows, reflections, cubemaps, UI.

Evidence against it as a drop-in solution:

- `ForzaMultiCam` resolves one active camera through active-camera slots, not a left/right pair.
- Known `ForzaMultiCam+0x650..0x680` state is aggregate/readback/bridge state, not a scheduler for simultaneous views.
- No current artifact exposes a retail FH5 scheduler that renders two main gameplay viewports in one tick.

Actionable discriminator:

1. Search specifically for a render viewport loop or mode switch that consumes the `splitscreen` mode value.
2. If it only loads camera offsets or dormant launch flags, drop split-screen as a VR architecture.
3. If it schedules two main viewports/RTs, map where each viewport's camera object and output RT are selected.

Until step 3 is proven, split-screen should not be treated as a planned implementation path.

## Freeze-between-eyes possibility

This is the closest conceptual match to UEVRJ synchronized sequential, but it needs an FH5 render/update split.

The desired sequence:

1. World tick/update runs once.
2. Render left eye from that world state.
3. Prevent world tick/update from advancing.
4. Render right eye from the same world state with only eye pose/projection changed.
5. Submit both OpenXR eyes.
6. Resume normal updates.

Potential forms:

- Skip draw: force an extra right-eye draw, then skip the next normal draw so there is only one extra render.
- Skip tick: allow a second render but suppress the next world tick/update.
- Paired-AFR hold: do not actually freeze the whole engine, but hold the camera base and pose across the left/right pair.

Risks:

- Pausing the wrong thing may stop render and present too.
- Freezing only camera state does not freeze crowds, traffic, particles, animation, physics, route chevrons, TAA jitter/history, or UI timing.
- FH5 may tie update and render tightly enough that there is no safe standalone world-draw call.

Offline next probe:

- Find a function pair analogous to `UGameEngine::Tick` and `FViewport::Draw`: one updates world time, the other builds/submits the render graph.
- Use existing Present/ECL/producers logs to determine whether the camera producer fires before or after world update.
- Add a no-op duplicate-draw probe only if a callable render entry is identified. Do not attempt this from closed D3D12 command lists.

## Command-list replay possibility

Not recommended.

Why:

- A closed D3D12 command list is already baked with descriptor heaps, root descriptors/tables, barriers, render targets, resource states, and engine-owned allocator lifetime.
- Replaying it with changed camera constants would duplicate a mono-culled draw list, not produce a correctly derived eye.
- Rerouting output to the other eye would require controlling RTV/DSV targets and resource state transitions for the whole engine pass.
- Current vrframework D3D12 command contexts are for copy/transport, not intercepting FH5 scene command lists.
- Existing FH5 cbuffer code notes that ECL upload-ring scanning was tried and removed after crashes/stale mapped upload buffer issues.

Use command-list/backbuffer work only for:

- OpenXR image transport.
- HUD/quad extraction and composition.
- Diagnostics, screenshots, and resource naming.
- Mono fallback/mirroring tests.

## Ranked plan

### 1. Prove and harden current AFR cadence

Goal: eliminate accidental eye mismatch before deeper RE.

Work:

- Log producer-applied eye, cbuffer-patched eye, copied eye, present index, backbuffer index, and OpenXR swapchain index in one compact line.
- Verify every pair is `L producer -> L copy`, then `R producer -> R copy`, then one `xrEndFrame`.
- Add a visible frame/eye watermark or cbuffer marker for one run if logs are ambiguous.

Acceptance:

- No pair submits left image to right swapchain or vice versa.
- No duplicate same-eye pair during gameplay/menu transitions.

### 2. Promote `CCamDriver` upstream pose to the default path

Goal: stop relying on downstream 6912 VP/camRelVP edits for head translation and IPD.

Work:

- Use `CCamDriver+0x320` as the camera-to-world base.
- Apply full per-eye camera pose there: shared head rotation, shared head translation, symmetric IPD.
- Recompute the inverse/view-tail block that downstream consumers use.
- Keep the 6912 cbuffer hook as diagnostic/fallback only.

Acceptance:

- Large controlled offsets at `CCamDriver+0x320` move the world and shadow together.
- Car geometry, car shadow, route chevrons, and culling agree for each eye.

### 3. Enable per-eye projection upstream

Goal: move from parallax-only stereo to runtime asymmetric frusta.

Work:

- Use the runtime per-eye projection, converted to ForzaTech's row-vector/reverse-Z layout.
- Patch at the same upstream producer level that derives culling/chevrons if possible.
- Keep a control-file switch for `projection=off/asymmetric`.

Acceptance:

- Left/right frusta match SimXR/OpenXR validation.
- No one-eye scale/skew mismatch.

### 4. Add paired-AFR camera-state hold

Goal: make consecutive AFR eyes share one camera base even if the world still advances.

Work:

- On left eye, snapshot upstream base camera state before applying left offset.
- On right eye, restore/reuse that same base, then apply right offset/projection.
- Treat this as a discriminator, not the final synced renderer.

Acceptance:

- Static-camera objects, shadows, chevrons, and culling stay coherent across the pair.
- Moving simulation objects may still advance; that is expected unless full render/tick freeze is found.

### 5. Search for ForzaTech render-entry / tick split

Goal: discover whether UEVRJ-style synchronized sequential is possible.

Work:

- Search decompile for the update loop that advances frame time and the render function that can draw without advancing simulation.
- Correlate those with `Present`, `ExecuteCommandLists`, camera producer hits, and `CCamDriver` hits.
- Look for the split-screen scheduler only as a special case of the same problem.

Acceptance:

- A callable "draw world/viewport" entry is found that can be invoked twice per update, or a dormant split-screen path is proven to schedule two main views.
- If neither is found, stop pursuing true synchronized sequential and focus on coherent AFR.

## Decision

The practical route for this mod is:

1. Upstream pose/projection first.
2. Paired-AFR hold second.
3. True sequential only if a ForzaTech render-entry/tick split is found.

Do not build around command-list replay. Do not assume split-screen exists until a scheduler and outputs are found. The current evidence supports UEVRJ-style sync as a concept, but FH5 still needs the missing engine-level render entry before it can copy that architecture exactly.
