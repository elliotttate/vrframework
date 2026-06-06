# FH5 synced-sequential equivalent probe

Date: 2026-06-06  
Scope: active Empress/FH5 `.i64` and current `E:\Github\vrframework` source. No game/runtime launch was used.

## Why this pass was needed

UEVR's synced sequential path depends on three concrete Unreal boundaries:

- `UGameEngine::Tick` / `UGameViewportClient::Draw` / `FViewport::Draw`
- a way to draw a second eye before the next world tick
- skip/suppress logic so the second draw reuses the same simulation state

FH5 does not expose those names. This pass searched the active `.i64` for the closest ForzaTech equivalents rather than trusting older Steam-build notes.

Artifacts generated:

- `E:\ForzaHorizon5_IDA_Decompile\synced_sequential_targets_20260606\synced_targets.md`
- `E:\ForzaHorizon5_IDA_Decompile\synced_sequential_targets_20260606\synced_targets.json`
- `E:\ForzaHorizon5_IDA_Decompile\synced_sequential_targets_20260606\xref_chain.json`
- temporary IDA scripts in the FH5 folder:
  - `E:\SteamLibrary\steamapps\common\ForzaHorizon5\_codex_ida_synced_targets.py`
  - `E:\SteamLibrary\steamapps\common\ForzaHorizon5\_codex_ida_xref_chain.py`

## Confirmed static dead ends

Most obvious string hits are registration/config code, not live frame functions.

| String / family | IDA function | Result |
|---|---:|---|
| `PresentationPreRenderTimers` | `0x140063420` | registrar; callers=0 |
| `PresentationRenderTimers` | `0x1400634B0` | registrar; callers=0 |
| `DrawBundleRecorderThread1/2` | `0x1400B3CA0` | thread-name enum registrar; callers=0 |
| `ImageProcessor_FinalPresent*` / `ImageProcessor_PQToSwapchain*` | `0x140043CC0` | image-processor task-name registrar; callers=0 |
| `RenderData_Presentation` pass params | `0x14002EB70`, `0x140032270`, etc. | resource/param registration; callers=0 |
| `SimulationReady` / `TrackSwapSimulationReady` | `0x14012AA00`, `0x14012B690` | named-event init; callers=0 |
| `SimulationModule` | `0x14012E6B0` | type/registration wrapper; callers=0 |

Conclusion: the pass names and thread names are useful taxonomy, but they are not hookable equivalents of Unreal's `Draw`/`Tick`.

## Best static render-side candidate found

The only render-side string hit with a real caller was:

- `d:\p4\woodstock_rc\engine\presentation11\src\TrackPresentationHelpers11.cpp:320`
- xref function: `sub_14143B550`
- caller: `sub_1413FD090`
- wrapper: `sub_1414441D0`
- wrapper table/data xref: `.rdata` around `0x1468E69B0`

`sub_1413FD090` is a lock/ref handoff around `TrackPresentationHelpers11`, with calls into `loc_1412ED7B0` and `sub_141433C20`. This looks like a presentation resource/visibility handoff, not the top-level "draw world now" entry.

It is worth instrumenting if the producer-stack path points at it, but by static evidence alone it is not the UEVR-style viewport draw equivalent.

## Producer dispatch finding

The known live producer address `0x140BB1EE0` does not have static call xrefs. The xref chain confirms why:

- exact `0x140BB1EE0`: no xrefs
- surrounding protected functions: `sub_140BB1E40`, `sub_140BB1E90`
- table/descriptor xref: `.rdata` around `0x146866B40`
- table contents mix data/floats and function pointers:
  - `0x146866B48 -> sub_140BB1F20`
  - `0x146866B60 -> sub_140BB1F60`
  - `0x146866B68 -> sub_140BB1E40`
  - `0x146866B70 -> sub_140BB1E90`
  - other entries point at unnamed `unk_14767...` data

Conclusion: static callers will not identify the frame boundary. The producer is reached indirectly through an engine descriptor/protected dispatch path. The correct way to find FH5's viewport/render-world parent is to capture runtime stacks from the producer hook and map the parent RVAs back into IDA.

## New runtime discriminator added

Added an opt-in producer stack probe in:

- `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp`
- helper: `producer_stack_probe_enabled()`
- helper: `maybe_log_producer_stack(...)`
- call site: inside the main-camera gate in `Hook_Producer`

It is disabled by default. Enable it only when searching:

```powershell
$env:FH5_PRODUCER_STACK_PROBE = '1'
# launch FH5 through the normal FH5VR launch path
```

Expected log line:

```text
[FH5] producer stack probe #1 mainHits=... near=0.1000 far=... frames=+0x... +0x... +0x...
```

The useful frame is the first stable in-module parent above the producer/trampoline/hook frames. Repeated stacks should reveal the real render-view driver that calls into the producer's dispatch path. That is FH5's practical equivalent of UEVR's `FViewport::Draw`/`UGameViewportClient::Draw` boundary.

## Current conclusion

I did not find a safe static FH5 equivalent for `UGameViewportClient::Draw` or `UGameEngine::Tick`.

The closest confirmed path is:

1. Keep using the proven producer `0x140BB1EE0` for per-eye view/projection mutation.
2. Use the new producer stack probe to identify the runtime parent frame.
3. Once that parent is identified, decide whether it is callable/reentrant enough for in-frame sequential rendering.
4. If it is not callable, FH5 probably cannot do true UEVR-style synced sequential without a deeper render scheduler hook; AFR remains the practical fallback.

Native split-screen/two-camera support was not found in this pass. The strings show one active presentation/render stack with task/pass registrars, not an obvious "render two gameplay cameras" scheduler.
