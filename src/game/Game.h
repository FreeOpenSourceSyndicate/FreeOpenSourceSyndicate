/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SDL.h"

#include "core/i18n/StringTable.h"
#include "core/math/Timestep.h"
#include "core/math/Vec2.h"
#include "core/sim/Mission.h"
#include "core/world/TileGrid.h"
#include "game/CampaignScreen.h"
#include "game/Font.h"
#include "game/GroundTiles.h"
#include "game/Minimap.h"
#include "game/Sprite.h"

namespace fossynd::game
{

// The SDL2 application: window, main loop, input, and rendering.
//
// All game rules live in fossynd::core; this class only translates input into core
// commands and draws core state. It must stay lightweight enough for old,
// low-end hardware (see AGENTS.md 1b): the static map is rendered once to a
// texture and blitted each frame, the font is a single atlas, and the renderer
// is software-capable.
class Game
{
public:
    Game() = default;
    ~Game();

    // Creates the window and renderer and builds the mission. When `headless`
    // is true the dummy video driver is used so the game can run without a
    // display (used for smoke tests). When `lowSpec` is true the software
    // renderer is forced (old-hardware profile, ADR-0008). Returns false on
    // failure.
    bool init(int width, int height, bool headless, bool lowSpec = false);

    // Runs the main loop until the player quits. `maxFrames` > 0 runs that many
    // frames then returns (used for smoke tests).
    int run(int maxFrames = 0);

    // When enabled, the squad automatically advances on the nearest enemy.
    // Used for headless verification and as an attract mode for demos.
    void setAutoPlay(bool enabled) { autoPlay_ = enabled; }

    // Sets the mission file to load (already resolved by main from the config
    // file, a CLI flag, or the fallback search). An empty path means "use the
    // copy embedded in the binary". Call before init().
    void setMissionPath(const std::string& path) { missionPath_ = path; }

    // Sets the campaign file to load for the management screen (Milestone B). An
    // empty path means "search the default locations". Call before init().
    void setCampaignPath(const std::string& path) { campaignPath_ = path; }

    // Starts with the campaign management screen (the world map) up instead of
    // the tactical board. Set by `--map`; `M` toggles it at runtime. Call before
    // init().
    void setShowMap(bool enabled) { showMap_ = enabled; }

    // True when a campaign was loaded and the management screen is available.
    // Used by the `--check-campaign` guard.
    bool campaignValid() const { return campaign_.valid(); }

    // Headless guard for the campaign management loop (Milestone B): on a copy of
    // the loaded campaign, exercises tax -> income -> research -> buy -> deploy ->
    // win through the real core systems, and draws the R&D shop overlay so its
    // render path is exercised too. Returns false on a broken invariant.
    // Used by the `game_campaign` ctest.
    bool campaignSelfTest();

    // Headless guard for deploying from the map onto the tactical board (issue
    // #312): the deployed mission must start with the chosen loadout applied to
    // the player agents, and a mission result must feed back to the campaign and
    // return to the map. Drives the real CampaignScreen + Game path and restores
    // the deployment state before returning. Used by the `game_deploy` ctest.
    bool deploySelfTest();

    // Directory containing the executable, used to find an external language
    // file. May be empty (relative search paths are then tried). Call before
    // init().
    void setExeDir(const std::string& dir) { exeDir_ = dir; }

    // Selects the UI language (a locale code such as "de"). Empty or "en" means
    // English. An unknown locale is not fatal: init() warns and keeps English.
    // Call before init().
    void setLang(const std::string& locale) { langCode_ = locale; }

    // Shows the authored pixel-art title card before play. Off by default so
    // automated runs (smoke tests, screenshots, attract mode) go straight to the
    // map; the interactive player sees it and presses a key to start.
    void setShowTitleCard(bool enabled) { showTitleCard_ = enabled; }

    // Renders one frame and saves it to `path` as a BMP. Returns false on
    // failure. Used for headless verification.
    bool saveScreenshot(const std::string& path);

    // True once the authored sprite sheets have baked successfully. When false
    // the renderer falls back to flat markers. Used by the sprite ctest.
    bool spritesValid() const { return sprites_.valid(); }

    // True once the minimap panel has baked. Used by the minimap ctest.
    bool minimapValid() const { return minimap_.valid(); }

    // Headless guard for the minimap: bakes the blip list from the live mission
    // and checks a blip exists for every living agent inside the panel. Used by
    // the game_minimap ctest via --check-minimap.
    bool checkMinimap();

    // Headless self-check for the selection / control-group logic (issue #33).
    // Exercises the real selection helpers and returns false if any invariant
    // fails. Used by the --check-selection flag so the input path is covered
    // without a display.
    bool selectionSelfTest();

    // Headless self-check for the shift-click waypoint queue (issue #75):
    // exercises orderSelectedMove against the real Mission queue and returns
    // false if any invariant fails. Used by --check-waypoints.
    bool waypointSelfTest();

    // Headless self-check for box-select (issue #94): exercises the real
    // screen-space rectangle helper the drag path uses and confirms a box
    // selects exactly the enclosed living player agents, an empty box selects
    // none, and a shift-box adds to the existing selection. Used by
    // --check-box-select.
    bool boxSelectSelfTest();

    // Headless self-check for the queued-route overlay (issue #89): builds a
    // real queued route on a selected agent and confirms the screen-space route
    // the renderer would draw starts at the agent and visits every queued
    // waypoint in order. Used by --check-waypoint-path.
    bool waypointPathSelfTest();

    // Headless self-check for the UI string table (issue #53): the table must
    // have loaded and resolve the keys the HUD/title card use, and a missing
    // key must fall back to its literal. Used by --check-lang.
    bool languageSelfTest() const;

    // Headless self-check for integer zoom (issue #122): every level keeps the
    // screen<->tile round-trip exact, the level stays clamped, and the on-screen
    // distance between two tiles scales with the level. Used by --check-zoom.
    bool zoomSelfTest();

    // Current integer magnification (1 = authored scale). Exposed for the
    // self-check and the HUD.
    int zoomLevel() const { return zoom_; }

    // Headless guard for the fullscreen toggle (issue #194): exercises the
    // toggle state machine without a real display. Returns false on a broken
    // invariant. Used by the game_fullscreen ctest.
    bool fullscreenSelfTest();

    // Sets the initial integer zoom level (issue #122), clamped to
    // [kZoomMin, kZoomMax]. Used by `--zoom` for headless captures. Must be set
    // before init() so the initial camera clamp uses the right level.
    void setInitialZoom(int level);

    // Starts the game in fullscreen (issue #194). F11 toggles it at runtime.
    // The renderer keeps its logical size and the frame scales to the display
    // (SDL_WINDOW_FULLSCREEN_DESKTOP). A no-op on a headless run. Must be set
    // before init().
    void setFullscreen(bool enabled) { startFullscreen_ = enabled; }

    // True while the window is fullscreen. Exposed for the headless self-check.
    bool isFullscreen() const { return fullscreen_; }

    // Pause/resume (issue #210). `setPaused` is the pre-init request (used by
    // `--pause` for a frozen screenshot); `togglePause` is the Space hotkey.
    // While paused, update() advances no simulation steps. Resuming drops any
    // banked time so no catch-up burst fires on the first unpaused frame.
    void setPaused(bool paused)
    {
        if (paused_ && !paused)
        {
            clock_.reset();
        }
        paused_ = paused;
    }
    void togglePause() { setPaused(!paused_); }
    bool isPaused() const { return paused_; }

    // Headless guard for pause/resume (issue #210): a paused update() must
    // advance no simulation time while an unpaused one does, and the toggle must
    // round-trip. Returns false on a broken invariant. Used by game_pause.
    bool pauseSelfTest();

    // Headless guard for breached-tile rendering (issue #218): a destructible
    // wall must bake as the destructible art, then as the rubble tile once it is
    // breached — and the map must re-bake on the breach without a per-frame
    // rescan. Returns false on a broken invariant. Used by game_rubble.
    bool rubbleSelfTest();

    // Headless guard for the objective banner (issue #281): every objective kind
    // must render a non-empty, kind-specific, localised banner with no unsubstituted
    // `{placeholder}` left in it. Returns false on a broken invariant. Used by
    // game_objectives. Restores the mission's real objective before returning.
    bool objectiveSelfTest();

    // Breaches every destructible wall and re-bakes the map (issue #218). Used
    // by `--breach` so a headless screenshot can show the rubble art without
    // waiting for combat to break a wall.
    void breachAllDestructibles();

    // True when a non-English locale file was found and overlaid over the
    // English fallback (issue #123). Used by --check-lang to prove a requested
    // locale actually resolved, so a missing locale file fails the gate instead
    // of silently passing on the English fallback.
    bool langOverlayLoaded() const { return langOverlayLoaded_; }

    // True when the loaded table carries `key` with a non-empty value.
    bool langHasKey(const std::string& key) const
    {
        return lang_.has(key) && !lang_.get(key, "").empty();
    }

    // The current mission's display name, resolved through the active string
    // table (issue #142/#176). Falls back to the mission's authored name when the
    // table has no `mission.<id>.name` key, so it is never blank. The view reads
    // it to label the mission; it is populated by buildMission().
    const std::string& missionName() const { return missionName_; }

    // True when the mission name was actually overlaid from the active locale
    // (the table carries a non-empty `mission.<id>.name` different from the
    // authored text). Used by --check-lang to prove the mission text resolves
    // for the requested locale, not just that a locale file loaded.
    bool missionNameLocalized() const { return missionNameLocalized_; }

private:
    // Looks up a localised UI string, falling back to the literal when the table
    // has no such key (an empty table therefore renders today's text).
    std::string tr(const char* key, const char* fallback) const
    {
        return lang_.get(key, fallback);
    }

    void buildMission();
    void buildMapTexture();
    // The authored tile kind a cell draws, from its state (issue #218): wall or
    // destructible art when blocked, rubble once breached, else floor. Shared by
    // the map bake and rubbleSelfTest() so the test checks the real decision.
    core::TileKind tileKindAt(int x, int y) const;

    // The objective banner text for the current mission (issue #281). Shared by
    // renderHud() and objectiveSelfTest() so the guard checks the real string.
    // `urgent` reports the low-count/low-time emphasis the banner also uses.
    std::string objectiveBannerText(bool& urgent) const;
    // Breaches the first intact destructible wall, without re-baking. Used by
    // rubbleSelfTest() so it can prove update() performs the re-bake itself.
    void forceBreachForTest();
    void buildSprites();
    void buildTiles();
    void buildMinimap();
    void loadLanguage();
    // Reflects the localised mission name in the OS window title (issue #176).
    // No-op when there is no window (e.g. a headless check path).
    void updateWindowTitle();
    // Applies `fullscreen_` to the window (issue #194). No-op when there is no
    // window, so a headless run is unaffected.
    void applyFullscreen();
    // Toggles fullscreen/windowed (issue #194, F11).
    void toggleFullscreen();
    void restart();
    // Starts a mission deployed from the campaign map (issue #312): loads the
    // mission at `missionPath` (a resolved file, never the embedded fallback) and
    // applies `loadout` to its player agents via R1's `applyLoadout`, so the
    // bought squad reaches the board. Replaces the current mission; `restart()`
    // replays this same deployment, so a loss can be retried.
    void startDeployedMission(const std::string& missionPath, const core::PlayerLoadout& loadout);
    // Records the ended deployment's result on the campaign and shows the map
    // again (issue #312). No-op when no deployment is pending, so it is safe to
    // call every frame. Shared by update() and deploySelfTest() so the guard
    // exercises the real feedback path.
    void finishDeployment();
    void handleEvents(bool& running);
    void update(float dt);
    void updateAutoPlay(float dt);
    void followSquad(float dt);
    void updateAgentViews(float dt);
    void render();
    void renderAgents();
    void renderWaypointPath();
    void renderSelectionBox();
    void renderHud();
    void renderMinimap();
    void renderTitleCard();
    // Draws the campaign management screen (Milestone B) over everything. A no-op
    // when the screen is hidden or no campaign loaded.
    void renderCampaignScreen();
    void drawTitleDiamond(float cx, float cy, float radius, std::uint8_t r, std::uint8_t g,
                          std::uint8_t b);

    // Draws one sprite frame anchored on `screenAnchor`, scaled by kSpriteScale.
    // A non-zero `flash` alpha blends a brief white veil over the sprite.
    void drawSpriteFrame(const SpriteSheet& sheet, int frameIndex, int screenAnchorX,
                         int screenAnchorY, std::uint8_t flash = 0);

    // Selection helpers, all clamped to living player agents.
    int agentAtScreen(int x, int y) const;
    int enemyAtScreen(int x, int y) const;
    void selectOnly(std::size_t index);
    void toggleSelect(std::size_t index);
    void selectAllPlayers();
    // Selects the nth living player agent (0-based), or does nothing if there is
    // no such agent. Used by the `5`-`8` direct-selection keys.
    void selectPlayerByOrdinal(int ordinal);
    // Box-select (issue #94): replaces the selection with every living player
    // agent whose projected position lies inside the screen-space rectangle, or
    // adds them when `add` is set (shift-drag). The selection logic lives here
    // and is shared by the interactive drag path and the headless self-test.
    void selectInScreenRect(int x0, int y0, int x1, int y1, bool add);
    // Control groups: stores the current selection under `slot`, or replaces the
    // selection with that group. Slots are 0-based; dead members are dropped.
    void assignControlGroup(int slot);
    void recallControlGroup(int slot);
    void orderAttackOn(int targetIndex);
    // Orders the current selection to move to `goal`. A shift-click appends the
    // goal to each agent's waypoint queue instead of replacing its order, so a
    // squad can be walked along a path (issue #75).
    void orderSelectedMove(const core::TilePos& goal, bool append);
    // Each selected agent uses its ComplianceBeacon on the nearest civilian in range.
    void persuadeNearestCivilian();
    // Releases the followers of every selected agent.
    void freeSelectedFollowers();

    core::Vec2 tileToScreen(const core::Vec2& tile) const;
    core::TilePos screenToTile(int screenX, int screenY) const;
    void centerOn(const core::Vec2& tile);
    void clampCamera();

    // Zoom (issue #122). The camera and map offset live in unzoomed surface
    // pixels; `zoom_` is applied only at the screen boundary, so all the picking
    // and order code that goes through tileToScreen/screenToTile keeps working
    // unchanged. `tileToSurface`/`screenToSurface` are those boundaries;
    // `applyZoom`/`clampZoom` step and clamp the level.
    core::Vec2 tileToSurface(const core::Vec2& tile) const;
    core::Vec2 screenToSurface(int screenX, int screenY) const;
    void applyZoom(int delta);
    void clampZoom();

    // Screen-space route the queued-route overlay draws for `agentIndex`: the
    // agent's tile followed by each queued waypoint, in order. Empty when the
    // agent is not alive or has no queued moves. Exposed so the headless
    // self-test can verify the exact geometry the renderer uses.
    std::vector<core::Vec2> waypointRoute(std::size_t agentIndex) const;

    // Writes a filled isometric diamond directly into a surface (used once).
    void fillDiamond(SDL_Surface* surface, int centerX, int centerY, float halfW, float halfH,
                     std::uint32_t color);

    // World-space filled diamond for the renderer (agents, selection, health).
    void fillDiamondWorld(const core::Vec2& worldCenter, float radius, std::uint8_t r, std::uint8_t g,
                          std::uint8_t b);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* mapTexture_ = nullptr;

    int screenWidth_ = 0;
    int screenHeight_ = 0;

    // Tile size in pixels (a tile diamond is tileWidth_ x tileHeight_).
    static constexpr float kTileWidth = 64.0f;
    static constexpr float kTileHeight = 32.0f;

    // Integer magnification of the authored 14x18 sprite cells. Kept small so
    // agents read as people on a 64px tile and stay cheap to blit (AGENTS.md 1b).
    static constexpr int kSpriteScale = 3;

    // Integer zoom levels (issue #122): the whole world pass is magnified by
    // `zoom_`, nearest-neighbour so the pixel-art look holds (AGENTS.md §1).
    // 1 = authored scale; the level is clamped to this small set.
    static constexpr int kZoomMin = 1;
    static constexpr int kZoomMax = 3;

    // Roughly how fast the walk cycle alternates, in steps per second. A view
    // detail only: it never feeds back into the simulation.
    static constexpr float kWalkCadence = 2.4f;

    static constexpr int kMapWidth = 24;
    static constexpr int kMapHeight = 24;

    int mapSurfaceWidth_ = 0;
    int mapSurfaceHeight_ = 0;
    float mapOffsetX_ = 0.0f;
    float mapOffsetY_ = 0.0f;

    // Breach count the map texture was last baked at (issue #218). update()
    // re-bakes only when the world reports a new breach, so the static map is
    // not rebuilt every frame.
    int mapBreachEpoch_ = 0;

    // Camera position, in pixels: the map texture is drawn at (-cameraX_, -cameraY_).
    float cameraX_ = 0.0f;
    float cameraY_ = 0.0f;

    // Integer zoom (issue #122), applied at the screen boundary only. 1 is the
    // authored scale; stepping is clamped to [kZoomMin, kZoomMax].
    int zoom_ = 1;

    // Fullscreen state (issue #194). `startFullscreen_` is the requested initial
    // state (set before init); `fullscreen_` is the live state, toggled by F11.
    // Both are ignored on a headless run (no window).
    bool startFullscreen_ = false;
    bool fullscreen_ = false;

    // True on a headless run (dummy video driver). Fullscreen is a no-op then.
    bool headless_ = false;

    // Pause/resume (issue #210). While true, update() advances no simulation
    // steps, so the fixed-step clock banks nothing and the board is frozen.
    bool paused_ = false;

    core::TileGrid grid_{kMapWidth, kMapHeight};
    core::Mission mission_{grid_};

    // Fixed-step clock (AGENTS.md golden rule 4): the simulation advances in
    // whole steps of a fixed size and the renderer draws between steps using
    // `clock_.alpha()` with `Agent::interpolatedPosition`. This keeps gameplay
    // independent of frame rate while motion stays smooth. It runs no
    // simulation itself — `update()` drives `mission_.update(clock_.step())`.
    core::FixedTimestep clock_;

    // Resolved mission file, or empty to use the embedded copy (ADR-0003).
    std::string missionPath_;

    // The loadout of a mission deployed from the campaign map (issue #312). An
    // empty loadout means the mission uses its authored squad, so a normal
    // (non-campaign) start is unchanged. Applied in buildMission() through R1's
    // `applyLoadout`; kept so `restart()` replays the same deployment.
    core::PlayerLoadout activeLoadout_;

    // The campaign mission currently being played, or -1 when the tactical board
    // is not a deployment. Set when a deployment starts, consumed once when the
    // mission ends: the win/lose is recorded on the campaign and the map is shown
    // again, so the campaign loop closes.
    core::TerritoryId deployedTerritory_ = -1;

    // The campaign management screen (Milestone B): the world map, funds, tax,
    // research, and deployment, driven from its own translation unit. Invalid
    // (and therefore skipped) when no campaign data is found, so the tactical
    // game always runs (AGENTS.md §1b).
    CampaignScreen campaign_;

    // Resolved campaign file, or empty to search the default locations. Set by
    // `--campaign` before init().
    std::string campaignPath_;

    // Start on the management screen (`--map`). `M` toggles it at runtime.
    bool showMap_ = false;

    // Directory containing the executable, used to find an external language
    // file (see loadLanguage).
    std::string exeDir_;

    // Requested locale code (issue #123), e.g. "de". Empty or "en" means
    // English. Set before init(); loadLanguage() uses it to overlay a locale
    // file over the English fallback.
    std::string langCode_;

    // True once a non-English locale file has been overlaid (issue #123). Lets
    // --check-lang tell "locale resolved" from "silently fell back to English".
    bool langOverlayLoaded_ = false;

    // Localised UI strings (ADR-0016). Loaded once; empty means "no
    // localisation", in which case every lookup falls back to the literal.
    core::i18n::StringTable lang_;

    // The current mission's display name, resolved through `lang_` in
    // buildMission() (issue #142/#176). Never blank: falls back to the mission's
    // authored name.
    std::string missionName_;

    // True when `missionName_` came from the active locale rather than the
    // mission's authored text. See missionNameLocalized().
    bool missionNameLocalized_ = false;

    BitmapFont font_;

    // Authored pixel-art sprites (Phase 2). When `sprites_.valid()` is false the
    // renderer falls back to the original flat diamonds, so a bad sheet never
    // stops the game from running.
    AgentSprites sprites_;

    // Authored pixel-art ground tiles (Phase 2, issue #189). Baked into the
    // static map once; when invalid the map falls back to the original flat
    // diamond fills, so a bad sheet never stops the game from running.
    GroundTiles tiles_;

    // Whole-board overview panel (Phase 2). Display-only; skipped when invalid.
    Minimap minimap_;

    // Per-agent view-only animation state. Kept out of core: it is presentation,
    // so it must never influence the simulation (AGENTS.md golden rule 4).
    struct AgentView
    {
        bool initialized = false;
        core::Vec2 lastPosition{0.0f, 0.0f};
        float facingX = 0.0f;
        float facingY = 1.0f;
        float walkPhase = 0.0f;
        float walkTimer = 0.0f;
    };
    std::vector<AgentView> views_;

    // Indices into mission_.agents() selected for orders. For the demo the whole
    // squad moves together; selection is tracked for the HUD.
    std::vector<std::size_t> selected_;

    // Box-select drag (issue #94). A left-press starts a potential drag; the
    // press is treated as a click (select/order at the point) unless the pointer
    // moves past kDragThreshold before release, in which case the enclosed
    // player agents are selected. Purely view state: it never feeds the
    // simulation (golden rule 4).
    bool dragging_ = false;
    bool dragAdd_ = false;          // shift was held at press time
    bool dragFromAgent_ = false;    // press landed on a player agent
    int dragStartX_ = 0;
    int dragStartY_ = 0;
    int dragCurrentX_ = 0;
    int dragCurrentY_ = 0;
    static constexpr int kDragThreshold = 6; // pixels before a press becomes a drag

    // Control groups (view-only, like selection): Ctrl+1..4 stores the current
    // selection in a slot; 1..4 recalls it. Empty means "unassigned".
    static constexpr int kControlGroups = 4;
    std::vector<std::size_t> controlGroups_[kControlGroups];

    std::uint64_t seed_ = 0xC0FFEEu;

    bool autoPlay_ = false;
    float autoPlayTimer_ = 0.0f;

    // Authored title card (pixel art), shown before play.
    bool showTitleCard_ = false;
    bool titleCardActive_ = false;
    float titleCardTime_ = 0.0f;
    bool titleCardStartRequested_ = false;

    bool running_ = false;
};

} // namespace fossynd::game
