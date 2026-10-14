/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/Config.h"
#include "core/data/ContentPaths.h"

using namespace fossynd::core;

FOSSYND_TEST(Config, ParsesKeysAndIgnoresComments)
{
    GameConfig config;
    std::string error;
    const std::string text = "# a comment\n"
                             "; another\n"
                             "mission = assets/data/missions/demo.json\n"
                             "mission_dir =/usr/share/fossynd/missions\n"
                             "lang = de\n"
                             "unknown_key = ignored\n";
    FOSSYND_CHECK(parseConfig(text, config, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(config.mission, std::string("assets/data/missions/demo.json"));
    FOSSYND_CHECK_EQ(config.missionDir, std::string("/usr/share/fossynd/missions"));
    FOSSYND_CHECK_EQ(config.lang, std::string("de"));
}

FOSSYND_TEST(Config, LangDefaultsToEmpty)
{
    // A config without a `lang` key leaves the language unset (English).
    GameConfig config;
    std::string error;
    FOSSYND_CHECK(parseConfig("mission = demo.json\n", config, error));
    FOSSYND_CHECK(config.lang.empty());
}

FOSSYND_TEST(Config, RejectsMalformedLine)
{
    GameConfig config;
    std::string error;
    FOSSYND_CHECK(!parseConfig("mission assets/demo.json\n", config, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Config, MissingFileIsNotAnError)
{
    GameConfig config;
    std::string error;
    bool found = true;
    FOSSYND_CHECK(loadConfigFile("this/config/does/not/exist.conf", config, found, error));
    FOSSYND_CHECK(!found);
    FOSSYND_CHECK(error.empty());
}

FOSSYND_TEST(ContentPaths, CliFlagWinsWhenItExists)
{
    GameConfig config;
    config.mission = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions/demo.json";

    const std::string cli = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions/demo.json";
    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSSYND_CHECK(resolveMissionPath(config, cli, "", out, source, error));
    FOSSYND_CHECK(source == MissionSource::CliFlag);
    FOSSYND_CHECK_EQ(out, cli);
}

FOSSYND_TEST(ContentPaths, ConfigPathUsedWhenNoCliFlag)
{
    GameConfig config;
    config.mission = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions/demo.json";

    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSSYND_CHECK(resolveMissionPath(config, "", "", out, source, error));
    FOSSYND_CHECK(source == MissionSource::ConfigFile);
    FOSSYND_CHECK_EQ(out, config.mission);
}

FOSSYND_TEST(ContentPaths, MissingExplicitPathIsAnError)
{
    GameConfig config;
    config.mission = "does/not/exist.json";

    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSSYND_CHECK(!resolveMissionPath(config, "", "", out, source, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(ContentPaths, MissionDirResolvesTheDefaultFile)
{
    GameConfig config;
    config.missionDir = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions";

    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSSYND_CHECK(resolveMissionPath(config, "", "", out, source, error));
    FOSSYND_CHECK(source == MissionSource::ConfigDir);
    FOSSYND_CHECK_EQ(out, config.missionDir + "/demo.json");
    FOSSYND_CHECK(fileExists(out));
}

FOSSYND_TEST(ContentPaths, DefaultMissionPathsAreOrdered)
{
    // Pure function, independent of the working directory.
    const std::vector<std::string> paths = defaultMissionPaths("/opt/fossynd/bin");
    FOSSYND_CHECK(!paths.empty());
    FOSSYND_CHECK_EQ(paths.front(), std::string("assets/data/missions/demo.json"));
    bool hasShare = false;
    for (const std::string& p : paths)
    {
        if (p == "/usr/share/fossynd/missions/demo.json")
        {
            hasShare = true;
        }
    }
    FOSSYND_CHECK(hasShare);
}

FOSSYND_TEST(ContentPaths, FallsBackToTheFirstExistingLocation)
{
    // No CLI flag and no config: the built-in fallbacks are checked in order.
    // Point exeDir at the source tree so a fallback exists regardless of the
    // working directory (ctest runs from the build tree).
    GameConfig config;
    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSSYND_CHECK(resolveMissionPath(config, "", std::string(FOSSYND_SOURCE_DIR), out, source, error));
    FOSSYND_CHECK(source == MissionSource::Fallback);
    FOSSYND_CHECK(fileExists(out));
    const std::string suffix = "assets/data/missions/demo.json";
    FOSSYND_CHECK(out.size() >= suffix.size());
    FOSSYND_CHECK_EQ(out.substr(out.size() - suffix.size()), suffix);
}

FOSSYND_TEST(ContentPaths, DefaultConfigPathsAreOrdered)
{
    const std::vector<std::string> paths = defaultConfigPaths("/opt/fossynd/bin");
    FOSSYND_CHECK(!paths.empty());
    FOSSYND_CHECK_EQ(paths.front(), std::string("fossynd.conf"));
    FOSSYND_CHECK_EQ(paths.back(), std::string("/usr/local/etc/fossynd.conf"));
}

FOSSYND_TEST(ContentPaths, NothingFoundLeavesTheEmbeddedFallbackToTheCaller)
{
    // Point every fallback at a non-existent exeDir and no config/CLI. The
    // repo-relative fallback may still exist, so this test only asserts the
    // contract: either a Fallback path is returned, or None with no error.
    GameConfig config;
    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSSYND_CHECK(resolveMissionPath(config, "", "/no/such/exe/dir", out, source, error));
    FOSSYND_CHECK(error.empty());
    if (source == MissionSource::None)
    {
        FOSSYND_CHECK(out.empty());
    }
    else
    {
        FOSSYND_CHECK(fileExists(out));
    }
}
