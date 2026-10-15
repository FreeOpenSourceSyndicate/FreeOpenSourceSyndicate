/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/data/Catalogues.h"
#include "core/data/Json.h"
#include "core/data/MissionSpecs.h"
#include "core/sim/Objective.h"
#include "core/world/TileGrid.h"

namespace fossynd::core
{

class Mission;

// A mission described by data (ADR-0003). Engine-agnostic; the view decides
// where the file lives and how to draw the result.
struct MissionData
{
    std::string name = "Mission";

    // Optional stable identifier for this mission, used to key localised mission
    // text (`mission.<id>.name` / `mission.<id>.desc`, core/i18n/MissionText).
    // Empty when the mission JSON omits `id`; a mission then falls back to a
    // path-derived id, so this is purely additive.
    std::string id;

    int width = 24;
    int height = 24;
    std::vector<BuildingSpec> buildings;
    std::vector<TileSpec> blocked;
    std::vector<AgentSpec> agents;

    // Win condition. Defaults to EliminateAll, so missions that omit it behave
    // exactly as before. See `ObjectiveSpec` for the supported kinds.
    ObjectiveSpec objective;

    // Parses an `objective` JSON object into `out` (missing fields keep their
    // defaults). `mapWidth`/`mapHeight` bound a target tile (`reach`, `escort`,
    // `destroy`), so a tile off the map is a readable error rather than a
    // silently unwinnable mission. Returns false and sets `error` on a schema
    // error. The escort `agentIndex` is bounds-checked by `fromJson`, once the
    // agents are parsed.
    static bool parseObjectiveJson(const Json& node, int mapWidth, int mapHeight, ObjectiveSpec& out,
                                   std::string& error);

    // Loads a mission from a JSON file. Returns false and sets `error` on
    // failure (including a readable parse or schema error).
    static bool loadFromFile(const std::string& path, MissionData& out, std::string& error);

    // Parses an already-parsed JSON document into `out`. `weapons` and `agents`
    // are the shared catalogues; a mission may reference them by `weaponId` /
    // `agentId`. Empty catalogues are fine for a mission that only inlines its
    // content (the common case), but a reference then fails with a readable
    // error rather than being silently ignored.
    static bool fromJson(const Json& root, MissionData& out, const WeaponCatalogue& weapons,
                         const AgentCatalogue& agents, std::string& error);

    // Convenience overload for missions that do not use catalogue references.
    static bool fromJson(const Json& root, MissionData& out, std::string& error);
};

// Builds the tile grid described by `data`: every tile walkable except the
// blocked tiles and the perimeters of the buildings.
TileGrid buildGrid(const MissionData& data);

// Adds the agents from `data` to `mission`. `mission` must already reference a
// grid built by `buildGrid(data)` (it stores a pointer to the grid).
void populateMission(const MissionData& data, Mission& mission);

} // namespace fossynd::core
