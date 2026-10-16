/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>

namespace fossynd::core
{

// A cybernetic upgrade's effect on one agent's combat stats. An upgrade is a
// passive, permanent modifier applied once at mission load (Milestone A
// cybernetics: the original's legs/arms/chest/eyes). Engine-agnostic and
// data-only: it holds multipliers, never a resolved `Agent`, so `core/sim` stays
// free of the content loader (ADR-0031).
//
// Every field is a **multiplier** (1.0 = unchanged), so a cybernetic scales with
// the agent's authored stat: a chest upgrade raises a 100-health agent to 125 and
// a 200-health one to 250. The applied accuracy is clamped to 1.0 (see
// `Mission::applyCybernetic`).
struct CyberneticEffect
{
    float speedMultiplier = 1.0f;    // multiplier on movement speed (legs)
    float accuracyMultiplier = 1.0f; // multiplier on weapon hit chance (arms)
    float healthMultiplier = 1.0f;   // multiplier on maximum health (chest)
    float sightMultiplier = 1.0f;    // multiplier on sight range, tiles (eyes)
};

// Resolves a cybernetic id to its stat modifiers. Returns false (leaving `out`
// at 1.0 across the board) when `id` is not a known cybernetic, so a typo is
// rejected instead of silently deploying nothing.
//
// This is the closed vocabulary of the equipment catalogue's `kind: "cybernetic"`
// items (`assets/data/world/equipment.json`): the original's four upgrades plus
// the already-shipped `targeting-implant` (an accuracy upgrade). Balance lives
// here until a data file format for cybernetic modifiers is fixed (ADR-0003);
// adding an upgrade is one arm here plus one catalogue entry, not a schema
// change.
inline bool cyberneticEffectForId(const std::string& id, CyberneticEffect& out)
{
    if (id == "legs")
    {
        out = CyberneticEffect{1.20f, 1.0f, 1.0f, 1.0f};
        return true;
    }
    if (id == "arms")
    {
        out = CyberneticEffect{1.0f, 1.15f, 1.0f, 1.0f};
        return true;
    }
    if (id == "chest")
    {
        out = CyberneticEffect{1.0f, 1.0f, 1.25f, 1.0f};
        return true;
    }
    if (id == "eyes")
    {
        out = CyberneticEffect{1.0f, 1.0f, 1.0f, 1.30f};
        return true;
    }
    if (id == "targeting-implant")
    {
        out = CyberneticEffect{1.0f, 1.10f, 1.0f, 1.0f};
        return true;
    }
    return false;
}

} // namespace fossynd::core
