# Example: an Anvil adapter on the vrframework SPI

This shows how `anvilengine2vr`'s three engine classes collapse onto **one**
`IEngineAdapter`, and where the duplicated frame-sync / stereo logic moves into the core.

| anvil today | maps to |
|---|---|
| `EngineEntry : public Mod` (settings + `on_draw_ui`) | `ExampleAnvilAdapter : public IEngineAdapter` (still a `Mod`) |
| `EngineRendererModule::on_begin_engine_frame` | `timeline().report(ENGINE_FRAME_BEGIN)` |
| `EngineRendererModule::on_begin_render_frame` | `timeline().report(RENDER_FRAME_BEGIN)` + `WAIT_RENDER` |
| `EngineCameraModule::onCalcProjection` (builds frustum) | core builds `StereoView::projection`; adapter just writes it in `apply_stereo` |
| `EngineCameraModule::onCalcFinalView` (`view * BASIS*offset*hmd*eye*INV_BASIS`) | core does the basis math (from `EngineCaps`); adapter writes `StereoView::view` |
| `onCalcUIViewportHook` (HUD scale) | `reproject_hud(scale_x, scale_y)` |
| `EngineTwicks::DisableBadEffects()` | `disable_incompatible_effects()` |
| `memory::*_fn_addr()` in `offsets.h` | `install_hooks()` via `OffsetManifest` / `FuncRelocation` |

Net effect: the AFR/TAA history buffering, the L/R cadence + skip-present recovery, and
the per-eye matrix composition stop being copy-pasted per engine. The adapter shrinks to
"find ~6 functions, write the matrices the core hands me, scale the HUD."

Registration is unchanged from the ports:

```cpp
Mods::Mods() {
    m_mods.emplace_back(VRConfig::get());
    m_mods.emplace_back(VR::get());
    m_mods.emplace_back(ExampleAnvilAdapter::get());  // the IEngineAdapter
}
```
