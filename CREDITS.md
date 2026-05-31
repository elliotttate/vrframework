# Credits & sources

This repository would not exist without the work it is built on. The credit belongs
upstream — please star and support the original projects.

## Primary source

- **[REFramework](https://github.com/praydog/REFramework)** by **praydog** — MIT.
  The original scripting platform, modding framework and VR support for RE Engine games.
  Every technique documented in the guides and every header scaffolded in this repo
  traces back to REFramework. This project is a derivative work of it.

## Reference ports studied in the guides

- **starfield2vr** by **mutars** — VR for Bethesda's Creation Engine 2 (Starfield).
  REFramework-derived. Studied for its Reflex-marker frame timing and CommonLibSF-based
  engine access.
- **anvilengine2vr** by **mutars** — VR for Ubisoft's Anvil engine (Assassin's Creed
  Odyssey / Valhalla / Mirage). REFramework-derived. Studied for its clean per-game
  adapter structure and pattern-scan address resolution.

Both ports share a private common core (`vrframework`); the scaffold in this repo is an
independent reconstruction of that shape from the public REFramework, not a copy of it.

## Libraries the techniques rely on

MinHook, safetyhook, ImGui, GLM, spdlog, nlohmann/json, OpenVR, OpenXR, DirectXTK12,
CommonLibSF — each under its own license.

## This repo

The scaffolded core and the guide series were authored by Elliott Tate
(github.com/elliotttate), MIT-licensed, preserving praydog's upstream copyright (see
[LICENSE](LICENSE)).
