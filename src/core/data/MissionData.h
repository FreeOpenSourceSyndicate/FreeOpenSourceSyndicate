/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/data/Json.h"
#include "core/sim/Agent.h"
#include "core/world/TileGrid.h"

namespace fossynd::core
{

class Mission;

// One agent as described in a mission data file.
struct AgentSpec
{
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

// A single tile that blocks movement.
struct TileSpec
{
    int x = 0;
    int y = 0;
};

// A mission described by data (ADR-0003). Engine-agnostic; the view decides
// where the file lives and how to draw the result.
struct MissionData
{
    std::string name = "Mission";
    int width = 24;
    int height = 24;
    std::vector<BuildingSpec> buildings;
    std::vector<TileSpec> blocked;
    std::vector<AgentSpec> agents;

    // Loads a mission from a JSON file. Returns false and sets `error` on
    // failure (including a readable parse or schema error).
    static bool loadFromFile(const std::string& path, MissionData& out, std::string& error);

    // Parses an already-parsed JSON document into `out`.
    static bool fromJson(const Json& root, MissionData& out, std::string& error);
};

// Builds the tile grid described by `data`: every tile walkable except the
// blocked tiles and the perimeters of the buildings.
TileGrid buildGrid(const MissionData& data);

// Adds the agents from `data` to `mission`. `mission` must already reference a
// grid built by `buildGrid(data)` (it stores a pointer to the grid).
void populateMission(const MissionData& data, Mission& mission);

} // namespace fossynd::core
