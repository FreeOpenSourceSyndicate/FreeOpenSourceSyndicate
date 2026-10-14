/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#include "core/data/Config.h"
#include "core/data/ContentPaths.h"
#include "core/data/ContentValidation.h"
#include "game/Game.h"

#if defined(__linux__)
#include <unistd.h>
#endif

namespace
{

// Directory containing the running executable, or "" if it cannot be found.
// Used to locate mission data and config files next to the binary.
std::string executableDir(const char* argv0)
{
#if defined(__linux__)
    char buffer[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n > 0)
    {
        buffer[n] = '\0';
        std::string path(buffer);
        const std::size_t slash = path.find_last_of('/');
        if (slash != std::string::npos)
        {
            return path.substr(0, slash);
        }
    }
#endif
    // Fall back to the directory of argv[0] if it contains a slash.
    if (argv0 != nullptr)
    {
        const std::string path(argv0);
        const std::size_t slash = path.find_last_of('/');
        if (slash != std::string::npos)
        {
            return path.substr(0, slash);
        }
    }
    return "";
}

} // namespace

int main(int argc, char** argv)
{
    int maxFrames = 0;
    bool headless = false;
    bool demo = false;
    bool lowSpec = false;
    bool title = false;
    bool checkSprites = false;
    bool checkMinimap = false;
    bool checkSelection = false;
    bool checkWaypoints = false;
    bool checkLang = false;
    const char* checkContent = nullptr;   // --check-content [<dataDir>]
    const char* screenshot = nullptr;
    std::string cliMission;   // --mission <path>
    std::string cliConfig;    // --config <path>

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--headless") == 0)
        {
            headless = true;
        }
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
        {
            maxFrames = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
        {
            screenshot = argv[++i];
        }
        else if (std::strcmp(argv[i], "--demo") == 0)
        {
            demo = true;
        }
        else if (std::strcmp(argv[i], "--low-spec") == 0)
        {
            lowSpec = true;
        }
        else if (std::strcmp(argv[i], "--title") == 0)
        {
            title = true;
        }
        else if (std::strcmp(argv[i], "--check-sprites") == 0)
        {
            checkSprites = true;
        }
        else if (std::strcmp(argv[i], "--check-minimap") == 0)
        {
            checkMinimap = true;
        }
        else if (std::strcmp(argv[i], "--check-selection") == 0)
        {
            checkSelection = true;
        }
        else if (std::strcmp(argv[i], "--check-waypoints") == 0)
        {
            checkWaypoints = true;
        }
        else if (std::strcmp(argv[i], "--check-lang") == 0)
        {
            checkLang = true;
        }
        else if (std::strcmp(argv[i], "--check-content") == 0)
        {
            // Optional data directory argument; defaults to the repo layout.
            checkContent = "assets/data";
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                checkContent = argv[++i];
            }
        }
        else if (std::strcmp(argv[i], "--mission") == 0 && i + 1 < argc)
        {
            cliMission = argv[++i];
        }
        else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            cliConfig = argv[++i];
        }
    }

#if defined(FOSSYND_LOW_SPEC_BUILD)
    // A low-spec build always runs in low-spec mode unless the caller opts out.
    lowSpec = true;
#endif

    // Resolve which mission to load: config file first, then a CLI --mission
    // flag overrides it; if neither gives a path, scan the built-in fallback
    // locations and take the first that exists; otherwise use the embedded copy.
    const std::string exeDir = executableDir(argc > 0 ? argv[0] : nullptr);

    // Content validation (issue #81) needs no SDL, so run it before init() and
    // exit: this lets CI and packagers check shipped data without a display.
    if (checkContent != nullptr)
    {
        const fossynd::core::content::ValidationReport report =
            fossynd::core::content::validateContentDir(checkContent);
        std::fputs(fossynd::core::content::formatReport(report).c_str(), stdout);
        if (report.ok())
        {
            std::printf("content: ok (%zu missions, %zu catalogue entries)\n",
                        report.missionsChecked, report.cataloguesChecked);
            return 0;
        }
        return 1;
    }

    fossynd::core::GameConfig config;
    std::string configError;
    if (!cliConfig.empty())
    {
        bool found = false;
        if (!fossynd::core::loadConfigFile(cliConfig, config, found, configError))
        {
            std::fprintf(stderr, "config error (%s): %s\n", cliConfig.c_str(), configError.c_str());
            return 1;
        }
    }
    else
    {
        for (const std::string& candidate : fossynd::core::defaultConfigPaths(exeDir))
        {
            bool found = false;
            if (!fossynd::core::loadConfigFile(candidate, config, found, configError))
            {
                std::fprintf(stderr, "config error (%s): %s\n", candidate.c_str(),
                             configError.c_str());
                return 1;
            }
            if (found)
            {
                break;
            }
        }
    }

    std::string missionPath;
    fossynd::core::MissionSource missionSource = fossynd::core::MissionSource::None;
    std::string missionError;
    if (!fossynd::core::resolveMissionPath(config, cliMission, exeDir, missionPath, missionSource,
                                        missionError))
    {
        std::fprintf(stderr, "mission path error: %s\n", missionError.c_str());
        return 1;
    }
    if (missionSource != fossynd::core::MissionSource::None)
    {
        std::printf("mission: %s\n", missionPath.c_str());
    }

    // Default window size; the low-spec profile shrinks it (ADR-0008).
    int width = lowSpec ? 320 : 960;
    int height = lowSpec ? 200 : 600;

    fossynd::game::Game game;
    game.setMissionPath(missionPath);
    game.setExeDir(exeDir);

    // Show the authored title card for a real, interactive player. Automated
    // runs (headless, screenshots, attract mode) skip it and go straight to the
    // map. `--title` forces it (used to capture the card headlessly). Must be set
    // before init(), which latches the flag into the initial game state.
    const bool interactive = !headless && screenshot == nullptr && !demo;
    game.setShowTitleCard(title || interactive);

    if (!game.init(width, height, headless, lowSpec))
    {
        std::fprintf(stderr, "Failed to start the game.\n");
        return 1;
    }
    game.setAutoPlay(demo);

    if (checkSprites)
    {
        // Headless guard for the sprite pipeline: the authored sheet must bake
        // into textures for every role. Used by the game_sprites ctest.
        const bool ok = game.spritesValid();
        std::printf(ok ? "sprites: ok\n" : "sprites: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkMinimap)
    {
        // Headless guard for the minimap: the panel must bake and place a blip
        // for every living agent. Used by the game_minimap ctest.
        const bool ok = game.checkMinimap();
        std::printf(ok ? "minimap: ok\n" : "minimap: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkSelection)
    {
        // Headless guard for the selection / control-group logic (issue #33),
        // which is otherwise only reachable through the interactive input path.
        const bool ok = game.selectionSelfTest();
        std::printf(ok ? "selection: ok\n" : "selection: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkWaypoints)
    {
        // Headless guard for the shift-click waypoint queue (issue #75), which is
        // otherwise only reachable through the interactive mouse handler.
        const bool ok = game.waypointSelfTest();
        std::printf(ok ? "waypoints: ok\n" : "waypoints: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkLang)
    {
        // Headless guard for the UI string table (issue #53): the table must
        // load and resolve every key the HUD/title card reads. Used by the
        // game_lang ctest.
        const bool ok = game.languageSelfTest();
        std::printf(ok ? "language: ok\n" : "language: FAILED\n");
        return ok ? 0 : 1;
    }

    if (screenshot != nullptr)
    {
        // Optionally advance the simulation first (e.g. to capture mid-combat).
        // `--frames N` means N *rendered* frames, so run N-1 here and let
        // saveScreenshot() render the Nth; otherwise the two together would
        // advance one frame too many and change what a screenshot captures.
        if (maxFrames > 1)
        {
            game.run(maxFrames - 1);
        }
        const bool ok = game.saveScreenshot(screenshot);
        std::printf(ok ? "wrote %s\n" : "failed to write %s\n", screenshot);
        return ok ? 0 : 1;
    }

    return game.run(maxFrames);
}
