# Building VR Support Into a Game Engine

### A field guide, reverse-engineered from three real mods

This is a complete, multi-part guide to one of the strangest disciplines in graphics
programming: taking a shipping game whose source code you will **never** see, and
bending its flat, one-camera renderer into a stereo VR experience **from the outside** —
by injecting a DLL, hooking DirectX, lying to the engine about where the camera is, and
stealing its rendered frames before they reach the monitor.

It is built by reading, line by line, three real open-source projects that each did
exactly this for a different AAA engine:

| Project | Engine | Games | What makes it interesting |
|---|---|---|---|
| [**REFramework**](https://github.com/praydog/REFramework) (praydog) | Capcom **RE Engine** | Resident Evil, Monster Hunter, Dragon's Dogma 2 | The origin. Uses the engine's own reflection/type system. D3D11 + D3D12, OpenVR + OpenXR, Lua, plugin API. The most complete. |
| **starfield2vr** (mutars) | Bethesda **Creation Engine 2** | Starfield | Drives frame timing off Nvidia **Reflex markers**; uses the CommonLibSF address library + hand-written reclass headers. |
| **anvilengine2vr** (mutars) | Ubisoft **Anvil** | AC Odyssey / Valhalla / Mirage | The cleanest per-game structure; safetyhook + pattern-scan offsets; OpenXR. |

The two `*2vr` projects were forked from REFramework and share a common core
(`vrframework`). The guides treat all three as one body of accumulated wisdom and
constantly **compare how each solved the same problem differently** — because the
differences are where the real lessons live.

> Every claim in these documents is grounded in the actual code, cited as
> `repo/path/file:line` (clickable in most editors). When a guide says "Anvil does X",
> there is a line number next to it. Read the cited code alongside the prose.

---

## How to read this

**If you want to understand the field:** read [01](01-the-big-picture.md) →
[16](16-porting-checklist-for-a-new-engine.md) in order. It's written as a narrative.

**If you're actually porting an engine right now:** read [01](01-the-big-picture.md) for
the mental model, then jump straight to
[16 — the porting checklist](16-porting-checklist-for-a-new-engine.md), which is a
milestone-by-milestone playbook, and dip back into the deep-dives below as each
milestone bites you.

**If a specific thing is broken:** go straight to its document. The world is tilting →
[09](09-camera-and-coordinate-systems.md). Ghosting/smearing →
[10](10-submission-and-the-taa-problem.md). Juddery, wrong-eye frames →
[07](07-frame-timing-and-synchronization.md).

---

## The series

### Foundations — getting inside and owning the frame
- **[01 · The Big Picture: Anatomy of an Engine VR Mod](01-the-big-picture.md)**
  The mental model you keep open while reading everything else: the three-layer
  architecture, the life of one VR frame, and a glossary of every term used later.
- **[02 · Getting In: Injection & Bootstrap](02-injection-and-bootstrap.md)**
  Proxy DLLs (`dxgi.dll`), `DllMain`, the worker-thread + `Sleep` trick, and why
  blocking the loader is fatal.
- **[03 · Hooking & Finding Things: Pattern Scanning & Address Resolution](03-hooking-and-pattern-scanning.md)**
  Inline/vtable/pointer/mid hooks, MinHook vs safetyhook, AOB signatures, RIP-relative
  resolution, fallback offsets, and address libraries (REL::ID / CommonLibSF).
- **[04 · Owning the Frame: Hooking D3D11 & D3D12](04-graphics-api-interception.md)**
  The dummy device/swapchain vtable steal, the command-queue offset hunt, two-phase
  hooking, capturing the live device/queue/backbuffers, and the DLSS/Proton gotchas.
- **[05 · The Spine: Framework Singleton, Mods & Overlay](05-the-framework-core.md)**
  The reusable core: lifecycle, the `Mod` base (and the one rule — zero engine types in
  it), the config system, ImGui overlay, and input plumbing.

### The VR pipeline — eyes, timing, and the math
- **[06 · Talking to the Headset: OpenVR & OpenXR](06-vr-runtime-integration.md)**
  One abstraction, two runtimes: init, per-eye projections, eye-to-head transforms, the
  pose lifecycle, and why the first `WaitGetPoses` must happen before the game renders.
- **[07 · The Hard Part: Frame Timing & Synchronization](07-frame-timing-and-synchronization.md)**
  The deepest dive. The three frame counters, Starfield's Reflex-marker approach vs
  Anvil's two-hook approach, the left/right cadence, and drift recovery.
- **[08 · Two Eyes From One Engine: Stereo Rendering & Projection](08-stereo-rendering-strategies.md)**
  Why Alternate-Frame Rendering, building the per-eye projection from runtime frustum
  bounds, and injecting it (hook-return vs constant-buffer patch).
- **[09 · Putting Your Head in the Game: Camera & Coordinate Math](09-camera-and-coordinate-systems.md)**
  The master equation `view * BASIS * offset * hmd * eye * INV_BASIS` derived term by
  term; handedness, up-axis, decoupled pitch, head-aim, world scale.
- **[10 · Submitting to the Headset & The TAA/History Problem](10-submission-and-the-taa-problem.md)**
  Copying eye textures, D3D12 barriers, and the notorious temporal-smear fix
  (double-buffering past matrices and resources).

### Making it playable
- **[11 · HUD, UI & Menus in VR](11-hud-ui-and-menus-in-vr.md)**
  Reprojecting/scaling the HUD, detecting flat menus and switching to a quad, Scaleform,
  and the limits you'll have to document.
- **[12 · Input: Gamepads, Motion Controllers & Head Tracking](12-input-and-motion-controllers.md)**
  XInput emulation, motion-controller bindings, head-tracking modes, menu chords, haptics.
- **[13 · Reading the Engine: Three Ways to Find the Player & Camera](13-reading-the-engine-object-model.md)**
  The ladder: full reflection (RE) → RTTI + address library + reclass (Creation) → pure
  pattern-scan reclass (Anvil). When to use each.
- **[14 · Fighting the Engine: Compatibility Tweaks & Quirks](14-engine-tweaks-and-quirks.md)**
  Disabling DOF/TAA/letterbox (by patching code or via the engine's settings), forcing
  window/render resolution, and the anti-tamper minefield.

### Shipping & doing it yourself
- **[15 · Shipping It: Multi-Title Architecture & Build System](15-multi-title-architecture-and-build.md)**
  The per-game folder pattern, externalized offsets, CMake/submodules, and how the proxy
  DLL gets built and named.
- **[16 · Doing It Yourself: A Porting Checklist for a New Engine](16-porting-checklist-for-a-new-engine.md)**
  The capstone. Ten milestones, each producing a visible result — inject → overlay →
  hook present → read the camera → poses → one eye → AFR → timing → HUD → input — with
  how to verify each and what's irreducibly hard.

---

## The four problems, one diagram

Every line of code in all three projects exists to solve one of these:

```
        ┌───────────────────────────────────────────────────────────┐
        │  1. GET INSIDE      inject DLL, hook DirectX  → docs 02-05  │
        │  2. RENDER 2 EYES   AFR + per-eye projection  → docs 07,08  │
        │  3. PLACE THE HEAD  fold HMD pose into view   → doc 09      │
        │  4. SHIP THE IMAGE  copy eyes to compositor   → doc 10      │
        └───────────────────────────────────────────────────────────┘
   ...everything else (UI, input, reading state, tweaks, build) makes it playable.
```

## Companion code

The repo this lives in (`vrframework/`) is a scaffolded, engine-agnostic core extracted
from REFramework — see [`../README.md`](../README.md) and [`../PORTING.md`](../PORTING.md).
Guides 05, 07, 08 and 16 reference its `IEngineAdapter` / `FrameTimeline` / `StereoView`
design as the "clean" shape these techniques converge toward.

## A note on scope and ethics

These projects are single-player visual mods that inject into a local process you own.
That's the context this guide assumes. The same hooking techniques exist in a broader
world with very different rules — respect anti-cheat, EULAs, and online play. Don't take
any of this near a multiplayer game.

---

*Total series: 17 documents, ~5,900 lines, ~180 distinct source-code citations across
REFramework, starfield2vr, and anvilengine2vr.*
