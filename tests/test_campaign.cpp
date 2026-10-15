/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/economy/Campaign.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace fossynd::core;

namespace
{

std::string shipped(const std::string& name)
{
    return std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/" + name;
}

Campaign loadedCampaign()
{
    std::string error;
    Campaign campaign;
    FOSSYND_CHECK(Campaign::loadFromFile(shipped("campaign.json"), campaign, error));
    FOSSYND_CHECK(error.empty());
    return campaign;
}

// Writes `text` to a uniquely named temp file and returns its path.
std::string writeTemp(const std::string& text)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("fossynd_campaign_" + std::to_string(::rand()) + ".json");
    std::ofstream file(path);
    file << text;
    file.close();
    return path.string();
}

} // namespace

FOSSYND_TEST(Campaign, ParsesCampaignFields)
{
    std::string error;
    const Json root = Json::parse(R"({
      "syndicate": "Riverside Combine", "startingFunds": 1000,
      "incomePerTaxLevel": 100, "tickSeconds": 30,
      "world": "world.json", "research": "research.json", "equipment": "equipment.json",
      "missions": ["demo"], "winCondition": "own-all-territories"
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    Campaign campaign;
    FOSSYND_CHECK(Campaign::fromJson(root, campaign, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(campaign.syndicateName(), std::string("Riverside Combine"));
    FOSSYND_CHECK_NEAR(campaign.startingFunds(), 1000.0f);
    FOSSYND_CHECK_EQ(campaign.missions().size(), std::size_t{1});
    FOSSYND_CHECK_EQ(campaign.winCondition(), std::string("own-all-territories"));
    FOSSYND_CHECK(campaign.state() == CampaignState::Running);
}

FOSSYND_TEST(Campaign, RejectsNegativeStartingFunds)
{
    std::string error;
    const Json root = Json::parse(R"({ "startingFunds": -5 })", error);
    FOSSYND_CHECK(error.empty());

    Campaign campaign;
    FOSSYND_CHECK(!Campaign::fromJson(root, campaign, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Campaign, LoadsShippedCampaignAndSiblings)
{
    const Campaign campaign = loadedCampaign();

    FOSSYND_CHECK_EQ(campaign.map().size(), std::size_t{6});
    FOSSYND_CHECK(campaign.researchTree().size() >= std::size_t{4});
    FOSSYND_CHECK(campaign.catalogue().size() >= std::size_t{4});
    FOSSYND_CHECK_NEAR(campaign.economy().config().tickSeconds, 30.0f);
}

FOSSYND_TEST(Campaign, StartingFundsAreGrantedToThePlayerOnLoad)
{
    // A loaded campaign must start the corporation with `startingFunds`, so the
    // "tax -> research -> buy" loop has money from the first frame. fromJson
    // (parsing only) must NOT move any money; only loadFromFile seeds the ledger.
    Campaign campaign = loadedCampaign();
    FOSSYND_CHECK_NEAR(campaign.economy().funds(Campaign::kPlayerSyndicate),
                       campaign.startingFunds());

    std::string error;
    const Json root = Json::parse(R"({ "startingFunds": 2500 })", error);
    FOSSYND_CHECK(error.empty());
    Campaign parsed;
    FOSSYND_CHECK(Campaign::fromJson(root, parsed, error));
    FOSSYND_CHECK_NEAR(parsed.economy().funds(Campaign::kPlayerSyndicate), 0.0f);
}

FOSSYND_TEST(Campaign, MissingSiblingIsTolerated)
{
    const std::string path = writeTemp(R"({
      "syndicate": "Empty", "world": "does-not-exist.json",
      "research": "does-not-exist.json", "equipment": "does-not-exist.json"
    })");

    std::string error;
    Campaign campaign;
    FOSSYND_CHECK(Campaign::loadFromFile(path, campaign, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(campaign.map().size(), std::size_t{0}); // started empty
    std::remove(path.c_str());
}

FOSSYND_TEST(Campaign, DeployTargetsAreOwnedOrOnTheFrontier)
{
    Campaign campaign = loadedCampaign();

    // Territory 0 is player-owned with a mission: deployable.
    FOSSYND_CHECK(campaign.canDeployTo(0));

    // A territory with no mission is never a deploy target, however owned.
    for (TerritoryId id = 0; id < static_cast<TerritoryId>(campaign.map().size()); ++id)
    {
        if (campaign.map().find(id)->missionId.empty())
        {
            FOSSYND_CHECK(!campaign.canDeployTo(id));
        }
    }

    // Territory 4 (mission-bearing) is not adjacent to anything the player holds,
    // so it is not yet attackable; claiming a neighbour puts it on the frontier.
    FOSSYND_CHECK(!campaign.canDeployTo(4));
    campaign.map().setOwner(2, Campaign::kPlayerSyndicate);
    FOSSYND_CHECK(campaign.canDeployTo(4));

    // Unknown territory.
    FOSSYND_CHECK(!campaign.canDeployTo(99));
}

FOSSYND_TEST(Campaign, SelectMissionGatesOnDeployabilityAndSlots)
{
    Campaign campaign = loadedCampaign();

    // Territory 0 is player-owned with a mission.
    FOSSYND_CHECK(campaign.selectMission(0, {0, 1}));
    FOSSYND_CHECK_EQ(campaign.selectedTerritory(), 0);
    FOSSYND_CHECK_EQ(campaign.selectedMissionId(), std::string("demo"));
    FOSSYND_CHECK_EQ(campaign.selectedSlots().size(), std::size_t{2});

    // Territory 2 is neutral but on the frontier (adjacent to the player's 0): a
    // legitimate attack target.
    FOSSYND_CHECK(campaign.selectMission(2, {}));
    FOSSYND_CHECK_EQ(campaign.selectedTerritory(), 2);

    // Territory 5 is neutral and behind other neutrals: not attackable yet.
    FOSSYND_CHECK(!campaign.selectMission(5, {}));

    // Territory 1 is owned but has no mission.
    FOSSYND_CHECK(!campaign.selectMission(1, {}));

    // Too many slots / bad slot.
    FOSSYND_CHECK(!campaign.selectMission(0, {0, 1, 2, 3, 4}));
    FOSSYND_CHECK(!campaign.selectMission(0, {99}));

    // Unknown territory.
    FOSSYND_CHECK(!campaign.selectMission(99, {}));
}

FOSSYND_TEST(Campaign, ShippedCampaignCanBeWonByClaimingTheFrontier)
{
    // The Definition of Done requires the loop to reach an end state. That is only
    // possible if every non-player territory can actually be attacked, which in
    // turn needs a mission on it and a player-held neighbour. Walk the real loop
    // (deploy -> win -> claim) until the map is won, so a data file that leaves the
    // campaign unwinnable fails here instead of in a playtest.
    Campaign campaign = loadedCampaign();
    const int player = Campaign::kPlayerSyndicate;
    const std::size_t total = campaign.map().size();

    for (std::size_t guard = 0;
         guard < total * 4 && campaign.state() == CampaignState::Running; ++guard)
    {
        bool acted = false;
        for (TerritoryId id = 0; id < static_cast<TerritoryId>(total); ++id)
        {
            if (campaign.map().owner(id) == player || !campaign.canDeployTo(id))
            {
                continue;
            }
            FOSSYND_CHECK(campaign.selectMission(id, {0, 1, 2, 3}));
            campaign.recordResult(true, player);
            acted = true;
            break;
        }
        if (!acted)
        {
            break; // nothing left to attack
        }
    }

    FOSSYND_CHECK_EQ(campaign.map().territoriesOwnedBy(player).size(), total);
    FOSSYND_CHECK(campaign.state() == CampaignState::Won);
}

FOSSYND_TEST(Campaign, RecordResultClaimsAndEvaluatesEndState)
{
    // Win: the player owns every territory.
    {
        Campaign campaign = loadedCampaign();
        for (TerritoryId id = 0; id < static_cast<TerritoryId>(campaign.map().size()); ++id)
        {
            campaign.map().setOwner(id, Campaign::kPlayerSyndicate);
        }
        campaign.recordResult(true, Campaign::kPlayerSyndicate);
        FOSSYND_CHECK(campaign.state() == CampaignState::Won);
    }

    // Loss: every territory is owned, none neutral, and the player owns only one.
    {
        Campaign campaign = loadedCampaign();
        for (TerritoryId id = 0; id < static_cast<TerritoryId>(campaign.map().size()); ++id)
        {
            campaign.map().setOwner(id, 1);
        }
        campaign.map().setOwner(0, Campaign::kPlayerSyndicate);
        campaign.recordResult(false, Campaign::kPlayerSyndicate);
        FOSSYND_CHECK(campaign.state() == CampaignState::Lost);
    }
}

FOSSYND_TEST(Campaign, TickAdvancesEconomyAndResearch)
{
    Campaign campaign = loadedCampaign();
    Rng rng(7);

    campaign.tick(30.0f, rng);
    FOSSYND_CHECK_EQ(campaign.economy().tickCount(), 1);

    // Start a cheap research node, then let the campaign clock finish it.
    campaign.economy().addFunds(Campaign::kPlayerSyndicate, 10000.0f);
    FOSSYND_CHECK(campaign.research().begin(Campaign::kPlayerSyndicate, "minigun",
                                            campaign.researchTree(), campaign.economy()));
    for (int i = 0; i < 10; ++i)
    {
        campaign.tick(10.0f, rng);
    }
    FOSSYND_CHECK(campaign.research().completed(Campaign::kPlayerSyndicate, "minigun"));
}
