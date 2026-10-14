<!-- AI/LLM-generated file. Do not treat as human-written source documentation. -->

# Changelog

All notable changes to FOSSynd. This project is an
independent, from-scratch recreation of the *gameplay* of 1993's Syndicate; it
contains no original assets, code, names, or trademarks.

## [Unreleased]

### Added

- **Minimap:** a whole-board overview panel in the top-right HUD corner shows the
  static terrain and a live blip per living agent (players green, enemies red,
  civilians pale, followers cyan; selected players highlighted). The terrain is
  baked once into a one-texel-per-tile texture and only the blips are drawn each
  frame, so it stays cheap on old hardware. New engine-layer `game/Minimap.*`
  (display-only; never touches the simulation) and a headless `--check-minimap`
  guard with the new `game_minimap` ctest. See `docs/ROADMAP.md` Phase 2.
- **Data-driven mission objectives (reach / survive):** a mission can now
  declare its win condition in JSON via an optional `objective` object. `reach`
  wins when any living player agent stands on a target tile; `survive` wins
  after a set number of seconds. The default remains `eliminate` (all enemies
  dead), so existing missions and the demo are unchanged. Types live in
  `core/sim/Objective.h`; parsing in `core/data/MissionData.*`; evaluation in
  `Mission::evaluateOutcome`; the HUD shows the active objective. New sample
  mission `assets/data/missions/escort.json` ("Rooftop Exfil"). Schema in
  `docs/DATA_FORMAT.md`.
- **Pixel-art agent sprites with a depth-sorted pass:** the flat agent diamonds
  are replaced by authored pixel-art sprites with a short walk cycle, drawn in a
  single back-to-front pass alongside static board props (crates and barrels).
  Art is authored as text in `assets/sprites/agents.txt`; `core::bakeSheet`
  (`src/core/data/AgentSprites.{h,cpp}`) parses and palettes it (engine-free,
  unit-tested) and `game::AgentSprites` uploads it to SDL textures. Per-role
  palettes give players, enemies, civilians, and followers distinct colours from
  one sheet. The sheet is embedded in the binary; a bad sheet falls back to the
  old markers. New ctest `game_sprites` and `tests/test_sprites.cpp`. See
  ADR-0013.
- **Persuadertron (recruit civilians):** a deterministic device in
  `src/core/sim/Persuadertron.h`. `Mission::persuade` turns the nearest
  un-recruited civilian in range into a *follower* of the using agent; followers
  keep `Team::Civilian` (so they are never valid combat targets) and trail their
  leader via the existing pathfinding. `Mission::freeFollowersOf` releases them
  and `Mission::followerCount` reports them. View: `P` persuades with the
  selection, `F` frees followers, followers render cyan, and the HUD shows a
  follower count. No RNG, no effect on win/lose.

## [0.4.0] — 2026-10-13 — "Smart Enemies"

Enemies think before they charge, the game opens on an authored pixel-art title
card, and the project is now FOSSynd. First release built on the JSON content
pipeline.

**Visibly different from v0.3.0:** a pixel-art title card on launch, smarter
enemies (patrol / engage / retreat / call backup), the name `FOSSynd` and the
version `v0.4.0` on screen, and an authored pixel-art release banner.

### Added

- **Enemy AI: patrol, retreat, call backup:** enemies now run a small state
  machine (`AiState` on `core::Agent`) instead of always charging. With no target
  in sight they walk an assigned patrol route; once a target is in sight they
  engage; and when wounded below 35% health they break off and retreat. On first
  engaging, an enemy calls backup (`Mission::alertAllies`), pulling same-team
  allies within 8 tiles onto the same target. All transitions are deterministic
  (no RNG), so seeded missions stay reproducible. Core only; the demo mission is
  unchanged (enemies simply behave better).
- **Content data format (ADR-0003):** missions are now described in JSON under
  `assets/data/missions/` and loaded by a small, dependency-free reader in
  `src/core/data/` (`Json`, `MissionData`). Schema in `docs/DATA_FORMAT.md`.
  `Game::buildMission` now loads the demo mission from
  `assets/data/missions/demo.json`, preferring an external file and falling back
  to the copy embedded at build time so the binary stays self-contained. The
  demo mission moved out of C++ into data; no third-party dependency was added.
- **Configurable mission selection:** the mission path can now come from a
  `--mission` CLI flag or a `mission`/`mission_dir` key in a config file
  (`--config`, else `./fossynd.conf`, `<exe-dir>/fossynd.conf`,
  `/etc/fossynd.conf`, `/usr/local/etc/fossynd.conf`). With neither, the built-in
  fallbacks are scanned and the first existing wins (repo-relative, next to the
  executable, `/usr/local/share`, `/usr/share`); otherwise the embedded copy is
  used. See `assets/data/fossynd.conf.example` and `docs/DATA_FORMAT.md`.
- **Install rules:** `cmake --install` now installs the binary plus
  `share/fossynd/missions/demo.json` and the example config, so an installed
  build finds its data via the fallback search.
- **Pixel-art title card:** launching the game opens on an authored, palette-based
  title screen (name, version, controls, a squad badge) drawn from the same
  isometric markers the game uses at runtime. Press any key or click to start; it
  also starts itself after a few seconds so unattended runs never stall. Shown
  only to an interactive player; `--title` forces it for headless capture. New
  ctest `game_title`.
- **Authored pixel-art release banner:** `assets/ui/banner.png` (indexed palette,
  nearest-neighbour), generated reproducibly by `assets/ui/make_banner.py` with no
  third-party dependency. Registered in `docs/ASSETS.md`.

### Changed

- **Project renamed to FOSSynd** (ADR-0010). The short name `FOSS` collided with
  the generic term *Free and Open Source Software*, so the project is now
  **FOSSynd** ("Free, Open-Source Syndicate"). Build options use the `FOSSYND_*`
  prefix, the core library is `fossynd_core`, the C++ namespace is `fossynd::`,
  and the game binary is `FOSSynd`.
- **Project goal (ADR-0009):** v1.0.0 is a faithful reproduction, so release and
  in-game graphics are **pixel art** (low-res, palette-based, nearest-neighbour).
  The earlier SVG-first guidance is retracted; vector art and modernising changes
  are v2.x.x/fork scope.
- **On-screen version stamp:** the window title, the HUD, and the title card now
  read `FOSSynd v<version>`, from a single `FOSSYND_VERSION` definition set by the
  CMake project version.
- **Published release branches are immutable (ADR-0011, `AGENTS.md` §6b):** once
  a `release/vX.Y.Z-*` branch is tagged and published, it is never committed to
  again. A fix that a shipped version needs to keep working goes on a new branch
  cut from that release branch.
- **PR review/merge policy (`AGENTS.md` §3a):** agents review and merge each
  other's PRs; while the repository's GitHub account is the only account working
  on the project, the creator may review and merge their own PR.

### Release

- Branch `release/v0.4.0-smart-enemies`, tag `v0.4.0`; per-version build, play,
  and copy-to-another-computer instructions in
  [`docs/releases/v0.4.0-smart-enemies.md`](docs/releases/v0.4.0-smart-enemies.md).

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
cmake -S . -B build -DFOSSYND_BUILD_GAME=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/src/FOSSynd          # play
./build/src/FOSSynd --demo   # hands-off attract mode
ctest --test-dir build --output-on-failure   # 37 unit tests + 3 ctests
```
