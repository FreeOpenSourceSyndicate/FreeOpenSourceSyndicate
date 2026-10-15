/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>

#include "core/sim/Agent.h"
#include "core/sim/Weapon.h"

namespace fossynd::core
{

// One agent as described in a mission data file, or as a template in the shared
// agent catalogue (ADR-0003). Kept in its own header so both the mission loader
// and the catalogue loader can reuse it without a circular include.
struct AgentSpec
{
    // Catalogue id. Empty for agents inlined directly in a mission; set for
    // templates in the shared agent catalogue.
    std::string id;
    std::string name = "AGENT";
    Team team = Team::Player;
    float x = 0.0f;
    float y = 0.0f;
    float speed = 3.0f;
    float health = 100.0f;
    Weapon weapon = makePistol();
};

// A rectangular building footprint. Only the perimeter blocks movement, so the
// inside stays walkable (matching the original hard-coded demo layout).
struct BuildingSpec
{
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;
};

// A single tile that blocks movement. `destructible` makes it a wall weapon
// fire can break through (issue #157); when breached it becomes rubble (issue
// #217). Off by default, so existing missions are unchanged.
struct TileSpec
{
    int x = 0;
    int y = 0;
    bool destructible = false;
};

} // namespace fossynd::core
