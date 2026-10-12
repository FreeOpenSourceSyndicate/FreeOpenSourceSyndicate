<!-- AI/LLM-generated file. Do not treat as human-written source documentation. -->

# Changelog

All notable changes to Free Open Source Syndicate. This project is an
independent, from-scratch recreation of the *gameplay* of 1993's Syndicate; it
contains no original assets, code, names, or trademarks.

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
