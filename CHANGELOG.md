<!-- AI/LLM-generated file. Do not treat as human-written source documentation. -->

# Changelog

All notable changes to Free Open Source Syndicate. This project is an
independent, from-scratch recreation of the *gameplay* of 1993's Syndicate; it
contains no original assets, code, names, or trademarks.

## [Unreleased]

_No changes yet._

## [0.3.0] — 2026-10-13 — "Bystanders"

The city is alive: bystanders walk the streets, react to gunfire, and can be
caught in the crossfire. Also the first release with an explicit old-hardware
profile.

**Visibly different from v0.2.0:** pale civilians on the map, a `COLLATERAL`
counter in the HUD, and the version `v0.3.0` shown in the window title and HUD.

### Added

- **Low-spec / old-hardware profile (ADR-0008):** `-DFOSS_LOW_SPEC=ON` builds a
  smaller, `-Os`, stripped binary; `--low-spec` (or a low-spec build) runs a
  320x200 window with the software renderer. A dedicated CI job builds this
  profile so old-hardware support cannot bit-rot.
- **Civilians and collateral damage:** unarmed bystanders roam the map, flee from
  nearby gunfire, and can be killed by stray shots. They are never auto-targeted
  and do not affect win/lose; player-caused civilian deaths are counted as
  *collateral* and shown in the HUD. Core: `Team::Civilian`, deterministic flee
  AI, and stray-fire collateral in `Mission`. View: paler civilian markers and a
  HUD counter.
- **On-screen version stamp:** the window title and HUD show the release version
  (`FOSS_VERSION`, set once from the CMake project version), so playtesters can
  tell builds apart at a glance.

### Release

- Branch `release/v0.3.0-bystanders`, tag `v0.3.0`; per-version build, play, and
  copy-to-another-computer instructions in
  [`docs/releases/v0.3.0-bystanders.md`](docs/releases/v0.3.0-bystanders.md).

## [0.2.0] — 2026-10-12 — "Command & Control"

Direct control over *which* agents you command and *what* they shoot.

### Added

- **Focus-fire / attack orders:** right-click (or left-click) an enemy to order
  the selected agents to engage it; a white/red reticle marks the target. The
  order clears on a move order or when the target dies.
- **Individual agent selection:** click an agent to select it, shift-click to
  add/remove, keys `1`–`4` for the Nth living agent, `A` for the whole squad.
  Orders apply to the current selection; the HUD roster marks selected agents
  with `>`.

## [0.1.0] — 2026-10-12 — "Playable Slice"

The first build worth putting in front of a person: a single playable mission.

### Added

- **Playable mission (SDL2):** a four-agent squad on an isometric city map.
  Objective: eliminate all enemy agents before your squad is wiped out.
- **Controls:** left-click to move the squad (2-wide formation), arrow keys to
  scroll the camera, `R` to restart, `Esc` to quit.
- **Combat:** weapons with damage, range, fire rate, and range-based accuracy
  falloff; health bars, hit flashes, kills.
- **Enemy AI:** guards sight, chase, and engage the squad.
- **HUD:** objective, squad roster (name / weapon / HP), win/lose banner.
- **Embedded bitmap font** (5×7 atlas), so there is no SDL_ttf dependency.
- **Attract mode** (`--demo`): the squad fights on its own and auto-restarts.
- **Headless verification flags:** `--headless`, `--frames N`,
  `--screenshot PATH`.
- **Core simulation layer (`src/core/`, engine-free):** `Mission` (orchestrates
  agents, AI, fire, win/lose), `Agent` (team/health/weapon), `Weapon`,
  `util/Rng` (deterministic xorshift64*).
- **Tests:** `test_mission`, `test_rng`, plus a headless `game_demo` ctest.

### Changed

- Engine is **SDL2 + SDL_Renderer** (ADR-0006); the earlier Cocos2d-x scaffold
  is gone.
- Gameplay lives in the core; the SDL layer is a thin view (ADR-0007).

### Known limitations

- Agents are flat markers, not sprites; no animation, zoom, or minimap yet.
- Mission content is hard-coded in `Game::buildMission` (data format: ADR-0003).
- Single mission; no metagame, gadgets, or cybernetics yet.
- Only original code and assets are used.

### Testing this build

```bash
sudo apt-get install -y build-essential cmake git libsdl2-dev
cmake -S . -B build -DFOSS_BUILD_GAME=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/src/FreeOpenSourceSyndicate          # play
./build/src/FreeOpenSourceSyndicate --demo   # hands-off attract mode
ctest --test-dir build --output-on-failure   # 31 unit tests + 3 ctests
```
