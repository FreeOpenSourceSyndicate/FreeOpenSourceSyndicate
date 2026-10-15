/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/economy/Economy.h"

namespace fossynd::core
{
namespace
{

// Ensures `values` has at least `count` elements.
void ensureSize(std::vector<float>& values, std::size_t count)
{
    if (values.size() < count)
    {
        values.resize(count, 0.0f);
    }
}

} // namespace

bool Economy::fromJson(const Json& root, EconomyConfig& out, std::string& error)
{
    error.clear();
    if (!root.isObject())
    {
        error = "economy config must be a JSON object";
        return false;
    }

    out.maxTaxLevel = root["maxTaxLevel"].asInt(out.maxTaxLevel);
    out.baseIncome = root["baseIncome"].asFloat(out.baseIncome);
    out.incomePerTaxLevel = root["incomePerTaxLevel"].asFloat(out.incomePerTaxLevel);
    out.revoltThresholdLevel = root["revoltThresholdLevel"].asInt(out.revoltThresholdLevel);
    out.revoltChancePerTick = root["revoltChancePerTick"].asFloat(out.revoltChancePerTick);
    out.tickSeconds = root["tickSeconds"].asFloat(out.tickSeconds);

    if (out.maxTaxLevel < 0)
    {
        error = "'maxTaxLevel' must be >= 0";
        return false;
    }
    if (out.baseIncome < 0.0f)
    {
        error = "'baseIncome' must not be negative";
        return false;
    }
    if (out.incomePerTaxLevel < 0.0f)
    {
        error = "'incomePerTaxLevel' must not be negative";
        return false;
    }
    if (out.revoltThresholdLevel < 0)
    {
        error = "'revoltThresholdLevel' must be >= 0";
        return false;
    }
    if (out.revoltChancePerTick < 0.0f || out.revoltChancePerTick > 1.0f)
    {
        error = "'revoltChancePerTick' must be between 0 and 1";
        return false;
    }
    if (out.tickSeconds <= 0.0f)
    {
        error = "'tickSeconds' must be greater than 0";
        return false;
    }
    return true;
}

void Economy::tick(float dt, WorldMap& map, Rng& rng)
{
    if (dt <= 0.0f || config_.tickSeconds <= 0.0f)
    {
        return;
    }
    accumulator_ += dt;
    while (accumulator_ >= config_.tickSeconds)
    {
        accumulator_ -= config_.tickSeconds;
        processTick(map, rng);
    }
}

void Economy::processTick(WorldMap& map, Rng& rng)
{
    // Income: each syndicate earns from the territories it owns, scaled by each
    // territory's tax level. Territories are visited in id order for determinism.
    int maxOwner = -1;
    for (const Territory& territory : map.territories())
    {
        if (territory.owner > maxOwner)
        {
            maxOwner = territory.owner;
        }
    }
    if (maxOwner >= 0)
    {
        ensureSize(funds_, static_cast<std::size_t>(maxOwner) + 1);
        for (const Territory& territory : map.territories())
        {
            if (territory.owner < 0)
            {
                continue;
            }
            const float income = config_.baseIncome +
                                 config_.incomePerTaxLevel * static_cast<float>(territory.taxLevel);
            funds_[static_cast<std::size_t>(territory.owner)] += income;
        }
    }

    // Revolt: an over-taxed territory can flip back to neutral. Rolled in id
    // order, so the Rng stream depends only on the map and the seed.
    for (const Territory& territory : map.territories())
    {
        if (territory.owner < 0 || territory.taxLevel < config_.revoltThresholdLevel)
        {
            continue;
        }
        if (rng.chance(config_.revoltChancePerTick))
        {
            map.setOwner(territory.id, -1);
            map.setTaxLevel(territory.id, 0);
        }
    }

    ++tickCount_;
}

float Economy::funds(int syndicate) const
{
    if (syndicate < 0 || static_cast<std::size_t>(syndicate) >= funds_.size())
    {
        return 0.0f;
    }
    return funds_[static_cast<std::size_t>(syndicate)];
}

void Economy::addFunds(int syndicate, float amount)
{
    if (syndicate < 0)
    {
        return;
    }
    ensureSize(funds_, static_cast<std::size_t>(syndicate) + 1);
    funds_[static_cast<std::size_t>(syndicate)] += amount;
}

bool Economy::canAfford(int syndicate, float amount) const
{
    return amount <= 0.0f || funds(syndicate) >= amount;
}

bool Economy::spend(int syndicate, float amount)
{
    if (amount < 0.0f || !canAfford(syndicate, amount))
    {
        return false;
    }
    if (amount == 0.0f)
    {
        return true;
    }
    ensureSize(funds_, static_cast<std::size_t>(syndicate) + 1);
    funds_[static_cast<std::size_t>(syndicate)] -= amount;
    return true;
}

} // namespace fossynd::core
