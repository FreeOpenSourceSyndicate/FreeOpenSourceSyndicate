<!-- AI/LLM-generated file. Do not treat as human-written source documentation. -->

# Changelog

All notable changes to FOSSynd. This project is an
independent, from-scratch recreation of the *gameplay* of 1993's Syndicate; it
contains no original assets, code, names, or trademarks.

## [Unreleased]

### Added

- **Explosions breach destructible walls.** Weapon fire could already open a
  cracked wall, but only the single tile a *blocked* round landed on: a rocket
  aimed at a defender in the open left every wall around the impact standing,
  and a blast never touched walls at all. `Mission::applyAreaDamage` now calls a
  new `breachTilesInBlast`, which chips every destructible tile whose centre is
  within the blast radius and opens the ones whose hit points run out — so the
  launcher (and any future area weapon) breaches a room the way a point shot
  chips one tile. A vehicle wreck's detonation breaches walls in its own blast
  too, and a solid wall is still unbreakable. Engine-free and deterministic
  (no RNG); `focus: tactical`. Issue #411.

### Changed

- **The low-spec binary-size budget rose to 352 KiB** (from 336 KiB): `main` had
  reached 334 KiB, leaving only ~2 KiB of headroom — less than one 4 KiB ELF page
  step — so the next real feature PR (the time bomb, #449) failed spuriously
  rather than on a genuine regression (issue #468). Same remedy as the
  288 → 320 KiB (#312/#356) and 320 → 336 KiB (#413) raises; the guard still fails
  loudly if the low-spec flags are dropped.
- **A major version is now gated behind a feature-complete, human-approved
  candidate.** `AGENTS.md` §6a defined how to cut *minor* releases (`0.x.0`) but
  said nothing about a **major** version (`X.0.0`), so `v1.0.0` could in
  principle have been tagged the moment CI went green — with no build that
  implemented the whole v1.0.0 scope, and no human ever having played it. §6a-i
  now requires, before any major version rolls out: a **release candidate** that
  implements the entire Definition of Done (no carve-outs), a build a **human can
  build and play** from the candidate's branch/tag, and **human approval**
  recorded via [`docs/releases/MAJOR_RELEASE_GATE.md`](docs/releases/MAJOR_RELEASE_GATE.md).
  The candidate is tagged as a pre-release (`v1.0.0-rc.N`, immutable under §6b);
  the final tag is cut only on approval. The gate is **human-owned** — an agent
  never self-approves a major version. Minor releases are unchanged. Recorded in
  **ADR-0036**; see also `docs/ROADMAP.md` (Milestone C) and `docs/FOCUS_AREAS.md`
  (release remit). Issue #402.

## [0.8.0] — 2026-10-16 — "Dominion"

The war now has a map. Above the tactical board sits a **management screen**: a
world of territories you tax for income, a research tree you fund, an equipment
shop you buy from, and a campaign you win by taking every region. Deploying sends
the squad you *bought* into the mission you *picked*, and the result feeds back —
win the map or lose it, and the end state is final. Milestone B (the metagame)
lands in full, alongside the bound gadget keys, the complete mission-objective
set, and breached-tile rubble.

### Added

- **The shop's `longshot`/`railgun` now exist as weapons, and dangling catalogue
  references fail CI:** `assets/data/world/equipment.json` sold a "Longshot
  Rifle" and a "Railgun" (and `research.json` gated them), but neither id was in
  the shared `weapons.json`, so buying either made `applyLoadout` reject the
  **whole** squad loadout at deploy — the player paid the research cost and the
  price, then lost every weapon and gadget for the mission. Both are now real
  catalogue entries (`longshot` 15/14/2/0.9, `railgun` 34/13/0.9/0.9, inside the
  catalogue DPS band). `content::validateContentDir` now cross-checks
  `assets/data/world/*.json`: every `kind: "weapon"` equipment id must be in
  `weapons.json`, and every research `unlocks` must be an equipment id, so this
  class of dangling reference fails `--check-content` in CI instead of shipping.
  Issue #369.

- **Hijackable vehicles reach play:** `Mission` now owns vehicle policy for the
  `core::Vehicle` entity (`focus: world`, #320), so cars and trains are actually
  reachable in a mission. `Mission::addVehicle`/`vehicles()` track them;
  `orderHijack` lets a living combatant within `kHijackRadius` take the wheel
  (its position then rides with the vehicle), `ejectVehicle` puts the driver out,
  and `orderDrive` drives an occupied vehicle via the entity's A* movement.
  Mounted fire (`orderVehicleAttack` or auto-acquire) uses the same range and
  line-of-sight rules as an agent and funnels damage through
  `Mission::damageAgent`/`applyAreaDamage`, so a shield still covers the hit; a
  driver fires only the mounted gun (no personal weapon while at the wheel). An
  explosive blast also wrecks vehicles, and a wreck detonates, hurting nearby
  agents — a car is cover that can backfire. Engine-free, deterministic
  (ADR-0032). Issue #318.

- **Hijackable vehicles are drawn.** The view now renders a live vehicle and its
  wreck in the same depth-sorted pass as agents and props, at their interpolated
  position, so an agent beside a car overlaps correctly; a destroyed vehicle is
  still drawn as a wreck because a wreck stays on the board. The art is authored
  text baked with a neutral steel palette, so a hijacked vehicle does not
  recolour with its driver. No mission ships a vehicle yet, so `--vehicle` spawns
  a demo car and train for a headless capture and `--check-vehicles` (ctest
  `game_vehicles`) drives the real draw decision. Issue #366.

- **Cybernetic upgrades are purchasable and deploy:** the tactical side of
  cybernetics landed in #313 (`CyberneticEffect`, `Mission::applyCybernetic`, a
  data-only `LoadoutSlot::cyberneticId`), but the metagame had no catalogue or
  research entries for `legs`/`arms`/`chest`/`eyes`, and
  `Campaign::selectedLoadout` **skipped** `EquipmentKind::Cybernetic` — so a
  cybernetic could be applied by `applyLoadout` yet had no in-game path. The four
  upgrades now ship in `assets/data/world/equipment.json` gated by matching
  `research.json` nodes, and `selectedLoadout` maps a bought cybernetic to the
  slot's `cyberneticId` (one per slot, like a weapon/gadget), so a purchased
  upgrade reaches the deployed agent for a permanent stat change. The
  shipped-catalogue guard now checks that every sold cybernetic id is in the
  tactical deploy vocabulary and that its research node unlocks it
  (`applyLoadout` rejects an unknown id), so the two ends cannot drift. Issue
  #341.

- **Buying an upgrade actually deploys it:** the R&D shop used to `assign` a
  purchase to the target slot, but a slot deploys at most one weapon and one
  gadget (`Campaign::selectedLoadout` reads the first of each kind), so buying a
  *second* weapon — the natural upgrade from the free pistol to a minigun — spent
  the funds and then silently dropped the purchase at deploy time. The new
  `Equipment::equip` replaces the slot's same-kind item (keeping the other kind),
  and the shop uses it, skips the charge when the item is already equipped, and
  says in the action line what it will replace (`REPLACE Pistol`) or that it is
  already equipped. The `game_campaign` guard buys the minigun over a pistol
  through the player keys and asserts the minigun is what deploys. Issue #346.

- **Tactical gadget hotkeys:** the medikit, shield, scanner, and performance
  drugs are now bound to **`H`**, **`S`**, **`C`**, and **`B`** on the tactical
  board, each acting on the current selection (`Game::useMedkitOnSelection` et
  al.). The four gadgets existed in `core` and reached a mission through a bought
  loadout, but there was no key to use them in play — this is the player path.
  Covered by the `game_gadgets` guard, which drives each key and checks the
  effect. Issue #339.

- **Cybernetic upgrades:** the last Milestone A tactical item lands in `core`,
  alongside the gadgets. `CyberneticEffect` (`core/sim/Cybernetic.h`) maps a
  `kind: "cybernetic"` id to permanent stat **multipliers** applied once at
  mission load by `Mission::applyCybernetic`: `legs` (movement speed), `arms`
  (weapon accuracy, clamped so a shot is never certain), `chest` (maximum
  health, filled as a clean gain), `eyes` (sight range / perception), and the
  already-shipped `targeting-implant` (accuracy). Perception is the per-agent
  `Agent::sightRange()` that `seesTarget` reads (extended by the eyes upgrade),
  so an eyes upgrade measurably extends a combatant's engagement range.
  `LoadoutSlot` gained a data-only `cyberneticId`, so a bought upgrade deploys
  through the same strict path as a weapon/gadget (an unknown id is a content
  error). Engine-free, deterministic (ADR-0031). Issue #313.

- **The equipment catalogue sells the scanner and performance drugs:** it now
  lists the two gadgets #314 added to `core`, so a player can actually buy and
  equip them from the R&D shop and they reach a deployed squad (they were
  otherwise unreachable — no catalogue entry). No code change: the deploy
  vocabulary (`gadgetChargesForId`) already accepted both ids. The `Equipment`
  shipped-catalogue guard now checks that the catalogue lists `scanner`/`drugs`
  and that *every* `kind: "gadget"` item is in the deploy vocabulary, so a
  future gadget that is sold but cannot deploy fails CI. #324.

- **The R&D shop (research and purchase in play):** `B` on the campaign map
  opens a shop listing the research tree and the equipment catalogue — Up/Down to
  move, Left/Right to pick the squad slot, Enter to fund a node or buy gear and
  assign it. This is the *player* path for the middle of the loop
  (tax → research → buy → deploy); before it, research and purchase were
  reachable only from the headless self-test, so Milestone B's "the whole loop
  works without a config file or debug flag" was not actually true. The shop is
  modal while open and refuses to open once the campaign is finished; every rule
  (cost, prerequisites, research gate, funds) stays in `core/economy/**`. The
  `game_campaign` guard now drives the shop through the player keys and draws it.
  Issue #249.

- **The campaign end state is terminal:** once the campaign is won or lost,
  `Campaign::finished()` freezes the board — `canDeployTo`, `recordResult`, and
  `tick` are no-ops, and the map screen refuses to deploy or change tax and draws
  a centred victory/defeat panel (headline, held-territory count, quit hint) over
  the frozen map. Before this the end state was cosmetic: a finished campaign
  kept ticking and could still be deployed into. Issue #249.

- **Shared vehicle catalogue (`assets/data/vehicles.json`):** vehicle kinds are
  now data, not code (golden rule 7, ADR-0003). A `vehicles.json` catalogue adds
  a `VehicleCatalogue`/`VehicleSpec` in `core/data`, mirroring
  `weapons.json`/`agents.json`: each entry sets `name`, `kind` (`car`/`train`),
  `speed`, `health`, and a mounted `weapon`/`weaponId`. A mission places vehicles
  with an optional `vehicles` array, inheriting a template by `vehicleId` or
  inlining one; the loader range-checks the fields and rejects an unknown id,
  unknown kind, or an off-map/missing position, and `--check-content` validates
  the shipped catalogue. The code defaults (`makeCar`/`makeTrain`) match the
  catalogue's `car`/`train`, pinned by a test so the two cannot drift. Issue #319.

- **The metagame: a world map, an economy, research, equipment, and a campaign.**
  Milestone B's five engine-free systems land in `src/core/economy/` (ADR-0002),
  data-driven from `assets/data/world/**` (ADR-0003):

  - **World map with territories** — a validated territory graph with dense ids,
    symmetric adjacency, and no self-loops; ownership queries and `setOwner` for
    progression; a shipped six-territory `world.json` (issue #249).
  - **Economy: income, taxation, and revolt** — a per-syndicate funds ledger with
    a fixed-step tick that pays income from owned territories scaled by tax
    level, `spend`/`canAfford`, and a revolt check that flips an over-taxed
    territory back to neutral. Deterministic: the only randomness is the injected
    `core::Rng` (issue #249).
  - **Research tree, progress, and funding** — `assets/data/world/research.json`
    (nodes with cost, time, prerequisites, unlocks) validated for unique ids,
    resolvable prerequisites, and no prerequisite cycles; `Research::begin` gates
    on availability, affordability, and idleness, and `tick` advances and
    completes work (issue #249).
  - **Equipment catalogue, purchase, and loadouts** — `equipment.json` items
    (`weapon`/`gadget`/`cybernetic`) with a research gate; `buy` checks the gate
    then funds and spends through the economy; per-syndicate four-slot squad
    loadouts (issue #249).
  - **Campaign composition and progression** — the top-level `Campaign` owns the
    world, economy, research, and equipment; `selectMission` gates on ownership
    and a valid slot list, `recordResult` claims on a win and evaluates the
    `own-all-territories` win condition, and `tick` advances the economy and
    research. `loadFromFile` grants the parsed `startingFunds` (issue #249).

- **The campaign management screen.** The core systems were merged but nothing in
  `src/game/` drove them, so a player could never reach the loop. A new
  `game::CampaignScreen` (its own translation unit) renders the world map and
  reads input; every rule stays in `core/economy/**`. `M` toggles the screen,
  click selects a territory, `-`/`+` change its tax, and `D` deploys. While the
  screen is up the tactical sim is frozen and the campaign clock ticks instead.
  `--campaign <path>` picks a campaign file and `--map` starts on the screen
  (issue #249, #289).

- **Deploy a mission from the map, and feed the result back.** Deploying starts
  the selected territory's mission on the tactical board with the *bought* squad
  applied, and the mission's win/lose is recorded on the campaign so the map
  advances — the loop closes (issue #312).

- **The campaign can be won (frontier attacks):** a region is now a deploy
  target when the player owns it **or holds an adjacent region**
  (`Campaign::canDeployTo`), and a win claims it, so ownership spreads inward
  from the border. Before this, only the one already-owned territory had a
  `missionId`, so `own-all-territories` was unreachable and the campaign could
  never progress — Milestone B's "reach an end state" step was impossible.
  A `missionId` may now be shared by several regions (the original reuses city
  maps), and the map screen shows `ATTACK READY` / `NOT REACHABLE`; the
  `game_campaign` guard wins by attacking, so unwinnable data fails CI instead
  of a playtest (ADR-0029). Issue #249.

- **Scanner and performance-drug gadgets:** the last two Milestone A gadgets
  land in `core`, alongside the medikit/shield/recruiter. A **scanner**
  (`Mission::activateScanner`/`scanTargets`) spends a charge to mark living
  enemies within a radius for a few seconds; the reveal is presentation-only, so
  it changes no combat outcome and the sim never reads it. **Performance drugs**
  (`Mission::useDrugs`/`drugsTimer`) spend a dose for a timed buff that raises
  movement speed and hit chance, then wears off exactly. Both keep their state on
  the `Mission` (no `Agent` change), are no-ops for a civilian/dead agent/already
  active, and are deterministic (no RNG). The loadout vocabulary gains
  `"scanner"`/`"drugs"`, so a bought gadget deploys through the same path as the
  medkit/shield (ADR-0028). Issue #314.

- **Squad loadout at mission load (deployment, R1):** a mission can now take a
  `core::PlayerLoadout` — per player slot, a weapon id and/or a gadget id — so a
  bought squad reaches the deployed agents instead of the mission's authored
  defaults. `core::applyLoadout` (in `core/data/MissionData`) resolves the ids
  against the shared `WeaponCatalogue` and the tactical gadget vocabulary
  (`medkit`/`shield`); an empty id keeps the authored default and an unknown id
  is a content error, not a silent fallback. Slot `i` maps to the i-th player
  agent, and a slot past the squad is ignored. Engine-free, data-only, and
  unit-tested (ADR-0027). The metagame's engine-side deploy wiring is issue #312;
  this is its frozen interface. Issue #311.

- **Complete mission objective set:** `ObjectiveSpec` now supports the full 1993
  set beyond eliminate/reach/survive — **escort** (a named agent must reach a
  tile alive; losing it loses the mission), **destroy** (breach a destructible
  tile — the win is the breach, not the defenders), and **persuade** (recruit a
  target number of ComplianceBeacon followers). All three are declared in mission
  JSON (`docs/DATA_FORMAT.md`) and evaluated in `core`
  (`Mission::evaluateOutcome`); the existing kinds and every existing mission are
  unchanged. `assets/data/missions/escort.json` is now the first escort mission
  (escort DELTA to the exfil tile alive), and the HUD banner reads a per-kind
  string. Issue #281.

- **More weapon families (content-only):** `weapons.json` gains **shotgun**
  (close, heavy, slow), **sniper** (longest range, hardest single shot, slowest),
  and **gauss** (mid-range, high-rate, accurate slug with low per-shot damage as
  its compensating weakness) — original numbers, no engine change, so designers
  can tune them without recompiling (golden rule 7).
  New enemy carriers `breacher`/`sharpshooter`/`shocktrooper` reference them by
  id, so the weapons are mission-referenceable rather than inert. Guarded by
  `tests/test_content_validation.cpp`; `docs/DATA_FORMAT.md` updated. Issue #219.

- **Breached tiles now render as rubble.** A destructible wall that has been
  opened by fire is drawn as rubble instead of a plain floor cell, so the breach
  a player makes is visible on the board (issue #218, part 1).

- **`core::Vehicle` (hijackable cars and trains).** The engine-free entity the
  vehicle feature builds on: a car or train with fractional tile position, the
  same 8-way A* movement and render interpolation as `Agent`, health and
  destruction (a wreck cannot be driven or fired from), single-agent occupancy,
  and a mounted weapon. Policy-free — legality and damage belong to `Mission`.
  Mission integration, the view, and the data catalogue are follow-ups
  (issue #315, #320).

### Changed

- **Release codename** for this snapshot is `DOMINION` (the version stamp and the
  title-card tagline follow it).
- **The low-spec binary-size budget rose to 320 KiB** (from 256 KiB): the 256 KiB
  cap had under 2 KiB of headroom on `main`, so any real feature busted it. The
  metagame deploy wiring added ~8 KiB of engine code and needed the room, and the
  third shared catalogue (`assets/data/vehicles.json`, #319/#342) left only
  ~2 KiB again, so the cap was raised once more (issue #312, #356); the guard
  still fails loudly if the low-spec flags are dropped.
- **`--config`/`--campaign` fail fast** when an explicitly named file is missing
  (a non-zero exit and a stderr message), rather than starting with defaults
  (issue #248, #259). Unknown or incomplete options are likewise rejected, and
  `--frames` must be a positive integer (issue #264).

### Fixed

- **Tactical hotkeys no longer leak through the campaign map.** On the map
  screen, pressing a tactical key (the gadget keys `H`/`S`/`C`/`B`, control
  groups, pause, restart) used to fall through to the board underneath and fire
  the action — so a player managing the map could silently use a medikit, pause
  the sim, or restart. The map screen now consumes those keys while it is open.
  Covered by the `game_campaign_keys` guard. Issue #368.

## [0.7.0] — 2026-10-15 — "Demolition"

The battlefield is now destructible and legible. Weapon fire opens cracked walls
so a blocked corridor becomes a route, and enemies break for cover instead of
trading shots in the open. The map is drawn from authored pixel-art tiles, the
view zooms in whole steps and goes fullscreen, and the mission can be paused.
Missions are localised by a stable id — the mission name shows in the HUD and the
window title — and the content loader now rejects off-map geometry.

### Added

- **Shield gadget:** a combatant can spend a shield charge to raise a shield
  that absorbs the next incoming hit whole and then drops (the following hit
  lands normally). Ordinary, explosive-area, and stray damage all pass through
  the shield. Charges and the raised flag live on `Mission` (`setShields`,
  `shields`, `activateShield`, `shieldUp`) — no `core::Agent` change — and every
  damage path now funnels through `Mission::damageAgent`. Deterministic, no RNG.
  Covered by `tests/test_mission.cpp`; see ADR-0023. Not yet surfaced to the
  player (a UI binding and a content `shields` field are follow-ups). Issue #211.
- **Pixel-art ground tiles.** The map is no longer flat colour fills: each cell
  is drawn from an authored isometric tile — light/dark checkerboard floor, a
  raised wall, and a cracked destructible wall — baked from
  `assets/tiles/ground.txt` at the original chunky resolution and scaled
  nearest-neighbour into the pre-baked static map (one blit per cell, so it stays
  cheap on old hardware). If the sheet cannot be baked the map falls back to the
  previous flat diamonds. Issue #189.
- **Enemies take cover under fire:** a combatant that is being shot at now
  breaks for the nearest cover instead of trading shots in the open. While
  engaged and under fire (just hit, or the shooter is still firing), an enemy
  steps to a walkable neighbouring tile the shooter cannot see through
  (`hasLineOfSight` false) and holds there; it re-engages when the shooter loses
  the line, and slides to another covered neighbour when it can. An enemy in the
  open is unchanged, so existing missions play as before. Deterministic, no RNG
  (`Mission::findCoverTile`). Covered by `tests/test_mission.cpp`; see ADR-0022.
- **Warnings-as-errors build + CI lane.** A `FOSSYND_WERROR=ON` CMake option
  promotes the `-Wall -Wextra -Wpedantic` warnings to hard errors, and a new
  `Warnings as errors (-Werror)` CI job (core + game) fails the build on any
  warning so one cannot land on `main` (golden rule 9). OFF by default, so local
  and release builds are unchanged. Issue #192.
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
- **Git-ref coordination leases (`scripts/coord`, ADR-0021).** Coordination
  moved out of issue comments and into a **machine-checked ledger**. A claim is a
  git ref `refs/coord/leases/<area>` whose target is a JSON record with a
  TTL; taking one is `git push --force-with-lease`, an atomic compare-and-swap, so
  two agents racing for an area can never both win. `scripts/coord claim|renew|
  release|status|check|board|sweep|migrate` is the whole interface, and
  `coord check` is a **CI gate** that fails on any live collision. A scheduled
  workflow sweeps expired leases and regenerates `docs/COORDINATION_BOARD.md`, so
  a lease that nobody renews frees its area on the clock — an agent that crashes
  or pauses mid-work strands nothing and no peer or human has to notice. This
  removes the hand-maintained workload plan (and its 22 `refresh the queue` PRs),
  the hub check-ins, and the `status:`-label-as-truth rule. The lease ref is
  keyed by **area** (`refs/coord/leases/<area>`), because the ref *is* the lock:
  keying it by issue would let two agents claiming different issues in one area
  CAS different refs and both win. 34 unit tests in `tests/test_coord.py` (wired
  into `ctest` as `coord_tests`). Migration from the old `CLAIM:`/`LEASE:`
  protocol is lossless: `scripts/coord migrate` imports live legacy claims, the
  tool still reads the legacy lines during the transition, and a pre-fix client's
  old-layout ref is cleared by `sweep` or `claim --takeover`
  (`docs/COORDINATION_MIGRATION.md`).

- **Destructible walls can be breached by fire.** Weapon fire is now wired into
  the destructible-tile primitive, so a wall can actually be opened in play.
  When a combatant has a target in range but the shot line is blocked, the shot
  lands on the first blocked tile on that line: a destructible tile is chipped
  by the weapon's damage and, at zero HP, opens (walkable, decor cleared) so
  `hasLineOfSight`/`findPath` route through it on the next search. A solid wall
  is unchanged — the round is simply absorbed, exactly the old no-op. A
  destructible hit burns the normal fire cooldown, so wall damage is rate-limited
  like any other shot. New private `Mission::firstWallOnLine` walks the same
  supercover line as `hasLineOfSight` (not a plain Bresenham walk, which can
  disagree at a corner crossing). Issue #163.
- **A breached wall is remembered as rubble.** `core::TileGrid` gained a
  permanent `rubble` flag (`isRubble(x, y)`) set when a destructible tile is
  destroyed, so a breached wall is no longer indistinguishable from floor that
  was never a wall — the renderer has the signal to draw rubble instead of plain
  floor (art is a follow-up). The tile stays walkable, so pathfinding and line of
  sight are unchanged. Issue #217.
- **Fullscreen (`F11`) and a `--fullscreen` flag.** The renderer fixes a logical
  resolution and scales the frame to the display, so the drawing code and the
  pixel-art look are unchanged, and SDL maps mouse events back to logical space
  so selection stays correct when scaled. Fullscreen is a guarded no-op
  headlessly; `--check-fullscreen` exercises the toggle state machine (ctest
  `game_fullscreen`, `game_fullscreen_flag`). Issue #194.
- **Pause / resume (`Space`) with a visible cue.** A view-only pause: `Space`
  toggles it and `--pause` starts paused. While paused, the fixed-step loop does
  not run, so the mission clock banks nothing and resuming does not fire a
  catch-up burst; `R` clears a stale pause. A distinct, centred `PAUSED` band
  (localised) shows while frozen. `--headless --check-pause` (ctest `game_pause`,
  `game_pause_flag`) proves a paused update advances `Mission::elapsedSeconds()`
  by 0 while an unpaused one advances it, and that the toggle round-trips. Issue
  #210.
- **The mission name is localised and shown in-game.** Mission-authored copy
  (name/description) now resolves through the UI string table, keyed by a stable
  mission id, so a mission gains a translation by adding two JSON keys with no
  C++ or schema change (`mission.<id>.{name,desc}`; an untranslated mission keeps
  its authored name). The name is shown in the **HUD top bar** next to the
  version as well as the OS **window title**, and is dropped rather than allowed
  to overlap the objective block in a narrow window. `--check-lang <locale>`
  asserts the name resolved, so removing the wiring fails the test. New
  `core/i18n/MissionText.*` (engine-free, pure, idempotent). Issues #174, #176,
  #187.
- **Optional mission `id` for stable localisation keys.** An optional `"id"`
  string survives a file rename, where the path-derived fallback would not; a
  present-but-non-string `id` is a readable schema error rather than a silent
  empty id. Both shipped missions set it, and `docs/DATA_FORMAT.md` documents it.
  Issue #184.
- **Enemy templates carrying the weapon tiers.** Three original enemy templates —
  `SUPPRESSOR` (minigun), `PYRO` (flamethrower), `MARKSMAN` (laser) — so the
  weapon tiers are used by real content instead of being inert. A shipped-
  catalogue test pins the counts (7 weapons + 7 templates) so a tier or template
  cannot silently vanish. Issue #160.
- **Off-map mission geometry is rejected.** The loader previously accepted
  geometry outside the map and silently ignored it (clipped buildings, undrawn
  agents, an unwinnable `reach` objective) and `--check-content` still reported
  `ok`. Every position — building rectangles, blocked tiles, agent `x`/`y`, and a
  `reach` tile — is now bounded to `[0, width) x [0, height)` while parsing, and
  a violation is a readable, file-named error that fails `--check-content` in CI.
  Issue #212.
- **Weapon catalogue field ranges are validated.** A weapon with a non-positive
  damage/range/fireRate, an accuracy outside `[0,1]`, or a negative `areaRadius`
  loaded without complaint and misbehaved at play time; `parseWeaponNode` now
  range-checks every numeric field, and the catalogue and mission loaders prefix
  the offending entry's id, so a `--check-content` failure names the file, the
  entry, and the field. Issue #190.
- **Reachability primitive (`reachableTiles`/`areConnected`).** A pure,
  engine-free query in `core::world` asks "which tiles are reachable from here?"
  or "can A reach B?" without running an A* per target — what mission validation
  (is the objective walled off?) and AI reachability need. The BFS in
  `findPathToNearest` is factored into a shared `bfsReachable` helper and reused,
  so reachability and `findPath` agree exactly on the walk, the corner rule, and
  the node budget. Issue #188.
- **Locale coverage guards.** The shipped-locale parity gate now compares the
  actual **key sets**, not just the entry counts, so a locale that drops one key
  and invents a typo'd one can no longer pass; `tests/test_i18n_keys.py` also
  fails on a missing key (would show English), an invented key (a typo rendered
  raw), and a duplicate key (a copy-paste that silently shadows an earlier
  entry). Both report the offending keys. Issues #214, #220.

### Changed

- **The on-screen version stamp reads `v0.7.0`** and the title-card codename is
  **`DEMOLITION`** (German `ABBRUCH`), so you can confirm at a glance which build
  you are running (`--version`, the window title, the HUD top bar, and the title
  card are all single-sourced).
- **Coordination is now machine-checked** (ADR-0021): a claim is a git-ref lease
  with a TTL rather than a comment, so a lease that is not renewed frees its area
  on the clock and the fleet never waits on an agent that stopped mid-work.
  `scripts/coord` is the interface and `coord check` is a CI gate.

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
