/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/data/MissionData.h"

#include <utility>

#include "core/sim/Mission.h"

namespace fossynd::core
{
namespace
{

bool parseAgent(const Json& node, int mapWidth, int mapHeight, const AgentCatalogue& agents,
                const WeaponCatalogue& weapons, AgentSpec& out, std::string& error)
{
    if (!node.isObject())
    {
        error = "each agent must be an object";
        return false;
    }

    // A mission agent may inherit a template from the shared catalogue, then
    // override any field inline. An unknown id is a readable error, never a
    // silent fallback to the default.
    if (node.has("agentId"))
    {
        const std::string id = node["agentId"].asString();
        const AgentEntry* entry = agents.find(id);
        if (entry == nullptr)
        {
            error = "unknown agentId '" + id + "' (no such agent in the catalogue)";
            return false;
        }
        out = entry->spec;
        out.id = id;
    }

    if (!node.has("x") || !node.has("y"))
    {
        error = "each agent needs numeric 'x' and 'y'";
        return false;
    }
    out.x = node["x"].asFloat(out.x);
    out.y = node["y"].asFloat(out.y);
    // An agent outside the map is never drawn and never reachable, so a
    // misplaced one silently breaks the mission (issue #212).
    if (!(out.x >= 0.0f && out.x < static_cast<float>(mapWidth) && out.y >= 0.0f &&
          out.y < static_cast<float>(mapHeight)))
    {
        error = "agent position ('x', 'y') must be inside the " + std::to_string(mapWidth) + "x" +
                std::to_string(mapHeight) + " map";
        return false;
    }

    if (!detail::applyAgentCommon(node, out, error))
    {
        return false;
    }
    if (!AgentCatalogue::applyWeaponToAgent(node, weapons, out.weapon, error))
    {
        error = "agent '" + out.name + "': " + error;
        return false;
    }
    return true;
}

// Parses one mission `vehicles` entry. Like `parseAgent`, a vehicle may inherit
// a shared catalogue template by `vehicleId`, then override any field inline.
// An unknown id is a readable error, never a silent fallback. A vehicle needs a
// position, and an off-map one is never drawn or reachable (the same class of
// error as a misplaced agent, issue #212).
bool parseVehicle(const Json& node, int mapWidth, int mapHeight, const VehicleCatalogue& vehicles,
                  const WeaponCatalogue& weapons, VehicleSpec& out, std::string& error)
{
    if (!node.isObject())
    {
        error = "each vehicle must be an object";
        return false;
    }

    if (node.has("vehicleId"))
    {
        const std::string id = node["vehicleId"].asString();
        const VehicleEntry* entry = vehicles.find(id);
        if (entry == nullptr)
        {
            error = "unknown vehicleId '" + id + "' (no such vehicle in the catalogue)";
            return false;
        }
        out = entry->spec;
        out.id = id;
    }

    if (!node.has("x") || !node.has("y"))
    {
        error = "each vehicle needs numeric 'x' and 'y'";
        return false;
    }
    out.x = node["x"].asFloat(out.x);
    out.y = node["y"].asFloat(out.y);
    if (!(out.x >= 0.0f && out.x < static_cast<float>(mapWidth) && out.y >= 0.0f &&
          out.y < static_cast<float>(mapHeight)))
    {
        error = "vehicle position ('x', 'y') must be inside the " + std::to_string(mapWidth) + "x" +
                std::to_string(mapHeight) + " map";
        return false;
    }

    if (!detail::applyVehicleCommon(node, out, error))
    {
        return false;
    }
    if (!AgentCatalogue::applyWeaponToAgent(node, weapons, out.weapon, error))
    {
        error = "vehicle '" + out.name + "': " + error;
        return false;
    }
    return true;
}

bool parseBuildings(const Json& array, int mapWidth, int mapHeight, std::vector<BuildingSpec>& out,
                    std::string& error)
{
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        const Json& node = array[i];
        if (!node.isObject() || !node.has("x") || !node.has("y"))
        {
            error = "each building needs numeric 'x' and 'y'";
            return false;
        }
        BuildingSpec building;
        building.x = node["x"].asInt();
        building.y = node["y"].asInt();
        building.width = node["width"].asInt(1);
        building.height = node["height"].asInt(1);
        if (building.width <= 0 || building.height <= 0)
        {
            error = "building 'width' and 'height' must be positive";
            return false;
        }
        // A building that runs off the map is clipped by buildGrid, so only the
        // part inside the grid is ever blocked. Reject it instead (issue #212).
        if (building.x < 0 || building.y < 0 || building.x + building.width > mapWidth ||
            building.y + building.height > mapHeight)
        {
            error = "building at (" + std::to_string(building.x) + ", " +
                    std::to_string(building.y) + ") size " + std::to_string(building.width) + "x" +
                    std::to_string(building.height) + " is not inside the " +
                    std::to_string(mapWidth) + "x" + std::to_string(mapHeight) + " map";
            return false;
        }
        out.push_back(building);
    }
    return true;
}

bool parseBlocked(const Json& array, int mapWidth, int mapHeight, std::vector<TileSpec>& out,
                  std::string& error)
{
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        const Json& node = array[i];
        if (!node.isObject() || !node.has("x") || !node.has("y"))
        {
            error = "each blocked tile needs numeric 'x' and 'y'";
            return false;
        }
        TileSpec tile;
        tile.x = node["x"].asInt();
        tile.y = node["y"].asInt();
        // Optional: a destructible blocked tile is a wall weapon fire can break
        // (issue #157). Absent, the tile is a plain solid wall.
        tile.destructible = node.has("destructible") && node["destructible"].asBool();
        // An off-map blocked tile is silently dropped by buildGrid (issue #212).
        if (tile.x < 0 || tile.y < 0 || tile.x >= mapWidth || tile.y >= mapHeight)
        {
            error = "blocked tile (" + std::to_string(tile.x) + ", " + std::to_string(tile.y) +
                    ") is outside the " + std::to_string(mapWidth) + "x" + std::to_string(mapHeight) +
                    " map";
            return false;
        }
        out.push_back(tile);
    }
    return true;
}

// Shared bounds check for objective kinds that name a target tile (reach,
// escort, destroy). A tile off the map can never be reached or breached, so the
// mission would be silently unwinnable (issue #212). `label` names the kind in
// the error message.
bool objectiveTileOnMap(const ObjectiveSpec& spec, int mapWidth, int mapHeight, const char* label,
                        std::string& error)
{
    if (spec.tileX < 0 || spec.tileY < 0 || spec.tileX >= mapWidth || spec.tileY >= mapHeight)
    {
        error = "a '" + std::string(label) + "' objective tile (" + std::to_string(spec.tileX) +
                ", " + std::to_string(spec.tileY) + ") is outside the " + std::to_string(mapWidth) +
                "x" + std::to_string(mapHeight) + " map";
        return false;
    }
    return true;
}

} // namespace

bool MissionData::parseObjectiveJson(const Json& node, int mapWidth, int mapHeight,
                                     ObjectiveSpec& out, std::string& error)
{
    out = ObjectiveSpec{};
    if (!node.isObject())
    {
        error = "'objective' must be an object";
        return false;
    }

    const std::string kind = node.has("kind") ? node["kind"].asString() : std::string("eliminate");
    if (kind == "eliminate")
    {
        out.kind = ObjectiveSpec::Kind::EliminateAll;
    }
    else if (kind == "reach")
    {
        out.kind = ObjectiveSpec::Kind::ReachTile;
        if (!node.has("tileX") || !node.has("tileY"))
        {
            error = "a 'reach' objective needs numeric 'tileX' and 'tileY'";
            return false;
        }
        out.tileX = node["tileX"].asInt();
        out.tileY = node["tileY"].asInt();
        // A reach target off the map can never be reached, so the mission would
        // be silently unwinnable (issue #212).
        if (!objectiveTileOnMap(out, mapWidth, mapHeight, "reach", error))
        {
            return false;
        }
    }
    else if (kind == "survive")
    {
        out.kind = ObjectiveSpec::Kind::Survive;
        out.seconds = node["seconds"].asFloat(out.seconds);
        if (out.seconds <= 0.0f)
        {
            error = "a 'survive' objective needs 'seconds' > 0";
            return false;
        }
    }
    else if (kind == "escort")
    {
        out.kind = ObjectiveSpec::Kind::Escort;
        if (!node.has("tileX") || !node.has("tileY") || !node.has("agentIndex"))
        {
            error = "an 'escort' objective needs numeric 'tileX', 'tileY', and 'agentIndex'";
            return false;
        }
        out.tileX = node["tileX"].asInt();
        out.tileY = node["tileY"].asInt();
        out.agentIndex = node["agentIndex"].asInt();
        if (out.agentIndex < 0)
        {
            error = "an 'escort' objective needs an 'agentIndex' >= 0";
            return false;
        }
        if (!objectiveTileOnMap(out, mapWidth, mapHeight, "escort", error))
        {
            return false;
        }
        // `agentIndex` is bounds-checked against the agent list in `fromJson`,
        // once the agents are parsed (the objective is parsed first).
    }
    else if (kind == "destroy")
    {
        out.kind = ObjectiveSpec::Kind::Destroy;
        if (!node.has("tileX") || !node.has("tileY"))
        {
            error = "a 'destroy' objective needs numeric 'tileX' and 'tileY'";
            return false;
        }
        out.tileX = node["tileX"].asInt();
        out.tileY = node["tileY"].asInt();
        if (!objectiveTileOnMap(out, mapWidth, mapHeight, "destroy", error))
        {
            return false;
        }
    }
    else if (kind == "persuade")
    {
        out.kind = ObjectiveSpec::Kind::Persuade;
        out.targetCount = node["targetCount"].asInt(out.targetCount);
        if (out.targetCount < 1)
        {
            error = "a 'persuade' objective needs 'targetCount' >= 1";
            return false;
        }
    }
    else
    {
        error = "unknown objective kind '" + kind +
                "' (expected eliminate, reach, survive, escort, destroy, or persuade)";
        return false;
    }
    return true;
}

bool MissionData::fromJson(const Json& root, MissionData& out, const WeaponCatalogue& weapons,
                           const AgentCatalogue& agents, const VehicleCatalogue& vehicles,
                           std::string& error)
{
    if (!root.isObject())
    {
        error = "mission root must be a JSON object";
        return false;
    }

    out = MissionData{};

    if (root.has("name"))
    {
        out.name = root["name"].asString();
    }
    // Optional stable localisation key. Absent leaves `id` empty (the mission
    // then falls back to a path-derived id); a present-but-non-string `id` is a
    // readable error rather than a silent empty id.
    if (root.has("id"))
    {
        if (!root["id"].isString())
        {
            error = "mission 'id' must be a string";
            return false;
        }
        out.id = root["id"].asString();
    }
    out.width = root["width"].asInt(out.width);
    out.height = root["height"].asInt(out.height);
    if (out.width <= 0 || out.height <= 0)
    {
        error = "mission 'width' and 'height' must be positive";
        return false;
    }

    if (root.has("buildings") &&
        !parseBuildings(root["buildings"], out.width, out.height, out.buildings, error))
    {
        return false;
    }
    if (root.has("blocked") &&
        !parseBlocked(root["blocked"], out.width, out.height, out.blocked, error))
    {
        return false;
    }

    if (root.has("objective") &&
        !MissionData::parseObjectiveJson(root["objective"], out.width, out.height, out.objective,
                                         error))
    {
        return false;
    }

    if (!root.has("agents") || !root["agents"].isArray() || root["agents"].size() == 0)
    {
        error = "mission needs a non-empty 'agents' array";
        return false;
    }
    for (std::size_t i = 0; i < root["agents"].size(); ++i)
    {
        AgentSpec spec;
        if (!parseAgent(root["agents"][i], out.width, out.height, agents, weapons, spec, error))
        {
            return false;
        }
        out.agents.push_back(std::move(spec));
    }

    // Optional: hijackable vehicles. Absent leaves the mission vehicle-free, so
    // an existing mission is unchanged (golden rule 7, ADR-0003).
    if (root.has("vehicles"))
    {
        if (!root["vehicles"].isArray())
        {
            error = "'vehicles' must be an array";
            return false;
        }
        for (std::size_t i = 0; i < root["vehicles"].size(); ++i)
        {
            VehicleSpec spec;
            if (!parseVehicle(root["vehicles"][i], out.width, out.height, vehicles, weapons, spec,
                              error))
            {
                return false;
            }
            out.vehicles.push_back(std::move(spec));
        }
    }

    // The escorted unit must exist: an escort objective naming an agent that is
    // not in the mission can never be won, so it is a content error (issue #212
    // pattern). Checked here because the objective is parsed before the agents.
    if (out.objective.kind == ObjectiveSpec::Kind::Escort &&
        static_cast<std::size_t>(out.objective.agentIndex) >= out.agents.size())
    {
        error = "an 'escort' objective 'agentIndex' (" + std::to_string(out.objective.agentIndex) +
                ") is outside the " + std::to_string(out.agents.size()) + "-agent mission";
        return false;
    }
    return true;
}

bool MissionData::fromJson(const Json& root, MissionData& out, std::string& error)
{
    // Missions that inline their content (the demo and most sample missions)
    // need no catalogues; a `weaponId`/`agentId`/`vehicleId` reference then
    // reports a readable "unknown id" error instead of being ignored.
    static const WeaponCatalogue kNoWeapons;
    static const AgentCatalogue kNoAgents;
    static const VehicleCatalogue kNoVehicles;
    return fromJson(root, out, kNoWeapons, kNoAgents, kNoVehicles, error);
}

bool MissionData::loadFromFile(const std::string& path, MissionData& out, std::string& error)
{
    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }

    // Load the shared catalogues that sit beside the mission (if present), so a
    // mission can reference weapons/agents/vehicles by id. A missing catalogue
    // is fine; a malformed one is a readable error.
    WeaponCatalogue weapons;
    AgentCatalogue agents;
    VehicleCatalogue vehicles;
    bool found = false;
    const std::string weaponsPath = cataloguePathForMission(path, kWeaponsFileName);
    if (!weaponsPath.empty() && !weapons.loadFromFile(weaponsPath, found, error))
    {
        return false;
    }
    const std::string agentsPath = cataloguePathForMission(path, kAgentsFileName);
    if (!agentsPath.empty() && !agents.loadFromFile(agentsPath, weapons, found, error))
    {
        return false;
    }
    const std::string vehiclesPath = cataloguePathForMission(path, kVehiclesFileName);
    if (!vehiclesPath.empty() && !vehicles.loadFromFile(vehiclesPath, weapons, found, error))
    {
        return false;
    }

    return fromJson(root, out, weapons, agents, vehicles, error);
}

TileGrid buildGrid(const MissionData& data)
{
    TileGrid grid(data.width, data.height);
    for (int y = 0; y < data.height; ++y)
    {
        for (int x = 0; x < data.width; ++x)
        {
            grid.setWalkable(x, y, true);
            grid.setTileType(x, y, (x + y) % 2);
        }
    }

    for (const BuildingSpec& building : data.buildings)
    {
        const int x1 = building.x + building.width - 1;
        const int y1 = building.y + building.height - 1;
        for (int y = building.y; y <= y1; ++y)
        {
            for (int x = building.x; x <= x1; ++x)
            {
                if (x == building.x || x == x1 || y == building.y || y == y1)
                {
                    grid.setWalkable(x, y, false);
                }
            }
        }
    }

    for (const TileSpec& tile : data.blocked)
    {
        grid.setWalkable(tile.x, tile.y, false);
        // A destructible wall keeps its blocking but can be breached by fire
        // (issue #157); it then reads as rubble (issue #217/#218).
        if (tile.destructible)
        {
            grid.setDestructible(tile.x, tile.y, true);
        }
    }

    // Scatter presentational decor over some interior blocked tiles (crates and
    // barrels) so the depth-sorted sprite pass has static scenery. Deterministic:
    // chosen by a fixed integer hash of the tile, never by RNG, so layouts are
    // stable. The outer ring is skipped so boundary walls stay plain, and lights
    // are sparse so the board does not turn into a warehouse.
    for (int y = 1; y < data.height - 1; ++y)
    {
        for (int x = 1; x < data.width - 1; ++x)
        {
            if (grid.isWalkable(x, y))
            {
                continue;
            }
            const unsigned hash =
                static_cast<unsigned>(x * 73856093) ^ static_cast<unsigned>(y * 19349663);
            switch (hash % 7u)
            {
            case 0:
                grid.setDecor(x, y, Decor::Crate);
                break;
            case 1:
                grid.setDecor(x, y, Decor::Barrel);
                break;
            default:
                break;
            }
        }
    }

    return grid;
}

void populateMission(const MissionData& data, Mission& mission)
{
    mission.setObjective(data.objective);
    for (const AgentSpec& spec : data.agents)
    {
        Agent agent(spec.x, spec.y, spec.speed);
        agent.setTeam(spec.team);
        agent.setName(spec.name);
        agent.setWeapon(spec.weapon);
        agent.setMaxHealth(spec.health);
        mission.addAgent(std::move(agent));
    }
}

bool applyLoadout(const PlayerLoadout& loadout, Mission& mission, const WeaponCatalogue& weapons,
                  std::string& error)
{
    // Slots map onto the mission's *player* agents in authored order, so the
    // mapping is independent of any enemy/civilian interleaving (Milestone B
    // deployment, docs/METAGAME.md §7 R1). A slot past the last player agent is
    // ignored: a full four-slot loadout is safe against a one-agent mission.
    std::vector<Agent>& agents = mission.agents();
    std::size_t slot = 0;
    for (std::size_t i = 0; i < agents.size() && slot < loadout.slots.size(); ++i)
    {
        if (agents[i].team() != Team::Player)
        {
            continue;
        }
        const LoadoutSlot& item = loadout.slots[slot];
        ++slot;

        if (!item.weaponId.empty())
        {
            const Weapon* weapon = weapons.find(item.weaponId);
            if (weapon == nullptr)
            {
                error = "loadout slot " + std::to_string(slot - 1) + ": unknown weaponId '" +
                        item.weaponId + "' (no such weapon in the catalogue)";
                return false;
            }
            agents[i].setWeapon(*weapon);
        }

        if (!item.gadgetId.empty())
        {
            const int charges = gadgetChargesForId(item.gadgetId);
            if (charges == kUnknownGadget)
            {
                error = "loadout slot " + std::to_string(slot - 1) + ": unknown gadgetId '" +
                        item.gadgetId + "' (expected 'medkit', 'shield', 'scanner' or 'drugs')";
                return false;
            }
            if (item.gadgetId == "medkit")
            {
                mission.setMedikits(i, charges);
            }
            else if (item.gadgetId == "shield")
            {
                mission.setShields(i, charges);
            }
            else if (item.gadgetId == "scanner")
            {
                mission.setScanners(i, charges);
            }
            else
            {
                mission.setDrugs(i, charges);
            }
        }

        if (!item.cyberneticId.empty())
        {
            // A cybernetic is a passive, permanent stat change: a known id is
            // applied once here; an unknown one is a content error (a typo must
            // not silently deploy an un-upgraded agent), matching the weapon and
            // gadget strictness above (ADR-0027/ADR-0031).
            if (!mission.applyCybernetic(i, item.cyberneticId))
            {
                error = "loadout slot " + std::to_string(slot - 1) + ": unknown cyberneticId '" +
                        item.cyberneticId +
                        "' (expected 'legs', 'arms', 'chest', 'eyes' or 'targeting-implant')";
                return false;
            }
        }
    }
    return true;
}

} // namespace fossynd::core
