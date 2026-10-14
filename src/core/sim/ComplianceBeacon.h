/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

namespace fossynd::core
{

// The ComplianceBeacon (ADR-0018): a device that turns a civilian into a follower
// instead of killing them. Renamed from the original game's gadget name, which
// docs/LEGAL.md hard rule 4 forbids. All original values; balance lives here until
// a data format is fixed (ADR-0003).
struct ComplianceBeacon
{
    // Maximum range, in tiles (straight-line).
    float range = 4.0f;

    // Uses per second.
    float fireRate = 1.0f;

    // Seconds between uses.
    float cooldown() const { return fireRate > 0.0f ? 1.0f / fireRate : 1.0f; }
};

inline ComplianceBeacon makeComplianceBeacon()
{
    return ComplianceBeacon{4.0f, 1.0f};
}

} // namespace fossynd::core
