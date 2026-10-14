/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/MissionData.h"
#include "core/sim/Mission.h"

using namespace fossynd::core;

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

FOSSYND_TEST(MissionData, ParsesGridBuildingsAndBlockedTiles)
{
    std::string error;
    const Json root = Json::parse(kDemoJson, error);
    FOSSYND_CHECK(error.empty());

    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(data.name, std::string("Test"));
    FOSSYND_CHECK_EQ(data.width, 8);
    FOSSYND_CHECK_EQ(data.height, 6);

    const TileGrid grid = buildGrid(data);
    FOSSYND_CHECK_EQ(grid.width(), 8);
    FOSSYND_CHECK_EQ(grid.height(), 6);
    // Building perimeter is blocked, its centre is walkable.
    FOSSYND_CHECK(!grid.isWalkable(2, 2));
    FOSSYND_CHECK(!grid.isWalkable(4, 2));
    FOSSYND_CHECK(!grid.isWalkable(3, 4));
    FOSSYND_CHECK(grid.isWalkable(3, 3));
    // Explicitly blocked tile.
    FOSSYND_CHECK(!grid.isWalkable(0, 5));
    // Ordinary ground.
    FOSSYND_CHECK(grid.isWalkable(0, 0));
}

FOSSYND_TEST(MissionData, ParsesAgentsAndWeapons)
{
    std::string error;
    const Json root = Json::parse(kDemoJson, error);
    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(root, data, error));
    FOSSYND_CHECK_EQ(data.agents.size(), static_cast<std::size_t>(3));

    FOSSYND_CHECK_EQ(data.agents[0].name, std::string("HERO"));
    FOSSYND_CHECK(data.agents[0].team == Team::Player);
    FOSSYND_CHECK_NEAR(data.agents[0].health, 90.0f);
    FOSSYND_CHECK_EQ(data.agents[0].weapon.name, std::string("Rifle"));
    FOSSYND_CHECK_NEAR(data.agents[0].weapon.damage, 8.0f);
    FOSSYND_CHECK_NEAR(data.agents[0].weapon.range, 9.0f);

    FOSSYND_CHECK(data.agents[1].team == Team::Enemy);
    // No weapon given: the default pistol is used.
    FOSSYND_CHECK_EQ(data.agents[1].weapon.name, std::string("Pistol"));

    FOSSYND_CHECK(data.agents[2].team == Team::Civilian);
}

FOSSYND_TEST(MissionData, PopulatesAMissionFromData)
{
    std::string error;
    const Json root = Json::parse(kDemoJson, error);
    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(root, data, error));

    const TileGrid grid = buildGrid(data);
    Mission mission(grid);
    populateMission(data, mission);

    FOSSYND_CHECK_EQ(mission.agents().size(), static_cast<std::size_t>(3));
    FOSSYND_CHECK_EQ(mission.aliveCount(Team::Player), static_cast<std::size_t>(1));
    FOSSYND_CHECK_EQ(mission.aliveCount(Team::Enemy), static_cast<std::size_t>(1));
    FOSSYND_CHECK_EQ(mission.agents()[0].name(), std::string("HERO"));
    FOSSYND_CHECK_NEAR(mission.agents()[0].maxHealth(), 90.0f);
    FOSSYND_CHECK(mission.agents()[2].team() == Team::Civilian);
}

FOSSYND_TEST(MissionData, RejectsInvalidSchema)
{
    std::string error;

    // No agents.
    Json root = Json::parse(R"({"width": 4, "height": 4, "agents": []})", error);
    MissionData data;
    FOSSYND_CHECK(!MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(!error.empty());

    // Unknown team.
    error.clear();
    root = Json::parse(R"({"width": 4, "height": 4,
                           "agents": [{"name":"X","team":"droid","x":1,"y":1}]})", error);
    FOSSYND_CHECK(!MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(!error.empty());

    // Agent without coordinates.
    error.clear();
    root = Json::parse(R"({"width": 4, "height": 4,
                           "agents": [{"name":"X","team":"player"}]})", error);
    FOSSYND_CHECK(!MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(!error.empty());

    // Non-positive dimensions.
    error.clear();
    root = Json::parse(R"({"width": 0, "height": 4,
                           "agents": [{"name":"X","team":"player","x":1,"y":1}]})", error);
    FOSSYND_CHECK(!MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(MissionData, LoadsTheDemoMissionFromDisk)
{
    MissionData data;
    std::string error;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions/demo.json";
    FOSSYND_CHECK(MissionData::loadFromFile(path, data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(data.agents.size(), static_cast<std::size_t>(15));
    FOSSYND_CHECK_EQ(data.width, 24);
    FOSSYND_CHECK_EQ(data.height, 24);
}

// --- Objective parsing (issue #30) ----------------------------------------

FOSSYND_TEST(MissionData, DefaultsToEliminateAllWhenObjectiveIsOmitted)
{
    std::string error;
    const Json root = Json::parse(kDemoJson, error);
    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::EliminateAll);
}

FOSSYND_TEST(MissionData, ParsesReachObjective)
{
    std::string error;
    const Json root = Json::parse(R"({"width": 8, "height": 6,
        "objective": { "kind": "reach", "tileX": 5, "tileY": 3 },
        "agents": [{"team":"player","x":1,"y":1}]})", error);
    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::ReachTile);
    FOSSYND_CHECK_EQ(data.objective.tileX, 5);
    FOSSYND_CHECK_EQ(data.objective.tileY, 3);
}

FOSSYND_TEST(MissionData, ParsesSurviveObjective)
{
    std::string error;
    const Json root = Json::parse(R"({"width": 8, "height": 6,
        "objective": { "kind": "survive", "seconds": 45 },
        "agents": [{"team":"player","x":1,"y":1}]})", error);
    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::Survive);
    FOSSYND_CHECK_NEAR(data.objective.seconds, 45.0f);
}

FOSSYND_TEST(MissionData, ExplicitEliminateObjectiveIsAccepted)
{
    std::string error;
    const Json root = Json::parse(R"({"width": 8, "height": 6,
        "objective": { "kind": "eliminate" },
        "agents": [{"team":"player","x":1,"y":1}]})", error);
    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::EliminateAll);
}

FOSSYND_TEST(MissionData, RejectsInvalidObjectives)
{
    std::string error;
    MissionData data;

    // Unknown kind.
    Json root = Json::parse(R"({"width": 8, "height": 6,
        "objective": { "kind": "escort" },
        "agents": [{"team":"player","x":1,"y":1}]})", error);
    FOSSYND_CHECK(!MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(!error.empty());

    // Reach without a target tile.
    error.clear();
    root = Json::parse(R"({"width": 8, "height": 6,
        "objective": { "kind": "reach" },
        "agents": [{"team":"player","x":1,"y":1}]})", error);
    FOSSYND_CHECK(!MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(!error.empty());

    // Survive with a non-positive time.
    error.clear();
    root = Json::parse(R"({"width": 8, "height": 6,
        "objective": { "kind": "survive", "seconds": 0 },
        "agents": [{"team":"player","x":1,"y":1}]})", error);
    FOSSYND_CHECK(!MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(!error.empty());

    // Objective is not an object.
    error.clear();
    root = Json::parse(R"({"width": 8, "height": 6,
        "objective": "reach",
        "agents": [{"team":"player","x":1,"y":1}]})", error);
    FOSSYND_CHECK(!MissionData::fromJson(root, data, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(MissionData, PopulateMissionCarriesTheObjective)
{
    std::string error;
    const Json root = Json::parse(R"({"width": 8, "height": 6,
        "objective": { "kind": "survive", "seconds": 30 },
        "agents": [{"team":"player","x":1,"y":1}]})", error);
    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(root, data, error));

    const TileGrid grid = buildGrid(data);
    Mission mission(grid, 1u);
    populateMission(data, mission);
    FOSSYND_CHECK(mission.objective().kind == ObjectiveSpec::Kind::Survive);
    FOSSYND_CHECK_NEAR(mission.objective().seconds, 30.0f);
}

FOSSYND_TEST(MissionData, LoadsTheEscortMissionFromDisk)
{
    MissionData data;
    std::string error;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions/escort.json";
    FOSSYND_CHECK(MissionData::loadFromFile(path, data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::ReachTile);
    FOSSYND_CHECK_EQ(data.objective.tileX, 18);
    FOSSYND_CHECK_EQ(data.objective.tileY, 18);
    FOSSYND_CHECK_EQ(data.agents.size(), static_cast<std::size_t>(9));
}

// --- Shared weapon/agent catalogues (issue #34) ---------------------------

FOSSYND_TEST(Catalogues, WeaponCatalogueLoadsAndFindsById)
{
    std::string error;
    const Json root = Json::parse(R"({"weapons": [
        { "id": "pistol", "name": "Pistol", "damage": 10, "range": 6, "fireRate": 2.5, "accuracy": 0.72 },
        { "id": "rifle",  "name": "Rifle",  "damage": 8,  "range": 9, "fireRate": 4,   "accuracy": 0.65 }
    ]})", error);
    WeaponCatalogue catalogue;
    FOSSYND_CHECK(catalogue.loadFromJson(root, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(catalogue.size(), static_cast<std::size_t>(2));

    const Weapon* rifle = catalogue.find("rifle");
    FOSSYND_CHECK(rifle != nullptr);
    FOSSYND_CHECK_EQ(rifle->name, std::string("Rifle"));
    FOSSYND_CHECK_NEAR(rifle->damage, 8.0f);
    FOSSYND_CHECK_NEAR(rifle->range, 9.0f);
    FOSSYND_CHECK(catalogue.find("plasma") == nullptr);
}

FOSSYND_TEST(Catalogues, WeaponCatalogueParsesAreaRadius)
{
    std::string error;
    const Json root = Json::parse(R"({"weapons": [
        { "id": "rocket", "name": "Rocket", "damage": 40, "range": 12, "fireRate": 1,
          "accuracy": 0.9, "areaRadius": 2.5 },
        { "id": "pistol", "name": "Pistol", "damage": 10, "range": 6, "fireRate": 2.5,
          "accuracy": 0.72 }
    ]})", error);
    WeaponCatalogue catalogue;
    FOSSYND_CHECK(catalogue.loadFromJson(root, error));
    FOSSYND_CHECK(error.empty());

    const Weapon* rocket = catalogue.find("rocket");
    FOSSYND_CHECK(rocket != nullptr);
    FOSSYND_CHECK_NEAR(rocket->areaRadius, 2.5f);
    // Omitted areaRadius defaults to 0 (single-target), never a stale value.
    const Weapon* pistol = catalogue.find("pistol");
    FOSSYND_CHECK(pistol != nullptr);
    FOSSYND_CHECK_NEAR(pistol->areaRadius, 0.0f);
}

FOSSYND_TEST(Catalogues, WeaponCatalogueRejectsMalformedInput)
{
    std::string error;
    WeaponCatalogue catalogue;

    // Root is not an object.
    FOSSYND_CHECK(!catalogue.loadFromJson(Json::parse("[]", error), error));
    FOSSYND_CHECK(!error.empty());

    // 'weapons' is not an array.
    error.clear();
    FOSSYND_CHECK(!catalogue.loadFromJson(Json::parse(R"({"weapons": 5})", error), error));
    FOSSYND_CHECK(!error.empty());

    // An entry without an id.
    error.clear();
    FOSSYND_CHECK(!catalogue.loadFromJson(Json::parse(R"({"weapons": [{"name": "X"}]})", error),
                                          error));
    FOSSYND_CHECK(!error.empty());

    // A duplicate id.
    error.clear();
    FOSSYND_CHECK(!catalogue.loadFromJson(
        Json::parse(R"({"weapons": [{"id":"a"}, {"id":"a"}]})", error), error));
    FOSSYND_CHECK(!error.empty());

    // A weapon with a non-positive fireRate.
    error.clear();
    FOSSYND_CHECK(!catalogue.loadFromJson(
        Json::parse(R"({"weapons": [{"id":"a","fireRate":0}]})", error), error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Catalogues, AgentCatalogueResolvesWeaponIds)
{
    std::string error;
    WeaponCatalogue weapons;
    FOSSYND_CHECK(weapons.loadFromJson(
        Json::parse(R"({"weapons": [{"id":"rifle","name":"Rifle","damage":8,"range":9,
                                     "fireRate":4,"accuracy":0.65}]})", error), error));

    AgentCatalogue agents;
    FOSSYND_CHECK(agents.loadFromJson(
        Json::parse(R"({"agents": [
            { "id": "guard", "name": "GUARD", "team": "enemy", "speed": 3, "health": 70,
              "weaponId": "rifle" }
        ]})", error), weapons, error));
    FOSSYND_CHECK(error.empty());

    const AgentEntry* guard = agents.find("guard");
    FOSSYND_CHECK(guard != nullptr);
    FOSSYND_CHECK(guard->spec.team == Team::Enemy);
    FOSSYND_CHECK_NEAR(guard->spec.health, 70.0f);
    FOSSYND_CHECK_EQ(guard->spec.weapon.name, std::string("Rifle"));
    FOSSYND_CHECK_NEAR(guard->spec.weapon.range, 9.0f);
}

FOSSYND_TEST(Catalogues, AgentCatalogueRejectsBadWeaponReferences)
{
    WeaponCatalogue weapons; // empty

    std::string error;
    AgentCatalogue agents;
    FOSSYND_CHECK(!agents.loadFromJson(
        Json::parse(R"({"agents": [{"id":"g","weaponId":"rifle"}]})", error), weapons, error));
    FOSSYND_CHECK(!error.empty());

    error.clear();
    FOSSYND_CHECK(!agents.loadFromJson(
        Json::parse(R"({"agents": [{"id":"g","weapon":{},"weaponId":"rifle"}]})", error), weapons,
                    error));
    FOSSYND_CHECK(!error.empty());

    // An unknown team is also a readable error.
    error.clear();
    FOSSYND_CHECK(!agents.loadFromJson(
        Json::parse(R"({"agents": [{"id":"g","team":"droid"}]})", error), weapons, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Catalogues, MissionResolvesWeaponByIdAndInlineStillWorks)
{
    WeaponCatalogue weapons;
    std::string error;
    FOSSYND_CHECK(weapons.loadFromJson(
        Json::parse(R"({"weapons": [{"id":"uzi","name":"Uzi","damage":5,"range":5,
                                     "fireRate":9,"accuracy":0.55}]})", error), error));
    const AgentCatalogue noAgents;

    // An agent referencing a catalogue weapon.
    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(
        Json::parse(R"({"width": 8, "height": 6,
            "agents": [{"team":"player","x":1,"y":1,"weaponId":"uzi"}]})", error),
        data, weapons, noAgents, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(data.agents[0].weapon.name, std::string("Uzi"));
    FOSSYND_CHECK_NEAR(data.agents[0].weapon.fireRate, 9.0f);

    // An unknown id is a readable error, not a silent default.
    error.clear();
    FOSSYND_CHECK(!MissionData::fromJson(
        Json::parse(R"({"width": 8, "height": 6,
            "agents": [{"team":"player","x":1,"y":1,"weaponId":"nope"}]})", error),
        data, weapons, noAgents, error));
    FOSSYND_CHECK(!error.empty());

    // The inline form still works with no catalogues at all (regression).
    error.clear();
    FOSSYND_CHECK(MissionData::fromJson(
        Json::parse(R"({"width": 8, "height": 6,
            "agents": [{"team":"player","x":1,"y":1,
                        "weapon":{"name":"Custom","damage":3,"fireRate":2}}]})", error),
        data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(data.agents[0].weapon.name, std::string("Custom"));
}

FOSSYND_TEST(Catalogues, MissionInheritsAnAgentTemplateAndOverridesPosition)
{
    WeaponCatalogue weapons;
    std::string error;
    FOSSYND_CHECK(weapons.loadFromJson(
        Json::parse(R"({"weapons": [{"id":"rifle","name":"Rifle","damage":8,"range":9,
                                     "fireRate":4,"accuracy":0.65}]})", error), error));
    AgentCatalogue agents;
    FOSSYND_CHECK(agents.loadFromJson(
        Json::parse(R"({"agents": [{"id":"guard","name":"GUARD","team":"enemy",
                                    "speed":3,"health":70,"weaponId":"rifle"}]})", error),
        weapons, error));

    MissionData data;
    FOSSYND_CHECK(MissionData::fromJson(
        Json::parse(R"({"width": 8, "height": 6,
            "agents": [{"agentId":"guard","x":5,"y":4},
                       {"agentId":"guard","x":6,"y":1,"health":120}]})", error),
        data, weapons, agents, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(data.agents.size(), static_cast<std::size_t>(2));
    FOSSYND_CHECK_EQ(data.agents[0].name, std::string("GUARD"));
    FOSSYND_CHECK(data.agents[0].team == Team::Enemy);
    FOSSYND_CHECK_NEAR(data.agents[0].health, 70.0f);
    FOSSYND_CHECK_EQ(data.agents[0].weapon.name, std::string("Rifle"));
    FOSSYND_CHECK_NEAR(data.agents[0].x, 5.0f);
    // The second agent overrides health but keeps the template's other fields.
    FOSSYND_CHECK_NEAR(data.agents[1].health, 120.0f);
    FOSSYND_CHECK_EQ(data.agents[1].name, std::string("GUARD"));

    // An unknown template id is a readable error.
    error.clear();
    FOSSYND_CHECK(!MissionData::fromJson(
        Json::parse(R"({"width": 8, "height": 6,
            "agents": [{"agentId":"ghost","x":1,"y":1}]})", error),
        data, weapons, agents, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Catalogues, CataloguePathForMissionResolvesTheSiblingFile)
{
    FOSSYND_CHECK_EQ(cataloguePathForMission("assets/data/missions/demo.json", "weapons.json"),
                     std::string("assets/data/weapons.json"));
    FOSSYND_CHECK_EQ(cataloguePathForMission("/abs/data/missions/escort.json", "agents.json"),
                     std::string("/abs/data/agents.json"));
    // No directory component: nothing to resolve.
    FOSSYND_CHECK_EQ(cataloguePathForMission("demo.json", "weapons.json"), std::string(""));
}

FOSSYND_TEST(Catalogues, MissingCatalogueFileIsNotAnError)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/does-not-exist.json";
    WeaponCatalogue weapons;
    AgentCatalogue agents;
    bool found = true;
    std::string error;
    FOSSYND_CHECK(weapons.loadFromFile(path, found, error));
    FOSSYND_CHECK(!found);
    FOSSYND_CHECK(error.empty());

    found = true;
    FOSSYND_CHECK(agents.loadFromFile(path, weapons, found, error));
    FOSSYND_CHECK(!found);
    FOSSYND_CHECK(error.empty());
}

FOSSYND_TEST(Catalogues, LoadsTheShippedCataloguesFromDisk)
{
    const std::string dir = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/";
    WeaponCatalogue weapons;
    AgentCatalogue agents;
    bool found = false;
    std::string error;

    FOSSYND_CHECK(weapons.loadFromFile(dir + "weapons.json", found, error));
    FOSSYND_CHECK(found);
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(weapons.find("pistol") != nullptr);
    FOSSYND_CHECK(weapons.find("rifle") != nullptr);
    FOSSYND_CHECK(weapons.find("uzi") != nullptr);

    FOSSYND_CHECK(agents.loadFromFile(dir + "agents.json", weapons, found, error));
    FOSSYND_CHECK(found);
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(agents.find("trooper") != nullptr);
    FOSSYND_CHECK(agents.find("guard") != nullptr);
    FOSSYND_CHECK(agents.find("civilian") != nullptr);
    // The shipped 'trooper' template references the rifle by id.
    FOSSYND_CHECK_EQ(agents.find("trooper")->spec.weapon.name, std::string("Rifle"));

    // A mission loaded from disk picks up the catalogues sitting beside it.
    MissionData data;
    const std::string missionPath =
        std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions/escort.json";
    FOSSYND_CHECK(MissionData::loadFromFile(missionPath, data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(data.agents.size(), static_cast<std::size_t>(9));
}
