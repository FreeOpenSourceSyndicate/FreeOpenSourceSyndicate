# Free Open Source Syndicate

An open-source recreation of the classic 1993 DOS game **Syndicate**, inspired by both the original and Syndicate Wars.

## Project Overview

**Free Open Source Syndicate** aims to recreate the gameplay, mechanics, and aesthetic of the original 1993 Syndicate game:
- **Real-time tactical squad-based gameplay** with 4 cyborg agents
- **Isometric perspective** rendering of dystopian urban environments
- **Mission-driven campaign** across a globe divided by mega-corporations
- **Agent customization & upgrades** (weapons, cybernetics, abilities)
- **Syndicate management metagame** (research, finance, territory control)
- **Environmental destruction** and tactical options

## Technology Stack

- **Engine:** Cocos2d-x (C++)
- **Build System:** CMake + helper scripts
- **Language:** C++17
- **Target Platforms:** Linux, macOS, Windows
- **License:** MIT

## Setup

```bash
# Fetch the Cocos2d-x dependency
./scripts/fetch_cocos2dx.sh

# Configure and build the project
./scripts/build.sh
```

The repository now contains a minimal but real Cocos2d-x integration path, including dependency fetch scripts and platform-aware build notes.

## Project Structure

```text
FreeOpenSourceSyndicate/
├── CMakeLists.txt              # Root CMake configuration
├── build/                      # Build output (generated)
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                # Entry point
│   ├── game/
│   │   ├── GameManager.h/.cpp
│   │   ├── GameScene.h/.cpp
│   │   └── scenes/
│   ├── entities/
│   │   ├── Agent.h/.cpp
│   │   ├── Entity.h/.cpp
│   │   ├── Enemy.h/.cpp
│   │   └── NPC.h/.cpp
│   ├── systems/
│   │   ├── RenderSystem.h/.cpp
│   │   ├── InputSystem.h/.cpp
│   │   ├── PathfindingSystem.h/.cpp
│   │   ├── CombatSystem.h/.cpp
│   │   └── AISystem.h/.cpp
│   ├── world/
│   │   ├── Tilemap.h/.cpp
│   │   ├── Camera.h/.cpp
│   │   └── Environment.h/.cpp
│   ├── equipment/
│   │   ├── Weapon.h/.cpp
│   │   ├── Equipment.h/.cpp
│   │   └── Upgrades.h/.cpp
│   ├── ui/
│   │   ├── HUD.h/.cpp
│   │   ├── Menu.h/.cpp
│   │   └── Widgets.h/.cpp
│   └── utils/
│       ├── Math.h/.cpp
│       ├── Config.h/.cpp
│       └── Logger.h/.cpp
├── assets/
│   ├── sprites/
│   ├── tilesets/
│   ├── ui/
│   ├── sounds/
│   ├── music/
│   └── maps/
├── docs/
│   ├── BUILD.md
│   ├── ARCHITECTURE.md
│   ├── GAMEPLAY.md
│   └── CONTRIBUTING.md
├── scripts/
│   ├── fetch_cocos2dx.sh
│   └── build.sh
├── tests/
│   ├── CMakeLists.txt
│   └── unit_tests.cpp
└── third_party/
    └── cocos2d-x/             # downloaded engine dependency
```

## Roadmap

### Phase 1: Foundation (Current)
- [x] Repository foundation
- [x] Build documentation for Linux/macOS/Windows
- [x] Cocos2d-x dependency fetch script
- [ ] Full Cocos2d-x engine integration
- [ ] Isometric rendering system
- [ ] Basic agent movement

## License

MIT License
