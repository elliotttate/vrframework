# vrframework

**A field guide to building VR support into a game engine — plus a scaffolded,
engine-agnostic VR/modding core to build it on.**

This repo is two things that belong together:

1. **[`guides/`](guides/00-README.md) — a 17-part written guide** (~6,000 lines) that
   teaches, from first principles, how to bend a shipping game's flat one-camera renderer
   into stereo VR *from the outside* — by injecting a DLL, hooking DirectX, lying to the
   engine about the camera, and stealing its frames before they reach the monitor. Every
   technique is grounded in real, cited code from three production VR mods.
2. **A scaffolded engine-agnostic core** (`include/`, `src/`) — the reusable Layer-1
   framework (injection, D3D hooks, ImGui overlay, mod system, VR runtime, frame timing)
   plus a clean engine **SPI** (`IEngineAdapter` / `FrameTimeline` / `StereoView`) that
   the duplicated per-engine logic collapses into. See [`PORTING.md`](PORTING.md).

> ## Source & credit
> This work is derived from **[praydog/REFramework](https://github.com/praydog/REFramework)**
> — the original scripting/modding/VR framework for Capcom's RE Engine, and the project
> every technique here traces back to. **REFramework is MIT-licensed, © praydog.** If you
> find this useful, the credit belongs upstream: ⭐ [praydog/REFramework](https://github.com/praydog/REFramework).
>
> The guides additionally study two REFramework-derived ports by **mutars**:
> **starfield2vr** (Bethesda Creation Engine 2) and **anvilengine2vr** (Ubisoft Anvil).

---

## The guide series

Start at **[`guides/00-README.md`](guides/00-README.md)** for reading paths. The arc:

| # | Document | What it answers |
|---|----------|-----------------|
| [01](guides/01-the-big-picture.md) | The Big Picture | What does "VR-ifying" a closed engine even mean? |
| [02](guides/02-injection-and-bootstrap.md) | Injection & Bootstrap | How does my code get into the game? |
| [03](guides/03-hooking-and-pattern-scanning.md) | Hooking & Pattern Scanning | How do I intercept functions with no symbols? |
| [04](guides/04-graphics-api-interception.md) | Hooking D3D11 & D3D12 | How do I own the frame? |
| [05](guides/05-the-framework-core.md) | The Framework Core | The reusable spine: singleton, mods, overlay |
| [06](guides/06-vr-runtime-integration.md) | OpenVR & OpenXR | Talking to the headset behind one abstraction |
| [07](guides/07-frame-timing-and-synchronization.md) | Frame Timing | The hardest part — Reflex markers vs two-hook pacing |
| [08](guides/08-stereo-rendering-strategies.md) | Stereo Rendering | Two eyes from a one-camera engine (AFR + projection) |
| [09](guides/09-camera-and-coordinate-systems.md) | Camera & Coordinate Math | Putting your head in the world (basis/handedness) |
| [10](guides/10-submission-and-the-taa-problem.md) | Submission & the TAA Problem | Sending eyes out + the history-smear fix |
| [11](guides/11-hud-ui-and-menus-in-vr.md) | HUD, UI & Menus | Making flat UI usable in stereo |
| [12](guides/12-input-and-motion-controllers.md) | Input & Controllers | Wiring VR input into a gamepad game |
| [13](guides/13-reading-the-engine-object-model.md) | Reading the Engine | Three ways to find the player/camera |
| [14](guides/14-engine-tweaks-and-quirks.md) | Engine Tweaks & Quirks | Disabling effects that fight VR |
| [15](guides/15-multi-title-architecture-and-build.md) | Multi-Title & Build | One codebase, many games; the build/proxy setup |
| [16](guides/16-porting-checklist-for-a-new-engine.md) | Porting Checklist | The capstone, milestone by milestone |

The guides are grounded in **~180 `file:line` citations** across the three reference
projects, and constantly compare how RE Engine, Creation Engine 2, and Anvil each solved
the same problem differently.

### The three reference projects

| Project | Engine | Games |
|---|---|---|
| [REFramework](https://github.com/praydog/REFramework) (praydog) | RE Engine | Resident Evil, Monster Hunter, DD2 |
| starfield2vr (mutars) | Creation Engine 2 | Starfield |
| anvilengine2vr (mutars) | Ubisoft Anvil | AC Odyssey / Valhalla / Mirage |

---

## The scaffolded core

Layered architecture the guides converge toward:

```
LAYER 3  per-game DATA         offsets manifest + per-title settings + reclass structs
LAYER 2  per-engine ADAPTER    implements spi/IEngineAdapter (frame pacing, projection,
                               view, HUD, input) — the per-engine .cpp hooks
─────────────────────────────────────────────────────────────────────────────────────
LAYER 1  UNIVERSAL CORE  ← include/ + src/ in this repo
  Framework / g_framework      lifecycle, window, input, d3d hooks, imgui overlay
  Mod + ModValue widgets       config-backed UI; zero engine types
  hooks/  vr/  mods/VR         D3D11/12 hooks, OpenVR/OpenXR runtime, stereo submit
  spi/                         IEngineAdapter, FrameTimeline, StereoView, EngineCaps
  utility/  memory/            Config, ScopeProfiler, pattern-scan relocation helpers
```

**Status:** the contract headers are complete and clean; several `.cpp` bodies are
**stubs with `PORT FROM:` pointers** at the exact REFramework source to lift. This is a
teaching/starting scaffold, not a finished runtime — see [`PORTING.md`](PORTING.md) for
the file-by-file map and [`guides/16`](guides/16-porting-checklist-for-a-new-engine.md)
for the bring-up order.

```cmake
add_subdirectory(extern/vrframework)
target_link_libraries(MyEngineVR PRIVATE vrframework)
```

---

## Repository layout

```
vrframework/
├── guides/            the 17-part written guide (start at 00-README.md)
├── include/           public headers: Framework, Mod, spi/, mods/VR, hooks/, vr/, utility/
├── src/               implementations (real where trivial; stubs with PORT FROM: notes)
├── examples/          an example IEngineAdapter + a per-game offsets manifest
├── PORTING.md         stub → REFramework source map
├── README.md          you are here
└── LICENSE            MIT (preserves praydog's upstream copyright)
```

## License

MIT. This is a derivative of [praydog/REFramework](https://github.com/praydog/REFramework)
(MIT, © praydog); that copyright notice is preserved in [`LICENSE`](LICENSE). See also
[`CREDITS.md`](CREDITS.md).

## A note on scope & ethics

The reference projects are single-player visual mods that inject into a local process you
own — that's the context this guide assumes. The same techniques exist in a broader world
with different rules: respect anti-cheat, EULAs, and online play. Don't take this near a
multiplayer game.
