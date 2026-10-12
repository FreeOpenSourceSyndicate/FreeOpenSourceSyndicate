<!-- AI/LLM-generated file. Do not treat as human-written source documentation. This comment must be preserved in future generated files. -->
# Free Open Source Syndicate

An open-source recreation of the *gameplay* of the classic 1993 DOS game
**Syndicate**, inspired by both the original and Syndicate Wars. We build
everything from scratch — no original assets, code, names, or trademarks.

> **Contributing with an AI agent?** Read [`AGENTS.md`](AGENTS.md) first. It is
> the shared rulebook for all AI (and human) contributors.

## Project Overview

**Free Open Source Syndicate** aims to recreate the gameplay, mechanics, and aesthetic of the original 1993 Syndicate game:
- **Real-time tactical squad-based gameplay** with 4 cyborg agents
- **Isometric perspective** rendering of dystopian urban environments
- **Mission-driven campaign** across a globe divided by mega-corporations
- **Agent customization & upgrades** (weapons, cybernetics, abilities)
- **Syndicate management metagame** (research, finance, territory control)
- **Environmental destruction** and tactical options

## Technology Stack

- **Language:** C++17
- **Game framework:** Cocos2d-x v4 (provisional — see
  [`docs/DECISIONS.md`](docs/DECISIONS.md) ADR-0001)
- **Build system:** CMake ≥ 3.15 + helper scripts
- **Target platforms:** Linux, macOS, Windows
- **License:** MIT

## Architecture in one paragraph

Game logic is **engine-agnostic** and lives in `src/core/`. It never includes
engine headers and is fully unit-tested with a plain compiler, which is what CI
builds. The engine layer (`src/game/`, `src/world/`, `src/entities/`) is a thin
adapter that draws core state and handles input. This separation lets many
contributors (human and AI) work in parallel without stepping on each other.
See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Building

### Core + tests (fast, no engine needed — this is what CI runs)

```bash
cmake -S . -B build -DFOSS_BUILD_GAME=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Full game (requires the Cocos2d-x engine)

```bash
./scripts/fetch_cocos2dx.sh        # one-time, large download
cmake -S . -B build -DFOSS_BUILD_GAME=ON
cmake --build build -j
```

Platform-specific setup guides are in [`docs/SETUP_GUIDE.md`](docs/SETUP_GUIDE.md).

## Project Structure

```text
FreeOpenSourceSyndicate/
├── AGENTS.md              # shared rulebook for AI/human contributors
├── CMakeLists.txt
├── assets/                # original/licensed assets (see docs/ASSETS.md)
├── docs/
│   ├── ARCHITECTURE.md    # design overview
│   ├── COORDINATION.md    # how multiple agents work together
│   ├── ROADMAP.md         # phased plan
│   ├── DECISIONS.md       # architecture decision records (ADRs)
│   ├── LEGAL.md           # copyright policy
│   ├── ASSETS.md          # asset registry & provenance
│   └── SETUP_*.md         # per-platform setup
├── scripts/               # bootstrap / fetch / build helpers
├── src/
│   ├── core/              # ENGINE-AGNOSTIC logic (no cocos2d includes)
│   │   ├── math/          # Vec2, isometric transforms
│   │   └── world/         # tile grid, A* pathfinding
│   ├── game/              # Cocos2d-x app shell
│   ├── entities/          # engine-side entity views
│   └── world/             # engine-side map rendering
├── tests/                 # unit tests for src/core (run via ctest)
└── third_party/           # fetched dependencies (git-ignored)
```

## Roadmap

See [`docs/ROADMAP.md`](docs/ROADMAP.md). Current focus: **Phase 1 — the
headless simulation core** (math, world, pathfinding, then agents/combat/AI),
all developed and tested without the engine.

## Contributing

See [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md) and
[`docs/COORDINATION.md`](docs/COORDINATION.md). In short:

1. Find or open an issue; claim it.
2. Branch (`feat/…`, `fix/…`, …), make a small change.
3. Build and test the core locally; keep the build warning-free.
4. Open a PR using the template, including the handoff block and the model used.

## License

MIT License. See [`LICENSE`](LICENSE) and [`docs/LEGAL.md`](docs/LEGAL.md).
