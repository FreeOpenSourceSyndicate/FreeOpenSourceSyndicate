/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>

namespace foss::core
{

// A weapon's gameplay parameters. All original values; no ties to any existing
// game's data. Balance numbers live here until a data file format is fixed
// (ADR-0003).
struct Weapon
{
    std::string name = "Pistol";

    // Damage applied per successful hit.
    float damage = 12.0f;

    // Maximum range, in tiles (straight-line).
    float range = 6.0f;

    // Shots per second.
    float fireRate = 2.0f;

    // Probability [0, 1] that a shot within range hits at point-blank.
    // Accuracy falls off toward maximum range.
    float accuracy = 0.75f;

    // Seconds between firing decisions by the AI (reaction cadence).
    float cooldown() const { return fireRate > 0.0f ? 1.0f / fireRate : 1.0f; }
};

// A small set of distinct weapons for the demo. Names are generic and original.
inline Weapon makePistol()
{
    return Weapon{"Pistol", 10.0f, 6.0f, 2.5f, 0.72f};
}

inline Weapon makeRifle()
{
    return Weapon{"Rifle", 8.0f, 9.0f, 4.0f, 0.65f};
}

inline Weapon makeUzi()
{
    return Weapon{"Uzi", 5.0f, 5.0f, 9.0f, 0.55f};
}

} // namespace foss::core
