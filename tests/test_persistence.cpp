/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/util/CampaignSave.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

// A campaign that differs from the freshly loaded one in every field the save
// carries, so a round-trip has something to lose.
Campaign progressedCampaign()
{
    Campaign campaign = loadedCampaign();
    const int player = Campaign::kPlayerSyndicate;

    // Ownership + tax.
    campaign.map().setOwner(2, player);
    campaign.map().setTaxLevel(0, 3);
    campaign.map().setTaxLevel(2, 5);
    campaign.map().setOwner(3, player);
    campaign.map().setTaxLevel(3, 1);
    // A rival (syndicate 1) holds a territory, so the save's syndicate range is
    // not just the player.
    campaign.map().setOwner(4, 1);
    campaign.map().setTaxLevel(4, 2);

    // Funds: spend and earn so the ledger is not the starting amount.
    campaign.economy().addFunds(player, 1234.5f);
    campaign.economy().addFunds(1, 200.0f);

    // Research progress: captured, serialised, and (since #431/#480) restored by
    // applyTo.
    FOSSYND_CHECK(campaign.research().begin(player, "minigun", campaign.researchTree(),
                                           campaign.economy()));

    // A player-chosen identity, so the save has one to lose.
    campaign.setSyndicateName("Nightingale Trust");
    campaign.setSyndicateLogo("serpent");

    // Squad loadouts.
    campaign.equipment().assign(player, 0, "minigun");
    campaign.equipment().assign(player, 0, "medkit");
    campaign.equipment().assign(player, 1, "targeting-implant");

    return campaign;
}

// Writes `text` to a uniquely named temp file and returns its path.
std::string writeTemp(const std::string& text)
{
    static int counter = 0;
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
                                       ("fossynd_save_" + std::to_string(++counter) + ".json");
    std::ofstream file(path);
    file << text;
    file.close();
    return path.string();
}

} // namespace

FOSSYND_TEST(CampaignSave, RoundTripsProgressExactly)
{
    // Capture -> serialize -> parse must reproduce the snapshot exactly, so a
    // save/load cycle loses no progress.
    const CampaignSave original = CampaignSave::capture(progressedCampaign());

    std::string text;
    std::string error;
    FOSSYND_CHECK(original.serialize(text, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(!text.empty());

    CampaignSave restored;
    FOSSYND_CHECK(CampaignSave::parse(text, restored, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(restored == original);
}

FOSSYND_TEST(CampaignSave, SerializeIsDeterministic)
{
    // The same snapshot must serialise to the same bytes, so a save file is
    // diffable and a re-save of an unchanged game is a no-op.
    const CampaignSave save = CampaignSave::capture(progressedCampaign());

    std::string first;
    std::string second;
    std::string error;
    FOSSYND_CHECK(save.serialize(first, error));
    FOSSYND_CHECK(save.serialize(second, error));
    FOSSYND_CHECK_EQ(first, second);
}

FOSSYND_TEST(CampaignSave, CapturesOwnershipTaxFundsResearchAndLoadouts)
{
    // Guard against a capture that silently drops a field: every mutated field
    // must appear in the snapshot.
    const CampaignSave save = CampaignSave::capture(progressedCampaign());
    const int player = Campaign::kPlayerSyndicate;

    bool sawTerritory2Owned = false;
    bool sawTax = false;
    for (const CampaignSave::TerritoryProgress& territory : save.territories)
    {
        if (territory.id == 2 && territory.owner == player)
        {
            sawTerritory2Owned = true;
        }
        if (territory.taxLevel != 0)
        {
            sawTax = true;
        }
    }
    FOSSYND_CHECK(sawTerritory2Owned);
    FOSSYND_CHECK(sawTax);

    bool sawFunds = false;
    for (const CampaignSave::FundsProgress& entry : save.funds)
    {
        if (entry.syndicate == player && entry.amount > 1000.0f)
        {
            sawFunds = true;
        }
    }
    FOSSYND_CHECK(sawFunds);

    bool sawResearch = false;
    for (const CampaignSave::ResearchProgress& entry : save.research)
    {
        if (entry.syndicate == player && entry.inProgress == "minigun")
        {
            sawResearch = true;
        }
    }
    FOSSYND_CHECK(sawResearch);

    bool sawLoadout = false;
    for (const CampaignSave::LoadoutProgress& entry : save.loadouts)
    {
        if (entry.syndicate == player && entry.slot == 0)
        {
            FOSSYND_CHECK_EQ(entry.items.size(), std::size_t{2});
            sawLoadout = true;
        }
    }
    FOSSYND_CHECK(sawLoadout);

    // The player's chosen identity is captured too.
    FOSSYND_CHECK_EQ(save.syndicate, std::string("Nightingale Trust"));
    FOSSYND_CHECK_EQ(save.syndicateLogo, std::string("serpent"));
}

FOSSYND_TEST(CampaignSave, ApplyRestoresProgressOntoAFreshCampaign)
{
    // The real use: capture a progressed game, then reload the base campaign and
    // overlay the save. Ownership, tax, funds, and loadouts must come back.
    const CampaignSave save = CampaignSave::capture(progressedCampaign());
    const int player = Campaign::kPlayerSyndicate;

    Campaign fresh = loadedCampaign();
    std::string error;
    FOSSYND_CHECK(save.applyTo(fresh, error));
    FOSSYND_CHECK(error.empty());

    const CampaignSave applied = CampaignSave::capture(fresh);
    FOSSYND_CHECK(applied.territories == save.territories);
    FOSSYND_CHECK(applied.funds == save.funds);
    FOSSYND_CHECK(applied.loadouts == save.loadouts);
    // Research progress comes back too (#480): the save's research vector is
    // reproduced exactly by re-capturing the applied campaign.
    FOSSYND_CHECK(applied.research == save.research);
    FOSSYND_CHECK(!applied.research.empty());
    // The restored progress is live state, not just re-serialised: the node the
    // save had in progress is in progress on the reloaded campaign.
    FOSSYND_CHECK_EQ(fresh.research().state(player).inProgress, std::string("minigun"));
    // The player's chosen identity survives the round-trip.
    FOSSYND_CHECK_EQ(fresh.syndicateName(), std::string("Nightingale Trust"));
    FOSSYND_CHECK_EQ(fresh.syndicateLogo(), std::string("serpent"));
    FOSSYND_CHECK_EQ(fresh.map().owner(2), player);
    FOSSYND_CHECK_EQ(fresh.map().taxLevel(2), 5);
    // The saved amount is restored exactly, and the base campaign's starting
    // funds are *replaced*, not added to.
    FOSSYND_CHECK_NEAR(fresh.economy().funds(player), save.funds.front().amount);
    FOSSYND_CHECK(fresh.economy().funds(player) < fresh.startingFunds() + save.funds.front().amount);
}

FOSSYND_TEST(CampaignSave, ApplyRejectsATerritoryTheCampaignDoesNotHave)
{
    // A save taken from a larger map must not silently drop the extra territory.
    CampaignSave save;
    save.territories.push_back(CampaignSave::TerritoryProgress{99, 0, 1});

    Campaign fresh = loadedCampaign();
    std::string error;
    FOSSYND_CHECK(!save.applyTo(fresh, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(CampaignSave, RejectsCorruptJson)
{
    CampaignSave save;
    std::string error;
    FOSSYND_CHECK(!CampaignSave::parse("{ this is not json", save, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(CampaignSave, RejectsMissingFormatTag)
{
    CampaignSave save;
    std::string error;
    FOSSYND_CHECK(!CampaignSave::parse(R"({"version": 1, "territories": []})", save, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(CampaignSave, RejectsWrongFormatTag)
{
    CampaignSave save;
    std::string error;
    FOSSYND_CHECK(!CampaignSave::parse(
        R"({"format": "some-other-game", "version": 1, "territories": []})", save, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(CampaignSave, RejectsOldVersion)
{
    // The whole point of the version tag: an older file is refused with a
    // readable error rather than loaded with fields silently missing. Version 1
    // is the interesting case: it carried the name but no logo, so a v2 build
    // must refuse it rather than load a game whose chosen logo vanished.
    const char* old[] = {
        R"({"format": "fossynd-save", "version": 0, "territories": [], "funds": [], "research": [], "loadouts": []})",
        R"({"format": "fossynd-save", "version": 1, "syndicate": "Riverside Combine", "territories": [], "funds": [], "research": [], "loadouts": []})",
    };
    for (const char* text : old)
    {
        CampaignSave save;
        std::string error;
        FOSSYND_CHECK(!CampaignSave::parse(text, save, error));
        FOSSYND_CHECK(!error.empty());
    }
}

FOSSYND_TEST(CampaignSave, RejectsNewerVersion)
{
    CampaignSave save;
    std::string error;
    FOSSYND_CHECK(!CampaignSave::parse(
        R"({"format": "fossynd-save", "version": 999, "territories": [], "funds": [], "research": [], "loadouts": []})",
        save, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(CampaignSave, RejectsMissingVersion)
{
    CampaignSave save;
    std::string error;
    FOSSYND_CHECK(!CampaignSave::parse(R"({"format": "fossynd-save", "territories": []})", save,
                                       error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(CampaignSave, RejectsMalformedFields)
{
    // A field of the wrong type must fail the parse, not be silently defaulted.
    const char* bad[] = {
        // territories missing
        R"({"format": "fossynd-save", "version": 2, "funds": [], "research": [], "loadouts": []})",
        // a territory with a non-numeric id
        R"({"format": "fossynd-save", "version": 2, "territories": [{"id": "x", "owner": 0, "taxLevel": 0}], "funds": [], "research": [], "loadouts": []})",
        // a negative owner
        R"({"format": "fossynd-save", "version": 2, "territories": [{"id": 0, "owner": -2, "taxLevel": 0}], "funds": [], "research": [], "loadouts": []})",
        // a loadout slot out of range
        R"({"format": "fossynd-save", "version": 2, "territories": [], "funds": [], "research": [], "loadouts": [{"syndicate": 0, "slot": 99, "items": ["minigun"]}]})",
        // a negative funds amount
        R"({"format": "fossynd-save", "version": 2, "territories": [], "funds": [{"syndicate": 0, "amount": -5}], "research": [], "loadouts": []})",
    };
    for (const char* text : bad)
    {
        CampaignSave save;
        std::string error;
        FOSSYND_CHECK(!CampaignSave::parse(text, save, error));
        FOSSYND_CHECK(!error.empty());
    }
}

FOSSYND_TEST(CampaignSave, SavesAndLoadsThroughAFile)
{
    const CampaignSave save = CampaignSave::capture(progressedCampaign());
    const std::string path = writeTemp("{}"); // create a unique name, then overwrite

    std::string error;
    FOSSYND_CHECK(save.saveToFile(path, error));
    FOSSYND_CHECK(error.empty());

    CampaignSave loaded;
    FOSSYND_CHECK(CampaignSave::loadFromFile(path, loaded, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(loaded == save);

    std::remove(path.c_str());
}

FOSSYND_TEST(CampaignSave, LoadFromFileReportsAMissingFile)
{
    CampaignSave save;
    std::string error;
    FOSSYND_CHECK(!CampaignSave::loadFromFile("/no/such/fossynd-save.json", save, error));
    FOSSYND_CHECK(!error.empty());
}
