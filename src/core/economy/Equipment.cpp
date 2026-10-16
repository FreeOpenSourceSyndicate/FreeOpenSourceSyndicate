/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/economy/Equipment.h"

#include <algorithm>

namespace fossynd::core
{
namespace
{

bool parseKind(const std::string& text, EquipmentKind& out)
{
    if (text == "weapon")
    {
        out = EquipmentKind::Weapon;
        return true;
    }
    if (text == "gadget")
    {
        out = EquipmentKind::Gadget;
        return true;
    }
    if (text == "cybernetic")
    {
        out = EquipmentKind::Cybernetic;
        return true;
    }
    return false;
}

bool validSlot(int slot)
{
    return slot >= 0 && slot < Equipment::kSquadSlots;
}

} // namespace

bool EquipmentCatalogue::fromJson(const Json& root, EquipmentCatalogue& out, std::string& error)
{
    error.clear();
    out.items_.clear();

    if (!root.isObject())
    {
        error = "equipment catalogue root must be a JSON object";
        return false;
    }
    const Json& array = root["items"];
    if (!array.isArray())
    {
        error = "'items' must be an array";
        return false;
    }

    for (std::size_t i = 0; i < array.size(); ++i)
    {
        const Json& node = array[i];
        if (!node.isObject())
        {
            error = "each equipment item must be an object";
            return false;
        }

        EquipmentItem item;
        item.id = node.has("id") ? node["id"].asString() : std::string();
        if (item.id.empty())
        {
            error = "each equipment item needs a non-empty 'id'";
            return false;
        }
        const std::string where = "equipment item '" + item.id + "'";
        if (out.find(item.id) != nullptr)
        {
            error = where + ": duplicate id";
            return false;
        }

        const std::string kind = node.has("kind") ? node["kind"].asString() : std::string("weapon");
        if (!parseKind(kind, item.kind))
        {
            error = where + ": unknown kind '" + kind + "'";
            return false;
        }

        item.name = node.has("name") ? node["name"].asString() : std::string();
        if (item.name.empty())
        {
            error = where + ": missing non-empty 'name'";
            return false;
        }
        item.cost = node["cost"].asFloat(0.0f);
        if (item.cost < 0.0f)
        {
            error = where + ": 'cost' must not be negative";
            return false;
        }
        item.requiresResearch =
            node.has("requiresResearch") ? node["requiresResearch"].asString() : std::string();

        out.items_.push_back(std::move(item));
    }
    return true;
}

bool EquipmentCatalogue::loadFromFile(const std::string& path, EquipmentCatalogue& out,
                                      std::string& error)
{
    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }
    return fromJson(root, out, error);
}

const EquipmentItem* EquipmentCatalogue::find(const std::string& id) const
{
    for (const EquipmentItem& item : items_)
    {
        if (item.id == id)
        {
            return &item;
        }
    }
    return nullptr;
}

bool Equipment::buy(int syndicate, const std::string& id, const EquipmentCatalogue& catalogue,
                    const Research& research, Economy& economy)
{
    if (syndicate < 0)
    {
        return false;
    }
    const EquipmentItem* item = catalogue.find(id);
    if (item == nullptr)
    {
        return false;
    }
    if (!item->requiresResearch.empty() && !research.completed(syndicate, item->requiresResearch))
    {
        return false;
    }
    return economy.spend(syndicate, item->cost);
}

std::vector<std::string> Equipment::loadout(int syndicate, int slot) const
{
    if (syndicate < 0 || static_cast<std::size_t>(syndicate) >= loadouts_.size() || !validSlot(slot))
    {
        return {};
    }
    return loadouts_[static_cast<std::size_t>(syndicate)][static_cast<std::size_t>(slot)];
}

void Equipment::assign(int syndicate, int slot, const std::string& itemId)
{
    if (syndicate < 0 || !validSlot(slot) || itemId.empty())
    {
        return;
    }
    if (loadouts_.size() < static_cast<std::size_t>(syndicate) + 1)
    {
        loadouts_.resize(static_cast<std::size_t>(syndicate) + 1);
    }
    std::vector<std::string>& items = loadouts_[static_cast<std::size_t>(syndicate)]
                                               [static_cast<std::size_t>(slot)];
    if (std::find(items.begin(), items.end(), itemId) == items.end())
    {
        items.push_back(itemId);
    }
}

void Equipment::clear(int syndicate, int slot)
{
    if (syndicate < 0 || static_cast<std::size_t>(syndicate) >= loadouts_.size() || !validSlot(slot))
    {
        return;
    }
    loadouts_[static_cast<std::size_t>(syndicate)][static_cast<std::size_t>(slot)].clear();
}

bool Equipment::equip(int syndicate, int slot, const std::string& itemId,
                      const EquipmentCatalogue& catalogue)
{
    if (syndicate < 0 || !validSlot(slot) || itemId.empty())
    {
        return false;
    }
    const EquipmentItem* item = catalogue.find(itemId);
    if (item == nullptr)
    {
        return false;
    }
    if (loadouts_.size() < static_cast<std::size_t>(syndicate) + 1)
    {
        loadouts_.resize(static_cast<std::size_t>(syndicate) + 1);
    }
    std::vector<std::string>& items = loadouts_[static_cast<std::size_t>(syndicate)]
                                               [static_cast<std::size_t>(slot)];
    if (std::find(items.begin(), items.end(), itemId) != items.end())
    {
        return true; // already equipped; nothing to change
    }
    // A LoadoutSlot carries one weapon, one gadget, and one cybernetic, so a new
    // item of a kind replaces the slot's existing item of that kind rather than
    // being appended and then ignored at deploy time.
    for (auto it = items.begin(); it != items.end(); ++it)
    {
        const EquipmentItem* existing = catalogue.find(*it);
        if (existing != nullptr && existing->kind == item->kind)
        {
            *it = itemId;
            return true;
        }
    }
    items.push_back(itemId);
    return true;
}

} // namespace fossynd::core
