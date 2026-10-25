/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/economy/Economy.h"
#include "core/economy/WorldMap.h"

using namespace fossynd::core;

namespace
{

// Three territories, all owned by syndicate 0, none taxed.
const char* kWorldJson = R"({
  "territories": [
    { "id": 0, "name": "A", "neighbours": [1] },
    { "id": 1, "name": "B", "neighbours": [0, 2] },
    { "id": 2, "name": "C", "neighbours": [1] }
  ]
})";

WorldMap ownedMap(int owner)
{
    std::string error;
    const Json root = Json::parse(kWorldJson, error);
    FOSSYND_CHECK(error.empty());
    WorldMap map;
    FOSSYND_CHECK(WorldMap::fromJson(root, map, error));
    for (TerritoryId id = 0; id < static_cast<TerritoryId>(map.size()); ++id)
    {
        map.setOwner(id, owner);
    }
    return map;
}

// The economy config the shipped campaign actually ships. Loading it here (not
// hard-coding a copy) means the balance pins below fail if campaign.json drifts.
EconomyConfig shippedEconomyConfig()
{
    std::string error;
    Json root;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/campaign.json";
    FOSSYND_CHECK(Json::parseFile(path, root, error));
    FOSSYND_CHECK(error.empty());

    EconomyConfig config;
    FOSSYND_CHECK(Economy::fromJson(root, config, error));
    FOSSYND_CHECK(error.empty());
    return config;
}

// Expected total income from one territory taxed at `tax` over `ticks` ticks
// under `config`. A territory earns every tick until a revolt flips it neutral,
// which can happen only at or above `revoltThresholdLevel` and does so with
// probability `revoltChancePerTick` per tick. Below the threshold it never
// rolls, so the sum is simply the per-tick income times the tick count.
float expectedIncome(const EconomyConfig& config, int tax, int ticks)
{
    const float perTick = config.baseIncome + config.incomePerTaxLevel * static_cast<float>(tax);
    if (tax < config.revoltThresholdLevel)
    {
        return perTick * static_cast<float>(ticks);
    }
    float total = 0.0f;
    float survival = 1.0f;
    for (int k = 0; k < ticks; ++k)
    {
        total += perTick * survival;
        survival *= 1.0f - config.revoltChancePerTick;
    }
    return total;
}

} // namespace

FOSSYND_TEST(Economy, ConfigParsesAndDefaults)
{
    std::string error;
    const Json root = Json::parse(R"({
      "maxTaxLevel": 4, "baseIncome": 5, "incomePerTaxLevel": 50,
      "revoltThresholdLevel": 2, "revoltChancePerTick": 0.5, "tickSeconds": 10
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    EconomyConfig config;
    FOSSYND_CHECK(Economy::fromJson(root, config, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(config.maxTaxLevel, 4);
    FOSSYND_CHECK_NEAR(config.baseIncome, 5.0f);
    FOSSYND_CHECK_NEAR(config.incomePerTaxLevel, 50.0f);
    FOSSYND_CHECK_EQ(config.revoltThresholdLevel, 2);
    FOSSYND_CHECK_NEAR(config.revoltChancePerTick, 0.5f);
    FOSSYND_CHECK_NEAR(config.tickSeconds, 10.0f);

    // Missing fields keep their defaults.
    EconomyConfig defaults;
    const Json empty = Json::parse("{}", error);
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(Economy::fromJson(empty, defaults, error));
    FOSSYND_CHECK_EQ(defaults.maxTaxLevel, 5);
}

FOSSYND_TEST(Economy, ConfigRejectsBadValues)
{
    std::string error;
    const Json root = Json::parse(R"({ "tickSeconds": 0 })", error);
    FOSSYND_CHECK(error.empty());
    EconomyConfig config;
    FOSSYND_CHECK(!Economy::fromJson(root, config, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Economy, IncomeFromOwnedTerritories)
{
    EconomyConfig config;
    config.baseIncome = 10.0f;
    config.incomePerTaxLevel = 0.0f;
    config.tickSeconds = 30.0f;

    Economy economy;
    economy.configure(config);
    WorldMap map = ownedMap(0);
    Rng rng(1);

    economy.tick(30.0f, map, rng);
    FOSSYND_CHECK_EQ(economy.tickCount(), 1);
    // Three owned territories * 10.
    FOSSYND_CHECK_NEAR(economy.funds(0), 30.0f);
    FOSSYND_CHECK_NEAR(economy.funds(1), 0.0f);
}

FOSSYND_TEST(Economy, TaxLevelScalesIncome)
{
    EconomyConfig config;
    config.baseIncome = 0.0f;
    config.incomePerTaxLevel = 100.0f;
    config.tickSeconds = 30.0f;

    Economy economy;
    economy.configure(config);
    WorldMap map = ownedMap(0);
    map.setTaxLevel(0, 2); // 200 from territory 0
    map.setTaxLevel(1, 1); // 100 from territory 1
    Rng rng(1);

    economy.tick(30.0f, map, rng);
    FOSSYND_CHECK_NEAR(economy.funds(0), 300.0f);
}

FOSSYND_TEST(Economy, TickAccumulatesBeforeFiring)
{
    EconomyConfig config;
    config.baseIncome = 10.0f;
    config.tickSeconds = 30.0f;

    Economy economy;
    economy.configure(config);
    WorldMap map = ownedMap(0);
    Rng rng(1);

    economy.tick(20.0f, map, rng); // below one tick
    FOSSYND_CHECK_EQ(economy.tickCount(), 0);
    FOSSYND_CHECK_NEAR(economy.funds(0), 0.0f);

    economy.tick(20.0f, map, rng); // crosses 30 -> one tick
    FOSSYND_CHECK_EQ(economy.tickCount(), 1);
    FOSSYND_CHECK_NEAR(economy.funds(0), 30.0f);
}

FOSSYND_TEST(Economy, SpendAndAfford)
{
    Economy economy;
    economy.addFunds(0, 100.0f);

    FOSSYND_CHECK(economy.canAfford(0, 100.0f));
    FOSSYND_CHECK(!economy.canAfford(0, 101.0f));
    FOSSYND_CHECK(economy.spend(0, 40.0f));
    FOSSYND_CHECK_NEAR(economy.funds(0), 60.0f);
    FOSSYND_CHECK(!economy.spend(0, 1000.0f)); // insufficient: no change
    FOSSYND_CHECK_NEAR(economy.funds(0), 60.0f);
}

FOSSYND_TEST(Economy, RevoltFlipsOverTaxedTerritoryToNeutral)
{
    EconomyConfig config;
    config.baseIncome = 0.0f;
    config.incomePerTaxLevel = 0.0f;
    config.revoltThresholdLevel = 3;
    config.revoltChancePerTick = 1.0f; // guaranteed
    config.tickSeconds = 30.0f;

    Economy economy;
    economy.configure(config);
    WorldMap map = ownedMap(0);
    map.setTaxLevel(0, 3); // at the threshold
    map.setTaxLevel(1, 1); // below the threshold
    Rng rng(1);

    economy.tick(30.0f, map, rng);
    FOSSYND_CHECK_EQ(map.owner(0), -1); // revolted
    FOSSYND_CHECK_EQ(map.taxLevel(0), 0);
    FOSSYND_CHECK_EQ(map.owner(1), 0); // untouched
    FOSSYND_CHECK_EQ(map.owner(2), 0);
}

FOSSYND_TEST(Economy, NoRevoltBelowThreshold)
{
    EconomyConfig config;
    config.revoltThresholdLevel = 3;
    config.revoltChancePerTick = 1.0f; // guaranteed if it were to roll
    config.tickSeconds = 30.0f;

    Economy economy;
    economy.configure(config);
    WorldMap map = ownedMap(0);
    map.setTaxLevel(0, 2); // below the threshold
    Rng rng(1);

    economy.tick(30.0f, map, rng);
    FOSSYND_CHECK_EQ(map.owner(0), 0);
}

FOSSYND_TEST(Economy, IsDeterministicForASeed)
{
    EconomyConfig config;
    config.baseIncome = 10.0f;
    config.revoltThresholdLevel = 1;
    config.revoltChancePerTick = 0.5f;
    config.tickSeconds = 1.0f;

    Economy a;
    Economy b;
    a.configure(config);
    b.configure(config);
    WorldMap mapA = ownedMap(0);
    WorldMap mapB = ownedMap(0);
    mapA.setTaxLevel(0, 3);
    mapB.setTaxLevel(0, 3);
    Rng rngA(12345);
    Rng rngB(12345);

    for (int i = 0; i < 50; ++i)
    {
        a.tick(1.0f, mapA, rngA);
        b.tick(1.0f, mapB, rngB);
    }
    FOSSYND_CHECK_NEAR(a.funds(0), b.funds(0));
    for (TerritoryId id = 0; id < static_cast<TerritoryId>(mapA.size()); ++id)
    {
        FOSSYND_CHECK_EQ(mapA.owner(id), mapB.owner(id));
    }
}

// --- Balance pass pins (issue #419) ---------------------------------------
//
// These lock the shipped campaign's *pacing* relationships, not just the raw
// values: a later edit that silently makes the win a walkover or an unwinnable
// grind fails here instead of in a playtest. The numbers are justified in the
// issue/PR; the tests read the real campaign.json so they cannot drift.

FOSSYND_TEST(Economy, ShippedConfigHasTheTunedPacing)
{
    const EconomyConfig config = shippedEconomyConfig();

    FOSSYND_CHECK_EQ(config.maxTaxLevel, 5);
    FOSSYND_CHECK_NEAR(config.baseIncome, 0.0f);
    FOSSYND_CHECK_NEAR(config.incomePerTaxLevel, 60.0f);
    FOSSYND_CHECK_EQ(config.revoltThresholdLevel, 3);
    FOSSYND_CHECK_NEAR(config.revoltChancePerTick, 0.06f);
    FOSSYND_CHECK_NEAR(config.tickSeconds, 30.0f);
}

FOSSYND_TEST(Economy, ShippedIncomePerTickAtEachTaxLevel)
{
    // The income curve is the spine of the pacing: each tax level adds exactly
    // `incomePerTaxLevel` per owned territory per tick, and `baseIncome` is 0,
    // so tax 0 pays nothing (a region must be taxed to be worth holding).
    const EconomyConfig config = shippedEconomyConfig();

    FOSSYND_CHECK_NEAR(config.baseIncome, 0.0f);
    for (int tax = 0; tax <= config.maxTaxLevel; ++tax)
    {
        const float expected =
            static_cast<float>(tax) * config.incomePerTaxLevel; // one territory
        FOSSYND_CHECK_NEAR(expectedIncome(config, tax, 1), expected);
    }

    // The starting position (two owned territories, tax 2) and the fully taxed
    // end game bracket the whole campaign. The world map is twelve regions
    // (issue #448), so the end-game bracket uses the real region count.
    FOSSYND_CHECK_NEAR(expectedIncome(config, 2, 1) * 2.0f, 240.0f);
    FOSSYND_CHECK_NEAR(expectedIncome(config, 5, 1) * 12.0f, 3600.0f);
}

FOSSYND_TEST(Economy, ShippedRevoltIsARealTradeOffNotAFormality)
{
    // Taxing at or above the threshold is a genuine gamble, not free money: the
    // safe tax (threshold - 1) must beat the risky max tax over a medium horizon
    // while the risky tax must pay much better over a short one. If the revolt
    // chance were tiny (the old 0.02) the risky tax would dominate for ~112
    // ticks and there would be no decision to make; at 0.06 it stops paying
    // after ~38 ticks, well inside a campaign.
    const EconomyConfig config = shippedEconomyConfig();
    const int safeTax = config.revoltThresholdLevel - 1; // 2, never rolls
    const int riskyTax = config.maxTaxLevel;             // 5, rolls every tick

    // Short horizon: gambling pays roughly 2.5x the safe rate (5/2), so it is
    // clearly the better short-term play.
    const float shortRisky = expectedIncome(config, riskyTax, 1);
    const float shortSafe = expectedIncome(config, safeTax, 1);
    FOSSYND_CHECK(shortRisky > 1.5f * shortSafe);

    // Medium horizon: the expected value of the gamble has fallen below the safe
    // path, so a player who plans ahead should de-escalate. This is the
    // trade-off the balance pass exists to create.
    const float longRisky = expectedIncome(config, riskyTax, 60);
    const float longSafe = expectedIncome(config, safeTax, 60);
    FOSSYND_CHECK(longRisky < longSafe);

    // And the revolt is not so punishing that the gamble never wins: the
    // break-even horizon is a meaningful window, not one tick.
    int crossover = 1;
    while (expectedIncome(config, riskyTax, crossover) > expectedIncome(config, safeTax, crossover))
    {
        ++crossover;
    }
    FOSSYND_CHECK(crossover > 20); // the gamble is viable for a while...
    FOSSYND_CHECK(crossover < 60); // ...but not for the whole campaign
}

FOSSYND_TEST(Economy, ShippedStartingFundsCoverTheFirstResearchNode)
{
    // The opening must not stall: `startingFunds` has to buy the first research
    // node outright, so the "tax -> research -> buy" loop can start on turn one
    // without waiting for income (which is 0 until a region is taxed).
    std::string error;
    Json root;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/campaign.json";
    FOSSYND_CHECK(Json::parseFile(path, root, error));
    FOSSYND_CHECK(error.empty());
    const float startingFunds = root["startingFunds"].asFloat(0.0f);

    Json tree;
    const std::string treePath = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/research.json";
    FOSSYND_CHECK(Json::parseFile(treePath, tree, error));
    FOSSYND_CHECK(error.empty());

    // The cheapest root node (no prerequisites) must be affordable at once.
    float cheapestRoot = -1.0f;
    const Json& nodes = tree["nodes"];
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        const Json& requiresNode = nodes[i]["requires"];
        if (requiresNode.isArray() && requiresNode.size() == 0)
        {
            const float cost = nodes[i]["cost"].asFloat(0.0f);
            if (cheapestRoot < 0.0f || cost < cheapestRoot)
            {
                cheapestRoot = cost;
            }
        }
    }
    FOSSYND_CHECK(cheapestRoot > 0.0f);
    FOSSYND_CHECK(startingFunds >= cheapestRoot);
}
