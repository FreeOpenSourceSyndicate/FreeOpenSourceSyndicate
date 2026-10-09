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
- **Build System:** CMake
- **Language:** C++17
- **Target Platforms:** Linux, macOS, Windows
- **License:** MIT

## Project Structure

```
FreeOpenSourceSyndicate/
├── CMakeLists.txt              # Root CMake configuration
├── build/                      # Build output (generated)
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                # Entry point
│   ├── game/
│   │   ├── GameManager.h/.cpp  # Core game state & logic
│   │   ├── GameScene.h/.cpp    # Main game scene
│   │   └── scenes/             # Individual game scenes
│   ├── entities/
│   │   ├── Agent.h/.cpp        # Player-controlled cyborg agents
│   │   ├── Entity.h/.cpp       # Base entity class
│   │   ├── Enemy.h/.cpp        # Enemy agents
│   │   └── NPC.h/.cpp          # Civilians/NPCs
│   ├── systems/
│   │   ├── RenderSystem.h/.cpp        # Isometric rendering
│   │   ├── InputSystem.h/.cpp         # Input handling
│   │   ├── PathfindingSystem.h/.cpp   # Pathfinding & movement
│   │   ├── CombatSystem.h/.cpp        # Combat resolution
│   │   └── AISystem.h/.cpp            # Enemy AI
│   ├── world/
│   │   ├── Tilemap.h/.cpp             # Isometric tilemap
│   │   ├── Camera.h/.cpp              # Camera system
│   │   └── Environment.h/.cpp         # Destructible objects, vehicles
│   ├── equipment/
│   │   ├── Weapon.h/.cpp              # Weapon system
│   │   ├── Equipment.h/.cpp           # Equipment/gadgets
│   │   └── Upgrades.h/.cpp            # Cybernetic upgrades
│   ├── ui/
│   │   ├── HUD.h/.cpp                 # Heads-up display
│   │   ├── Menu.h/.cpp                # Menu systems
│   │   └── Widgets.h/.cpp             # UI components
│   └── utils/
│       ├── Math.h/.cpp                # Isometric math utilities
│       ├── Config.h/.cpp              # Configuration
│       └── Logger.h/.cpp              # Logging
├── assets/
│   ├── sprites/                # Agent and entity sprites
│   ├── tilesets/               # Tilemap graphics
│   ├── ui/                     # UI graphics
│   ├── sounds/                 # Sound effects
│   ├── music/                  # Background music
│   └── maps/                   # Game maps/missions
├── docs/
│   ├── BUILD.md                # Detailed build instructions
│   ├── ARCHITECTURE.md         # Design and architecture
│   ├── GAMEPLAY.md             # Gameplay mechanics
│   └── CONTRIBUTING.md         # Contribution guidelines
├── tests/
│   ├── CMakeLists.txt
│   └── unit_tests.cpp          # Unit tests
└── scripts/
    ├── fetch_cocos2dx.sh       # Download Cocos2d-x
    └── build.sh                # Build helper script
```

## Build Instructions

See [BUILD.md](docs/BUILD.md) for detailed platform-specific instructions:
- **Linux/Ubuntu/Linux Mint**
- **Windows (MSVC/MinGW)**
- **macOS**

## Getting Started

1. Clone the repository
2. Run build setup script
3. Build the project
4. Run the game executable

## Roadmap

### Phase 1: Foundation (Current)
- [ ] Cocos2d-x project setup
- [ ] Isometric rendering system
- [ ] Basic agent movement
- [ ] Camera controls

### Phase 2: Core Gameplay
- [ ] Weapon system
- [ ] Combat mechanics
- [ ] Basic AI
- [ ] Mission structure

### Phase 3: Metagame
- [ ] Syndicate management
- [ ] Research system
- [ ] Territory control
- [ ] Campaign progression

### Phase 4: Polish
- [ ] Audio system
- [ ] UI/UX
- [ ] Performance optimization
- [ ] Bug fixes

## Contributing

See [CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines.

## License

MIT License - See LICENSE file for details.
