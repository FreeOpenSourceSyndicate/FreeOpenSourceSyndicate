/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/economy/NamePools.h"

namespace fossynd::core
{

bool NamePools::fromJson(const Json& root, NamePools& out, std::string& error)
{
    error.clear();
    out.entries_.clear();
    out.regions_.clear();

    if (!root.isObject())
    {
        error = "name-pool root must be a JSON object";
        return false;
    }
    const Json& pools = root["pools"];
    if (!pools.isArray())
    {
        error = "'pools' must be an array";
        return false;
    }

    for (std::size_t i = 0; i < pools.size(); ++i)
    {
        const Json& node = pools[i];
        if (!node.isObject())
        {
            error = "each name pool must be an object";
            return false;
        }

        const int region = node["region"].asInt(-1);
        if (region < 0)
        {
            error = "each name pool needs a non-negative 'region'";
            return false;
        }
        if (out.pool(region) != nullptr)
        {
            error = "name pool for region " + std::to_string(region) + ": duplicate region";
            return false;
        }

        const Json& names = node["names"];
        if (!names.isArray() || names.size() == 0)
        {
            error = "name pool for region " + std::to_string(region) +
                    ": 'names' must be a non-empty array";
            return false;
        }

        Entry entry;
        entry.region = region;
        for (std::size_t j = 0; j < names.size(); ++j)
        {
            const std::string name = names[j].asString();
            if (name.empty())
            {
                error = "name pool for region " + std::to_string(region) +
                        ": each name must be a non-empty string";
                return false;
            }
            entry.names.push_back(name);
        }

        out.entries_.push_back(std::move(entry));
        out.regions_.push_back(region);
    }

    return true;
}

bool NamePools::loadFromFile(const std::string& path, NamePools& out, std::string& error)
{
    error.clear();
    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }
    return fromJson(root, out, error);
}

const std::vector<std::string>* NamePools::pool(TerritoryId region) const
{
    for (const Entry& entry : entries_)
    {
        if (entry.region == region)
        {
            return &entry.names;
        }
    }
    return nullptr;
}

} // namespace fossynd::core
