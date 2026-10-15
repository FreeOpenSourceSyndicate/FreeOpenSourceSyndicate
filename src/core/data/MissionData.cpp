/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/data/MissionData.h"

#include <utility>

#include "core/sim/Mission.h"

namespace fossynd::core
{
namespace
{

bool parseAgent(const Json& node, const AgentCatalogue& agents, const WeaponCatalogue& weapons,
                AgentSpec& out, std::string& error)
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

    if (!detail::applyAgentCommon(node, out, error))
    {
        return false;
    }
    if (!AgentCatalogue::applyWeaponToAgent(node, weapons, out.weapon, error))
    {
        return false;
    }
    return true;
}

bool parseBuildings(const Json& array, std::vector<BuildingSpec>& out, std::string& error)
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
        out.push_back(building);
    }
    return true;
}

bool parseBlocked(const Json& array, std::vector<TileSpec>& out, std::string& error)
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
        out.push_back(tile);
    }
    return true;
}

} // namespace

bool MissionData::parseObjectiveJson(const Json& node, ObjectiveSpec& out, std::string& error)
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
    else
    {
        error = "unknown objective kind '" + kind +
                "' (expected eliminate, reach, or survive)";
        return false;
    }
    return true;
}

bool MissionData::fromJson(const Json& root, MissionData& out, const WeaponCatalogue& weapons,
                           const AgentCatalogue& agents, std::string& error)
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

    if (root.has("buildings") && !parseBuildings(root["buildings"], out.buildings, error))
    {
        return false;
    }
    if (root.has("blocked") && !parseBlocked(root["blocked"], out.blocked, error))
    {
        return false;
    }

    if (root.has("objective") &&
        !MissionData::parseObjectiveJson(root["objective"], out.objective, error))
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
        if (!parseAgent(root["agents"][i], agents, weapons, spec, error))
        {
            return false;
        }
        out.agents.push_back(std::move(spec));
    }
    return true;
}

bool MissionData::fromJson(const Json& root, MissionData& out, std::string& error)
{
    // Missions that inline their content (the demo and most sample missions)
    // need no catalogues; a `weaponId`/`agentId` reference then reports a
    // readable "unknown id" error instead of being ignored.
    static const WeaponCatalogue kNoWeapons;
    static const AgentCatalogue kNoAgents;
    return fromJson(root, out, kNoWeapons, kNoAgents, error);
}

bool MissionData::loadFromFile(const std::string& path, MissionData& out, std::string& error)
{
    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }

    // Load the shared catalogues that sit beside the mission (if present), so a
    // mission can reference weapons/agents by id. A missing catalogue is fine;
    // a malformed one is a readable error.
    WeaponCatalogue weapons;
    AgentCatalogue agents;
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

    return fromJson(root, out, weapons, agents, error);
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

} // namespace fossynd::core
