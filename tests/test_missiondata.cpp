/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/MissionData.h"
#include "core/sim/Mission.h"

using namespace foss::core;

namespace
{

const char* kDemoJson = R"({
  "name": "Test",
  "width": 8,
  "height": 6,
  "buildings": [ { "x": 2, "y": 2, "width": 3, "height": 3 } ],
  "blocked": [ { "x": 0, "y": 5 } ],
  "agents": [
    { "name": "HERO", "team": "player", "x": 1.5, "y": 1.5, "health": 90,
      "weapon": { "name": "Rifle", "damage": 8, "range": 9, "fireRate": 4, "accuracy": 0.65 } },
    { "name": "FOE", "team": "enemy", "x": 6.5, "y": 1.5, "health": 40 },
    { "name": "CIV", "team": "civilian", "x": 4.5, "y": 4.5, "health": 20 }
  ]
})";

} // namespace

FOSS_TEST(MissionData, ParsesGridBuildingsAndBlockedTiles)
{
    std::string error;
    const Json root = Json::parse(kDemoJson, error);
    FOSS_CHECK(error.empty());

    MissionData data;
    FOSS_CHECK(MissionData::fromJson(root, data, error));
    FOSS_CHECK(error.empty());
    FOSS_CHECK_EQ(data.name, std::string("Test"));
    FOSS_CHECK_EQ(data.width, 8);
    FOSS_CHECK_EQ(data.height, 6);

    const TileGrid grid = buildGrid(data);
    FOSS_CHECK_EQ(grid.width(), 8);
    FOSS_CHECK_EQ(grid.height(), 6);
    // Building perimeter is blocked, its centre is walkable.
    FOSS_CHECK(!grid.isWalkable(2, 2));
    FOSS_CHECK(!grid.isWalkable(4, 2));
    FOSS_CHECK(!grid.isWalkable(3, 4));
    FOSS_CHECK(grid.isWalkable(3, 3));
    // Explicitly blocked tile.
    FOSS_CHECK(!grid.isWalkable(0, 5));
    // Ordinary ground.
    FOSS_CHECK(grid.isWalkable(0, 0));
}

FOSS_TEST(MissionData, ParsesAgentsAndWeapons)
{
    std::string error;
    const Json root = Json::parse(kDemoJson, error);
    MissionData data;
    FOSS_CHECK(MissionData::fromJson(root, data, error));
    FOSS_CHECK_EQ(data.agents.size(), static_cast<std::size_t>(3));

    FOSS_CHECK_EQ(data.agents[0].name, std::string("HERO"));
    FOSS_CHECK(data.agents[0].team == Team::Player);
    FOSS_CHECK_NEAR(data.agents[0].health, 90.0f);
    FOSS_CHECK_EQ(data.agents[0].weapon.name, std::string("Rifle"));
    FOSS_CHECK_NEAR(data.agents[0].weapon.damage, 8.0f);
    FOSS_CHECK_NEAR(data.agents[0].weapon.range, 9.0f);

    FOSS_CHECK(data.agents[1].team == Team::Enemy);
    // No weapon given: the default pistol is used.
    FOSS_CHECK_EQ(data.agents[1].weapon.name, std::string("Pistol"));

    FOSS_CHECK(data.agents[2].team == Team::Civilian);
}

FOSS_TEST(MissionData, PopulatesAMissionFromData)
{
    std::string error;
    const Json root = Json::parse(kDemoJson, error);
    MissionData data;
    FOSS_CHECK(MissionData::fromJson(root, data, error));

    const TileGrid grid = buildGrid(data);
    Mission mission(grid);
    populateMission(data, mission);

    FOSS_CHECK_EQ(mission.agents().size(), static_cast<std::size_t>(3));
    FOSS_CHECK_EQ(mission.aliveCount(Team::Player), static_cast<std::size_t>(1));
    FOSS_CHECK_EQ(mission.aliveCount(Team::Enemy), static_cast<std::size_t>(1));
    FOSS_CHECK_EQ(mission.agents()[0].name(), std::string("HERO"));
    FOSS_CHECK_NEAR(mission.agents()[0].maxHealth(), 90.0f);
    FOSS_CHECK(mission.agents()[2].team() == Team::Civilian);
}

FOSS_TEST(MissionData, RejectsInvalidSchema)
{
    std::string error;

    // No agents.
    Json root = Json::parse(R"({"width": 4, "height": 4, "agents": []})", error);
    MissionData data;
    FOSS_CHECK(!MissionData::fromJson(root, data, error));
    FOSS_CHECK(!error.empty());

    // Unknown team.
    error.clear();
    root = Json::parse(R"({"width": 4, "height": 4,
                           "agents": [{"name":"X","team":"droid","x":1,"y":1}]})", error);
    FOSS_CHECK(!MissionData::fromJson(root, data, error));
    FOSS_CHECK(!error.empty());

    // Agent without coordinates.
    error.clear();
    root = Json::parse(R"({"width": 4, "height": 4,
                           "agents": [{"name":"X","team":"player"}]})", error);
    FOSS_CHECK(!MissionData::fromJson(root, data, error));
    FOSS_CHECK(!error.empty());

    // Non-positive dimensions.
    error.clear();
    root = Json::parse(R"({"width": 0, "height": 4,
                           "agents": [{"name":"X","team":"player","x":1,"y":1}]})", error);
    FOSS_CHECK(!MissionData::fromJson(root, data, error));
    FOSS_CHECK(!error.empty());
}

FOSS_TEST(MissionData, LoadsTheDemoMissionFromDisk)
{
    MissionData data;
    std::string error;
    const std::string path = std::string(FOSS_SOURCE_DIR) + "/assets/data/missions/demo.json";
    FOSS_CHECK(MissionData::loadFromFile(path, data, error));
    FOSS_CHECK(error.empty());
    FOSS_CHECK_EQ(data.agents.size(), static_cast<std::size_t>(15));
    FOSS_CHECK_EQ(data.width, 24);
    FOSS_CHECK_EQ(data.height, 24);
}
