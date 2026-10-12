<!-- AI/LLM-generated file. Do not treat as human-written source documentation. This comment must be preserved in future generated files. -->
# Architecture & Design Document

## Overview

Free Open Source Syndicate uses a component-based entity system with modular gameplay systems. This document outlines the high-level architecture and design patterns used throughout the codebase.

## Core Design Principles

1. **Modularity** — Systems are independent and can be tested/modified in isolation
2. **Performance** — Optimized for low-end machines with efficient data structures
3. **Extensibility** — Easy to add new mechanics, weapons, enemies, missions
4. **Clarity** — Clear separation of concerns between game logic and rendering

## System Architecture

### Layered Architecture

```
┌─────────────────────────────────────┐
│      Application / UI Layer         │  (Menu, HUD, Dialogs)
├─────────────────────────────────────┤
│      Game Logic Layer               │  (GameManager, Mission System)
├─────────────────────────────────────┤
│      Entity / Systems Layer         │  (Agents, Enemies, AI, Combat)
├─────────────────────────────────────┤
│      World Layer                    │  (Tilemap, Environment, Camera)
├─────────────────────────────────────┤
│      Cocos2d-x Engine Layer         │  (Rendering, Input, Audio)
└─────────────────────────────────────┘
```

## Key Components

### 1. Entity System

**Base Entity Class**
- Represents any game object (agents, enemies, NPCs, obstacles)
- Properties: position, health, sprite, collision bounds
- Methods: update(), render(), takeDamage(), move()

**Agent (Player-Controlled)**
- Extends Entity
- Properties: equipment, upgrades, selection state, orders
- Can receive movement/attack commands
- Part of squad of 4

**Enemy Agent**
- Extends Entity
- Controlled by AI system
- Uses same weapons/equipment as player agents
- Tactical positioning and response

**NPC / Civilian**
- Non-combatant
- Can be persuaded (Persuadertron device)
- Follows player or wanders autonomously

### 2. World System

**Tilemap**
- Isometric tile grid
- Supports multiple layers (terrain, objects, effects)
- Each tile has walkability, height, properties

**Camera**
- Isometric projection camera
- Follows player squad or allows panning
- Zoom support for various display sizes

**Environment**
- Destructible objects (buildings, cars, crates)
- Vehicles (cars, trains) that can be hijacked
- Dynamic obstacles and hazards

### 3. Gameplay Systems

**RenderSystem**
- Converts world coordinates to isometric screen coordinates
- Handles depth sorting (z-order)
- Sprite rendering and animation

**InputSystem**
- Keyboard input (arrow keys, WASD for movement, spacebar for actions)
- Mouse input (point-to-move, click-to-select)
- Squad command interface

**PathfindingSystem**
- A* pathfinding on tilemap
- Avoids obstacles and enemies
- Predicts collision with moving agents

**CombatSystem**
- Weapon damage calculation
- Hit/miss determination
- Status effects (fire, poison, etc.)
- Death and destruction

**AISystem**
- Enemy decision-making
- Pathfinding to player agents
- Tactical positioning
- Aggression/retreat logic

### 4. Equipment System

**Weapons**
- Types: Pistol, Uzi, Flamethrower, Minigun, Rocket Launcher
- Properties: damage, range, rate of fire, magazine size
- Different behaviors (automatic, burst, area-of-effect)

**Equipment/Gadgets**
- Persuadertron (convert NPCs/enemies)
- Shield (temporary damage reduction)
- Radar (reveal enemies/NPCs)
- Armor upgrades

**Cybernetic Upgrades**
- Speed boost
- Intelligence (weapon accuracy)
- Health (damage resistance)
- Perception (view range)

### 5. Game Manager

- Manages game state (menu, mission, game-over)
- Handles missions and objectives
- Tracks player progress and statistics
- Manages sound and music
- Controls game pause/resume

## Data Flow

```
Input → InputSystem → GameManager → Entity Systems → RenderSystem → Display
                      ↓
                   World State (Tilemap, Entities)
                      ↓
               CombatSystem, AISystem, PathfindingSystem
```

## Isometric Coordinate System

The game uses isometric projection with a tilemap-based world:

```
World Coordinates (x, y) → Screen Coordinates (sx, sy)

sx = (x - y) * tile_width / 2
sy = (x + y) * tile_height / 2
```

All pathfinding and movement logic uses world coordinates; only rendering uses screen coordinates.

## File Organization

- **src/game/** — Game flow and scene management
- **src/entities/** — Entity classes and base types
- **src/systems/** — Core gameplay systems
- **src/world/** — World state and environment
- **src/equipment/** — Weapons, gadgets, upgrades
- **src/ui/** — User interface components
- **src/utils/** — Utility functions, math, logging, configuration

## Performance Considerations

1. **Entity Pooling** — Reuse entity instances instead of allocating/deallocating
2. **Spatial Partitioning** — Use quadtrees for efficient collision/visibility queries
3. **Lazy Rendering** — Only render visible tiles and entities
4. **Object Caching** — Cache frequently accessed world objects
5. **Pathfinding** — Limit A* searches to nearby tiles; use simplified pathfinding for distant agents

## Future Extensibility

- Easily add new mission types (objectives, scripting)
- Add new weapons/equipment via data files
- Add new enemy types and AI behaviors
- Support for campaign/story progression
- Modding support through scripting layer (Lua/AngelScript)

## Development Workflow

1. **New Feature:** Create system or entity class in appropriate directory
2. **Testing:** Write unit tests in `tests/`
3. **Integration:** Add to GameManager or appropriate system
4. **Polish:** Optimize, add assets, integrate with UI
