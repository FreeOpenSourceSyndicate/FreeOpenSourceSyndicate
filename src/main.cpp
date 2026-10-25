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
        "      --campaign <path>      Load the given campaign file (world map).\n"
        "      --config <path>        Load the given config file.\n"
        "      --save <path>          Save campaign progress to <path> on exit.\n"
        "      --load <path>          Restore campaign progress from <path>.\n"
        "      --new-game             Name the syndicate and pick a logo first.\n"
        "      --lang <locale>        Set the UI language (e.g. en, de).\n"
        "      --demo                 Attract mode: the AI plays the mission.\n"
        "      --title                Show the title card.\n"
        "      --map                  Start on the campaign management screen.\n"
        "      --low-spec             Small window, software renderer (old hardware).\n"
        "      --window <W>x<H>       Set the window size in pixels (e.g. 1280x800).\n"
        "      --fullscreen           Start in fullscreen (F11 toggles it).\n"
        "      --pause                Start paused (Space toggles it).\n"
        "      --breach               Breach all destructible walls before rendering.\n"
        "      --scan                 Activate a scanner pulse on the squad before rendering.\n"
        "      --shield               Raise a shield on the squad before rendering.\n"
        "      --drugs                Take performance drugs on the squad before rendering.\n"
        "      --vehicle              Spawn a demo car and train before rendering.\n"
        "      --bomb                 Plant a demo time bomb on the squad before rendering.\n"
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
        "      --check-gadgets        Validate the tactical gadget hotkeys.\n"
        "      --check-rubble         Validate breached-tile rubble rendering.\n"
        "      --check-scanner        Validate the scanner ping overlay.\n"
        "      --check-shield         Validate the personal shield overlay.\n"
        "      --check-drugs          Validate the performance-drug aura overlay.\n"
        "      --check-drug-controls  Validate the three drug-bar controls.\n"
        "      --check-drug-bars      Validate the three drug-bar HUD.\n"
        "      --check-bomb           Validate the time-bomb demo hook.\n"
        "      --check-vehicles       Validate vehicle rendering.\n"
        "      --check-vehicle-input  Validate the hijack/drive/exit keys.\n"
        "      --check-audio          Validate the sound-effect cue table.\n"
        "      --check-objectives     Validate the objective banner for every kind.\n"
        "      --check-campaign       Validate the campaign management loop.\n"
        "      --check-deploy         Validate deploying a mission from the map.\n"
        "      --check-campaign-keys  Validate map-screen key isolation.\n"
        "      --check-save-load      Validate the campaign save/load round-trip.\n"
        "      --check-save-hotkeys   Validate the in-game F5/F9 save/load keys.\n"
        "      --check-newgame        Validate the new-game name/logo decision.\n"
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

// Parse a frame count for `--frames`. Requires a positive integer with no
// trailing garbage, so a typo cannot silently mean "no limit" (0) or run a
// partial count (`5x`) — `std::atoi` would accept both (issue #264).
bool parsePositiveCount(const char* spec, int& out)
{
    if (spec == nullptr)
    {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(spec, &end, 10);
    if (end == spec || *end != '\0' || value <= 0 || value > 1000000000L)
    {
        return false;
    }
    out = static_cast<int>(value);
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
    bool breachWalls = false;
    bool scanSquad = false;               // --scan
    bool shieldSquad = false;             // --shield
    bool drugsSquad = false;              // --drugs
    bool spawnVehicles = false;           // --vehicle
    bool plantBomb = false;               // --bomb
    bool checkVehicles = false;           // --check-vehicles
    bool checkVehicleInput = false;       // --check-vehicle-input
    bool checkAudio = false;              // --check-audio
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
    bool checkGadgets = false;
    bool checkRubble = false;
    bool checkScanner = false;
    bool checkShield = false;
    bool checkDrugs = false;
    bool checkDrugControls = false;       // --check-drug-controls
    bool checkDrugBars = false;           // --check-drug-bars
    bool checkBomb = false;               // --check-bomb
    bool checkObjectives = false;
    bool checkCampaign = false;
    bool checkDeploy = false;
    bool checkCampaignKeys = false;
    bool checkSaveLoad = false;           // --check-save-load
    bool checkSaveHotkeys = false;        // --check-save-hotkeys
    bool checkNewGame = false;            // --check-newgame
    bool newGame = false;                 // --new-game
    bool showMap = false;                 // --map
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
    std::string cliCampaign;  // --campaign <path>
    std::string cliSave;      // --save <path>
    std::string cliLoad;      // --load <path>
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
        else if (std::strcmp(argv[i], "--frames") == 0)
        {
            // Require a positive count: atoi would turn `abc`/`0`/`-3` into 0
            // ("no limit", so the game never exits) and accept `5x` (issue #264).
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "unknown or incomplete option '%s' (try --help)\n", argv[i]);
                cliError = true;
            }
            else if (!parsePositiveCount(argv[++i], maxFrames))
            {
                std::fprintf(stderr, "invalid --frames '%s': expected a positive count\n", argv[i]);
                cliError = true;
            }
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
        else if (std::strcmp(argv[i], "--breach") == 0)
        {
            breachWalls = true;
        }
        else if (std::strcmp(argv[i], "--scan") == 0)
        {
            scanSquad = true;
        }
        else if (std::strcmp(argv[i], "--shield") == 0)
        {
            shieldSquad = true;
        }
        else if (std::strcmp(argv[i], "--drugs") == 0)
        {
            drugsSquad = true;
        }
        else if (std::strcmp(argv[i], "--vehicle") == 0)
        {
            spawnVehicles = true;
        }
        else if (std::strcmp(argv[i], "--bomb") == 0)
        {
            plantBomb = true;
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
        else if (std::strcmp(argv[i], "--check-gadgets") == 0)
        {
            checkGadgets = true;
        }
        else if (std::strcmp(argv[i], "--check-rubble") == 0)
        {
            checkRubble = true;
        }
        else if (std::strcmp(argv[i], "--check-scanner") == 0)
        {
            checkScanner = true;
        }
        else if (std::strcmp(argv[i], "--check-shield") == 0)
        {
            checkShield = true;
        }
        else if (std::strcmp(argv[i], "--check-drugs") == 0)
        {
            checkDrugs = true;
        }
        else if (std::strcmp(argv[i], "--check-drug-controls") == 0)
        {
            checkDrugControls = true;
        }
        else if (std::strcmp(argv[i], "--check-drug-bars") == 0)
        {
            checkDrugBars = true;
        }
        else if (std::strcmp(argv[i], "--check-bomb") == 0)
        {
            checkBomb = true;
        }
        else if (std::strcmp(argv[i], "--check-vehicles") == 0)
        {
            checkVehicles = true;
        }
        else if (std::strcmp(argv[i], "--check-vehicle-input") == 0)
        {
            checkVehicleInput = true;
        }
        else if (std::strcmp(argv[i], "--check-audio") == 0)
        {
            checkAudio = true;
        }
        else if (std::strcmp(argv[i], "--check-objectives") == 0)
        {
            checkObjectives = true;
        }
        else if (std::strcmp(argv[i], "--check-campaign") == 0)
        {
            checkCampaign = true;
        }
        else if (std::strcmp(argv[i], "--check-deploy") == 0)
        {
            checkDeploy = true;
        }
        else if (std::strcmp(argv[i], "--check-campaign-keys") == 0)
        {
            checkCampaignKeys = true;
        }
        else if (std::strcmp(argv[i], "--check-save-load") == 0)
        {
            checkSaveLoad = true;
        }
        else if (std::strcmp(argv[i], "--check-save-hotkeys") == 0)
        {
            checkSaveHotkeys = true;
        }
        else if (std::strcmp(argv[i], "--check-newgame") == 0)
        {
            checkNewGame = true;
        }
        else if (std::strcmp(argv[i], "--new-game") == 0)
        {
            newGame = true;
        }
        else if (std::strcmp(argv[i], "--map") == 0)
        {
            showMap = true;
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
        else if (std::strcmp(argv[i], "--campaign") == 0 && i + 1 < argc)
        {
            cliCampaign = argv[++i];
        }
        else if (std::strcmp(argv[i], "--save") == 0 && i + 1 < argc)
        {
            cliSave = argv[++i];
        }
        else if (std::strcmp(argv[i], "--load") == 0 && i + 1 < argc)
        {
            cliLoad = argv[++i];
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
        // An explicit --config that names a missing file is a user error, not a
        // skip: fail fast rather than silently run with defaults (issue #259).
        // Auto-discovery below still treats an absent candidate as a skip.
        if (!found)
        {
            std::fprintf(stderr, "config error (%s): file not found\n", cliConfig.c_str());
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

    // An explicit `--campaign <path>` naming a missing file fails fast, matching
    // `--mission`/`--config` (issue #259): a typo must not silently hide the map
    // screen. An empty value keeps the default search, which skips absent
    // candidates.
    if (!cliCampaign.empty() && !fossynd::core::fileExists(cliCampaign))
    {
        std::fprintf(stderr, "campaign error (%s): file not found\n", cliCampaign.c_str());
        return 1;
    }

    // An explicit `--load <path>` naming a missing file fails fast, like
    // `--mission`/`--campaign`/`--config` (issue #259, #433): a typo must not
    // silently start a fresh campaign. A corrupt or mismatched save is rejected
    // after init(), where the base campaign exists and the parse error is known.
    if (!cliLoad.empty() && !fossynd::core::fileExists(cliLoad))
    {
        std::fprintf(stderr, "load error (%s): file not found\n", cliLoad.c_str());
        return 1;
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
    game.setCampaignPath(cliCampaign);
    game.setShowMap(showMap);
    game.setSavePath(cliSave);
    game.setLoadPath(cliLoad);

    // Show the authored title card for a real, interactive player. Automated
    // runs (headless, screenshots, attract mode) skip it and go straight to the
    // map. `--title` forces it (used to capture the card headlessly). Must be set
    // before init(), which latches the flag into the initial game state.
    const bool interactive = !headless && screenshot == nullptr && !demo;
    game.setShowTitleCard(title || interactive);
    // The new-game screen (issue #447): the player names the syndicate before the
    // campaign begins. `--new-game` forces it (scripted/demo); the interactive
    // start path opens it after the title card. A missing campaign leaves it
    // inert (it needs an identity to seed from), so it never blocks the game.
    game.setShowNewGameScreen(newGame || (interactive && !title && cliLoad.empty()));

    if (!game.init(width, height, headless, lowSpec))
    {
        std::fprintf(stderr, "Failed to start the game.\n");
        return 1;
    }
    game.setAutoPlay(demo);

    // Restore saved campaign progress on top of the base campaign (issue #433).
    // The two-step load: init() loaded the base campaign; this overlays the save.
    // A requested `--load` that cannot be applied is fatal (the #259 fail-fast
    // pattern) — a corrupt or mismatched save must not silently start fresh. Done
    // after init() so the resolved base campaign exists and the error names the
    // real reason (parse error, topology mismatch).
    if (!cliLoad.empty())
    {
        std::string loadError;
        if (!game.loadCampaignProgress(cliLoad, loadError))
        {
            std::fprintf(stderr, "load error (%s): %s\n", cliLoad.c_str(), loadError.c_str());
            return 1;
        }
    }

    // `--breach` opens every destructible wall up front so a headless screenshot
    // (or a manual run) shows the rubble art without waiting for combat (#218).
    if (breachWalls)
    {
        game.breachAllDestructibles();
    }

    // `--scan` fires a scanner pulse on the squad up front so a headless
    // screenshot shows the ping overlay without a hotkey (the gadget is not yet
    // bound to a key; see #323). Kept beside `--breach` as a demo hook.
    if (scanSquad)
    {
        game.activateScannerForSquad();
    }

    // `--shield` raises a shield on every living player agent up front so a
    // headless screenshot shows the shield dome without a hotkey (issue #375).
    // Kept beside `--scan` as a demo hook.
    if (shieldSquad)
    {
        game.raiseShieldForSquad();
    }

    // `--drugs` doses every living player agent up front so a headless screenshot
    // shows the drug aura without a hotkey (issue #387). Kept beside `--shield`.
    if (drugsSquad)
    {
        game.takeDrugsForSquad();
    }

    // `--vehicle` spawns a demo car and train so a headless screenshot shows the
    // vehicle art without a mission that ships one (#363). Kept beside `--breach`
    // and `--scan` as a demo hook.
    if (spawnVehicles)
    {
        game.addVehiclesForDemo();
    }

    // `--bomb` plants one time bomb on the squad up front so a headless
    // screenshot/guard exercises the real `Mission::orderTimeBomb` path without a
    // mission that ships one (issue #460). Kept beside `--scan` as a demo hook.
    if (plantBomb)
    {
        game.plantDemoTimeBomb();
    }

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

    if (checkGadgets)
    {
        // Headless guard for the tactical gadget hotkeys (issue #339): each
        // handler must spend a charge and apply its effect on the selected agent,
        // and a charge-less selection must be a safe no-op. Used by game_gadgets.
        const bool ok = game.gadgetSelfTest();
        std::printf(ok ? "gadgets: ok\n" : "gadgets: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkRubble)
    {
        // Headless guard for breached-tile rendering (issue #218): a destructible
        // wall must bake as its art, then as the rubble tile once breached, and
        // the breach must re-bake the static map. Used by the game_rubble ctest.
        const bool ok = game.rubbleSelfTest();
        std::printf(ok ? "rubble: ok\n" : "rubble: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkScanner)
    {
        // Headless guard for the scanner ping overlay (issue #323): no ping
        // before a scan, then a ring at the scanning agent and a mark per
        // revealed enemy, and no mark for a civilian scanner. Used by the
        // game_scanner ctest.
        const bool ok = game.scannerSelfTest();
        std::printf(ok ? "scanner: ok\n" : "scanner: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkShield)
    {
        // Headless guard for the personal shield overlay (issue #375): no dome
        // before a shield is raised, one dome on the shielded combatant, and the
        // dome stays while a re-raise is a no-op. Used by the game_shield ctest.
        const bool ok = game.shieldSelfTest();
        std::printf(ok ? "shield: ok\n" : "shield: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkDrugs)
    {
        // Headless guard for the performance-drug aura overlay (issue #387): no
        // aura before a dose, one aura on the dosed combatant, no aura for a
        // civilian dose, and the aura clearing once the buff expires. Used by the
        // game_drugs ctest.
        const bool ok = game.drugsSelfTest();
        std::printf(ok ? "drugs: ok\n" : "drugs: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkDrugControls)
    {
        // Headless guard for the drug-bar controls (issue #503, ADR-0043): `[`/`]`
        // cycle the active bar, `;`/`'` move it for the selection, it clamps at
        // both ends, an empty selection is a no-op, and the map screen swallows
        // the keys. Used by the game_drug_controls ctest.
        const bool ok = game.drugControlSelfTest();
        std::printf(ok ? "drug-controls: ok\n" : "drug-controls: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkDrugBars)
    {
        // Headless guard for the drug-bar HUD (issue #502, ADR-0043): three bars
        // per selected agent with the level and neutral point read from the live
        // mission, the bars tracking `Mission::setDrugLevel`, and an empty
        // selection drawing none. Drives the shared drugBars() decision the
        // render pass uses. Used by the game_drug_bars ctest.
        const bool ok = game.drugBarSelfTest();
        std::printf(ok ? "drug-bars: ok\n" : "drug-bars: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkBomb)
    {
        // Headless guard for the time-bomb demo hook (issue #460): no bomb before,
        // exactly one armed after planting through the real order path, a civilian
        // cannot plant one, a second bomb on the same agent is refused, and the
        // bomb clears once its fuse runs out. Used by the game_bomb ctest.
        const bool ok = game.bombSelfTest();
        std::printf(ok ? "bomb: ok\n" : "bomb: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkVehicles)
    {
        // Headless guard for vehicle rendering (issue #363): every vehicle must
        // yield a draw entry at its tile with the right kind, and a destroyed
        // vehicle must still draw as a wreck. Used by the game_vehicles ctest.
        const bool ok = game.vehicleSelfTest();
        std::printf(ok ? "vehicles: ok\n" : "vehicles: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkAudio)
    {
        // Headless guard for the audio subsystem (issue #423, ADR-0040): the cue
        // table must be complete, non-silent, and deterministic, mixing must sum
        // active cues, and a disabled Audio must be a safe no-op. Needs no
        // device, so it runs in CI. Used by the game_audio ctest.
        const bool ok = game.audioSelfTest();
        std::printf(ok ? "audio: ok\n" : "audio: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkVehicleInput)
    {
        // Headless guard for the vehicle input bindings (issue #373): E hijacks
        // and ejects, a move order from a driver steers the vehicle, and an
        // attack order from a driver aims the mounted weapon. Used by the
        // game_vehicle_input ctest. Needs the demo mission, so run from the repo
        // root (or with a `mission` in the config).
        const bool ok = game.vehicleInputSelfTest();
        std::printf(ok ? "vehicle-input: ok\n" : "vehicle-input: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkObjectives)
    {
        // Headless guard for the objective banner (issue #281): every objective
        // kind must render a non-empty, distinct, fully substituted localised
        // string. Used by the game_objectives ctest.
        const bool ok = game.objectiveSelfTest();
        std::printf(ok ? "objectives: ok\n" : "objectives: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkCampaign)
    {
        // Headless guard for the campaign management loop (Milestone B): the
        // shipped campaign must load and its tax -> research -> buy -> deploy ->
        // win loop must hold. Used by the game_campaign ctest.
        if (!game.campaignValid())
        {
            std::fprintf(stderr, "campaign: no campaign loaded (run from the repo root?)\n");
            return 1;
        }
        const bool ok = game.campaignSelfTest();
        std::printf(ok ? "campaign: ok\n" : "campaign: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkDeploy)
    {
        // Headless guard for deploying from the map onto the tactical board
        // (issue #312): the territory's mission must start with the chosen
        // loadout applied, and a result must feed back to the campaign and return
        // to the map. Used by the game_deploy ctest.
        if (!game.campaignValid())
        {
            std::fprintf(stderr, "deploy: no campaign loaded (run from the repo root?)\n");
            return 1;
        }
        const bool ok = game.deploySelfTest();
        std::printf(ok ? "deploy: ok\n" : "deploy: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkCampaignKeys)
    {
        // Headless guard for map-screen key isolation (issue #364): while the
        // management screen is up the tactical board is frozen, so a tactical
        // hotkey must not act. Runs from the repo root so the shipped campaign
        // loads. Used by the game_campaign_keys ctest.
        if (!game.campaignValid())
        {
            std::fprintf(stderr, "campaign-keys: no campaign loaded (run from the repo root?)\n");
            return 1;
        }
        const bool ok = game.campaignKeySelfTest();
        std::printf(ok ? "campaign-keys: ok\n" : "campaign-keys: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkSaveLoad)
    {
        // Headless guard for the campaign save/load round-trip (issue #433): a
        // progressed campaign must save, reload into a fresh base campaign with
        // every captured field restored, and a corrupt/missing file must be
        // rejected. Runs from the repo root so the shipped campaign loads. Used by
        // the game_save_load ctest.
        if (!game.campaignValid())
        {
            std::fprintf(stderr, "save-load: no campaign loaded (run from the repo root?)\n");
            return 1;
        }
        const bool ok = game.saveLoadSelfTest();
        std::printf(ok ? "save-load: ok\n" : "save-load: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkSaveHotkeys)
    {
        // Headless guard for the in-game save/load keys (issue #527): drives the
        // real `handleEvents` dispatch with pushed F5/F9 events and asserts the
        // campaign round-trips, a bad path fails soft, and the keys are disabled
        // on the tactical board. Run from the repo root so the shipped campaign
        // loads. Used by the game_save_hotkeys ctest.
        if (!game.campaignValid())
        {
            std::fprintf(stderr, "save-hotkeys: no campaign loaded (run from the repo root?)\n");
            return 1;
        }
        const bool ok = game.saveHotkeySelfTest();
        std::printf(ok ? "save-hotkeys: ok\n" : "save-hotkeys: FAILED\n");
        return ok ? 0 : 1;
    }

    if (checkNewGame)
    {
        // Headless guard for the new-game name/logo decision (issue #447): drives
        // the real NewGameScreen decisions and the real apply-to-campaign path.
        // Runs from the repo root so the shipped campaign loads. Used by the
        // game_newgame ctest.
        if (!game.campaignValid())
        {
            std::fprintf(stderr, "new-game: no campaign loaded (run from the repo root?)\n");
            return 1;
        }
        const bool ok = game.newGameSelfTest();
        std::printf(ok ? "new-game: ok\n" : "new-game: FAILED\n");
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
