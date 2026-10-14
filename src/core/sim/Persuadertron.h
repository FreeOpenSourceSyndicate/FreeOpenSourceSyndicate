/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

namespace fossynd::core
{

// The Persuadertron: a device that turns a civilian into a follower instead of
// killing them. All original values; balance lives here until a data format is
// fixed (ADR-0003).
struct Persuadertron
{
    // Maximum range, in tiles (straight-line).
    float range = 4.0f;

    // Uses per second.
    float fireRate = 1.0f;

    // Seconds between uses.
    float cooldown() const { return fireRate > 0.0f ? 1.0f / fireRate : 1.0f; }
};

inline Persuadertron makePersuadertron()
{
    return Persuadertron{4.0f, 1.0f};
}

} // namespace fossynd::core
