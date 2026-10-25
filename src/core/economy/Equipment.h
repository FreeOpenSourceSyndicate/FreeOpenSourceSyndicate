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
    // A drug **bar ceiling** (ADR-0043): it does not grant a charge (a gadget) or
    // a one-off stat change (a cybernetic); it raises the maximum a squad member
    // may regulate a drug bar to. The three bars are `adrenalin`,
    // `intelligence`, and `physical`. This replaces the single-dose `"drugs"`
    // consumable of ADR-0028 (its drug half; the scanner half is unaffected).
    Drug,
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

    // Drug only (`kind == Drug`, ADR-0043): the bar this item raises and the
    // maximum level the bar may be regulated to (`0 < ceiling <= 1`). Both are
    // ignored for every other kind. A ceiling is required to name a real bar and
    // to be gated by research, so a bought ceiling always deploys.
    std::string bar;      // "adrenalin" | "intelligence" | "physical"
    float ceiling = 1.0f;
};

// The maximum each drug bar may be raised to, for one squad slot (ADR-0043).
// `0.0f` is the **sentinel** for "the metagame grants no ceiling for this bar"
// (a real ceiling is always `> 0`), so a slot with no ceiling item returns an
// all-zero value. How the tactical bar model treats an absent ceiling is its own
// default (the `LoadoutSlot`/`Mission` half is a Rule 4 request, #499); this
// struct only reports what the metagame sells.
struct DrugCeilings
{
    float adrenalin = 0.0f;
    float intelligence = 0.0f;
    float physical = 0.0f;

    bool any() const
    {
        return adrenalin > 0.0f || intelligence > 0.0f || physical > 0.0f;
    }
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

    // Equips a *catalogue* item in the slot, respecting the deployed-loadout rule:
    // a slot deploys at most one weapon, one gadget, and one cybernetic
    // (LoadoutSlot), so a new item replaces the slot's item of the same kind. This
    // is what the shop uses, so buying a better weapon actually replaces the old
    // one instead of being appended and then silently ignored at deploy time.
    // Returns false (no change) for an unknown syndicate or slot, an empty id, or
    // an id not in `catalogue`.
    bool equip(int syndicate, int slot, const std::string& itemId,
               const EquipmentCatalogue& catalogue);

    // Empties the slot's loadout. A no-op for an unknown syndicate or slot.
    void clear(int syndicate, int slot);

    // The drug bar ceilings a slot's equipped items grant (ADR-0043). A slot with
    // no ceiling item returns an all-zero `DrugCeilings` (no bar may be raised).
    // When two ceilings name the same bar the higher one wins, so equipping an
    // upgrade can never lower a bar. The tactical deploy path reads this to know
    // how high each bar may be regulated (the LoadoutSlot/Mission half is a Rule 4
    // request to `focus: tactical`).
    DrugCeilings ceilings(int syndicate, int slot, const EquipmentCatalogue& catalogue) const;

private:
    using Slots = std::array<std::vector<std::string>, static_cast<std::size_t>(kSquadSlots)>;

    std::vector<Slots> loadouts_; // indexed by syndicate; grown on demand
};

} // namespace fossynd::core
