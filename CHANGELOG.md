<!-- AI/LLM-generated file. Do not treat as human-written source documentation. -->

# Changelog

All notable changes to FOSSynd. This project is an
independent, from-scratch recreation of the *gameplay* of 1993's Syndicate; it
contains no original assets, code, names, or trademarks.

## [Unreleased]

### Added

- **"Walk as close as you can" to a blocked goal.** Clicking an unreachable or
  blocked tile used to drop the move order. The new `core::findPathToNearest`
  returns the normal path when the goal is reachable, and otherwise a path to the
  walkable tile reachable from the start that is nearest the goal — so the squad
  moves as close as it can instead of ignoring the click. Deterministic
  (Manhattan distance, ties by lowest tile index), reuses the node cap, and never
  returns a zero-length fallback; `NoPath` only when nothing is reachable. New
  unit tests cover the reachable, blocked-goal, sealed-off, and enclosed cases.
  Issue #146.
- **Interpolated rendering over a fixed-step simulation:** the view now drives
  the mission with `core::FixedTimestep` (accumulate-then-step) instead of one
  variable-`dt` update per frame, and draws every agent at
  `Agent::interpolatedPosition(clock.alpha())`. Gameplay outcomes are now
  independent of frame rate while motion stays smooth on any refresh rate
  (AGENTS.md golden rule 4). No core change — this consumes the API from #43.
  The renderer and the orders pass share one fixed step; the camera and walk
  cycle remain view-only and follow real elapsed time. Issue #44.
- **Control groups:** the squad view now supports four control-group slots.
  **Ctrl+`1`–`4`** assigns the current selection to a slot and **`1`–`4`** recalls
  it; dead members are dropped on recall and an empty group is a safe no-op.
  **`5`–`8`** select the 1st–4th living agent (the previous meaning of `1`–`4`).
  The squad panel shows each agent's group tag. View-only state; new ctest
  `game_selection` (`--headless --check-selection`) covers it. Issue #33.
- **Medikit gadget (heal a nearby ally):** a player agent can spend a medikit to
  heal the most-wounded living same-team agent (itself or a team-mate) within
  range, for a fixed amount. Kits are never wasted on a full-health target and
  never cross teams; the choice is deterministic (ties break by agent index).
  New `Agent::heal(amount)` primitive and `Mission::setMedikits` / `medikits` /
  `useMedikit`. Engine-free, unit-tested. Issue #88.
- **Agents path diagonally (smooth routes):** `Agent::moveTo` now uses 8-way A*
  with string-pulling, so agents walk straight across open ground instead of
  staircasing along the tile grid. Issue #93 (wiring fix for #99).
- **Queued-route overlay:** a selected agent's shift-click waypoint queue is now
  drawn as a yellow polyline with numbered dots, so the route built with
  shift-click is visible before it is walked. Reads `Agent::moveQueue()`; draws
  nothing when the queue is empty. View-only; new ctest `game_waypoint_path`
  (`--headless --check-waypoint-path`) covers the drawn geometry. Issue #89.
- **Complete German UI translation:** `assets/lang/de.json` now carries every
  key `en.json` does (was 7 of 23), so running with `de` shows no English
  fallbacks. Values are ASCII transliterations (`AE`/`OE`/`UE`/`SS`) because the
  embedded 5x7 font draws ASCII 32..126 only. Two `tests/test_i18n.cpp` gates
  keep locales honest: one requires each shipped locale's key set to equal
  English's, the other requires the same `{placeholder}` set per key and
  ASCII-renderable values. Issue #125.
- **Choose the UI language at startup (`--lang`):** the game now actually loads
  a locale instead of only shipping the API. `--lang de` (or `lang = de` in the
  config file; the flag wins) overlays `assets/lang/de.json` on the English
  fallback, so untranslated keys stay English. An unknown code warns on stderr
  and keeps English — it never stops the game starting. `--check-lang <code>`
  proves a locale resolved, so a missing locale file fails CI instead of
  silently passing on the fallback. New ctests `game_lang_de` and
  `game_lang_unknown_falls_back`. Issue #123.

### Changed

- **A path search now says *why* it failed.** `findPath` returned an empty vector
  for both "no route exists" and "the A* node cap was hit" (#121), so a rejected
  move order could not distinguish a genuinely unreachable target from one the
  search abandoned for frame time. The new `core::findPathDetailed` returns a
  `PathResult` whose `outcome` is `Found`, `NoPath`, or `CapExceeded`; `findPath`
  becomes a thin wrapper (`findPathDetailed(...).path`), so existing callers see
  no change. New tests cover all three outcomes, the out-of-bounds/blocked-goal
  cases, and wrapper equality. Issue #141.
- **The A* search is bounded by a node-expansion cap.** `findPath` now takes a
  `maxExpandedNodes` budget (default `kDefaultMaxExpandedNodes`, 2048) and
  returns "no path" instead of expanding the whole grid when it is exhausted, so
  a pathological or very large request cannot stall a frame on the target
  hardware (ADR-0008). The cap is a fixed node count, not a time budget, so the
  search stays deterministic, and it only ever turns a would-be path into "no
  path" — when it is not reached the result is unchanged. New tests in
  `tests/test_pathfinding.cpp` cover the worst case, determinism, and the
  unchanged-under-cap case. Issue #121.
- **Coordination is flat; the coordinator role is removed** (ADR-0019). Agents
  abort mid-cycle, and a single role that owned the merge queue, overlap
  arbitration, and the workload plan was a single point of failure. Coordination
  now derives from the eight focus-area boundaries: claims are **self-service
  leases** with a 24 h takeover (`CLAIM:`/`LEASE:`/`TAKEOVER:` lines), the queue
  is drained peer-to-peer (any agent reviews, merges, or adopts an orphaned PR),
  and overlaps are settled by the fixed rules in `docs/FOCUS_AREAS.md` §4 with
  merged code winning ties. No gameplay change. `AGENTS.md`, `docs/COORDINATION.md`,
  `docs/FOCUS_AREAS.md`, the issue/PR templates, and `CODEOWNERS` updated.
- **The recruiter gadget is renamed to "Compliance Beacon"** (ADR-0018). The
  former name was the *original 1993 game's* gadget name, which
  `docs/LEGAL.md` hard rule 4 forbids. `core::Persuadertron` →
  `core::ComplianceBeacon` (`src/core/sim/ComplianceBeacon.h`),
  `Agent::persuadertron()` → `Agent::beacon()`, and every doc reference updated.
  No behaviour change. The generic verb (`Mission::persuade`, the `[P] PERSUADE`
  hint) is unchanged. The shipped v0.5.0 tag keeps the old name (immutable,
  `AGENTS.md` §6b / ADR-0011).
- **HUD and title card read from the UI string table:** every HUD objective
  label, the squad roster row, the win/lose banner, and the title-card strings
  now come from `core::i18n::StringTable` (`assets/lang/en.json`) instead of
  hard-coded C++ literals (ADR-0016). The external file is preferred and the
  copy embedded at build time is the fallback; a missing file is not fatal —
  every lookup keeps its literal fallback, so the UI renders exactly today's
  text. New headless `--check-lang` guard (ctest `game_lang`) fails if a key the
  UI reads is missing. Issue #53.
- **Title-card codename bumped to the current release:** the subtitle now shows
  the v0.5.0 codename `RECRUITMENT` (was `SMART ENEMIES`, v0.4.0's), sourced
  from a single `kReleaseCodename` constant (mirrored in `app.tagline`) so the
  Release lane can bump it in one place each cut. Issue #48.

## [0.5.0] — 2026-10-14 — "Recruitment"

The squad is no longer four agents on a bare floor: agents are animated pixel-art
sprites, the city has props, you can press a device to turn bystanders into
followers, and a mission can declare its own win condition.

**Visibly different from v0.4.0:** the flat agent diamonds are now authored
pixel-art sprites with a walk cycle, drawn depth-sorted with crates and barrels;
a **minimap** in the top-right shows the whole board with a live blip per agent;
pressing `P` recruits nearby civilians as cyan followers that trail your squad;
and the HUD can show a `FOLLOWERS` count and objective-specific text (reach /
survive), with a new sample mission to play.

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
- **Compliance Beacon (recruit civilians):** a deterministic device in
  `src/core/sim/ComplianceBeacon.h`. `Mission::persuade` turns the nearest
  un-recruited civilian in range into a *follower* of the using agent; followers
  keep `Team::Civilian` (so they are never valid combat targets) and trail their
  leader via the existing pathfinding. `Mission::freeFollowersOf` releases them
  and `Mission::followerCount` reports them. View: `P` persuades with the
  selection, `F` frees followers, followers render cyan, and the HUD shows a
  follower count. No RNG, no effect on win/lose.
- **Fixed-timestep timing groundwork (core):** a header-only `FixedTimestep`
  accumulator (`core/math/Timestep.h`) banks frame time into whole fixed steps
  (clamping a long frame so a stall cannot cause a catch-up burst), and
  `core::Agent` gained `previousPosition()` / `interpolatedPosition(alpha)` for
  smooth rendering between steps. `position()` and `update()` keep their exact
  semantics, so deterministic mission outcomes are unchanged. Engine-free and
  unit-tested (`tests/test_timestep.cpp`).

### Changed

- **On-screen version stamp reads `v0.5.0`** — window title, HUD, and title card
  (single-sourced from the CMake project version).
- **The v0.5.0 sample mission** `assets/data/missions/escort.json` ("Rooftop
  Exfil") uses a `reach` objective, demonstrating the data-driven win conditions.

### Release

- Branch `release/v0.5.0-recruitment`, tag `v0.5.0`; per-version build, play,
  and copy-to-another-computer instructions in
  [`docs/releases/v0.5.0-recruitment.md`](docs/releases/v0.5.0-recruitment.md).

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
