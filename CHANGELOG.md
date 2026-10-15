<!-- AI/LLM-generated file. Do not treat as human-written source documentation. -->

# Changelog

All notable changes to FOSSynd. This project is an
independent, from-scratch recreation of the *gameplay* of 1993's Syndicate; it
contains no original assets, code, names, or trademarks.

## [Unreleased]

### Added

- **Windows packaging lane (CPack `ZIP`):** a `WIN32`-guarded generator built
  from the existing `install()` rules ships `FOSSynd-<version>-Windows.zip`
  (with `SDL2.dll` beside `FOSSynd.exe`), and a `workflow_dispatch`-gated,
  non-blocking CI lane uses the MSYS2 UCRT64 toolchain. Linux stays the only hard
  gate (ADR-0017). Issue #133.
- **Integer zoom levels.** The world view now zooms in whole steps (1x/2x/3x)
  with **`+`**/**`-`** or the **mouse wheel** (up = in, down = out), nearest-
  neighbour so the pixel-art look holds. The camera, picking, box-select and
  order code stay in unzoomed surface pixels and the zoom is applied only at the
  screen boundary, so clicks and orders map correctly at every level; the HUD and
  minimap stay screen-fixed, and a `--zoom <n>` flag sets the initial level for
  captures (out-of-range values clamp). New ctest `game_zoom`
  (`--headless --check-zoom`) covers the round-trip and clamping. Issue #122.

## [0.6.0] — 2026-10-15 — "Firestorm"

The battlefield now has walls that matter and weapons that hurt more than one
agent. Sight and fire are blocked by walls, the catalogue gained an explosive
`launcher` (area blast) plus a minigun, flamethrower, and laser, and gadgets let
you heal allies and recruit bystanders. Squad control is smoother — drag a
rectangle to select, shift-click to queue a route that is drawn on screen — and
the project now ships as a real package for Linux (`.deb` / `.tar.gz`) and macOS
(`.dmg`), with a German UI and a discoverable CLI (`--version` / `--help`).

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
- **Area-of-effect weapons:** `core::Weapon` gained an optional `areaRadius`
  (tiles, default `0` = single-target, so existing weapons and aggregate
  initialisers are unchanged). On a successful hit with `areaRadius > 0`,
  `Mission::resolveFire` damages every other living agent within the blast
  (the primary target is hit once), and kill counters stay correct. Parsed from
  the weapon JSON. Issue #79.
- **An explosive weapon in the catalogue (area blast):** `assets/data/weapons.json`
  ships a `launcher` (`areaRadius` 2.0) and `agents.json` a `gunner` template
  that carries it, so the AoE support is usable by data alone — no C++ change.
  Issue #90.
- **Next weapon tier — minigun, flamethrower, laser:** three new original
  weapons in the catalogue, each a distinct niche — minigun (highest rate of
  fire), flamethrower (short range, high damage), laser (longest range, highest
  per-shot damage). Data only; no C++ change. Issue #124.
- **Line-of-sight primitive:** `core::hasLineOfSight(grid, from, to)` is a
  deterministic, SDL-free supercover walk that visits every tile a sight line
  touches, so a one-tile wall cannot be slipped between two points and a
  diagonal cannot thread a one-tile corner. RNG-free. Issue #59.
- **Shared weapon/agent catalogues referenced by id:** `assets/data/weapons.json`
  and `assets/data/agents.json` (loaded by `core/data/Catalogues.h`) let a
  mission reference weapons and agent templates by `weaponId` / `agentId`
  instead of duplicating them; a missing catalogue is not an error, so the game
  still starts with no data files. Extends the ADR-0003 schema. Issue #34.
- **Content validation for mission references:** `core/data/ContentValidation.h`
  resolves every mission's `weaponId` / `agentId` against the catalogues and
  reports readable problems (missing ids, duplicate ids, malformed missions), so
  bad content is caught in CI instead of at play time. Issue #71.
- **`--check-content` CLI flag:** runs the validator on a data directory
  (default `assets/data`) before SDL init and exits non-zero on any dangling
  reference, so packagers and CI can check shipped content headlessly. New
  `game_content` ctest. Issue #81.
- **`--version` and `--help`:** `--version`/`-V` prints `FOSSynd <version>` from
  the single-sourced `FOSSYND_VERSION` define (so a version bump changes it
  automatically); `--help`/`-h` prints a grouped usage block of every flag. Both
  print and exit 0 before any config lookup or SDL init. Issues #146/#147.
- **`--window WxH`:** choose the window resolution at startup (the default is
  unchanged), so the game can run at a size that suits the display. Issue #152.
- **Box-select by dragging:** hold the left button and drag to select every
  player agent inside the rectangle (shift-drag adds); a press that does not
  pass a small threshold is still a click, so click / shift-click / attack
  behaviour is unchanged. A translucent marquee is drawn while dragging. New
  `game_box_select` ctest. Issue #94.
- **Per-agent waypoint queue:** `Mission::queueMove` / `clearMoveQueue` /
  `moveQueueSize` let the view append move orders that execute in order;
  `orderMove`/`orderAttack` clear the queue, so a plain move or an attack
  supersedes a queued route. Deterministic and SDL-free. Issue #42.
- **Shift-click appends to the waypoint queue:** a plain ground click replaces
  the order; shift-click appends the tile to each selected agent's queue, so a
  squad can be walked along a path. New `game_waypoints` ctest. Issue #75.
- **Locale resolution + a partial German stub:** `StringTable::loadForLocale`
  resolves `assets/lang/<locale>.json`, overlays it on the English fallback
  per-key (a missing key keeps English, never the raw key), and an unknown
  locale warns but still yields English. Engine-free. Issue #95.
- **CPack packaging (Linux `.tar.gz` + `.deb`):** built from the existing
  `install()` rules so the layout stays single-sourced and versioned from
  `project()`; installs `LICENSE` and `README` under `share/doc/fossynd`. New
  packaging CI lane builds both artifacts and runs the packaged binary headless
  (ADR-0017). Issue #58.
- **macOS packaging lane (CPack DragNDrop `.dmg`):** appends the DragNDrop
  generator on Apple (Linux is untouched) and adds a packaging job that verifies
  the archive ships the binary, catalogues, every locale, and every mission, then
  runs the packaged binary headless. Issue #118.

### Changed

- **Walls block sight and fire:** the combat AI now uses `core::hasLineOfSight`
  so a wall stops an enemy acquiring, engaging, alerting allies about, or
  shooting a target behind it; `nearestEnemy` skips candidates with no clear
  line, and `resolveFire`/`alertAllies` reject a blocked line even on an explicit
  order. Some deterministic mission outcomes move as a result. Issue #63.
- **The recruiter gadget is renamed to "Compliance Beacon"** (ADR-0018). The
  former name was the *original 1993 game's* gadget name, which `docs/LEGAL.md`
  hard rule 4 forbids. `core::Persuadertron` → `core::ComplianceBeacon`,
  `Agent::persuadertron()` → `Agent::beacon()`, and every doc reference updated.
  No behaviour change. The shipped v0.5.0 tag keeps the old name (immutable).
- **The A* search is bounded by a node-expansion cap** (default 2048), so a
  pathological or very large request cannot stall a frame on the target hardware
  (ADR-0008). The cap is a fixed node count, so the search stays deterministic,
  and it only ever turns a would-be path into "no path". Issue #121.
- **A path search now says *why* it failed:** `findPath` returned an empty vector
  for both "no route exists" and "the A* node cap was hit". The new
  `core::findPathDetailed` returns a `PathResult` whose outcome is `Found`,
  `NoPath`, or `CapExceeded`; `findPath` is a thin wrapper, so existing callers
  see no change. Issue #141.
- **`Game.cpp` split into `Hud.cpp` + `TitleCard.cpp`** with a shared
  `HudStyle.h`, so future engine work stops growing one file. No behaviour
  change (Game.cpp 1709 → 1384 lines). Issue #69.
- **HUD objective line is tinted by state and kept on-screen:** urgent-red when
  the objective is near deciding (survive ≤5 s left, eliminate ≤1 enemy left),
  otherwise the dim HUD colour. Also fixes a latent layout bug that drew the
  objective off-screen at the default width. Issue #72.
- **Coordination is flat; the coordinator role is removed** (ADR-0019): claims
  are self-service leases with a 24 h takeover (`CLAIM:`/`LEASE:`/`TAKEOVER:`
  lines), the queue is drained peer-to-peer, and overlaps are settled by the
  fixed rules in `docs/FOCUS_AREAS.md` §4. No gameplay change. Issue #114.
- **The package ships all shipped data:** install every `assets/lang/*.json`,
  every `assets/data/missions/*.json`, and the shared catalogues, and validate
  the installed content in CI, so a locale, mission, or catalogue added to the
  tree cannot silently fail to ship. Issues #91/#103/#126.
- **The macOS packaging lane is gated to avoid reddening `main`:** it runs on
  `pull_request` and manual dispatch only and is `continue-on-error`, so a macOS
  runner shortage is neutral rather than a hard failure. Issue #136.
- **On-screen version stamp reads `v0.6.0`** and the title-card codename is
  `FIRESTORM` — window title, HUD, and title card (single-sourced from the CMake
  project version and `kReleaseCodename`).

### Release

- Branch `release/v0.6.0-firestorm`, tag `v0.6.0`; per-version build, play, and
  copy-to-another-computer instructions in
  [`docs/releases/v0.6.0-firestorm.md`](docs/releases/v0.6.0-firestorm.md).

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
