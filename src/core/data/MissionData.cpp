/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/data/MissionData.h"

#include <utility>

#include "core/sim/Mission.h"

namespace fossynd::core
{
namespace
{

Team parseTeam(const std::string& value, bool& ok)
{
    ok = true;
    if (value == "player")
    {
        return Team::Player;
    }
    if (value == "enemy")
    {
        return Team::Enemy;
    }
    if (value == "civilian")
    {
        return Team::Civilian;
    }
    ok = false;
    return Team::Player;
}

Weapon parseWeapon(const Json& node, bool& ok)
{
    ok = true;
    if (!node.isObject())
    {
        ok = false;
        return makePistol();
    }
    Weapon weapon;
    if (node.has("name"))
    {
        weapon.name = node["name"].asString();
    }
    weapon.damage = node["damage"].asFloat(weapon.damage);
    weapon.range = node["range"].asFloat(weapon.range);
    weapon.fireRate = node["fireRate"].asFloat(weapon.fireRate);
    weapon.accuracy = node["accuracy"].asFloat(weapon.accuracy);
    if (weapon.fireRate <= 0.0f)
    {
        ok = false;
    }
    return weapon;
}

bool parseAgent(const Json& node, AgentSpec& out, std::string& error)
{
    if (!node.isObject())
    {
        error = "each agent must be an object";
        return false;
    }
    if (!node.has("x") || !node.has("y"))
    {
        error = "each agent needs numeric 'x' and 'y'";
        return false;
    }

    if (node.has("name"))
    {
        out.name = node["name"].asString();
    }

    if (node.has("team"))
    {
        bool ok = false;
        out.team = parseTeam(node["team"].asString(), ok);
        if (!ok)
        {
            error = "unknown team '" + node["team"].asString() +
                    "' (expected player, enemy, or civilian)";
            return false;
        }
    }

    out.x = node["x"].asFloat(out.x);
    out.y = node["y"].asFloat(out.y);
    out.speed = node["speed"].asFloat(out.speed);
    out.health = node["health"].asFloat(out.health);
    if (out.speed <= 0.0f || out.health <= 0.0f)
    {
        error = "agent 'speed' and 'health' must be positive";
        return false;
    }

    if (node.has("weapon"))
    {
        bool ok = false;
        out.weapon = parseWeapon(node["weapon"], ok);
        if (!ok)
        {
            error = "agent has an invalid 'weapon' (need an object with fireRate > 0)";
            return false;
        }
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

bool MissionData::fromJson(const Json& root, MissionData& out, std::string& error)
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

    if (!root.has("agents") || !root["agents"].isArray() || root["agents"].size() == 0)
    {
        error = "mission needs a non-empty 'agents' array";
        return false;
    }
    for (std::size_t i = 0; i < root["agents"].size(); ++i)
    {
        AgentSpec spec;
        if (!parseAgent(root["agents"][i], spec, error))
        {
            return false;
        }
        out.agents.push_back(std::move(spec));
    }
    return true;
}

bool MissionData::loadFromFile(const std::string& path, MissionData& out, std::string& error)
{
    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }
    return fromJson(root, out, error);
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

    return grid;
}

void populateMission(const MissionData& data, Mission& mission)
{
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
