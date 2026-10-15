/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/economy/Research.h"

#include <algorithm>
#include <functional>

namespace fossynd::core
{
namespace
{

bool contains(const std::vector<std::string>& values, const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

// Ensures `values` has at least `count` elements.
void ensureSize(std::vector<ResearchState>& values, std::size_t count)
{
    if (values.size() < count)
    {
        values.resize(count);
    }
}

} // namespace

bool ResearchTree::fromJson(const Json& root, ResearchTree& out, std::string& error)
{
    error.clear();
    out.nodes_.clear();

    if (!root.isObject())
    {
        error = "research tree root must be a JSON object";
        return false;
    }
    const Json& array = root["nodes"];
    if (!array.isArray())
    {
        error = "'nodes' must be an array";
        return false;
    }

    for (std::size_t i = 0; i < array.size(); ++i)
    {
        const Json& node = array[i];
        if (!node.isObject())
        {
            error = "each research node must be an object";
            return false;
        }

        ResearchNode entry;
        entry.id = node.has("id") ? node["id"].asString() : std::string();
        if (entry.id.empty())
        {
            error = "each research node needs a non-empty 'id'";
            return false;
        }
        const std::string where = "research node '" + entry.id + "'";
        if (out.find(entry.id) != nullptr)
        {
            error = where + ": duplicate id";
            return false;
        }

        entry.name = node.has("name") ? node["name"].asString() : std::string();
        if (entry.name.empty())
        {
            error = where + ": missing non-empty 'name'";
            return false;
        }
        entry.cost = node["cost"].asFloat(0.0f);
        entry.timeSeconds = node["time"].asFloat(0.0f);
        entry.unlocks = node.has("unlocks") ? node["unlocks"].asString() : std::string();
        if (entry.cost < 0.0f)
        {
            error = where + ": 'cost' must not be negative";
            return false;
        }
        if (entry.timeSeconds < 0.0f)
        {
            error = where + ": 'time' must not be negative";
            return false;
        }

        const Json& requiresNode = node["requires"];
        if (!requiresNode.isNull())
        {
            if (!requiresNode.isArray())
            {
                error = where + ": 'requires' must be an array";
                return false;
            }
            for (std::size_t r = 0; r < requiresNode.size(); ++r)
            {
                const std::string id = requiresNode[r].asString();
                if (id.empty())
                {
                    error = where + ": each prerequisite must be a non-empty string";
                    return false;
                }
                if (contains(entry.prerequisites, id))
                {
                    error = where + ": duplicate prerequisite '" + id + "'";
                    return false;
                }
                entry.prerequisites.push_back(id);
            }
        }

        out.nodes_.push_back(std::move(entry));
    }

    // Prerequisites must resolve, and must not form a cycle (which would make
    // part of the tree unreachable).
    for (const ResearchNode& node : out.nodes_)
    {
        for (const std::string& id : node.prerequisites)
        {
            if (out.find(id) == nullptr)
            {
                error = "research node '" + node.id + "': unknown prerequisite '" + id + "'";
                return false;
            }
            if (id == node.id)
            {
                error = "research node '" + node.id + "' requires itself";
                return false;
            }
        }
    }
    if (!out.validate(error))
    {
        return false;
    }
    return true;
}

bool ResearchTree::loadFromFile(const std::string& path, ResearchTree& out, std::string& error)
{
    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }
    return fromJson(root, out, error);
}

const ResearchNode* ResearchTree::find(const std::string& id) const
{
    for (const ResearchNode& node : nodes_)
    {
        if (node.id == id)
        {
            return &node;
        }
    }
    return nullptr;
}

bool ResearchTree::isAvailable(const std::string& id, const ResearchState& state) const
{
    const ResearchNode* node = find(id);
    if (node == nullptr)
    {
        return false;
    }
    for (const std::string& prerequisite : node->prerequisites)
    {
        if (!state.isCompleted(prerequisite))
        {
            return false;
        }
    }
    return true;
}

bool ResearchTree::validate(std::string& error) const
{
    error.clear();

    // Depth-first search over the prerequisite graph. A node re-entered while it
    // is still on the current path is part of a cycle.
    enum class Mark
    {
        Unvisited,
        OnPath,
        Done,
    };
    std::vector<Mark> marks(nodes_.size(), Mark::Unvisited);

    std::function<bool(const ResearchNode&)> visit = [&](const ResearchNode& node) -> bool {
        const std::size_t index = static_cast<std::size_t>(&node - nodes_.data());
        if (marks[index] == Mark::OnPath)
        {
            error = "research prerequisite cycle includes '" + node.id + "'";
            return false;
        }
        if (marks[index] == Mark::Done)
        {
            return true;
        }
        marks[index] = Mark::OnPath;
        for (const std::string& prerequisite : node.prerequisites)
        {
            const ResearchNode* next = find(prerequisite);
            if (next == nullptr)
            {
                error = "research node '" + node.id + "': unknown prerequisite '" + prerequisite +
                        "'";
                return false;
            }
            if (!visit(*next))
            {
                return false;
            }
        }
        marks[index] = Mark::Done;
        return true;
    };

    for (const ResearchNode& node : nodes_)
    {
        if (!visit(node))
        {
            return false;
        }
    }
    return true;
}

bool ResearchState::isCompleted(const std::string& id) const
{
    return contains(completed, id);
}

bool Research::begin(int syndicate, const std::string& id, const ResearchTree& tree, Economy& economy)
{
    if (syndicate < 0)
    {
        return false;
    }
    const ResearchNode* node = tree.find(id);
    if (node == nullptr)
    {
        return false;
    }
    const ResearchState& current = state(syndicate);
    if (!current.inProgress.empty() || current.isCompleted(id))
    {
        return false;
    }
    if (!tree.isAvailable(id, current))
    {
        return false;
    }
    if (!economy.spend(syndicate, node->cost))
    {
        return false;
    }

    ResearchState& target = mutableState(syndicate);
    target.inProgress = id;
    target.remainingSeconds = node->timeSeconds;
    return true;
}

void Research::tick(float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    for (ResearchState& state : states_)
    {
        if (state.inProgress.empty())
        {
            continue;
        }
        state.remainingSeconds -= dt;
        if (state.remainingSeconds <= 0.0f)
        {
            state.completed.push_back(state.inProgress);
            state.inProgress.clear();
            state.remainingSeconds = 0.0f;
        }
    }
}

bool Research::completed(int syndicate, const std::string& id) const
{
    return state(syndicate).isCompleted(id);
}

const ResearchState& Research::state(int syndicate) const
{
    static const ResearchState empty;
    if (syndicate < 0 || static_cast<std::size_t>(syndicate) >= states_.size())
    {
        return empty;
    }
    return states_[static_cast<std::size_t>(syndicate)];
}

ResearchState& Research::mutableState(int syndicate)
{
    ensureSize(states_, static_cast<std::size_t>(syndicate) + 1);
    return states_[static_cast<std::size_t>(syndicate)];
}

} // namespace fossynd::core
