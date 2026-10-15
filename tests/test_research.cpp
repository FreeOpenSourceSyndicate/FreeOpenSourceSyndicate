/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/economy/Economy.h"
#include "core/economy/Research.h"

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
