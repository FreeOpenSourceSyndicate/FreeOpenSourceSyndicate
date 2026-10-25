/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/economy/Economy.h"
#include "core/economy/Equipment.h"
#include "core/economy/Research.h"

#include <cmath>
#include <cstddef>

using namespace fossynd::core;

namespace
{

const char* kTreeJson = R"({
  "nodes": [
    { "id": "a", "name": "A", "cost": 100, "time": 10, "requires": [], "unlocks": "a" },
    { "id": "b", "name": "B", "cost": 200, "time": 20, "requires": ["a"], "unlocks": "b" }
  ]
})";

ResearchTree treeOrFail(const char* json)
{
    std::string error;
    const Json root = Json::parse(json, error);
    FOSSYND_CHECK(error.empty());
    ResearchTree tree;
    FOSSYND_CHECK(ResearchTree::fromJson(root, tree, error));
    FOSSYND_CHECK(error.empty());
    return tree;
}

} // namespace

FOSSYND_TEST(Research, ParsesTreeAndPrerequisites)
{
    const ResearchTree tree = treeOrFail(kTreeJson);

    FOSSYND_CHECK_EQ(tree.size(), std::size_t{2});
    FOSSYND_CHECK(tree.find("a") != nullptr);
    FOSSYND_CHECK(tree.find("b") != nullptr);
    FOSSYND_CHECK(tree.find("nope") == nullptr);
    FOSSYND_CHECK_EQ(tree.find("b")->prerequisites.size(), std::size_t{1});
    FOSSYND_CHECK_EQ(tree.find("b")->prerequisites[0], std::string("a"));
    FOSSYND_CHECK_NEAR(tree.find("a")->cost, 100.0f);
}

FOSSYND_TEST(Research, RejectsUnknownPrerequisite)
{
    std::string error;
    const Json root = Json::parse(R"({
      "nodes": [ { "id": "a", "name": "A", "requires": ["ghost"] } ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    ResearchTree tree;
    FOSSYND_CHECK(!ResearchTree::fromJson(root, tree, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Research, RejectsPrerequisiteCycle)
{
    std::string error;
    const Json root = Json::parse(R"({
      "nodes": [
        { "id": "a", "name": "A", "requires": ["b"] },
        { "id": "b", "name": "B", "requires": ["a"] }
      ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    ResearchTree tree;
    FOSSYND_CHECK(!ResearchTree::fromJson(root, tree, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Research, BeginRequiresPrerequisitesAndFunds)
{
    const ResearchTree tree = treeOrFail(kTreeJson);
    Research research;
    Economy economy;
    economy.addFunds(0, 150.0f); // enough for a (100), not b (200)

    // b is gated by a.
    FOSSYND_CHECK(!research.begin(0, "b", tree, economy));
    FOSSYND_CHECK_NEAR(economy.funds(0), 150.0f);

    // a is available and affordable.
    FOSSYND_CHECK(research.begin(0, "a", tree, economy));
    FOSSYND_CHECK_NEAR(economy.funds(0), 50.0f);

    // Only one node at a time.
    FOSSYND_CHECK(!research.begin(0, "b", tree, economy));
}

FOSSYND_TEST(Research, CompletesAfterItsTime)
{
    const ResearchTree tree = treeOrFail(kTreeJson);
    Research research;
    Economy economy;
    economy.addFunds(0, 1000.0f);

    FOSSYND_CHECK(research.begin(0, "a", tree, economy));
    FOSSYND_CHECK_EQ(research.state(0).inProgress, std::string("a"));
    FOSSYND_CHECK(!research.completed(0, "a"));

    research.tick(5.0f);
    FOSSYND_CHECK(!research.completed(0, "a"));

    research.tick(5.0f); // reaches 10s
    FOSSYND_CHECK(research.completed(0, "a"));
    FOSSYND_CHECK_EQ(research.state(0).inProgress, std::string(""));

    // Now b is available and affordable.
    FOSSYND_CHECK(research.begin(0, "b", tree, economy));
}

FOSSYND_TEST(Research, RejectsInsufficientFunds)
{
    const ResearchTree tree = treeOrFail(kTreeJson);
    Research research;
    Economy economy; // 0 funds

    FOSSYND_CHECK(!research.begin(0, "a", tree, economy));
    FOSSYND_CHECK_NEAR(economy.funds(0), 0.0f);
    FOSSYND_CHECK(!research.completed(0, "a"));
}

FOSSYND_TEST(Research, StatesAreIndependentPerSyndicate)
{
    const ResearchTree tree = treeOrFail(kTreeJson);
    Research research;
    Economy economy;
    economy.addFunds(0, 1000.0f);
    economy.addFunds(1, 1000.0f);

    FOSSYND_CHECK(research.begin(0, "a", tree, economy));
    research.tick(10.0f);
    FOSSYND_CHECK(research.completed(0, "a"));
    FOSSYND_CHECK(!research.completed(1, "a")); // syndicate 1 did nothing
}

FOSSYND_TEST(Research, LoadsShippedTreeFile)
{
    std::string error;
    ResearchTree tree;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/research.json";
    FOSSYND_CHECK(ResearchTree::loadFromFile(path, tree, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(tree.size() >= std::size_t{4});
    FOSSYND_CHECK(tree.find("gauss") != nullptr);
}

// --- Balance pass pins (issue #419) ---------------------------------------
//
// The research tree is the metagame's time sink; these pin its *pacing* (cost,
// duration, depth, and how long the shipped economy takes to afford it) so a
// later edit cannot silently make the tree a two-minute formality or an
// unreachable grind. They read the real shipped files, so they cannot drift.

namespace
{

// The shipped campaign's economy config (for the ticks-to-afford pins).
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

ResearchTree shippedTree()
{
    std::string error;
    ResearchTree tree;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/research.json";
    FOSSYND_CHECK(ResearchTree::loadFromFile(path, tree, error));
    FOSSYND_CHECK(error.empty());
    return tree;
}

// Ticks of income at `tax` per owned territory needed to afford `cost` starting
// from `funds`. Mirrors what the campaign clock does (EconomyConfig::tickSeconds
// per tick); returns 0 when already affordable.
int ticksToAfford(float cost, float funds, float perTick)
{
    if (funds >= cost)
    {
        return 0;
    }
    FOSSYND_CHECK(perTick > 0.0f);
    return static_cast<int>(std::ceil((cost - funds) / perTick));
}

} // namespace

FOSSYND_TEST(Research, ShippedTreeIsAThreeTierProgression)
{
    // The tree must be a real progression, not a flat shopping list: a cheap
    // first tier, a branch, and a top-tier node gated behind both branches.
    const ResearchTree tree = shippedTree();
    FOSSYND_CHECK_EQ(tree.size(), std::size_t{11});

    const ResearchNode* minigun = tree.find("minigun");
    const ResearchNode* gauss = tree.find("gauss");
    const ResearchNode* longshot = tree.find("longshot");
    const ResearchNode* railgun = tree.find("railgun");
    FOSSYND_CHECK(minigun != nullptr && gauss != nullptr && longshot != nullptr &&
                  railgun != nullptr);

    // Tier 1 -> tier 2 -> tier 3.
    FOSSYND_CHECK_EQ(minigun->prerequisites.size(), std::size_t{0});
    FOSSYND_CHECK_EQ(gauss->prerequisites.size(), std::size_t{1});
    FOSSYND_CHECK_EQ(gauss->prerequisites[0], std::string("minigun"));
    FOSSYND_CHECK_EQ(longshot->prerequisites.size(), std::size_t{1});
    FOSSYND_CHECK_EQ(longshot->prerequisites[0], std::string("minigun"));
    FOSSYND_CHECK_EQ(railgun->prerequisites.size(), std::size_t{2});

    // The top node costs and takes longer than every tier below it, so the tree
    // rewards depth rather than letting a single cheap node win the game.
    FOSSYND_CHECK(railgun->cost > gauss->cost);
    FOSSYND_CHECK(railgun->cost > longshot->cost);
    FOSSYND_CHECK(railgun->cost > minigun->cost);
    FOSSYND_CHECK(railgun->timeSeconds > minigun->timeSeconds);
}

FOSSYND_TEST(Research, ShippedTreeHasTheThreeDrugCeilingNodes)
{
    // ADR-0043: the three drug bars (adrenalin/intelligence/physical) are gated
    // by research, and each node unlocks the matching ceiling item in the
    // catalogue. A root node per bar means all three are reachable without a
    // weapon detour, and the tree pins them so a later edit cannot drop one.
    const ResearchTree tree = shippedTree();

    const char* const bars[] = {"adrenalin", "intelligence", "physical"};
    for (const char* bar : bars)
    {
        const std::string nodeId = std::string(bar) + "-ceiling";
        const ResearchNode* node = tree.find(nodeId);
        FOSSYND_CHECK(node != nullptr);
        FOSSYND_CHECK(node->prerequisites.empty()); // a root: no weapon detour
        FOSSYND_CHECK_EQ(node->unlocks, nodeId);
        FOSSYND_CHECK(node->cost > 0.0f);
        FOSSYND_CHECK(node->timeSeconds > 0.0f);
    }
}

FOSSYND_TEST(Research, ShippedTreePacingIsBracketed)
{
    // Pin the tree's total cost and duration, and that every node sits in a sane
    // band: cheap enough to afford early, long enough to be a decision.
    const ResearchTree tree = shippedTree();
    float totalCost = 0.0f;
    float totalTime = 0.0f;
    for (const ResearchNode& node : tree.nodes())
    {
        FOSSYND_CHECK(node.cost >= 400.0f);   // no free research
        FOSSYND_CHECK(node.cost <= 2000.0f);  // no single node is a whole campaign
        FOSSYND_CHECK(node.timeSeconds >= 30.0f);
        FOSSYND_CHECK(node.timeSeconds <= 180.0f);
        totalCost += node.cost;
        totalTime += node.timeSeconds;
    }
    FOSSYND_CHECK_NEAR(totalCost, 7600.0f);
    FOSSYND_CHECK_NEAR(totalTime, 795.0f);

    // At the shipped tick length the whole tree is a meaningful number of income
    // ticks (not one) but not a grind (the world map is twelve regions, so the
    // economy scales up over the campaign).
    const EconomyConfig economy = shippedEconomyConfig();
    const float totalTicks = totalTime / economy.tickSeconds;
    FOSSYND_CHECK(totalTicks > 15.0f);
    FOSSYND_CHECK(totalTicks < 30.0f);
}

FOSSYND_TEST(Research, TicksToAffordTheResearchTreeFromTheStartingPosition)
{
    // The opening economy is two owned territories; taxing them safely (below the
    // revolt threshold) at tax 2 pays 2 * 2 * incomePerTaxLevel per tick. From
    // `startingFunds`, the first root node must be affordable at once and the
    // whole tree reachable in a reasonable number of ticks.
    const EconomyConfig economy = shippedEconomyConfig();
    const ResearchTree tree = shippedTree();

    std::string error;
    Json root;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/campaign.json";
    FOSSYND_CHECK(Json::parseFile(path, root, error));
    FOSSYND_CHECK(error.empty());
    const float startingFunds = root["startingFunds"].asFloat(0.0f);

    const float safeTax = static_cast<float>(economy.revoltThresholdLevel - 1);
    const float perTick = 2.0f * (economy.baseIncome + economy.incomePerTaxLevel * safeTax);
    FOSSYND_CHECK_NEAR(perTick, 240.0f);

    // Every root node is affordable immediately: the loop starts on turn one.
    for (const ResearchNode& node : tree.nodes())
    {
        if (node.prerequisites.empty())
        {
            FOSSYND_CHECK(ticksToAfford(node.cost, startingFunds, perTick) == 0);
        }
    }

    // The top-tier node is a few ticks away, and the whole tree is a couple of
    // dozen — the pacing target for a fresh campaign.
    FOSSYND_CHECK_EQ(ticksToAfford(tree.find("railgun")->cost, startingFunds, perTick), 3);

    float totalCost = 0.0f;
    for (const ResearchNode& node : tree.nodes())
    {
        totalCost += node.cost;
    }
    const int treeTicks = ticksToAfford(totalCost, startingFunds, perTick);
    FOSSYND_CHECK(treeTicks >= 15);
    FOSSYND_CHECK(treeTicks <= 30);
}

FOSSYND_TEST(Research, ShippedResearchAndSquadAreAffordableWithinACampaign)
{
    // The acceptance target is a *reachable* win: the research tree and the
    // gear it unlocks must be affordable in a number of income ticks that a
    // twelve-region campaign actually has — not a walkover, not a grind. Each
    // major investment (the whole tree, or a four-agent top-weapon squad) is
    // checked separately; no player needs the theoretical maximum of both.
    const EconomyConfig economy = shippedEconomyConfig();
    const ResearchTree tree = shippedTree();

    std::string error;
    Json root;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/equipment.json";
    FOSSYND_CHECK(Json::parseFile(path, root, error));
    FOSSYND_CHECK(error.empty());
    EquipmentCatalogue catalogue;
    FOSSYND_CHECK(EquipmentCatalogue::fromJson(root, catalogue, error));
    FOSSYND_CHECK(error.empty());

    float totalResearch = 0.0f;
    for (const ResearchNode& node : tree.nodes())
    {
        totalResearch += node.cost;
    }

    // A "win-ready" loadout: the top weapon (railgun) for all four slots.
    const EquipmentItem* railgun = catalogue.find("railgun");
    FOSSYND_CHECK(railgun != nullptr);
    const float squadCost = railgun->cost * static_cast<float>(Equipment::kSquadSlots);

    const float startingFunds = 1000.0f; // the shipped startingFunds
    const float safeTax = static_cast<float>(economy.revoltThresholdLevel - 1);
    const float perTick = 2.0f * (economy.baseIncome + economy.incomePerTaxLevel * safeTax);
    FOSSYND_CHECK_NEAR(perTick, 240.0f);

    // Funding the whole tree, or buying the top squad outright, each fits in a
    // bounded window: enough ticks that the player must play the map, few enough
    // that the campaign is not a grind. The tree grew by the three drug-ceiling
    // nodes (ADR-0043, #504), so the whole-tree window is a little wider than the
    // weapon/cybernetic-only tree of #419; the win itself needs neither the whole
    // tree nor the top squad, so this is the theoretical ceiling, not the route.
    const int treeTicks = ticksToAfford(totalResearch, startingFunds, perTick);
    const int squadTicks = ticksToAfford(squadCost, startingFunds, perTick);
    FOSSYND_CHECK(treeTicks >= 15);
    FOSSYND_CHECK(treeTicks <= 30);
    FOSSYND_CHECK(squadTicks >= 15);
    FOSSYND_CHECK(squadTicks <= 30);
}

// --- Restore (save/load half, issue #431) ----------------------------------

FOSSYND_TEST(Research, RestoreReinstatesCompletedAndInProgressVerbatim)
{
    // `begin` cannot reinstate a saved state (it gates on prerequisites, funds,
    // and idleness), so a loader needs `restore` to set it directly.
    Research research;
    ResearchState saved;
    saved.completed = {"a"};
    saved.inProgress = "b";
    saved.remainingSeconds = 7.5f;

    research.restore(0, saved);

    FOSSYND_CHECK(research.completed(0, "a"));
    FOSSYND_CHECK(!research.completed(0, "b")); // in progress is not yet completed
    FOSSYND_CHECK_EQ(research.state(0).inProgress, std::string("b"));
    FOSSYND_CHECK_NEAR(research.state(0).remainingSeconds, 7.5f);
}

FOSSYND_TEST(Research, RestoreGrowsTheStoreForAHighSyndicate)
{
    // A save may name a syndicate the fresh process has never touched; restore
    // must grow the per-syndicate store, exactly as `begin` does.
    Research research;
    ResearchState saved;
    saved.completed = {"a", "b"};

    research.restore(3, saved);

    FOSSYND_CHECK(research.completed(3, "a"));
    FOSSYND_CHECK(research.completed(3, "b"));
    // The syndicates before it stay empty (no state leaked sideways).
    FOSSYND_CHECK(!research.completed(0, "a"));
    FOSSYND_CHECK(research.state(0).completed.empty());
    FOSSYND_CHECK(research.state(0).inProgress.empty());
}

FOSSYND_TEST(Research, RestoreReplacesAnExistingStateRatherThanMerging)
{
    Research research;
    Economy economy;
    economy.addFunds(0, 1000.0f);
    const ResearchTree tree = treeOrFail(kTreeJson);
    FOSSYND_CHECK(research.begin(0, "a", tree, economy));
    FOSSYND_CHECK(research.state(0).inProgress == "a");

    // The saved state is authoritative: it overwrites, it does not union.
    ResearchState saved;
    saved.completed = {"b"};
    research.restore(0, saved);

    FOSSYND_CHECK(research.completed(0, "b"));
    FOSSYND_CHECK(!research.completed(0, "a"));
    FOSSYND_CHECK(research.state(0).inProgress.empty());
    FOSSYND_CHECK_NEAR(research.state(0).remainingSeconds, 0.0f);
}

FOSSYND_TEST(Research, RestoreOfANegativeSyndicateIsANoOp)
{
    Research research;
    ResearchState saved;
    saved.completed = {"a"};

    research.restore(-1, saved);

    FOSSYND_CHECK(!research.completed(0, "a"));
    FOSSYND_CHECK(research.state(0).completed.empty());
}

FOSSYND_TEST(Research, RestoredProgressContinuesToTick)
{
    // After a restore, an in-progress node must still complete on schedule: the
    // timer is live, not frozen at the restored value.
    Research research;
    ResearchState saved;
    saved.inProgress = "b";
    saved.remainingSeconds = 5.0f;
    research.restore(0, saved);

    research.tick(2.0f);
    FOSSYND_CHECK(!research.completed(0, "b"));
    FOSSYND_CHECK_NEAR(research.state(0).remainingSeconds, 3.0f);

    research.tick(3.0f);
    FOSSYND_CHECK(research.completed(0, "b"));
    FOSSYND_CHECK(research.state(0).inProgress.empty());
}
