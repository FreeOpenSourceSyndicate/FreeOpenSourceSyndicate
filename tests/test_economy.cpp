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
