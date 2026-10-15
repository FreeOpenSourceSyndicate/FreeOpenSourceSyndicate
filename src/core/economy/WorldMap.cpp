/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/economy/WorldMap.h"

#include <algorithm>

namespace fossynd::core
{
namespace
{

// True if `ids` contains `value`.
bool contains(const std::vector<TerritoryId>& ids, TerritoryId value)
{
    return std::find(ids.begin(), ids.end(), value) != ids.end();
}

// Parses the `neighbours` array of one territory. `where` names the territory
// for the error message.
bool parseNeighbours(const Json& node, TerritoryId self, std::vector<TerritoryId>& out,
                     const std::string& where, std::string& error)
{
    if (node.isNull())
    {
        return true;
    }
    if (!node.isArray())
    {
        error = where + ": 'neighbours' must be an array";
        return false;
    }
    for (std::size_t i = 0; i < node.size(); ++i)
    {
        const Json& entry = node[i];
        if (!entry.isNumber())
        {
            error = where + ": each neighbour must be a number";
            return false;
        }
        const TerritoryId id = entry.asInt(-1);
        if (id < 0)
        {
            error = where + ": neighbour ids must be >= 0";
            return false;
        }
        if (id == self)
        {
            error = where + ": a territory cannot neighbour itself";
            return false;
        }
        if (contains(out, id))
        {
            error = where + ": duplicate neighbour " + std::to_string(id);
            return false;
        }
        out.push_back(id);
    }
    return true;
}

} // namespace

bool WorldMap::fromJson(const Json& root, WorldMap& out, std::string& error)
{
    error.clear();
    out.territories_.clear();

    if (!root.isObject())
    {
        error = "world map root must be a JSON object";
        return false;
    }
    const Json& array = root["territories"];
    if (!array.isArray())
    {
        error = "'territories' must be an array";
        return false;
    }

    for (std::size_t i = 0; i < array.size(); ++i)
    {
        const Json& node = array[i];
        if (!node.isObject())
        {
            error = "each territory must be an object";
            return false;
        }

        Territory territory;
        if (!node.has("id") || !node["id"].isNumber())
        {
            error = "territory " + std::to_string(i) + ": missing numeric 'id'";
            return false;
        }
        territory.id = node["id"].asInt(-1);
        if (territory.id < 0)
        {
            error = "territory " + std::to_string(i) + ": 'id' must be >= 0";
            return false;
        }
        const std::string where = "territory " + std::to_string(territory.id);

        territory.name = node.has("name") ? node["name"].asString() : std::string();
        if (territory.name.empty())
        {
            error = where + ": missing non-empty 'name'";
            return false;
        }

        territory.owner = node["owner"].asInt(-1);
        if (territory.owner < -1)
        {
            error = where + ": 'owner' must be >= -1 (-1 = neutral)";
            return false;
        }
        territory.missionId = node.has("missionId") ? node["missionId"].asString() : std::string();
        territory.taxLevel = node["taxLevel"].asInt(0);
        if (territory.taxLevel < 0)
        {
            error = where + ": 'taxLevel' must be >= 0";
            return false;
        }

        if (!parseNeighbours(node["neighbours"], territory.id, territory.neighbours, where, error))
        {
            return false;
        }

        out.territories_.push_back(std::move(territory));
    }

    // Store in id order so the dense-id invariant find() relies on holds, and
    // so validate can report duplicates clearly.
    std::sort(out.territories_.begin(), out.territories_.end(),
              [](const Territory& a, const Territory& b) { return a.id < b.id; });

    return out.validate(error);
}

bool WorldMap::loadFromFile(const std::string& path, WorldMap& out, std::string& error)
{
    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }
    return fromJson(root, out, error);
}

const Territory* WorldMap::find(TerritoryId id) const
{
    if (id < 0 || static_cast<std::size_t>(id) >= territories_.size())
    {
        return nullptr;
    }
    return &territories_[static_cast<std::size_t>(id)];
}

const std::vector<TerritoryId>& WorldMap::neighbours(TerritoryId id) const
{
    static const std::vector<TerritoryId> empty;
    const Territory* territory = find(id);
    return territory != nullptr ? territory->neighbours : empty;
}

bool WorldMap::areNeighbours(TerritoryId a, TerritoryId b) const
{
    return contains(neighbours(a), b);
}

int WorldMap::owner(TerritoryId id) const
{
    const Territory* territory = find(id);
    return territory != nullptr ? territory->owner : -1;
}

void WorldMap::setOwner(TerritoryId id, int syndicate)
{
    if (id < 0 || static_cast<std::size_t>(id) >= territories_.size())
    {
        return;
    }
    territories_[static_cast<std::size_t>(id)].owner = syndicate;
}

std::vector<TerritoryId> WorldMap::territoriesOwnedBy(int syndicate) const
{
    std::vector<TerritoryId> owned;
    for (const Territory& territory : territories_)
    {
        if (territory.owner == syndicate)
        {
            owned.push_back(territory.id);
        }
    }
    return owned;
}

bool WorldMap::validate(std::string& error) const
{
    error.clear();
    for (std::size_t i = 0; i < territories_.size(); ++i)
    {
        const Territory& territory = territories_[i];
        const std::string where = "territory " + std::to_string(territory.id);

        // Territories are stored in id order (fromJson sorts them), so a
        // duplicate is adjacent and gets its own clear message.
        if (i > 0 && territory.id == territories_[i - 1].id)
        {
            error = "duplicate territory id " + std::to_string(territory.id);
            return false;
        }

        // Dense ids: the territory at index i must have id i, so find() can index
        // directly.
        if (territory.id != static_cast<TerritoryId>(i))
        {
            error = "territory ids must be dense (0..size-1); index " + std::to_string(i) +
                    " holds id " + std::to_string(territory.id);
            return false;
        }
        if (territory.name.empty())
        {
            error = where + ": 'name' must not be empty";
            return false;
        }

        for (const TerritoryId neighbour : territory.neighbours)
        {
            if (find(neighbour) == nullptr)
            {
                error = where + ": unknown neighbour " + std::to_string(neighbour);
                return false;
            }
            if (!contains(neighbours(neighbour), territory.id))
            {
                error = where + ": adjacency to " + std::to_string(neighbour) +
                        " is not symmetric";
                return false;
            }
        }
    }

    // Names are labels on the map, so they must be unique.
    for (std::size_t i = 0; i < territories_.size(); ++i)
    {
        for (std::size_t j = i + 1; j < territories_.size(); ++j)
        {
            if (territories_[i].name == territories_[j].name)
            {
                error = "duplicate territory name '" + territories_[i].name + "'";
                return false;
            }
        }
    }

    // Two territories may not be deployed to from the same mission id.
    for (std::size_t i = 0; i < territories_.size(); ++i)
    {
        if (territories_[i].missionId.empty())
        {
            continue;
        }
        for (std::size_t j = i + 1; j < territories_.size(); ++j)
        {
            if (territories_[i].missionId == territories_[j].missionId)
            {
                error = "mission id '" + territories_[i].missionId +
                        "' is used by more than one territory";
                return false;
            }
        }
    }

    return true;
}

} // namespace fossynd::core
