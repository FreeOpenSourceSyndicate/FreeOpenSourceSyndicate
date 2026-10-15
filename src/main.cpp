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

// Build version, injected by CMake from `project(... VERSION ...)` so a release
// bump changes the output automatically (issue #147). The fallback keeps a
// non-CMake build compiling; it is never the version of a packaged build.
#ifndef FOSSYND_VERSION
#define FOSSYND_VERSION "0.0.0-unknown"
#endif

// The CLI help text. Lists every flag the parser below accepts, split into
// player-facing options and the headless verification checks (issue #147).
void printUsage()
{
    std::printf(
        "FOSSynd %s\n"
        "Usage: FOSSynd [options]\n"
        "\n"
        "Options:\n"
        "  -h, --help                 Show this help and exit.\n"
        "  -V, --version              Show the version and exit.\n"
        "      --mission <path>       Load the given mission file.\n"
        "      --config <path>        Load the given config file.\n"
        "      --lang <locale>        Set the UI language (e.g. en, de).\n"
        "      --demo                 Attract mode: the AI plays the mission.\n"
        "      --title                Show the title card.\n"
        "      --low-spec             Small window, software renderer (old hardware).\n"
        "      --window <W>x<H>       Set the window size in pixels (e.g. 1280x800).\n"
        "      --fullscreen           Start in fullscreen (F11 toggles it).\n"
        "      --pause                Start paused (Space toggles it).\n"
        "      --zoom <n>             Set the integer zoom level (1-3).\n"
        "      --headless             Run without a display.\n"
        "      --frames <n>           Run n frames, then exit.\n"
        "      --screenshot <file>    Render a frame to <file> (BMP) and exit.\n"
        "\n"
        "Verification (headless checks, for CI):\n"
        "      --check-sprites        Validate the sprite sheet bakes.\n"
        "      --check-minimap        Validate the minimap bakes.\n"
        "      --check-selection      Validate selection and control groups.\n"
        "      --check-waypoints      Validate the shift-click waypoint queue.\n"
        "      --check-box-select     Validate box-select.\n"
        "      --check-waypoint-path  Validate the queued-route overlay.\n"
        "      --check-zoom           Validate integer zoom levels.\n"
        "      --check-fullscreen     Validate the fullscreen toggle.\n"
        "      --check-pause          Validate pause/resume.\n"
        "      --check-lang [locale]  Validate the UI string table.\n"
        "      --check-content [dir]  Validate mission/catalogue references.\n",
        FOSSYND_VERSION);
}

// Parse a "<W>x<H>" window size (e.g. "1280x800") into width/height. Returns
// false (leaving the outputs unchanged) on malformed input, zero, negative, or
// an absurdly large value, so a bad --window is rejected rather than silently
// ignored (issue #152).
bool parseWindowSize(const char* spec, int& width, int& height)
{
    if (spec == nullptr)
    {
        return false;
    }
    char* end = nullptr;
    const long w = std::strtol(spec, &end, 10);
    if (end == spec || (*end != 'x' && *end != 'X'))
    {
        return false;
    }
    const char* heightStart = end + 1;
    end = nullptr;
    const long h = std::strtol(heightStart, &end, 10);
    if (end == heightStart || *end != '\0')
    {
        return false;
    }
    // Reject zero, negative, and absurdly large sizes; the upper bound keeps the
    // window within what the software renderer and old hardware can handle.
    const long kMaxDimension = 16384;
    if (w <= 0 || h <= 0 || w > kMaxDimension || h > kMaxDimension)
    {
        return false;
    }
    width = static_cast<int>(w);
    height = static_cast<int>(h);
    return true;
}

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
    bool fullscreen = false;
    bool startPaused = false;
    bool checkSprites = false;
    bool checkMinimap = false;
    bool checkSelection = false;
    bool checkWaypoints = false;
    bool checkBoxSelect = false;
    bool checkWaypointPath = false;
    bool checkLang = false;
    bool checkZoom = false;
    bool checkFullscreen = false;
    bool checkPause = false;
    const char* checkLangLocale = nullptr; // --check-lang [<locale>]
    const char* checkContent = nullptr;   // --check-content [<dataDir>]
    const char* screenshot = nullptr;
    const char* windowSpec = nullptr;     // --window <W>x<H>
    bool showHelp = false;
    bool showVersion = false;
    bool cliError = false;    // set on an unrecognised option (issue #248)
    int zoomLevel = 1;        // --zoom <n>
    std::string cliMission;   // --mission <path>
    std::string cliConfig;    // --config <path>
    std::string cliLang;      // --lang <locale>

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--headless") == 0)
        {
            headless = true;
        }
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
        {
            showHelp = true;
        }
        else if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-V") == 0)
        {
            showVersion = true;
        }
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
        {
            maxFrames = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
        {
            screenshot = argv[++i];
        }
        else if (std::strcmp(argv[i], "--window") == 0 && i + 1 < argc)
        {
            windowSpec = argv[++i];
        }
        else if (std::strcmp(argv[i], "--fullscreen") == 0)
        {
            fullscreen = true;
        }
        else if (std::strcmp(argv[i], "--pause") == 0)
        {
            startPaused = true;
        }
        else if (std::strcmp(argv[i], "--demo") == 0)
        {
            demo = true;
        }
        else if (std::strcmp(argv[i], "--low-spec") == 0)
        {
            lowSpec = true;
        }
        else if (std::strcmp(argv[i], "--zoom") == 0 && i + 1 < argc)
        {
            zoomLevel = std::atoi(argv[++i]);
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
        else if (std::strcmp(argv[i], "--check-box-select") == 0)
        {
            checkBoxSelect = true;
        }
        else if (std::strcmp(argv[i], "--check-waypoint-path") == 0)
        {
            checkWaypointPath = true;
        }
        else if (std::strcmp(argv[i], "--check-zoom") == 0)
        {
            checkZoom = true;
        }
        else if (std::strcmp(argv[i], "--check-fullscreen") == 0)
        {
            checkFullscreen = true;
        }
        else if (std::strcmp(argv[i], "--check-pause") == 0)
        {
            checkPause = true;
        }
        else if (std::strcmp(argv[i], "--check-lang") == 0)
        {
            // Optional locale argument; defaults to English.
            checkLang = true;
            checkLangLocale = "en";
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                checkLangLocale = argv[++i];
            }
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
        else if (std::strcmp(argv[i], "--lang") == 0 && i + 1 < argc)
        {
            cliLang = argv[++i];
        }
        else if (std::strcmp(argv[i], "--mission") == 0 && i + 1 < argc)
        {
            cliMission = argv[++i];
        }
        else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            cliConfig = argv[++i];
        }
        else
        {
            // Unknown option or a value flag with a missing value: report it
            // rather than silently ignoring it, so a typo (`--pausee`) cannot
            // start the game with the wrong settings (issue #248).
            std::fprintf(stderr, "unknown or incomplete option '%s' (try --help)\n", argv[i]);
            cliError = true;
        }
    }

#if defined(FOSSYND_LOW_SPEC_BUILD)
    // A low-spec build always runs in low-spec mode unless the caller opts out.
    lowSpec = true;
#endif

    // --help / --version are informational: print and exit before any config
    // lookup, content load, or SDL init, so they are fast and scriptable
    // (issue #147). `--help` wins if both are given.
    if (showHelp)
    {
        printUsage();
        return 0;
    }
    if (showVersion)
    {
        std::printf("FOSSynd %s\n", FOSSYND_VERSION);
        return 0;
    }
    // A bad option must not start a game with the wrong settings: fail fast,
    // before any config lookup or content load (issue #248).
    if (cliError)
    {
        return 1;
    }

    // Validate --window up front so a bad value fails fast (before any config
    // lookup or content load), not just before init (issue #152).
    int width = lowSpec ? 320 : 960;
    int height = lowSpec ? 200 : 600;
    if (windowSpec != nullptr && !parseWindowSize(windowSpec, width, height))
    {
        std::fprintf(stderr, "invalid --window '%s': expected <W>x<H> with 1..16384 (e.g. 1280x800)\n",
                     windowSpec);
        return 1;
    }

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

    // Resolve the UI language (issue #123): a `--lang` flag overrides a `lang`
    // config key; neither set means English. An unknown locale is handled by the
    // game (warn and keep English), never a failure here.
    const std::string lang = !cliLang.empty() ? cliLang : config.lang;

    fossynd::game::Game game;
    game.setMissionPath(missionPath);
    game.setExeDir(exeDir);
    game.setLang(lang);
    game.setInitialZoom(zoomLevel);
    game.setFullscreen(fullscreen);
    game.setPaused(startPaused);

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

    if (checkBoxSelect)
    {
        // Headless guard for box-select (issue #94): the drag rectangle must
        // select exactly the enclosed living player agents. Used by the
        // game_box_select ctest.
        const bool ok = game.boxSelectSelfTest();
        std::printf(ok ? "box-select: ok\n" : "box-select: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkWaypointPath)
    {
        // Headless guard for the queued-route overlay (issue #89): the route the
        // renderer would draw must start at the agent and visit every queued
        // waypoint in order. Used by the game_waypoint_path ctest.
        const bool ok = game.waypointPathSelfTest();
        std::printf(ok ? "waypoint-path: ok\n" : "waypoint-path: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkLang)
    {
        // Headless guard for the UI string table (issue #53): the table must
        // load and resolve every key the HUD/title card reads. With a locale
        // argument (issue #123) it also proves that locale actually resolved —
        // a non-English locale must overlay and must translate a known key.
        bool ok = game.languageSelfTest();
        if (ok && checkLangLocale != nullptr && std::strcmp(checkLangLocale, "en") != 0)
        {
            // The requested locale must have actually resolved — not silently
            // fallen back to English — and must translate a known key.
            ok = game.langOverlayLoaded() && game.langHasKey("hud.squad");
            // The loaded mission's name must resolve through that locale too
            // (issue #176): a locale that carries `mission.<id>.name` must show
            // it, proving the mission-text wiring works, not just the UI chrome.
            ok = ok && game.missionNameLocalized();
            if (!ok)
            {
                std::fprintf(stderr, "language self-test: locale '%s' did not resolve\n",
                             checkLangLocale);
            }
        }
        std::printf(ok ? "language: ok\n" : "language: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkZoom)
    {
        // Headless guard for integer zoom (issue #122): the screen<->tile
        // round-trip and the clamping must hold at every level. Used by the
        // game_zoom ctest.
        const bool ok = game.zoomSelfTest();
        std::printf(ok ? "zoom: ok\n" : "zoom: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkFullscreen)
    {
        // Headless guard for the fullscreen toggle (issue #194): the F11 state
        // machine must flip and round-trip, and the SDL call must be a safe
        // no-op without a display. Used by the game_fullscreen ctest.
        const bool ok = game.fullscreenSelfTest();
        std::printf(ok ? "fullscreen: ok\n" : "fullscreen: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkPause)
    {
        // Headless guard for pause/resume (issue #210): a paused update() must
        // advance no simulation time, an unpaused one must, and the toggle must
        // round-trip. Used by the game_pause ctest.
        const bool ok = game.pauseSelfTest();
        std::printf(ok ? "pause: ok\n" : "pause: FAILED\n");
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
