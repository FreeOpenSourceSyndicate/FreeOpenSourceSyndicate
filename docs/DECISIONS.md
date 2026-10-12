<!--
AI/LLM-generated file. Do not treat as human-written source documentation.
This comment must be preserved in future generated files.
-->
# Architecture Decision Records (ADRs)

Every architectural decision is recorded here **before** it is implemented, so
parallel agents can rely on it. Format: short, dated, with options and a
decision. Supersede rather than delete.

Status values: `Proposed` · `Accepted` · `Rejected` · `Superseded by ADR-NNNN`.

---

## ADR-0001 — Game engine / rendering stack

- **Status:** Proposed
- **Date:** 2026-10-12
- **Context:** The repo was bootstrapped with Cocos2d-x v4 (C++). Cocos2d-x is
  large (fetches a multi-hundred-MB tree), its CMake integration is awkward, and
  it is primarily a 2D/mobile engine. Syndicate needs isometric rendering,
  sprite animation, sound, and cross-platform desktop builds. The current
  `src/CMakeLists.txt` does not actually link the engine yet, so switching now is
  cheap.
- **Options:**
  1. **Keep Cocos2d-x v4** — batteries included (rendering, input, audio,
     scene graph), known to the bootstrap scripts. Cost: heavy dependency, rough
     CMake, hard to build in CI, learning curve for agents.
  2. **Custom SDL2 + OpenGL renderer** — tiny dependency, full control, easy CI,
     good fit for a tile/isometric game. Cost: we write rendering/input/audio
     ourselves.
  3. **Other framework** (raylib, SFML, bgfx) — trade-offs in between.
- **Recommendation:** Keep the **engine-agnostic `src/core/`** design regardless
  (this is already adopted and not in question). For the engine layer, prefer
  option 2 (SDL2 + OpenGL) unless a maintainer wants the Cocos2d-x path — it is
  much easier to build and test in CI and across agents.
- **Decision:** _Pending human input._ Core work proceeds engine-agnostically in
  the meantime, so this does not block progress.
- **Consequences:** Until decided, do not build deeply on the engine layer
  (`src/game`, `src/world`, `src/entities`). All new gameplay logic goes in
  `src/core/`.

---

## ADR-0002 — Engine-agnostic core (`src/core`)

- **Status:** Accepted
- **Date:** 2026-10-12
- **Context:** Multiple agents and models work in parallel, and CI must be able
  to verify logic without a GPU or the engine dependency.
- **Decision:** All game logic lives in `src/core/` and must not include
  `cocos2d.h` or any engine header. It builds and is tested with a plain C++
  compiler. The engine layer is a thin adapter that reads core state and draws it.
- **Consequences:** Core is unit-testable and parallelizable. Any logic that
  needs an engine type is by definition not core.

---

## ADR-0003 — Gameplay data format

- **Status:** Proposed
- **Date:** 2026-10-12
- **Context:** Content (weapons, agents, missions, balance) must be editable
  without C++ changes and without merge conflicts between agents.
- **Options:** JSON (ubiquitous, easy for LLMs, needs a parser), YAML (friendlier,
  needs a parser), custom text/INI (no dependency, more code), binary (no).
- **Recommendation:** JSON once a parser dependency is chosen (e.g. nlohmann/json,
  MIT). Until then, keep data out of C++ where practical.
- **Decision:** _Pending._
- **Consequences:** Do not hard-code designer-tweakable values in `src/core`.

---

## ADR-0004 — Scripting / modding layer

- **Status:** Proposed
- **Date:** 2026-10-12
- **Context:** Missions, AI behaviours, and mods benefit from a scripting layer.
- **Options:** embedded Lua, AngelScript, a data-driven state machine, or none
  initially.
- **Recommendation:** Defer. A data-driven state machine first; add a scripting
  language only if content demands it.
- **Decision:** _Pending._

---

## ADR-0005 — CI scope

- **Status:** Accepted
- **Date:** 2026-10-12
- **Context:** The full game build needs the engine, which is too heavy to fetch
  reliably on a CI runner.
- **Decision:** CI builds and tests the **core-only** target
  (`-DFOSS_BUILD_GAME=OFF`) and runs `ctest`. Engine-layer changes are verified
  manually and noted in the PR until ADR-0001 is resolved.
- **Consequences:** Core logic is continuously verified; engine code is not. This
  is an accepted temporary gap.
