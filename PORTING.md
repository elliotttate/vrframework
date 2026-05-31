# Porting checklist — stub → REFramework source

This scaffold ships **complete, clean contract headers** and **stub implementations**.
Each stub marks `// PORT FROM: <path>` at the top. To turn the scaffold into a working
core, lift the bodies from REFramework and strip RE-Engine coupling as noted.

| vrframework file | Lift from REFramework | What to strip / change |
|---|---|---|
| `include/Framework.hpp` | `src/REFramework.hpp` | Remove `sdk/GameIdentity.hpp`, `ReClass`, `d3d12/CommandContext`, `GraphicsMemory`. Rename class `REFramework`→`Framework`, global `g_framework` type. Keep the public getters the ports call. |
| `src/Framework.cpp` | `src/REFramework.cpp` | Drop `initialize_game_data()` (RE reflection). Replace RE persistent-dir logic with engine-neutral version. Drive mods via `IEngineAdapter`. |
| `include/Mod.hpp` | `src/Mod.hpp` | **Delete** all RE-typed virtuals (`on_pre_update_transform(RETransform*)`, `MAKE_LAYER_CALLBACK`, `on_*_camera_controller`, `on_application_entry`). Add `bool set_defaults` to `on_config_load`. Keep ModValue widget family verbatim. |
| `include/Mods.hpp` / `src/Mods.cpp` | `src/Mods.cpp` | Body (the mod list) is **provided by the consumer** per game. Core ships the iteration logic only. |
| `include/hooks/D3D12Hook.hpp` | `src/D3D12Hook.hpp` | None — already engine-agnostic. Drop `pragma comment` if using CMake link. |
| `src/hooks/D3D12Hook.cpp` | `src/D3D12Hook.cpp` | None — pure DXGI/D3D12. Verbatim. |
| `include/hooks/D3D11Hook.hpp` + cpp | `src/D3D11Hook.{hpp,cpp}` | Verbatim. |
| `include/hooks/WindowsMessageHook.hpp` + cpp | `src/WindowsMessageHook.{hpp,cpp}` | Verbatim. |
| `include/vr/VRRuntime.hpp` | `src/mods/vr/runtimes/VRRuntime.hpp` | Replace `sdk/Math.hpp` with glm typedefs (`Matrix4x4f`, `Vector4f`). |
| `include/vr/runtimes/OpenVR.hpp` / `OpenXR.hpp` | `src/mods/vr/runtimes/OpenVR.*` `OpenXR.*` | These are already engine-neutral. Verbatim. |
| `include/mods/VR.hpp` | `src/mods/VR.hpp` | Keep the pose/projection/eye/frame-counter surface. **Cut** RE camera hooks (`on_pre_application_entry`, RE8VR game module). Stereo submit goes through `IEngineAdapter::apply_stereo`. |
| `src/mods/VR.cpp` | `src/mods/VR.cpp` + `D3D11Component`/`D3D12Component` | Lift submission + WaitGetPoses; remove RE scene/camera reads — those become adapter callbacks. |
| `include/utility/Config.hpp` + cpp | praydog `utility/Config.hpp` (submodule, not vendored here) | Authored fresh from the `get<T>/set<T>` usage. Swap in praydog's if you vendor `utility`. |
| `include/utility/ScopeProfiler.h` | praydog `utility/ScopeProfiler.hpp` | Minimal version authored here. |
| `include/memory/memory_mul.h` + cpp | `starfield2vr/.../ScanHelper.h` + praydog `utility/Scan.hpp` | Authored from the ports' `FuncRelocation/InstructionRelocation` usage. |

## New code (no REFramework equivalent — the universalization)

| File | Purpose |
|---|---|
| `include/spi/IEngineAdapter.hpp` | The 4 responsibilities every port implements, as one interface. |
| `include/spi/FrameTimeline.hpp` + cpp | Reusable engine→render→present counter state machine + drift recovery (replaces per-engine `setReflexMarker`). |
| `include/spi/StereoView.hpp` | Per-eye view+projection POD handed from core to adapter. |
| `include/spi/EngineCaps.hpp` | Capability flags so core knows D3D11/12, AFR vs sequential, TAA, etc. |

## External dependencies (not vendored)

`safetyhook`, `glm`, `imgui`, `spdlog`, `nlohmann/json`, `openvr`, OpenXR loader,
`directxtk12`. The consuming repos already provide these; `CMakeLists.txt` expects
them as targets/`find_package`. See the `# DEP:` comments there.
