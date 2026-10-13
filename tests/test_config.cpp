/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/Config.h"
#include "core/data/ContentPaths.h"

using namespace foss::core;

FOSS_TEST(Config, ParsesKeysAndIgnoresComments)
{
    GameConfig config;
    std::string error;
    const std::string text = "# a comment\n"
                             "; another\n"
                             "mission = assets/data/missions/demo.json\n"
                             "mission_dir =/usr/share/fossynd/missions\n"
                             "unknown_key = ignored\n";
    FOSS_CHECK(parseConfig(text, config, error));
    FOSS_CHECK(error.empty());
    FOSS_CHECK_EQ(config.mission, std::string("assets/data/missions/demo.json"));
    FOSS_CHECK_EQ(config.missionDir, std::string("/usr/share/fossynd/missions"));
}

FOSS_TEST(Config, RejectsMalformedLine)
{
    GameConfig config;
    std::string error;
    FOSS_CHECK(!parseConfig("mission assets/demo.json\n", config, error));
    FOSS_CHECK(!error.empty());
}

FOSS_TEST(Config, MissingFileIsNotAnError)
{
    GameConfig config;
    std::string error;
    bool found = true;
    FOSS_CHECK(loadConfigFile("this/config/does/not/exist.conf", config, found, error));
    FOSS_CHECK(!found);
    FOSS_CHECK(error.empty());
}

FOSS_TEST(ContentPaths, CliFlagWinsWhenItExists)
{
    GameConfig config;
    config.mission = std::string(FOSS_SOURCE_DIR) + "/assets/data/missions/demo.json";

    const std::string cli = std::string(FOSS_SOURCE_DIR) + "/assets/data/missions/demo.json";
    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSS_CHECK(resolveMissionPath(config, cli, "", out, source, error));
    FOSS_CHECK(source == MissionSource::CliFlag);
    FOSS_CHECK_EQ(out, cli);
}

FOSS_TEST(ContentPaths, ConfigPathUsedWhenNoCliFlag)
{
    GameConfig config;
    config.mission = std::string(FOSS_SOURCE_DIR) + "/assets/data/missions/demo.json";

    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSS_CHECK(resolveMissionPath(config, "", "", out, source, error));
    FOSS_CHECK(source == MissionSource::ConfigFile);
    FOSS_CHECK_EQ(out, config.mission);
}

FOSS_TEST(ContentPaths, MissingExplicitPathIsAnError)
{
    GameConfig config;
    config.mission = "does/not/exist.json";

    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSS_CHECK(!resolveMissionPath(config, "", "", out, source, error));
    FOSS_CHECK(!error.empty());
}

FOSS_TEST(ContentPaths, MissionDirResolvesTheDefaultFile)
{
    GameConfig config;
    config.missionDir = std::string(FOSS_SOURCE_DIR) + "/assets/data/missions";

    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSS_CHECK(resolveMissionPath(config, "", "", out, source, error));
    FOSS_CHECK(source == MissionSource::ConfigDir);
    FOSS_CHECK_EQ(out, config.missionDir + "/demo.json");
    FOSS_CHECK(fileExists(out));
}

FOSS_TEST(ContentPaths, DefaultMissionPathsAreOrdered)
{
    // Pure function, independent of the working directory.
    const std::vector<std::string> paths = defaultMissionPaths("/opt/fossynd/bin");
    FOSS_CHECK(!paths.empty());
    FOSS_CHECK_EQ(paths.front(), std::string("assets/data/missions/demo.json"));
    bool hasShare = false;
    for (const std::string& p : paths)
    {
        if (p == "/usr/share/fossynd/missions/demo.json")
        {
            hasShare = true;
        }
    }
    FOSS_CHECK(hasShare);
}

FOSS_TEST(ContentPaths, FallsBackToTheFirstExistingLocation)
{
    // No CLI flag and no config: the built-in fallbacks are checked in order.
    // Point exeDir at the source tree so a fallback exists regardless of the
    // working directory (ctest runs from the build tree).
    GameConfig config;
    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSS_CHECK(resolveMissionPath(config, "", std::string(FOSS_SOURCE_DIR), out, source, error));
    FOSS_CHECK(source == MissionSource::Fallback);
    FOSS_CHECK(fileExists(out));
    const std::string suffix = "assets/data/missions/demo.json";
    FOSS_CHECK(out.size() >= suffix.size());
    FOSS_CHECK_EQ(out.substr(out.size() - suffix.size()), suffix);
}

FOSS_TEST(ContentPaths, DefaultConfigPathsAreOrdered)
{
    const std::vector<std::string> paths = defaultConfigPaths("/opt/fossynd/bin");
    FOSS_CHECK(!paths.empty());
    FOSS_CHECK_EQ(paths.front(), std::string("fossynd.conf"));
    FOSS_CHECK_EQ(paths.back(), std::string("/usr/local/etc/fossynd.conf"));
}

FOSS_TEST(ContentPaths, NothingFoundLeavesTheEmbeddedFallbackToTheCaller)
{
    // Point every fallback at a non-existent exeDir and no config/CLI. The
    // repo-relative fallback may still exist, so this test only asserts the
    // contract: either a Fallback path is returned, or None with no error.
    GameConfig config;
    std::string out;
    MissionSource source = MissionSource::None;
    std::string error;
    FOSS_CHECK(resolveMissionPath(config, "", "/no/such/exe/dir", out, source, error));
    FOSS_CHECK(error.empty());
    if (source == MissionSource::None)
    {
        FOSS_CHECK(out.empty());
    }
    else
    {
        FOSS_CHECK(fileExists(out));
    }
}
