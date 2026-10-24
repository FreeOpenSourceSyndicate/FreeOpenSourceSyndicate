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

// The path a region's `missionId` resolves to when the campaign deploys: the
// engine looks for `<missionId>.json` under the content mission dir
// (CampaignScreen::resolveMissionFile). Checked here against the source tree.
std::string shippedMissionFile(const std::string& missionId)
{
    return std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions/" + missionId + ".json";
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

// A temp campaign file that reuses the shipped world/research/equipment via
// absolute sibling paths (a sibling named relative to a temp file would not be
// found) but switches the win condition to the terminal-mission rule (#456).
std::string finalMissionCampaignFile()
{
    const std::string dir = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/";
    return writeTemp("{\n"
                     "  \"syndicate\": \"Riverside Combine\", \"startingFunds\": 1000,\n"
                     "  \"world\": \"" +
                     dir +
                     "world.json\",\n"
                     "  \"research\": \"" +
                     dir +
                     "research.json\",\n"
                     "  \"equipment\": \"" +
                     dir +
                     "equipment.json\",\n"
                     "  \"missions\": [\"demo\", \"escort\", \"blackout\", \"assembly\", \"overpass\"],\n"
                     "  \"winCondition\": \"final-mission\", \"finalMission\": \"overpass\"\n"
                     "}");
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

    // Territory 1 is owned and now carries a mission (every shipped region
    // does), so a player may deploy to it.
    FOSSYND_CHECK(campaign.selectMission(1, {}));
    FOSSYND_CHECK_EQ(campaign.selectedTerritory(), 1);

    // Too many slots / bad slot.
    FOSSYND_CHECK(!campaign.selectMission(0, {0, 1, 2, 3, 4}));
    FOSSYND_CHECK(!campaign.selectMission(0, {99}));

    // Unknown territory.
    FOSSYND_CHECK(!campaign.selectMission(99, {}));
}

FOSSYND_TEST(Campaign, SelectedLoadoutMapsBoughtEquipmentToSlots)
{
    Campaign campaign = loadedCampaign();
    const int player = Campaign::kPlayerSyndicate;

    // Buy nothing yet: the loadout is still one (empty) slot per sent agent, so
    // the mission keeps its authored squad.
    FOSSYND_CHECK(campaign.selectMission(0, {0, 1, 2, 3}));
    PlayerLoadout empty = campaign.selectedLoadout();
    FOSSYND_CHECK_EQ(empty.slots.size(), std::size_t{4});
    for (const LoadoutSlot& slot : empty.slots)
    {
        FOSSYND_CHECK(slot.weaponId.empty());
        FOSSYND_CHECK(slot.gadgetId.empty());
    }

    // Assign a weapon and a gadget to squad slot 0, a cybernetic to slot 1, and
    // an id that is not in the catalogue to slot 2.
    campaign.equipment().assign(player, 0, "minigun");
    campaign.equipment().assign(player, 0, "medkit");
    campaign.equipment().assign(player, 1, "targeting-implant");
    campaign.equipment().assign(player, 2, "not-in-the-catalogue");

    FOSSYND_CHECK(campaign.selectMission(0, {0, 1, 2, 3}));
    const PlayerLoadout loadout = campaign.selectedLoadout();
    FOSSYND_CHECK_EQ(loadout.slots.size(), std::size_t{4});
    FOSSYND_CHECK_EQ(loadout.slots[0].weaponId, std::string("minigun"));
    FOSSYND_CHECK_EQ(loadout.slots[0].gadgetId, std::string("medkit"));
    // A cybernetic is a passive upgrade, but it still deploys: it reaches the
    // slot's `cyberneticId`, which applyLoadout applies once at mission load.
    FOSSYND_CHECK(loadout.slots[1].weaponId.empty());
    FOSSYND_CHECK(loadout.slots[1].gadgetId.empty());
    FOSSYND_CHECK_EQ(loadout.slots[1].cyberneticId, std::string("targeting-implant"));
    // An unknown id deploys nothing rather than a broken weapon.
    FOSSYND_CHECK(loadout.slots[2].weaponId.empty());
    FOSSYND_CHECK(loadout.slots[2].cyberneticId.empty());
    FOSSYND_CHECK(loadout.slots[3].weaponId.empty());

    // The loadout follows the *selected* slots, not the squad index: sending
    // only slot 2 yields one loadout entry carrying slot 2's equipment.
    FOSSYND_CHECK(campaign.selectMission(0, {2}));
    const PlayerLoadout one = campaign.selectedLoadout();
    FOSSYND_CHECK_EQ(one.slots.size(), std::size_t{1});
    FOSSYND_CHECK(one.slots[0].weaponId.empty());
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

FOSSYND_TEST(Campaign, FinishedCampaignIsTerminal)
{
    // The end state must be terminal: once won, the campaign cannot be changed by
    // a later loss, a further deployment, or a clock tick, so the victory/defeat
    // screen shows a frozen final board.
    const int player = Campaign::kPlayerSyndicate;

    // A win locks the campaign: canDeployTo refuses, recordResult is a no-op,
    // and tick stops advancing the economy.
    {
        Campaign campaign = loadedCampaign();
        for (TerritoryId id = 0; id < static_cast<TerritoryId>(campaign.map().size()); ++id)
        {
            campaign.map().setOwner(id, player);
        }
        campaign.recordResult(true, player);
        FOSSYND_CHECK(campaign.state() == CampaignState::Won);
        FOSSYND_CHECK(campaign.finished());

        // Every territory is player-owned and has a mission, so before the end
        // state canDeployTo would return true; after it, never.
        FOSSYND_CHECK(!campaign.canDeployTo(0));

        // A later loss cannot un-win the campaign.
        campaign.recordResult(false, player);
        FOSSYND_CHECK(campaign.state() == CampaignState::Won);

        // The clock is frozen: no further income ticks.
        Rng rng(3);
        const int before = campaign.economy().tickCount();
        campaign.tick(1000.0f, rng);
        FOSSYND_CHECK_EQ(campaign.economy().tickCount(), before);
    }

    // A loss is equally terminal: a later win cannot revive the campaign.
    {
        Campaign campaign = loadedCampaign();
        for (TerritoryId id = 0; id < static_cast<TerritoryId>(campaign.map().size()); ++id)
        {
            campaign.map().setOwner(id, 1);
        }
        campaign.map().setOwner(0, player);
        campaign.recordResult(false, player);
        FOSSYND_CHECK(campaign.state() == CampaignState::Lost);
        FOSSYND_CHECK(campaign.finished());

        campaign.recordResult(true, player);
        FOSSYND_CHECK(campaign.state() == CampaignState::Lost);
        FOSSYND_CHECK(!campaign.canDeployTo(0));
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

// --- Mission-set wiring pins (issue #418) ----------------------------------
//
// The campaign is only a campaign if every region is playable: each region's
// `missionId` must resolve to a shipped mission file, and that id must be in the
// campaign's `missions` list. Before #418, The Docks had no mission and five
// regions replayed just two boards, so most of the map was the same fight twice.

FOSSYND_TEST(Campaign, EveryShippedRegionNamesAMissionThatExists)
{
    const Campaign campaign = loadedCampaign();

    // No region is allowed to be a dead end: every territory carries a mission.
    for (const Territory& territory : campaign.map().territories())
    {
        FOSSYND_CHECK(!territory.missionId.empty());
        // ...and the id resolves to a real shipped mission file (the silent-break
        // class the closed #369 was: an id that nothing loads).
        FOSSYND_CHECK(std::filesystem::exists(shippedMissionFile(territory.missionId)));
    }
}

FOSSYND_TEST(Campaign, CampaignMissionsListCoversEveryRegionMission)
{
    const Campaign campaign = loadedCampaign();

    // `campaign.json`'s `missions` list is the shipped set; a region naming an id
    // that is not in it is a data error the loader cannot see.
    for (const Territory& territory : campaign.map().territories())
    {
        bool listed = false;
        for (const std::string& id : campaign.missions())
        {
            if (id == territory.missionId)
            {
                listed = true;
                break;
            }
        }
        FOSSYND_CHECK(listed);
    }
}

FOSSYND_TEST(Campaign, TheMapUsesTheWholeMissionSetNotJustTheDemo)
{
    // The point of #418: the map should exercise the tactical mission set, not
    // replay the opening two boards. Count the distinct missions the six regions
    // name and require most of the shipped set to be reachable from the map.
    const Campaign campaign = loadedCampaign();

    std::vector<std::string> used;
    for (const Territory& territory : campaign.map().territories())
    {
        bool seen = false;
        for (const std::string& id : used)
        {
            if (id == territory.missionId)
            {
                seen = true;
                break;
            }
        }
        if (!seen)
        {
            used.push_back(territory.missionId);
        }
    }

    FOSSYND_CHECK_EQ(used.size(), std::size_t{5}); // demo, escort, blackout, assembly, overpass
    // Every id named by the map must also be in the campaign's shipped list.
    for (const std::string& id : used)
    {
        bool listed = false;
        for (const std::string& shipped : campaign.missions())
        {
            if (shipped == id)
            {
                listed = true;
                break;
            }
        }
        FOSSYND_CHECK(listed);
    }
}

// --- Player-chosen syndicate identity (issue #445) --------------------------
//
// The player's syndicate is customisable at start, like the original: a free
// name and a configurable logo. The core carries the choice; the input field and
// the logo art are separate slices. Both fall back to the data default, so a
// fresh campaign always has an identity and an unknown value is never fatal.

FOSSYND_TEST(Campaign, DefaultsToTheDataAuthoredNameAndLogo)
{
    const Campaign campaign = loadedCampaign();

    FOSSYND_CHECK_EQ(campaign.syndicateName(), std::string("Riverside Combine"));
    FOSSYND_CHECK_EQ(campaign.defaultSyndicateName(), std::string("Riverside Combine"));
    FOSSYND_CHECK_EQ(campaign.defaultSyndicateLogo(), std::string("cog"));
    FOSSYND_CHECK_EQ(campaign.syndicateLogo(), std::string("cog"));
    // The pickable logo set comes from the data file, so a UI can build a picker
    // without hard-coding ids.
    FOSSYND_CHECK(campaign.syndicateLogos().size() >= std::size_t{2});
}

FOSSYND_TEST(Campaign, CarriesAPlayerChosenNameAndLogo)
{
    Campaign campaign = loadedCampaign();

    campaign.setSyndicateName("Nightingale Trust");
    FOSSYND_CHECK_EQ(campaign.syndicateName(), std::string("Nightingale Trust"));
    // The default is unchanged, so a UI can offer a reset.
    FOSSYND_CHECK_EQ(campaign.defaultSyndicateName(), std::string("Riverside Combine"));

    campaign.setSyndicateLogo("serpent");
    FOSSYND_CHECK_EQ(campaign.syndicateLogo(), std::string("serpent"));
}

FOSSYND_TEST(Campaign, EmptyOrUnknownIdentityFallsBackToTheDefault)
{
    Campaign campaign = loadedCampaign();

    // An empty name clears the choice: the map must never render a nameless
    // syndicate, so the data default comes back.
    campaign.setSyndicateName("Nightingale Trust");
    campaign.setSyndicateName("");
    FOSSYND_CHECK_EQ(campaign.syndicateName(), std::string("Riverside Combine"));

    // An empty or unknown logo id is never fatal: it falls back to the default
    // art rather than leaving the map with no logo.
    campaign.setSyndicateLogo("not-a-logo");
    FOSSYND_CHECK_EQ(campaign.syndicateLogo(), std::string("cog"));
    campaign.setSyndicateLogo("serpent");
    FOSSYND_CHECK_EQ(campaign.syndicateLogo(), std::string("serpent"));
    campaign.setSyndicateLogo("");
    FOSSYND_CHECK_EQ(campaign.syndicateLogo(), std::string("cog"));
}

FOSSYND_TEST(Campaign, ParsesLogoDefaultsAndRejectsABadLogoList)
{
    // The logo id and the pickable set come from data (ADR-0003), so a campaign
    // file can rename or extend the set without a code change.
    std::string error;
    const Json root = Json::parse(R"({
      "syndicate": "Riverside Combine", "logo": "bolt", "logos": ["cog", "bolt"],
      "startingFunds": 1000
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    Campaign campaign;
    FOSSYND_CHECK(Campaign::fromJson(root, campaign, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(campaign.defaultSyndicateLogo(), std::string("bolt"));
    FOSSYND_CHECK_EQ(campaign.syndicateLogo(), std::string("bolt"));
    FOSSYND_CHECK_EQ(campaign.syndicateLogos().size(), std::size_t{2});

    // A malformed logo list is a schema error, not a silent default.
    const Json bad = Json::parse(R"({ "syndicate": "X", "logos": "cog" })", error);
    FOSSYND_CHECK(error.empty());
    Campaign rejected;
    FOSSYND_CHECK(!Campaign::fromJson(bad, rejected, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Campaign, ParsesFinalMissionAndRejectsABadOne)
{
    // The terminal mission is named by data; it must be one of the campaign's own
    // missions, so a typo cannot leave the campaign unwinnable (issue #456).
    std::string error;
    const Json root = Json::parse(R"({
      "syndicate": "X", "missions": ["demo", "overpass"], "finalMission": "overpass"
    })",
                                   error);
    FOSSYND_CHECK(error.empty());
    Campaign campaign;
    FOSSYND_CHECK(Campaign::fromJson(root, campaign, error));
    FOSSYND_CHECK_EQ(campaign.finalMission(), std::string("overpass"));

    // A final mission that is not in `missions` is a schema error.
    const Json bad = Json::parse(R"({
      "syndicate": "X", "missions": ["demo"], "finalMission": "nope"
    })",
                                 error);
    FOSSYND_CHECK(error.empty());
    Campaign rejected;
    FOSSYND_CHECK(!Campaign::fromJson(bad, rejected, error));
    FOSSYND_CHECK(!error.empty());

    // A `final-mission` win condition without a final mission can never be won,
    // so it is a schema error too.
    const Json missing = Json::parse(R"({
      "syndicate": "X", "missions": ["demo"], "winCondition": "final-mission"
    })",
                                     error);
    FOSSYND_CHECK(error.empty());
    Campaign rejected2;
    FOSSYND_CHECK(!Campaign::fromJson(missing, rejected2, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Campaign, ShippedCampaignNamesAFinalMissionInItsMissionList)
{
    // The shipped campaign must name a final mission that is one of its missions,
    // so the terminal end state is reachable from data alone.
    Campaign campaign = loadedCampaign();
    FOSSYND_CHECK(!campaign.finalMission().empty());
    bool listed = false;
    for (const std::string& id : campaign.missions())
    {
        if (id == campaign.finalMission())
        {
            listed = true;
        }
    }
    FOSSYND_CHECK(listed);
}

FOSSYND_TEST(Campaign, WinningTheFinalMissionEndsTheCampaign)
{
    // A `final-mission` campaign is won by winning *that* mission, independent of
    // territory count — even with neutrals left on the map (issue #456). The
    // shipped map is reused so the test reads real data; only the win condition is
    // switched to the final-mission rule.
    const std::string path = finalMissionCampaignFile();

    Campaign campaign;
    std::string error;
    FOSSYND_CHECK(Campaign::loadFromFile(path, campaign, error));
    FOSSYND_CHECK(error.empty());
    std::remove(path.c_str());

    const int player = Campaign::kPlayerSyndicate;
    // "overpass" is the shipped mission on territory 5 (The Sprawl), a neutral.
    // Find it via the map rather than hard-coding the index.
    TerritoryId finalTerritory = -1;
    for (const Territory& territory : campaign.map().territories())
    {
        if (territory.missionId == campaign.finalMission())
        {
            finalTerritory = territory.id;
            break;
        }
    }
    FOSSYND_CHECK(finalTerritory >= 0);

    // The final territory is a neutral behind others; make it reachable by
    // granting it a player-held neighbour through the real deploy rule, then win
    // it. `canDeployTo` is the frontier check, so claim the neighbour first.
    FOSSYND_CHECK(!campaign.canDeployTo(finalTerritory)); // not yet on the frontier
    for (TerritoryId id = 0; id < static_cast<TerritoryId>(campaign.map().size()); ++id)
    {
        if (id == finalTerritory)
        {
            continue;
        }
        campaign.map().setOwner(id, player);
        if (campaign.canDeployTo(finalTerritory))
        {
            break;
        }
    }
    FOSSYND_CHECK(campaign.canDeployTo(finalTerritory));

    FOSSYND_CHECK(campaign.selectMission(finalTerritory, {0, 1, 2, 3}));
    FOSSYND_CHECK_EQ(campaign.selectedMissionId(), campaign.finalMission());

    // A non-final win does not end a final-mission campaign.
    const std::size_t ownedBefore = campaign.map().territoriesOwnedBy(player).size();
    FOSSYND_CHECK(ownedBefore < campaign.map().size()); // neutrals remain
    FOSSYND_CHECK(campaign.state() == CampaignState::Running);

    // Winning the final mission wins the campaign, neutrals and all.
    campaign.recordResult(true, player);
    FOSSYND_CHECK(campaign.state() == CampaignState::Won);
    FOSSYND_CHECK(campaign.wonFinalConfrontation());
    FOSSYND_CHECK(campaign.finished());
    FOSSYND_CHECK(campaign.map().territoriesOwnedBy(player).size() < campaign.map().size());

    // And the end state stays terminal: a later loss or tick cannot change it.
    campaign.recordResult(false, player);
    FOSSYND_CHECK(campaign.state() == CampaignState::Won);
    Rng rng(5);
    const int before = campaign.economy().tickCount();
    campaign.tick(1000.0f, rng);
    FOSSYND_CHECK_EQ(campaign.economy().tickCount(), before);
}

FOSSYND_TEST(Campaign, ANonFinalWinDoesNotEndAFinalMissionCampaign)
{
    // A win on any other region is an ordinary territorial claim: the campaign
    // keeps running until the final confrontation is won (issue #456).
    const std::string path = finalMissionCampaignFile();

    Campaign campaign;
    std::string error;
    FOSSYND_CHECK(Campaign::loadFromFile(path, campaign, error));
    FOSSYND_CHECK(error.empty());
    std::remove(path.c_str());

    const int player = Campaign::kPlayerSyndicate;
    // Territory 2 (Old Town) is a neutral on the frontier with a non-final
    // mission; winning it must not end the campaign.
    FOSSYND_CHECK(campaign.canDeployTo(2));
    FOSSYND_CHECK(campaign.map().find(2)->missionId != campaign.finalMission());
    FOSSYND_CHECK(campaign.selectMission(2, {0, 1, 2, 3}));
    campaign.recordResult(true, player);
    FOSSYND_CHECK(campaign.state() == CampaignState::Running);
    FOSSYND_CHECK(!campaign.wonFinalConfrontation());
    FOSSYND_CHECK(!campaign.finished());
}

FOSSYND_TEST(Campaign, OwnAllTerritoriesWinIsNotAFinalConfrontation)
{
    // Under the shipped `own-all-territories` rule the campaign is won by owning
    // the map; even though the shipped data also names a final mission, the win
    // did not come *from* the final confrontation, so the screen must show the
    // ordinary victory panel (issue #456).
    Campaign campaign = loadedCampaign();
    const int player = Campaign::kPlayerSyndicate;
    for (TerritoryId id = 0; id < static_cast<TerritoryId>(campaign.map().size()); ++id)
    {
        campaign.map().setOwner(id, player);
    }
    campaign.recordResult(true, player);
    FOSSYND_CHECK(campaign.state() == CampaignState::Won);
    FOSSYND_CHECK(!campaign.wonFinalConfrontation());
}
