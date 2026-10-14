/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/ContentValidation.h"

#include <filesystem>
#include <fstream>
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

// The shipped content must always be valid: this is the guard that keeps a
// broken mission or catalogue out of `main`.
FOSSYND_TEST(ContentValidation, ShippedContentPasses)
{
    const std::string dataDir = std::string(FOSSYND_SOURCE_DIR) + "/assets/data";
    const ValidationReport report = validateContentDir(dataDir);
    FOSSYND_CHECK(report.ok());
    FOSSYND_CHECK(report.missionsChecked >= static_cast<std::size_t>(2));
    FOSSYND_CHECK(report.cataloguesChecked >= static_cast<std::size_t>(1));
}
