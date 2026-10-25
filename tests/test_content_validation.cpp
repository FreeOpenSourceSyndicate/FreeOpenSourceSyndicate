/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/ContentValidation.h"
#include "core/data/Json.h"
#include "core/i18n/MissionText.h"
#include "core/sim/Cybernetic.h"
#include "core/sim/PlayerLoadout.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace fossynd::core;
using fossynd::core::content::ValidationReport;
using fossynd::core::content::validateContentDir;
using fossynd::core::content::formatReport;

namespace
{

// A scratch content directory (catalogues + missions/) built for one test and
// removed when it goes out of scope. Each test uses a distinct subdirectory so
// parallel or repeated runs cannot collide.
struct TempContent
{
    std::filesystem::path root;

    explicit TempContent(const std::string& name)
        : root(std::filesystem::temp_directory_path() / ("fossynd_content_" + name))
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::filesystem::create_directories(root / "missions", ec);
        std::filesystem::create_directories(root / "world", ec);
    }

    ~TempContent()
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void write(const std::filesystem::path& relative, const std::string& contents) const
    {
        std::ofstream stream(root / relative, std::ios::binary);
        stream << contents;
    }

    std::string dir() const { return root.string(); }
};

// True if any problem message contains `needle` (case-sensitive substring).
bool reportMentions(const ValidationReport& report, const std::string& needle)
{
    const std::string text = formatReport(report);
    return text.find(needle) != std::string::npos;
}

const char* kWeaponsJson = R"({"weapons": [
    {"id": "rifle", "name": "Rifle", "damage": 8, "range": 9, "fireRate": 4, "accuracy": 0.65}
]})";

const char* kAgentsJson = R"({"agents": [
    {"id": "guard", "name": "GUARD", "team": "enemy", "speed": 3, "health": 70, "weaponId": "rifle"}
]})";

} // namespace

FOSSYND_TEST(ContentValidation, CleanContentPasses)
{
    TempContent content("clean");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("missions/good.json", R"({
        "name": "Good", "width": 8, "height": 6,
        "agents": [
            {"agentId": "guard", "x": 5, "y": 4},
            {"team": "player", "x": 1, "y": 1, "weaponId": "rifle"}
        ]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(report.ok());
    FOSSYND_CHECK_EQ(report.problems.size(), static_cast<std::size_t>(0));
    FOSSYND_CHECK_EQ(report.missionsChecked, static_cast<std::size_t>(1));
    FOSSYND_CHECK_EQ(report.cataloguesChecked, static_cast<std::size_t>(2));
}

FOSSYND_TEST(ContentValidation, MissingWeaponIdIsReported)
{
    TempContent content("missing_weapon");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("missions/bad.json", R"({
        "name": "Bad", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1, "weaponId": "plasma"}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "weaponId"));
    FOSSYND_CHECK(reportMentions(report, "plasma"));
    // The offending file is named, not just the message.
    FOSSYND_CHECK(reportMentions(report, "bad.json"));
}

FOSSYND_TEST(ContentValidation, MissingAgentIdIsReported)
{
    TempContent content("missing_agent");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("missions/bad.json", R"({
        "name": "Bad", "width": 8, "height": 6,
        "agents": [{"agentId": "ghost", "x": 1, "y": 1}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "agentId"));
    FOSSYND_CHECK(reportMentions(report, "ghost"));
}

FOSSYND_TEST(ContentValidation, DuplicateCatalogueIdIsReported)
{
    TempContent content("duplicate_id");
    content.write("weapons.json", R"({"weapons": [
        {"id": "rifle", "name": "Rifle", "damage": 8, "range": 9, "fireRate": 4},
        {"id": "rifle", "name": "Rifle II", "damage": 9, "range": 9, "fireRate": 4}
    ]})");
    content.write("agents.json", kAgentsJson);
    content.write("missions/good.json", R"({
        "name": "Good", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1, "weaponId": "rifle"}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "duplicate"));
    FOSSYND_CHECK(reportMentions(report, "weapons.json"));
}

FOSSYND_TEST(ContentValidation, ReportsEveryBadMissionNotJustTheFirst)
{
    TempContent content("many_bad");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("missions/a.json", R"({
        "name": "A", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1, "weaponId": "nope"}]
    })");
    content.write("missions/b.json", R"({
        "name": "B", "width": 8, "height": 6,
        "agents": [{"agentId": "nobody", "x": 1, "y": 1}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK_EQ(report.missionsChecked, static_cast<std::size_t>(2));
    FOSSYND_CHECK_EQ(report.problems.size(), static_cast<std::size_t>(2));
    FOSSYND_CHECK(reportMentions(report, "a.json"));
    FOSSYND_CHECK(reportMentions(report, "b.json"));
}

FOSSYND_TEST(ContentValidation, MalformedMissionJsonIsReportedNotCrashed)
{
    TempContent content("malformed");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("missions/broken.json", R"({ "name": "Broken", "width": )");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "broken.json"));
}

FOSSYND_TEST(ContentValidation, MissingCataloguesAreReported)
{
    TempContent content("no_catalogues");
    content.write("missions/good.json", R"({
        "name": "Good", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "no catalogue files found"));
}

FOSSYND_TEST(ContentValidation, BadCatalogueFieldIsReportedWithFileEntryAndField)
{
    TempContent content("bad_weapon_field");
    // A catalogue entry with a non-positive damage: it must fail validation, and
    // the message must name the file, the entry id, and the field.
    content.write("weapons.json", R"({"weapons": [
        {"id": "weak", "name": "Weak", "damage": -5, "range": 9, "fireRate": 4, "accuracy": 0.65}
    ]})");
    content.write("agents.json", kAgentsJson);
    content.write("missions/good.json", R"({
        "name": "Good", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "weapons.json")); // the file
    FOSSYND_CHECK(reportMentions(report, "weak"));          // the entry
    FOSSYND_CHECK(reportMentions(report, "damage"));        // the field
}

FOSSYND_TEST(ContentValidation, BadAgentTeamInCatalogueIsReported)
{
    TempContent content("bad_agent_team");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", R"({"agents": [
        {"id": "droid", "name": "DROID", "team": "robot", "speed": 3, "health": 70, "weaponId": "rifle"}
    ]})");
    content.write("missions/good.json", R"({
        "name": "Good", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "agents.json"));
    FOSSYND_CHECK(reportMentions(report, "team"));
}

FOSSYND_TEST(ContentValidation, BadMissionInlineWeaponIsReported)
{
    TempContent content("bad_inline_weapon");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    // An inline weapon (not a catalogue reference) with a non-positive range.
    content.write("missions/bad.json", R"({
        "name": "Bad", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1,
                    "weapon": {"name": "Broken", "damage": 5, "range": 0, "fireRate": 2}}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "bad.json"));
    FOSSYND_CHECK(reportMentions(report, "range"));
}

FOSSYND_TEST(ContentValidation, OffMapMissionGeometryIsReported)
{
    TempContent content("off_map_geometry");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    // Every off-map hazard at once: an unwinnable 'reach' objective, a building
    // that runs off the edge, an off-map blocked tile, and an off-map agent. The
    // validator stops at the first per-mission error, so use one mission per
    // hazard to prove each is caught with the file name.
    content.write("missions/reach.json", R"({
        "name": "Unwinnable", "width": 8, "height": 8,
        "objective": { "kind": "reach", "tileX": 99, "tileY": 99 },
        "agents": [{"team": "player", "x": 1, "y": 1}]
    })");
    content.write("missions/building.json", R"({
        "name": "Clipped", "width": 8, "height": 8,
        "buildings": [ { "x": 6, "y": 6, "width": 5, "height": 5 } ],
        "agents": [{"team": "player", "x": 1, "y": 1}]
    })");
    content.write("missions/blocked.json", R"({
        "name": "Dropped", "width": 8, "height": 8,
        "blocked": [ { "x": 50, "y": 50 } ],
        "agents": [{"team": "player", "x": 1, "y": 1}]
    })");
    content.write("missions/agent.json", R"({
        "name": "Invisible", "width": 8, "height": 8,
        "agents": [{"team": "player", "x": 99.5, "y": 99.5}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK_EQ(report.missionsChecked, static_cast<std::size_t>(4));
    FOSSYND_CHECK(reportMentions(report, "reach.json"));
    FOSSYND_CHECK(reportMentions(report, "building.json"));
    FOSSYND_CHECK(reportMentions(report, "blocked.json"));
    FOSSYND_CHECK(reportMentions(report, "agent.json"));
}

// The metagame world data must agree with the weapon catalogue (#369): an
// `equipment.json` weapon the catalogue does not define, or a research `unlocks`
// that is not an equipment id, would be bought/researched and then make
// `applyLoadout` reject the whole squad loadout at deploy. Nothing read these
// files before, so the dangling reference shipped.
FOSSYND_TEST(ContentValidation, WorldDataWeaponReferenceIsChecked)
{
    TempContent content("world_weapon_ref");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/equipment.json", R"({"items": [
        {"id": "rifle", "kind": "weapon", "name": "Rifle", "cost": 0},
        {"id": "ghost", "kind": "weapon", "name": "Ghost", "cost": 900}
    ]})");
    content.write("world/research.json", R"({"nodes": [
        {"id": "rifle", "name": "Rifle", "cost": 100, "time": 10, "requires": [], "unlocks": "rifle"}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "equipment item 'ghost' is a weapon"));
    FOSSYND_CHECK(reportMentions(report, "not in the weapon catalogue"));
}

// A gadget or cybernetic equipment id must be one the mission loader accepts,
// or `applyLoadout` rejects the whole squad at deploy — after the shop sold it
// and the player paid (the #369 silent-break class, issue #580). This is the
// guard that makes a typo a build failure instead of a paid no-op.
FOSSYND_TEST(ContentValidation, WorldDataGadgetAndCyberneticIdsAreChecked)
{
    TempContent content("world_gadget_ref");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/equipment.json", R"({"items": [
        {"id": "medkit", "kind": "gadget", "name": "Medkit", "cost": 200},
        {"id": "med-kit", "kind": "gadget", "name": "Medkit Typo", "cost": 200},
        {"id": "legs", "kind": "cybernetic", "name": "Legs", "cost": 800},
        {"id": "wings", "kind": "cybernetic", "name": "Wings", "cost": 800}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    // The known ids are accepted; only the typos are reported.
    FOSSYND_CHECK(reportMentions(report, "unknown gadget id 'med-kit'"));
    FOSSYND_CHECK(reportMentions(report, "unknown cybernetic id 'wings'"));
    FOSSYND_CHECK(!reportMentions(report, "unknown gadget id 'medkit'"));
    FOSSYND_CHECK(!reportMentions(report, "unknown cybernetic id 'legs'"));
}

// The positive half: every id in the shipped equipment catalogue's gadget and
// cybernetic kinds resolves through the loader vocabulary, so the guard above
// never fires on the data we actually ship.
FOSSYND_TEST(ContentValidation, ShippedGadgetAndCyberneticIdsResolve)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/equipment.json";
    std::ifstream stream(path, std::ios::binary);
    FOSSYND_CHECK(stream.good());
    const std::string text((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());

    std::string error;
    const Json root = Json::parse(text, error);
    FOSSYND_CHECK(error.empty());
    const Json& items = root["items"];
    FOSSYND_CHECK(items.size() > 0);
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const Json& item = items[i];
        const std::string id = item["id"].asString();
        const std::string kind = item["kind"].asString();
        if (kind == "gadget")
        {
            FOSSYND_CHECK(gadgetChargesForId(id) != kUnknownGadget);
        }
        else if (kind == "cybernetic")
        {
            CyberneticEffect effect;
            FOSSYND_CHECK(cyberneticEffectForId(id, effect));
        }
    }
}

// A research `unlocks` that names no equipment id is reported too (it gates a
// purchase that can never resolve).
FOSSYND_TEST(ContentValidation, WorldDataResearchUnlockIsChecked)
{
    TempContent content("world_unlock_ref");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/equipment.json", R"({"items": [
        {"id": "rifle", "kind": "weapon", "name": "Rifle", "cost": 0}
    ]})");
    content.write("world/research.json", R"({"nodes": [
        {"id": "rifle", "name": "Rifle", "cost": 100, "time": 10, "requires": [], "unlocks": "rifle"},
        {"id": "ghost", "name": "Ghost", "cost": 100, "time": 10, "requires": [], "unlocks": "ghost"}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "research node 'ghost' unlocks 'ghost'"));
    FOSSYND_CHECK(reportMentions(report, "not an equipment id"));
}

// World files are optional: a content directory with only catalogues and
// missions must still validate (a minimal dir, or a package that ships no
// campaign).
FOSSYND_TEST(ContentValidation, MissingWorldDataIsNotAProblem)
{
    TempContent content("no_world_data");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(report.ok());
}

// A world map whose regions all name missions that exist must pass. This is the
// positive half of the region→mission guard.
FOSSYND_TEST(ContentValidation, WorldMapMissionIdsResolve)
{
    TempContent content("world_ids_ok");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("missions/alpha.json", R"({
        "name": "Alpha", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1, "weaponId": "rifle"}]
    })");
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [1], "owner": 0,  "missionId": "alpha"},
        {"id": 1, "name": "B", "neighbours": [0], "owner": -1, "missionId": "alpha"}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(report.ok());
}

// A `missionId` that names no shipped mission file is reported: otherwise the
// region ships a deploy target the player can never attack (#369's silent-break
// class). The message names the territory and the missing file.
FOSSYND_TEST(ContentValidation, DanglingWorldMissionIdIsReported)
{
    TempContent content("world_ids_dangling");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "Westmarch", "neighbours": [1], "owner": 0,  "missionId": "demo"},
        {"id": 1, "name": "Nordhelm",  "neighbours": [0], "owner": -1, "missionId": "ghost"}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "Nordhelm"));
    FOSSYND_CHECK(reportMentions(report, "missionId 'ghost'"));
    FOSSYND_CHECK(reportMentions(report, "missions/ghost.json"));
    FOSSYND_CHECK(reportMentions(report, "world.json"));
}

// A region with an empty (or absent) missionId is a valid state (the original
// leaves some regions with no battle), so it must not be reported.
FOSSYND_TEST(ContentValidation, WorldRegionWithoutMissionIsAllowed)
{
    TempContent content("world_ids_empty");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [1], "owner": 0, "missionId": ""},
        {"id": 1, "name": "B", "neighbours": [0], "owner": 0}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(report.ok());
}

// A drug bar-ceiling (ADR-0043) must name a real bar and resolve its research
// gate. A ceiling naming a bar outside the closed vocabulary is reported: the
// purchase would raise no bar (the economy loader also rejects it, but this is
// the cross-check layer that keeps a metagame data edit honest).
FOSSYND_TEST(ContentValidation, DrugCeilingNamesARealBar)
{
    TempContent content("drug_bad_bar");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/equipment.json", R"({"items": [
        {"id": "luck-ceiling", "kind": "drug", "name": "Luck", "bar": "luck",
         "ceiling": 0.8, "cost": 900, "requiresResearch": "luck-ceiling"}
    ]})");
    content.write("world/research.json", R"({"nodes": [
        {"id": "luck-ceiling", "name": "Luck", "cost": 500, "time": 60,
         "requires": [], "unlocks": "luck-ceiling"}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "drug ceiling 'luck-ceiling' names bar 'luck'"));
    FOSSYND_CHECK(reportMentions(report, "adrenalin"));
}

// A drug ceiling whose `requiresResearch` names no research node is reported:
// the item would be sold but never unlockable.
FOSSYND_TEST(ContentValidation, DrugCeilingResearchGateMustResolve)
{
    TempContent content("drug_bad_research");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/equipment.json", R"({"items": [
        {"id": "adrenalin-ceiling", "kind": "drug", "name": "Adrenalin",
         "bar": "adrenalin", "ceiling": 1.0, "cost": 700,
         "requiresResearch": "ghost-node"}
    ]})");
    content.write("world/research.json", R"({"nodes": [
        {"id": "adrenalin-ceiling", "name": "Adrenalin", "cost": 500, "time": 60,
         "requires": [], "unlocks": "adrenalin-ceiling"}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "requires research 'ghost-node'"));
    FOSSYND_CHECK(reportMentions(report, "not a research node"));
}

// A drug ceiling with an out-of-range ceiling (here > 1) is reported. The
// economy loader also rejects it; this keeps the rule visible in one place for
// a metagame data edit.
FOSSYND_TEST(ContentValidation, DrugCeilingRangeIsChecked)
{
    TempContent content("drug_bad_ceiling");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/equipment.json", R"({"items": [
        {"id": "adrenalin-ceiling", "kind": "drug", "name": "Adrenalin",
         "bar": "adrenalin", "ceiling": 1.5, "cost": 700,
         "requiresResearch": "adrenalin-ceiling"}
    ]})");
    content.write("world/research.json", R"({"nodes": [
        {"id": "adrenalin-ceiling", "name": "Adrenalin", "cost": 500, "time": 60,
         "requires": [], "unlocks": "adrenalin-ceiling"}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "drug ceiling 'adrenalin-ceiling' has ceiling"));
    FOSSYND_CHECK(reportMentions(report, "greater than 0 and at most 1"));
}

// A well-formed drug ceiling (real bar, in range, resolving research) must pass
// - the positive half of the drug rules.
FOSSYND_TEST(ContentValidation, WellFormedDrugCeilingPasses)
{
    TempContent content("drug_ok");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/equipment.json", R"({"items": [
        {"id": "adrenalin-ceiling", "kind": "drug", "name": "Adrenalin",
         "bar": "adrenalin", "ceiling": 1.0, "cost": 700,
         "requiresResearch": "adrenalin-ceiling"}
    ]})");
    content.write("world/research.json", R"({"nodes": [
        {"id": "adrenalin-ceiling", "name": "Adrenalin", "cost": 500, "time": 60,
         "requires": [], "unlocks": "adrenalin-ceiling"}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(report.ok());
}

// A drug ceiling with no research gate is reported: it must be unlocked by
// research (ADR-0043), so an empty `requiresResearch` is a content error.
FOSSYND_TEST(ContentValidation, DrugCeilingNeedsAResearchGate)
{
    TempContent content("drug_no_research");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/equipment.json", R"({"items": [
        {"id": "adrenalin-ceiling", "kind": "drug", "name": "Adrenalin",
         "bar": "adrenalin", "ceiling": 1.0, "cost": 700}
    ]})");
    content.write("world/research.json", R"({"nodes": []})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "drug ceiling 'adrenalin-ceiling' needs"));
    FOSSYND_CHECK(reportMentions(report, "requiresResearch"));
}

// A name pool naming a region that is not a territory is reported: the pool
// would never be drawn for a real region (#509).
FOSSYND_TEST(ContentValidation, NamePoolRegionMustExist)
{
    TempContent content("names_bad_region");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [1], "owner": 0}
    ]})");
    content.write("world/names.json", R"({"pools": [
        {"region": 0, "names": ["Aldric"]},
        {"region": 7, "names": ["Bram"]}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "name pool names region 7"));
    FOSSYND_CHECK(reportMentions(report, "not a territory"));
}

// Every region in world.json must have a pool, so no region ships without the
// names the screen draws for it.
FOSSYND_TEST(ContentValidation, EveryRegionNeedsANamePool)
{
    TempContent content("names_missing_region");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [1], "owner": 0},
        {"id": 1, "name": "B", "neighbours": [0], "owner": 0}
    ]})");
    content.write("world/names.json", R"({"pools": [
        {"region": 0, "names": ["Aldric"]}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "region 1 has no name pool"));
}

// Two pools for the same region are reported: the second would shadow the first,
// so the map's names would silently depend on file order.
FOSSYND_TEST(ContentValidation, DuplicateNamePoolRegionIsReported)
{
    TempContent content("names_duplicate");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [0], "owner": 0}
    ]})");
    content.write("world/names.json", R"({"pools": [
        {"region": 0, "names": ["Aldric"]},
        {"region": 0, "names": ["Bram"]}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "region 0 is defined more than once"));
}

// A pool with an empty `names` array is reported: a region with no names would
// draw nothing, so the pool must carry at least one name.
FOSSYND_TEST(ContentValidation, NamePoolNeedsAtLeastOneName)
{
    TempContent content("names_empty_pool");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [0], "owner": 0}
    ]})");
    content.write("world/names.json", R"({"pools": [
        {"region": 0, "names": []}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "region 0 must have a non-empty 'names' array"));
}

// A non-ASCII name is reported: the embedded 5x7 font draws ASCII 32-126 only,
// so the name would render blank (the same rule as the string table).
FOSSYND_TEST(ContentValidation, NamePoolNamesMustBePrintableAscii)
{
    TempContent content("names_non_ascii");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [0], "owner": 0}
    ]})");
    content.write("world/names.json", "{\"pools\": [{\"region\": 0, \"names\": [\"J\\u00fcrgen\"]}]}");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "not printable ASCII"));
}

// An absent names.json is the safe fallback (the game assigns no names), not a
// problem - matching the other optional world files.
FOSSYND_TEST(ContentValidation, AbsentNamePoolsAreNotAProblem)
{
    TempContent content("names_absent");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [0], "owner": 0}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(report.ok());
}

// A well-formed names.json covering every region must pass - the positive half.
FOSSYND_TEST(ContentValidation, WellFormedNamePoolsPass)
{
    TempContent content("names_ok");
    content.write("weapons.json", kWeaponsJson);
    content.write("agents.json", kAgentsJson);
    content.write("world/world.json", R"({"territories": [
        {"id": 0, "name": "A", "neighbours": [1], "owner": 0},
        {"id": 1, "name": "B", "neighbours": [0], "owner": 0}
    ]})");
    content.write("world/names.json", R"({"pools": [
        {"region": 0, "names": ["Aldric", "Bram"]},
        {"region": 1, "names": ["Corvin"]}
    ]})");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(report.ok());
}

// The shipped content must always be valid: this is the guard that keeps a
// broken mission or catalogue out of `main`.
FOSSYND_TEST(ContentValidation, ShippedContentPasses)
{
    const std::string dataDir = std::string(FOSSYND_SOURCE_DIR) + "/assets/data";
    const ValidationReport report = validateContentDir(dataDir);
    FOSSYND_CHECK(report.ok());
    FOSSYND_CHECK(report.missionsChecked >= static_cast<std::size_t>(2));
    // 13 weapons (the #124 tiers, the #219 additions, the #369 longshot/railgun,
    // and the #446 time bomb) + 12 agent templates (the #160 carriers, the #219
    // additions, and the #446 police/saboteur included) + 4 vehicle templates
    // (#319). Pinned exactly so an entry cannot silently vanish from a shipped
    // catalogue; bump it when content is added on purpose.
    FOSSYND_CHECK_EQ(report.cataloguesChecked, static_cast<std::size_t>(29));
}

// A weapon resolved from the shared catalogue carries its own id, so the view
// can localise its HUD name via `i18n::weaponNameKey(id)` (issue #534/#557). An
// inline mission weapon has no id and keeps an empty one, so the HUD falls back
// to the authored name. Pinned here because the stamp lives in the loader
// (`WeaponCatalogue::loadFromJson`) and a regression would silently de-localise
// every catalogue weapon in the HUD.
FOSSYND_TEST(ContentValidation, CatalogueWeaponsCarryTheirId)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/weapons.json";
    WeaponCatalogue catalogue;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(catalogue.loadFromFile(path, found, error));
    FOSSYND_CHECK(found);

    // Every shipped entry's weapon id equals its catalogue key, and the id is a
    // non-empty key-safe id (so `weaponNameKey` yields a real key, not "").
    for (const WeaponEntry& entry : catalogue.entries())
    {
        FOSSYND_CHECK_EQ(entry.weapon.id, entry.id);
        FOSSYND_CHECK(!i18n::weaponNameKey(entry.id).empty());
    }

    // The lookup path stamps the same id (a mission referencing `weaponId`).
    const Weapon* rifle = catalogue.find("rifle");
    FOSSYND_CHECK(rifle != nullptr);
    if (rifle != nullptr)
    {
        FOSSYND_CHECK_EQ(rifle->id, std::string("rifle"));
    }
}

// The Phase 3 weapon tiers (issue #124) must be present in the shipped catalogue
// and parse to their intended niches. This is the guard that keeps a weapon from
// silently vanishing from weapons.json, and it pins each tier's relationship to
// the rifle so a mis-edit (e.g. a fire rate of 0) cannot slip through.
FOSSYND_TEST(ContentValidation, ShippedWeaponTiersLoad)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/weapons.json";
    WeaponCatalogue catalogue;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(catalogue.loadFromFile(path, found, error));
    FOSSYND_CHECK(found);

    const Weapon* rifle = catalogue.find("rifle");
    const Weapon* minigun = catalogue.find("minigun");
    const Weapon* flamethrower = catalogue.find("flamethrower");
    const Weapon* laser = catalogue.find("laser");
    FOSSYND_CHECK(rifle != nullptr);
    FOSSYND_CHECK(minigun != nullptr);
    FOSSYND_CHECK(flamethrower != nullptr);
    FOSSYND_CHECK(laser != nullptr);
    if (rifle == nullptr || minigun == nullptr || flamethrower == nullptr || laser == nullptr)
    {
        return;
    }

    FOSSYND_CHECK_EQ(minigun->name, std::string("Minigun"));
    FOSSYND_CHECK_EQ(flamethrower->name, std::string("Flamethrower"));
    FOSSYND_CHECK_EQ(laser->name, std::string("Laser"));

    // Minigun: the highest fire rate in the catalogue, but low per-shot damage.
    FOSSYND_CHECK(minigun->fireRate > rifle->fireRate);
    FOSSYND_CHECK(minigun->damage < rifle->damage);

    // Flamethrower: short range, high rate.
    FOSSYND_CHECK(flamethrower->range < rifle->range);
    FOSSYND_CHECK(flamethrower->fireRate > rifle->fireRate);

    // Laser: longest range and the highest per-shot damage of the three.
    FOSSYND_CHECK(laser->range > rifle->range);
    FOSSYND_CHECK(laser->damage > minigun->damage);
    FOSSYND_CHECK(laser->damage > flamethrower->damage);
    FOSSYND_CHECK(laser->fireRate < rifle->fireRate);
}

// The #219 weapon additions (shotgun, sniper, gauss) must be present in the
// shipped catalogue and parse to their intended niches. Like the #124 guard,
// this pins each weapon's relationship to the rifle so a mis-edit (e.g. a
// non-positive fire rate) cannot slip through.
FOSSYND_TEST(ContentValidation, ShippedNewWeaponFamiliesLoad)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/weapons.json";
    WeaponCatalogue catalogue;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(catalogue.loadFromFile(path, found, error));
    FOSSYND_CHECK(found);

    const Weapon* rifle = catalogue.find("rifle");
    const Weapon* shotgun = catalogue.find("shotgun");
    const Weapon* sniper = catalogue.find("sniper");
    const Weapon* gauss = catalogue.find("gauss");
    FOSSYND_CHECK(rifle != nullptr);
    FOSSYND_CHECK(shotgun != nullptr);
    FOSSYND_CHECK(sniper != nullptr);
    FOSSYND_CHECK(gauss != nullptr);
    if (rifle == nullptr || shotgun == nullptr || sniper == nullptr || gauss == nullptr)
    {
        return;
    }

    FOSSYND_CHECK_EQ(shotgun->name, std::string("Shotgun"));
    FOSSYND_CHECK_EQ(sniper->name, std::string("Sniper"));
    FOSSYND_CHECK_EQ(gauss->name, std::string("Gauss"));

    // Shotgun: close range, heavy single shot, slow.
    FOSSYND_CHECK(shotgun->range < rifle->range);
    FOSSYND_CHECK(shotgun->damage > rifle->damage);
    FOSSYND_CHECK(shotgun->fireRate < rifle->fireRate);

    // Sniper: the longest range in the catalogue and the hardest single shot,
    // but the slowest rate.
    FOSSYND_CHECK(sniper->range > rifle->range);
    FOSSYND_CHECK(sniper->damage > shotgun->damage);
    FOSSYND_CHECK(sniper->fireRate < rifle->fireRate);

    // Gauss: a mid-range, high-rate, high-accuracy slug between the rifle and
    // the sniper. Its low per-shot damage is the compensating weakness: the
    // high rate must not buy strict dominance, so its sustained output stays
    // inside the catalogue band (below the flamethrower, the highest-DPS gun)
    // and its per-shot damage sits below the rifle's (tactical vet, #294).
    FOSSYND_CHECK(gauss->range > rifle->range);
    FOSSYND_CHECK(gauss->range < sniper->range);
    FOSSYND_CHECK(gauss->fireRate > rifle->fireRate);
    FOSSYND_CHECK(gauss->accuracy > rifle->accuracy);

    const Weapon* flamethrower = catalogue.find("flamethrower");
    FOSSYND_CHECK(flamethrower != nullptr);
    if (flamethrower != nullptr)
    {
        const auto dps = [](const Weapon& w) { return w.damage * w.fireRate * w.accuracy; };
        FOSSYND_CHECK(gauss->damage < rifle->damage);
        FOSSYND_CHECK(dps(*gauss) > dps(*rifle));
        FOSSYND_CHECK(dps(*gauss) < dps(*flamethrower));
    }

    // Every catalogue entry satisfies the schema's required-field ranges, so
    // validation accepts the shipped file (it already loaded above, but assert
    // the invariant directly too).
    for (const WeaponEntry& entry : catalogue.entries())
    {
        FOSSYND_CHECK(entry.weapon.damage > 0.0f);
        FOSSYND_CHECK(entry.weapon.range > 0.0f);
        FOSSYND_CHECK(entry.weapon.fireRate > 0.0f);
        FOSSYND_CHECK(entry.weapon.accuracy >= 0.0f && entry.weapon.accuracy <= 1.0f);
        FOSSYND_CHECK(entry.weapon.areaRadius >= 0.0f);
    }
}

// The #369 weapons (`longshot`, `railgun`) must be present in the shipped
// catalogue with stats in the catalogue band: the metagame already sold and
// researched them, so without a weapon entry the buy broke the whole squad
// loadout. Pins each niche so a mis-edit cannot slip through.
FOSSYND_TEST(ContentValidation, ShippedMetagameWeaponsLoad)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/weapons.json";
    WeaponCatalogue catalogue;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(catalogue.loadFromFile(path, found, error));
    FOSSYND_CHECK(found);

    const Weapon* rifle = catalogue.find("rifle");
    const Weapon* flamethrower = catalogue.find("flamethrower");
    const Weapon* longshot = catalogue.find("longshot");
    const Weapon* railgun = catalogue.find("railgun");
    FOSSYND_CHECK(rifle != nullptr);
    FOSSYND_CHECK(flamethrower != nullptr);
    FOSSYND_CHECK(longshot != nullptr);
    FOSSYND_CHECK(railgun != nullptr);
    if (rifle == nullptr || flamethrower == nullptr || longshot == nullptr || railgun == nullptr)
    {
        return;
    }

    FOSSYND_CHECK_EQ(longshot->name, std::string("Longshot"));
    FOSSYND_CHECK_EQ(railgun->name, std::string("Railgun"));

    // Longshot: a long, accurate, mid-rate rifle between the rifle and the laser.
    FOSSYND_CHECK(longshot->range > rifle->range);
    FOSSYND_CHECK(longshot->accuracy > rifle->accuracy);

    // Railgun: the hardest-hitting non-AoE slug, but the slowest rate of the
    // long guns, so it does not dominate the launcher or the sniper.
    FOSSYND_CHECK(railgun->damage > rifle->damage);
    FOSSYND_CHECK(railgun->fireRate < rifle->fireRate);

    // Both stay inside the catalogue DPS band (below the flamethrower, the
    // highest-DPS gun) so neither strictly dominates (tactical vet, #294).
    const auto dps = [](const Weapon& w) { return w.damage * w.fireRate * w.accuracy; };
    FOSSYND_CHECK(dps(*longshot) < dps(*flamethrower));
    FOSSYND_CHECK(dps(*railgun) < dps(*flamethrower));
}

// The catalogue trades sustained DPS for reach and per-shot punch: a short-
// ranged gun may out-DPS a longer-ranged one, because it must close the distance
// to fire at all. This pins the *intended* relationship behind issue #539 — the
// tier-1 `minigun` (range 6) has a higher raw DPS than every higher-tier rifle,
// and that is its close-quarters niche, not a defect. The ladder's *price* axis
// (`damage * range`, docs/BALANCE.md §4) already rises with the gun, so the two
// axes are coherent once the DPS trade-off is stated. This guard fails if a
// later edit "fixes" the minigun by lowering its rate so DPS rises with tier,
// re-opening the decision instead of letting it drift silently.
FOSSYND_TEST(ContentValidation, ShippedDpsTradesAgainstRange)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/weapons.json";
    WeaponCatalogue catalogue;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(catalogue.loadFromFile(path, found, error));
    FOSSYND_CHECK(found);

    const Weapon* rifle = catalogue.find("rifle");
    const Weapon* minigun = catalogue.find("minigun");
    const Weapon* gauss = catalogue.find("gauss");
    const Weapon* longshot = catalogue.find("longshot");
    const Weapon* railgun = catalogue.find("railgun");
    const Weapon* flamethrower = catalogue.find("flamethrower");
    const Weapon* sniper = catalogue.find("sniper");
    FOSSYND_CHECK(rifle != nullptr);
    FOSSYND_CHECK(minigun != nullptr);
    FOSSYND_CHECK(gauss != nullptr);
    FOSSYND_CHECK(longshot != nullptr);
    FOSSYND_CHECK(railgun != nullptr);
    FOSSYND_CHECK(flamethrower != nullptr);
    FOSSYND_CHECK(sniper != nullptr);
    if (rifle == nullptr || minigun == nullptr || gauss == nullptr || longshot == nullptr ||
        railgun == nullptr || flamethrower == nullptr || sniper == nullptr)
    {
        return;
    }

    // Reach and per-shot punch both rise with tier ...
    FOSSYND_CHECK(minigun->range < gauss->range);
    FOSSYND_CHECK(gauss->range < longshot->range);
    FOSSYND_CHECK(minigun->damage < gauss->damage);
    FOSSYND_CHECK(gauss->damage < longshot->damage);
    FOSSYND_CHECK(longshot->damage < railgun->damage);

    // ... while sustained DPS does *not*: the cheap short-range spray out-DPSes
    // every longer, harder-hitting rifle it is an upgrade *from*.
    const auto dps = [](const Weapon& w) { return w.damage * w.fireRate * w.accuracy; };
    FOSSYND_CHECK(dps(*minigun) > dps(*gauss));
    FOSSYND_CHECK(dps(*minigun) > dps(*longshot));
    FOSSYND_CHECK(dps(*minigun) > dps(*railgun));

    // The two extremes of the same trade-off: the flamethrower is the shortest-
    // ranged and highest-DPS gun, the sniper the longest-ranged and lowest-DPS.
    FOSSYND_CHECK(flamethrower->range < rifle->range);
    FOSSYND_CHECK(sniper->range > railgun->range);
    FOSSYND_CHECK(dps(*flamethrower) > dps(*sniper));
}

// The #446 time bomb is the last weapon family the Definition of Done names
// (`docs/ROADMAP.md`). The sim has no fuse/delay yet, so it ships as the
// closest existing model: a high-damage area weapon. This pins that shape so a
// later mis-edit cannot quietly turn it into a plain slug; the fuse itself is a
// tactical follow-up (#449).
FOSSYND_TEST(ContentValidation, ShippedTimeBombLoads)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/weapons.json";
    WeaponCatalogue catalogue;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(catalogue.loadFromFile(path, found, error));
    FOSSYND_CHECK(found);

    const Weapon* rifle = catalogue.find("rifle");
    const Weapon* launcher = catalogue.find("launcher");
    const Weapon* timeBomb = catalogue.find("time-bomb");
    FOSSYND_CHECK(rifle != nullptr);
    FOSSYND_CHECK(launcher != nullptr);
    FOSSYND_CHECK(timeBomb != nullptr);
    if (rifle == nullptr || launcher == nullptr || timeBomb == nullptr)
    {
        return;
    }

    FOSSYND_CHECK_EQ(timeBomb->name, std::string("Time Bomb"));

    // A planted charge: an area weapon (like the launcher) whose blast is the
    // hardest hit in the catalogue, but which is slow to place and short-ranged.
    FOSSYND_CHECK(timeBomb->areaRadius > 0.0f);
    FOSSYND_CHECK(timeBomb->areaRadius > launcher->areaRadius);
    FOSSYND_CHECK(timeBomb->damage > launcher->damage);
    FOSSYND_CHECK(timeBomb->damage > rifle->damage);
    FOSSYND_CHECK(timeBomb->fireRate < rifle->fireRate);
    FOSSYND_CHECK(timeBomb->range < rifle->range);
}

// `parseWeaponNode` must read an optional `fuseSeconds` (issue #461), so a
// data-only time bomb can carry a real fuse instead of firing as an immediate
// blast. Omitted keeps the default 0 (immediate) — the field is additive and
// must change no existing weapon.
FOSSYND_TEST(ContentValidation, WeaponFuseSecondsIsParsedAndDefaultsToZero)
{
    TempContent content("fuse_parsed");
    content.write("weapons.json", R"({"weapons": [
        {"id": "plain", "name": "Plain", "damage": 8, "range": 9, "fireRate": 4, "accuracy": 0.65},
        {"id": "fused", "name": "Fused", "damage": 60, "range": 5, "fireRate": 1,
         "accuracy": 0.9, "areaRadius": 2, "fuseSeconds": 3.5}
    ]})");
    content.write("agents.json", kAgentsJson);
    content.write("missions/good.json", R"({
        "name": "Good", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1}]
    })");

    WeaponCatalogue catalogue;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(catalogue.loadFromFile(content.dir() + "/weapons.json", found, error));
    FOSSYND_CHECK(found);

    const Weapon* plain = catalogue.find("plain");
    const Weapon* fused = catalogue.find("fused");
    FOSSYND_CHECK(plain != nullptr);
    FOSSYND_CHECK(fused != nullptr);
    if (plain == nullptr || fused == nullptr)
    {
        return;
    }

    FOSSYND_CHECK_EQ(fused->fuseSeconds, 3.5f); // parsed, not dropped
    FOSSYND_CHECK_EQ(plain->fuseSeconds, 0.0f); // omitted -> immediate, unchanged
}

// A negative `fuseSeconds` must fail the load with the entry and field named,
// like the other catalogue field ranges (issue #190), not misbehave at play time.
FOSSYND_TEST(ContentValidation, NegativeFuseSecondsIsReportedWithFileEntryAndField)
{
    TempContent content("bad_fuse");
    content.write("weapons.json", R"({"weapons": [
        {"id": "backwards", "name": "Backwards", "damage": 8, "range": 9,
         "fireRate": 4, "accuracy": 0.65, "fuseSeconds": -1}
    ]})");
    content.write("agents.json", kAgentsJson);
    content.write("missions/good.json", R"({
        "name": "Good", "width": 8, "height": 6,
        "agents": [{"team": "player", "x": 1, "y": 1}]
    })");

    const ValidationReport report = validateContentDir(content.dir());
    FOSSYND_CHECK(!report.ok());
    FOSSYND_CHECK(reportMentions(report, "weapons.json")); // the file
    FOSSYND_CHECK(reportMentions(report, "backwards"));    // the entry
    FOSSYND_CHECK(reportMentions(report, "fuseSeconds"));  // the field
}

// The #446 `police` enemy template the Definition of Done names
// (`docs/ROADMAP.md`: "Enemies: police, ..."). It must be an enemy that carries a
// catalogue weapon by id, so a mission can deploy one by data alone. The
// `saboteur` is the time-bomb's carrier, added so the new weapon is
// mission-referenceable rather than inert (AGENTS.md "Shared catalogues").
FOSSYND_TEST(ContentValidation, ShippedPoliceAndSaboteurLoad)
{
    const std::string dir = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/";
    WeaponCatalogue weapons;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(weapons.loadFromFile(dir + "weapons.json", found, error));
    FOSSYND_CHECK(found);

    AgentCatalogue agents;
    FOSSYND_CHECK(agents.loadFromFile(dir + "agents.json", weapons, found, error));
    FOSSYND_CHECK(found);
    FOSSYND_CHECK(error.empty());

    const AgentEntry* police = agents.find("police");
    const AgentEntry* saboteur = agents.find("saboteur");
    FOSSYND_CHECK(police != nullptr);
    FOSSYND_CHECK(saboteur != nullptr);
    if (police == nullptr || saboteur == nullptr)
    {
        return;
    }

    FOSSYND_CHECK_EQ(police->spec.name, std::string("POLICE"));
    FOSSYND_CHECK(police->spec.team == Team::Enemy);
    // A police officer is a trained, armed responder: tougher than a civilian
    // bystander and not a glass cannon.
    FOSSYND_CHECK(police->spec.health > 20.0f);
    FOSSYND_CHECK(police->spec.speed > 0.0f);
    FOSSYND_CHECK_EQ(police->spec.weapon.name, std::string("Uzi"));

    FOSSYND_CHECK_EQ(saboteur->spec.name, std::string("SABOTEUR"));
    FOSSYND_CHECK(saboteur->spec.team == Team::Enemy);
    FOSSYND_CHECK_EQ(saboteur->spec.weapon.name, std::string("Time Bomb"));
    FOSSYND_CHECK(saboteur->spec.weapon.areaRadius > 0.0f);
}

// The #219 weapons are carried by agent templates (#219), so they are
// referenceable from a mission rather than inert. Each new template's
// `weaponId` must resolve to the intended weapon.
FOSSYND_TEST(ContentValidation, ShippedNewWeaponCarriersLoad)
{
    const std::string dir = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/";
    WeaponCatalogue weapons;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(weapons.loadFromFile(dir + "weapons.json", found, error));
    FOSSYND_CHECK(found);

    AgentCatalogue agents;
    FOSSYND_CHECK(agents.loadFromFile(dir + "agents.json", weapons, found, error));
    FOSSYND_CHECK(found);
    FOSSYND_CHECK(error.empty());

    struct Carrier
    {
        const char* agentId;
        const char* weaponId;
        const char* weaponName;
    };
    const Carrier carriers[] = {
        {"breacher", "shotgun", "Shotgun"},
        {"sharpshooter", "sniper", "Sniper"},
        {"shocktrooper", "gauss", "Gauss"},
    };
    for (const Carrier& carrier : carriers)
    {
        const AgentEntry* agent = agents.find(carrier.agentId);
        FOSSYND_CHECK(agent != nullptr);
        FOSSYND_CHECK(weapons.find(carrier.weaponId) != nullptr);
        if (agent != nullptr)
        {
            FOSSYND_CHECK_EQ(agent->spec.weapon.name, std::string(carrier.weaponName));
        }
    }
}

// The shared vehicle catalogue (issue #319) must be present in the shipped
// content and parse to the intended kinds. This is the guard that keeps a
// vehicle from silently vanishing from vehicles.json, and it pins each
// template's kind and mounted-weapon reference so a mis-edit cannot slip
// through (golden rule 7: vehicles are data, not code).
FOSSYND_TEST(ContentValidation, ShippedVehicleCatalogueLoads)
{
    const std::string dir = std::string(FOSSYND_SOURCE_DIR) + "/assets/data";
    WeaponCatalogue weapons;
    bool found = false;
    std::string error;
    FOSSYND_CHECK(weapons.loadFromFile(dir + "/weapons.json", found, error));
    FOSSYND_CHECK(found);

    VehicleCatalogue vehicles;
    FOSSYND_CHECK(vehicles.loadFromFile(dir + "/vehicles.json", weapons, found, error));
    FOSSYND_CHECK(found);
    FOSSYND_CHECK_EQ(vehicles.size(), static_cast<std::size_t>(4));

    const VehicleEntry* car = vehicles.find("car");
    const VehicleEntry* train = vehicles.find("train");
    FOSSYND_CHECK(car != nullptr);
    FOSSYND_CHECK(train != nullptr);
    if (car == nullptr || train == nullptr)
    {
        return;
    }

    // The code defaults (`makeCar`/`makeTrain`) are the catalogue's car/train, so
    // the two representations cannot silently drift apart.
    FOSSYND_CHECK_EQ(car->spec.name, std::string("Car"));
    FOSSYND_CHECK(car->spec.kind == VehicleKind::Car);
    FOSSYND_CHECK_NEAR(car->spec.speed, kCarSpeed);
    FOSSYND_CHECK_NEAR(car->spec.health, kCarMaxHealth);
    FOSSYND_CHECK_EQ(car->spec.weapon.name, std::string("Uzi"));

    FOSSYND_CHECK_EQ(train->spec.name, std::string("Train"));
    FOSSYND_CHECK(train->spec.kind == VehicleKind::Train);
    FOSSYND_CHECK_NEAR(train->spec.speed, kTrainSpeed);
    FOSSYND_CHECK_NEAR(train->spec.health, kTrainMaxHealth);
    FOSSYND_CHECK_EQ(train->spec.weapon.name, std::string("Rifle"));

    // Every entry's `weaponId` must resolve (a typo would otherwise deploy a
    // default weapon silently); the loader already rejects an unknown id, so
    // reaching here with a parsed entry proves the reference resolved.
    for (const VehicleEntry& entry : vehicles.entries())
    {
        FOSSYND_CHECK(!entry.id.empty());
        FOSSYND_CHECK(entry.spec.weapon.damage > 0.0f);
        FOSSYND_CHECK(entry.spec.weapon.range > 0.0f);
        FOSSYND_CHECK(entry.spec.weapon.fireRate > 0.0f);
    }
}
