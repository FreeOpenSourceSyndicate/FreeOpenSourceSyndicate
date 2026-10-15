/* AI/LLM-generated file. Do not treat as human-written source documentation. */
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

FOSSYND_TEST(WorldMap, RejectsDuplicateNameAndMissionId)
{
    std::string error;
    {
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
    {
        const Json root = Json::parse(R"({
          "territories": [
            { "id": 0, "name": "A", "neighbours": [], "missionId": "demo" },
            { "id": 1, "name": "B", "neighbours": [], "missionId": "demo" }
          ]
        })",
                                       error);
        FOSSYND_CHECK(error.empty());
        WorldMap map;
        FOSSYND_CHECK(!WorldMap::fromJson(root, map, error));
    }
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

FOSSYND_TEST(WorldMap, LoadsShippedWorldFile)
{
    std::string error;
    WorldMap map;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/world.json";
    FOSSYND_CHECK(WorldMap::loadFromFile(path, map, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(map.size() >= std::size_t{3});
}
