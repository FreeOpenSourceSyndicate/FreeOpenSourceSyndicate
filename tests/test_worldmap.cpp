/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include <algorithm>

#include "TestHarness.h"
#include "core/economy/WorldMap.h"

using namespace fossynd::core;

namespace
{

// A small, valid three-territory map: 0 and 1 owned, 2 neutral.
const char* kWorldJson = R"({
  "territories": [
    { "id": 0, "name": "Riverside", "neighbours": [1, 2], "owner": 0, "missionId": "demo" },
    { "id": 1, "name": "The Docks", "neighbours": [0, 2], "owner": 0 },
    { "id": 2, "name": "Old Town", "neighbours": [0, 1], "owner": -1 }
  ]
})";

WorldMap parseOrFail(const char* json)
{
    std::string error;
    const Json root = Json::parse(json, error);
    FOSSYND_CHECK(error.empty());
    WorldMap map;
    FOSSYND_CHECK(WorldMap::fromJson(root, map, error));
    FOSSYND_CHECK(error.empty());
    return map;
}

} // namespace

FOSSYND_TEST(WorldMap, ParsesTerritoriesAndOwnership)
{
    const WorldMap map = parseOrFail(kWorldJson);

    FOSSYND_CHECK_EQ(map.size(), std::size_t{3});
    FOSSYND_CHECK_EQ(map.find(0)->name, std::string("Riverside"));
    FOSSYND_CHECK_EQ(map.owner(0), 0);
    FOSSYND_CHECK_EQ(map.owner(2), -1);
    FOSSYND_CHECK_EQ(map.find(0)->missionId, std::string("demo"));
    FOSSYND_CHECK(map.find(99) == nullptr);
}

FOSSYND_TEST(WorldMap, AdjacencyIsUndirected)
{
    const WorldMap map = parseOrFail(kWorldJson);

    FOSSYND_CHECK(map.areNeighbours(0, 1));
    FOSSYND_CHECK(map.areNeighbours(1, 0));
    FOSSYND_CHECK(!map.areNeighbours(1, 1));
    FOSSYND_CHECK_EQ(map.neighbours(0).size(), std::size_t{2});
    FOSSYND_CHECK_EQ(map.neighbours(99).size(), std::size_t{0});
}

FOSSYND_TEST(WorldMap, RejectsAsymmetricAdjacency)
{
    std::string error;
    const Json root = Json::parse(R"({
      "territories": [
        { "id": 0, "name": "A", "neighbours": [1] },
        { "id": 1, "name": "B", "neighbours": [] }
      ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    WorldMap map;
    FOSSYND_CHECK(!WorldMap::fromJson(root, map, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(WorldMap, RejectsNonDenseIds)
{
    std::string error;
    // id 0 missing: ids are not dense.
    const Json root = Json::parse(R"({
      "territories": [
        { "id": 1, "name": "A", "neighbours": [] }
      ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    WorldMap map;
    FOSSYND_CHECK(!WorldMap::fromJson(root, map, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(WorldMap, RejectsDuplicateIds)
{
    std::string error;
    // Two territories share id 0: ids are not unique.
    const Json root = Json::parse(R"({
      "territories": [
        { "id": 0, "name": "A", "neighbours": [] },
        { "id": 0, "name": "B", "neighbours": [] }
      ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    WorldMap map;
    FOSSYND_CHECK(!WorldMap::fromJson(root, map, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(WorldMap, AcceptsOutOfOrderDenseIds)
{
    std::string error;
    // Dense (0,1,2) but listed out of order; the parser sorts by id, so this is
    // a valid map.
    const Json root = Json::parse(R"({
      "territories": [
        { "id": 2, "name": "C", "neighbours": [1] },
        { "id": 0, "name": "A", "neighbours": [1] },
        { "id": 1, "name": "B", "neighbours": [0, 2] }
      ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    WorldMap map;
    FOSSYND_CHECK(WorldMap::fromJson(root, map, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(map.size(), std::size_t{3});
    FOSSYND_CHECK_EQ(map.find(0)->name, std::string("A"));
    FOSSYND_CHECK_EQ(map.find(2)->name, std::string("C"));
}

FOSSYND_TEST(WorldMap, RejectsUnknownNeighbourAndSelfLoop)
{
    std::string error;
    {
        const Json root = Json::parse(R"({
          "territories": [
            { "id": 0, "name": "A", "neighbours": [5] }
          ]
        })",
                                       error);
        FOSSYND_CHECK(error.empty());
        WorldMap map;
        FOSSYND_CHECK(!WorldMap::fromJson(root, map, error));
    }
    {
        const Json root = Json::parse(R"({
          "territories": [
            { "id": 0, "name": "A", "neighbours": [0] }
          ]
        })",
                                       error);
        FOSSYND_CHECK(error.empty());
        WorldMap map;
        FOSSYND_CHECK(!WorldMap::fromJson(root, map, error));
    }
}

FOSSYND_TEST(WorldMap, RejectsDuplicateName)
{
    std::string error;
    const Json root = Json::parse(R"({
      "territories": [
        { "id": 0, "name": "A", "neighbours": [] },
        { "id": 1, "name": "A", "neighbours": [] }
      ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());
    WorldMap map;
    FOSSYND_CHECK(!WorldMap::fromJson(root, map, error));
}

FOSSYND_TEST(WorldMap, AllowsMissionIdSharedByTerritories)
{
    // A small mission set must be able to cover a larger map: several regions may
    // be attacked through the same mission (the original reuses city maps), so a
    // repeated missionId is valid. This is what lets the shipped campaign be won
    // without authoring one mission per territory.
    std::string error;
    const Json root = Json::parse(R"({
      "territories": [
        { "id": 0, "name": "A", "neighbours": [1], "owner": 0, "missionId": "demo" },
        { "id": 1, "name": "B", "neighbours": [0], "missionId": "demo" }
      ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());
    WorldMap map;
    FOSSYND_CHECK(WorldMap::fromJson(root, map, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(map.find(0)->missionId, std::string("demo"));
    FOSSYND_CHECK_EQ(map.find(1)->missionId, std::string("demo"));
}

FOSSYND_TEST(WorldMap, SetOwnerUpdatesOwnershipQueries)
{
    WorldMap map = parseOrFail(kWorldJson);

    FOSSYND_CHECK_EQ(map.territoriesOwnedBy(0).size(), std::size_t{2});
    FOSSYND_CHECK_EQ(map.territoriesOwnedBy(1).size(), std::size_t{0});

    map.setOwner(2, 1);
    FOSSYND_CHECK_EQ(map.owner(2), 1);
    FOSSYND_CHECK_EQ(map.territoriesOwnedBy(1).size(), std::size_t{1});

    map.setOwner(99, 3); // unknown id: no-op, no crash
    FOSSYND_CHECK_EQ(map.owner(99), -1);
}

FOSSYND_TEST(WorldMap, TaxLevelGetSetAndClamp)
{
    WorldMap map = parseOrFail(kWorldJson);

    FOSSYND_CHECK_EQ(map.taxLevel(0), 0);
    map.setTaxLevel(0, 4);
    FOSSYND_CHECK_EQ(map.taxLevel(0), 4);

    map.setTaxLevel(1, -3); // clamped to 0
    FOSSYND_CHECK_EQ(map.taxLevel(1), 0);

    map.setTaxLevel(99, 2); // unknown id: no-op, no crash
    FOSSYND_CHECK_EQ(map.taxLevel(99), 0);
}

FOSSYND_TEST(WorldMap, LoadsShippedWorldFile)
{
    std::string error;
    WorldMap map;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/world.json";
    FOSSYND_CHECK(WorldMap::loadFromFile(path, map, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(map.size() >= std::size_t{3});
}

// The shipped world map is the v1.0.0 "world map" (issue #448): a larger set of
// culturally-flavoured regions, each reachable and playable. This pins the
// structural invariants a hand edit can break: dense ids, symmetric adjacency,
// no self-loops, a fully connected graph, and a mission on every region (the
// #418/#420 rule that keeps the campaign winnable).
FOSSYND_TEST(WorldMap, ShippedWorldIsAConnectedPlayableWorldMap)
{
    std::string error;
    WorldMap map;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/world.json";
    FOSSYND_CHECK(WorldMap::loadFromFile(path, map, error));
    FOSSYND_CHECK(error.empty());

    // A world map, not a city slice: the v1.0.0 map is materially larger than the
    // original six-region set.
    FOSSYND_CHECK(map.size() >= std::size_t{12});

    // Dense ids and the validate() invariants (symmetric adjacency, no unknown
    // neighbour, unique names) already hold from the load; re-assert explicitly so
    // a future loader change cannot silently relax them.
    FOSSYND_CHECK(map.validate(error));
    FOSSYND_CHECK(error.empty());

    for (const Territory& territory : map.territories())
    {
        // Every region is playable: no dead end (a region with no mission can
        // never be attacked, so the campaign could stall behind it).
        FOSSYND_CHECK(!territory.missionId.empty());
        // No self-loop.
        FOSSYND_CHECK(std::find(territory.neighbours.begin(), territory.neighbours.end(),
                                territory.id) == territory.neighbours.end());
    }

    // Fully connected: every region is reachable from region 0, so no region is
    // stranded off the frontier and unreachable.
    std::vector<bool> seen(map.size(), false);
    std::vector<TerritoryId> stack{0};
    seen[0] = true;
    while (!stack.empty())
    {
        const TerritoryId id = stack.back();
        stack.pop_back();
        for (const TerritoryId neighbour : map.neighbours(id))
        {
            if (!seen[static_cast<std::size_t>(neighbour)])
            {
                seen[static_cast<std::size_t>(neighbour)] = true;
                stack.push_back(neighbour);
            }
        }
    }
    for (std::size_t i = 0; i < seen.size(); ++i)
    {
        FOSSYND_CHECK(seen[i]);
    }
}
