/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "core/data/Json.h"
#include "core/economy/Economy.h"
#include "core/economy/Research.h"

namespace fossynd::core
{

enum class EquipmentKind
{
    Weapon,
    Gadget,
    Cybernetic,
};

// One purchasable item in the equipment catalogue. Data-driven (ADR-0003);
// Milestone B, docs/METAGAME.md.
struct EquipmentItem
{
    std::string id;                // unique, non-empty
    EquipmentKind kind = EquipmentKind::Weapon;
    std::string name;              // display name
    float cost = 0.0f;             // funds to buy
    std::string requiresResearch;  // research node gating it; empty = always available
};

// The equipment catalogue (`assets/data/world/equipment.json`). Engine-free and
// data-driven (ADR-0002, ADR-0003).
class EquipmentCatalogue
{
public:
    static bool fromJson(const Json& root, EquipmentCatalogue& out, std::string& error);
    static bool loadFromFile(const std::string& path, EquipmentCatalogue& out, std::string& error);

    std::size_t size() const { return items_.size(); }
    const std::vector<EquipmentItem>& items() const { return items_; }

    // Returns the item with `id`, or nullptr if there is no such item.
    const EquipmentItem* find(const std::string& id) const;

private:
    std::vector<EquipmentItem> items_;
};

// The metagame equipment system: buying items (gated by research and funds) and
// per-syndicate squad loadouts. Engine-free (ADR-0002); Milestone B,
// docs/METAGAME.md.
class Equipment
{
public:
    // The original squad is four agents.
    static constexpr int kSquadSlots = 4;

    // Buys `id` for `syndicate`: checks the research gate (via `research`), then
    // affordability (via `economy`), then spends. Returns false (no change) if
    // any check fails.
    bool buy(int syndicate, const std::string& id, const EquipmentCatalogue& catalogue,
             const Research& research, Economy& economy);

    // The item ids assigned to a syndicate's agent slot (empty if the syndicate
    // or slot is unknown).
    std::vector<std::string> loadout(int syndicate, int slot) const;

    // Adds `itemId` to the slot's loadout. A no-op for an unknown syndicate, an
    // out-of-range slot, an empty id, or an item already in the slot.
    void assign(int syndicate, int slot, const std::string& itemId);

    // Empties the slot's loadout. A no-op for an unknown syndicate or slot.
    void clear(int syndicate, int slot);

private:
    using Slots = std::array<std::vector<std::string>, static_cast<std::size_t>(kSquadSlots)>;

    std::vector<Slots> loadouts_; // indexed by syndicate; grown on demand
};

} // namespace fossynd::core
