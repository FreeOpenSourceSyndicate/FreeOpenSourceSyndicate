/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>
#include <vector>

#include "core/data/Json.h"
#include "core/economy/WorldMap.h"
#include "core/util/Rng.h"

namespace fossynd::core
{

// Tuning for the metagame economy (Milestone B, docs/METAGAME.md). Data-driven
// (ADR-0003): these are the values a campaign file sets, not hard-coded balance.
struct EconomyConfig
{
    int maxTaxLevel = 5;               // taxLevel upper bound
    float baseIncome = 0.0f;           // funds per owned territory per tick, at tax 0
    float incomePerTaxLevel = 100.0f;  // funds per tax level per tick
    int revoltThresholdLevel = 3;      // tax at/above this can trigger a revolt
    float revoltChancePerTick = 0.02f; // probability per tick, per over-taxed territory
    float tickSeconds = 30.0f;         // in-game seconds between income ticks
};

// The metagame economy: a per-syndicate funds ledger, income from owned
// territories, and the revolt risk of over-taxation (Milestone B,
// docs/METAGAME.md). Engine-free and deterministic: the only randomness is the
// injected Rng, so a seed reproduces a campaign (ADR-0002, golden rule 2).
class Economy
{
public:
    // Parses an `economy` object (or a campaign root). Missing fields keep their
    // defaults. Returns false and sets `error` on a schema error.
    static bool fromJson(const Json& root, EconomyConfig& out, std::string& error);

    void configure(const EconomyConfig& config) { config_ = config; }
    EconomyConfig& config() { return config_; }
    const EconomyConfig& config() const { return config_; }

    // Advances the economy by `dt` in-game seconds: every `tickSeconds` it adds
    // income to each syndicate from its owned territories, then rolls a revolt
    // for each territory taxed at or above `revoltThresholdLevel`. A revolt
    // flips the territory back to neutral and resets its tax to 0. Deterministic
    // in `rng`.
    void tick(float dt, WorldMap& map, Rng& rng);

    float funds(int syndicate) const;
    void addFunds(int syndicate, float amount);
    bool canAfford(int syndicate, float amount) const;
    bool spend(int syndicate, float amount); // false (no change) if insufficient

    int tickCount() const { return tickCount_; } // ticks processed; for tests

private:
    // Adds one tick of income for every syndicate and rolls revolts.
    void processTick(WorldMap& map, Rng& rng);

    EconomyConfig config_;
    std::vector<float> funds_; // indexed by syndicate; grown on demand
    float accumulator_ = 0.0f; // in-game seconds since the last income tick
    int tickCount_ = 0;
};

} // namespace fossynd::core
