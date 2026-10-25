/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace fossynd::core
{

// The metagame's per-slot drug bar ceilings (ADR-0043, issue #518), mirrored here
// so the mission loader can consume them without depending on `core/economy`
// (the loader sits *below* the metagame — golden rule 1). It is field-for-field
// the metagame's `DrugCeilings` (`core/economy/Equipment.h`), which the deploy
// path copies across; `0.0f` is the sentinel for "the metagame grants no ceiling
// for this bar", so an all-zero value leaves the bar unregulated.
struct DrugBarCeilings
{
    float adrenalin = 0.0f;
    float intelligence = 0.0f;
    float physical = 0.0f;
};

// A squad loadout chosen before a mission starts (Milestone B deployment,
// request R1 of docs/METAGAME.md §7). Engine-agnostic and **data-only**: it
// names catalogue ids, never a resolved `Weapon`, so `core/sim` stays free of
// the content loader and there is no include cycle.
//
// Slot `i` maps to the i-th **player** agent of the mission, in authored order.
// The original squad is four, so a loadout has at most `kMaxSlots` entries.
// An empty id (and a missing slot) means "keep the mission's authored default",
// so an absent loadout leaves every existing mission byte-for-byte unchanged
// (AGENTS.md §1b tolerance).
struct LoadoutSlot
{
    // Shared weapon catalogue id (`assets/data/weapons.json`), e.g. "rifle".
    // Empty keeps the mission's authored weapon for this slot.
    std::string weaponId;

    // Gadget id, from the same closed vocabulary the equipment catalogue uses
    // (`assets/data/world/equipment.json`): "medkit", "shield", "scanner" or
    // "drugs". Empty keeps the slot's authored (zero) gadget count.
    std::string gadgetId;

    // Cybernetic upgrade id, from the equipment catalogue's `kind: "cybernetic"`
    // items: "legs" (speed), "arms" (accuracy), "chest" (health), "eyes"
    // (perception), or "targeting-implant" (accuracy). Empty applies no upgrade,
    // so a passive upgrade is optional per slot. Unlike a weapon/gadget, a
    // cybernetic is a permanent stat change applied once at load
    // (`Mission::applyCybernetic`).
    std::string cyberneticId;
};

// How many charges of a gadget a slot carries when it is equipped. Gadgets are
// single-use in the tactical layer (a medikit heals once, a shield absorbs one
// hit, a scanner pulse or a drug dose lasts one activation), so a bought gadget
// ships one charge.
inline constexpr int kGadgetCharges = 1;

// `gadgetChargesForId` returns this for an id that is not a known gadget.
inline constexpr int kUnknownGadget = -1;

// Resolves a gadget id to the number of charges it grants, or `kUnknownGadget`
// when the id is not a known gadget. This is the closed vocabulary of the
// equipment catalogue's `kind: "gadget"` items; keeping it here (rather than
// depending on `core/economy`) lets the mission loader stay below the metagame
// and rejects a typo instead of silently deploying nothing.
inline int gadgetChargesForId(const std::string& id)
{
    if (id == "medkit" || id == "shield" || id == "scanner" || id == "drugs")
    {
        return kGadgetCharges;
    }
    return kUnknownGadget;
}

struct PlayerLoadout
{
    // The original squad is four agents (`Equipment::kSquadSlots`).
    static constexpr std::size_t kMaxSlots = 4;

    std::vector<LoadoutSlot> slots;

    // The metagame-bought drug bar ceilings, parallel to `slots` (ADR-0043,
    // #518): `ceilings[i]` caps the bars of the agent that `slots[i]` loads. An
    // empty vector (or a slot past its end) leaves every bar unregulated, so an
    // absent value changes nothing. `applyLoadout` applies a non-zero field via
    // `Mission::setDrugCeiling`.
    std::vector<DrugBarCeilings> ceilings;
};

} // namespace fossynd::core
