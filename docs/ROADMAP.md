<!--
AI/LLM-generated file. Do not treat as human-written source documentation.
This comment must be preserved in future generated files.
-->
# Roadmap

Goal: a playable, open-source tactical game that captures the *feel* of
Syndicate (1993) and Syndicate Wars — four cyborg agents, isometric city
missions, weapons and cybernetics, a corporation-management metagame — built
entirely from original code and assets.

Phases are ordered so that **logic is built and tested headless before it is
rendered**. That keeps the project parallelizable across many agents.

## Phase 0 — Foundation (mostly done)

- [x] Repo, license, docs, build scripts
- [x] Cocos2d-x bootstrap scaffold
- [x] Prototype isometric scene + squad stub
- [x] `AGENTS.md` coordination rules
- [ ] Decide engine (ADR-0001) and lock it
- [ ] CI green on `main`
- [ ] Core/sim test harness in place

## Phase 1 — Headless simulation core

The heart of the game, engine-independent and unit-tested.

- [ ] `core/math`: `Vec2`, isometric ↔ world transforms, fixed-timestep loop
- [ ] `core/world`: tile grid model, walkability, tile properties
- [ ] `core/world`: A* pathfinding on the grid
- [ ] `core/sim`: entity model (agents, civilians, vehicles, structures)
- [ ] `core/sim`: movement/steering along paths
- [ ] `core/sim`: squad model (4 agents, selection, orders)
- [ ] `core/sim`: combat (hitscan/projectile, damage, hit chance)
- [ ] `core/sim`: vision/fog, line-of-sight
- [ ] Deterministic replay of a scripted scenario in a test

**Deliverable:** a test that runs a tiny mission entirely headless and asserts
outcomes (agent A kills agent B, reaches tile X, etc.).

## Phase 2 — Render the simulation

- [ ] Engine layer consumes `core` state and draws it
- [ ] Isometric tile renderer with depth sorting
- [ ] Agent sprites + animation driven by sim state
- [ ] Camera: pan, zoom, follow squad
- [ ] Input: point-to-move, click-to-select, squad commands
- [ ] HUD: agent portraits, health, weapon, minimap
- [ ] Interpolated rendering over fixed-step sim

**Deliverable:** the Phase 1 scenario playable with mouse/keyboard.

## Phase 3 — Game systems

- [ ] Weapons: pistol, uzi, minigun, flamethrower, rocket launcher, laser
- [ ] Gadgets: Persuadertron, shield, medikit, scanner
- [ ] Cybernetic upgrades (speed, accuracy, health, perception)
- [ ] Vehicles: cars/trains, hijacking, mounted weapons
- [ ] Destructible environment
- [ ] Enemy AI: patrol, engage, retreat, call backup
- [ ] Civilian/persuasion behaviour
- [ ] Mission objectives + win/lose

## Phase 4 — Metagame

- [ ] World map with territories
- [ ] Research tree and R&D time
- [ ] Economy: income from territories, agent/equipment costs
- [ ] Mission selection and deployment
- [ ] Campaign progression

## Phase 5 — Content, polish, tooling

- [ ] Mission editor / data pipeline
- [ ] Original art and audio (`docs/ASSETS.md`)
- [ ] Save/load
- [ ] Localization scaffolding
- [ ] Modding / scripting layer (ADR-0004)
- [ ] Packaging for Linux/macOS/Windows

## Cross-cutting (every phase)

- Tests for all `core` logic
- No new build warnings
- ADRs for architectural decisions
- Legal hygiene: original assets only
