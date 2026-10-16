/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

#include "core/data/Catalogues.h"
#include "core/data/ContentPaths.h"
#include "core/data/Json.h"
#include "core/data/MissionData.h"
#include "core/i18n/MissionText.h"
#include "core/math/Isometric.h"
#include "game/EmbeddedLang.h"
#include "game/EmbeddedMission.h"
#include "game/EmbeddedSprites.h"
#include "game/EmbeddedTiles.h"
#include "game/HudStyle.h"

// The build sets this from the project() version (src/CMakeLists.txt); the
// fallback keeps a plain compile (no CMake definitions) working.
#ifndef FOSSYND_VERSION
#define FOSSYND_VERSION "0.0.0-dev"
#endif

namespace fossynd::game
{
namespace
{

constexpr std::uint32_t rgba(int r, int g, int b, int a = 255)
{
    return static_cast<std::uint32_t>((a << 24) | (b << 16) | (g << 8) | r);
}

// Flat palette (prototype art, no external assets).
constexpr std::uint32_t kTileLight = rgba(58, 66, 58);
constexpr std::uint32_t kTileDark = rgba(46, 53, 46);
constexpr std::uint32_t kTileBlocked = rgba(30, 34, 30);
constexpr std::uint32_t kBlockedEdge = rgba(90, 96, 90);
constexpr std::uint32_t kGridLine = rgba(24, 27, 24);
// Breached-tile rubble (no-art fallback only): a warm grey so a breach never
// reads as plain floor.
constexpr std::uint32_t kTileRubble = rgba(66, 58, 48);
constexpr std::uint32_t kBackground = rgba(12, 13, 12);

// Raised personal shield overlay (issue #375). The dome is a half-alpha fill at
// the agent's tile; the rim is a brighter outline so it reads as a field. The
// absorb burst holds for this long, so a shield that does its job leaves a
// visible cue instead of vanishing on the hit.
constexpr int kShieldFieldAlpha = 70;
constexpr int kShieldRimAlpha = 200;
constexpr float kShieldAbsorbFlashSeconds = 0.3f;

void setPixel(SDL_Surface* surface, int x, int y, std::uint32_t color)
{
    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    {
        return;
    }
    auto* pixels = static_cast<std::uint32_t*>(surface->pixels);
    pixels[y * (surface->pitch / 4) + x] = color;
}

// The global bindings that still act while the campaign management screen is up
// (issue #364). The screen's own keys (`B` shop, and shop navigation) are
// consumed earlier by `CampaignScreen::handleKey`; this is the small allow-list
// of *global* keys that make sense on the map. Every other tactical binding
// (restart, pause, gadgets, select, pan) must be swallowed, because `update()`
// freezes the tactical board while the map is visible — a key that acted would
// mutate a board the player cannot see.
bool mapScreenAllowsKey(int sym)
{
    switch (sym)
    {
    case SDLK_ESCAPE: // quit
    case SDLK_F11:    // fullscreen
    case SDLK_m:      // leave the map
    case SDLK_d:      // deploy the selected territory
    case SDLK_PLUS:
    case SDLK_EQUALS:
    case SDLK_KP_PLUS:
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
        return true;
    default:
        return false;
    }
}

// Queues a synthetic key-down so a headless self-test can drive the real
// `handleEvents` dispatch path (issue #364). Returns false if SDL rejects it.
bool pushKeyDown(int sym)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.type = SDL_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.keysym.sym = sym;
    event.key.keysym.mod = KMOD_NONE;
    return SDL_PushEvent(&event) == 1;
}

// Queues a synthetic mouse-wheel event (issue #364).
bool pushWheel(int y)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEWHEEL;
    event.wheel.type = SDL_MOUSEWHEEL;
    event.wheel.y = y;
    return SDL_PushEvent(&event) == 1;
}

} // namespace

Game::~Game()
{
    sprites_.destroy();
    if (mapTexture_ != nullptr)
    {
        SDL_DestroyTexture(mapTexture_);
    }
    if (renderer_ != nullptr)
    {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

bool Game::init(int width, int height, bool headless, bool lowSpec)
{
    headless_ = headless;
    if (headless)
    {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    screenWidth_ = width;
    screenHeight_ = height;

    window_ = SDL_CreateWindow(("FOSSynd v" FOSSYND_VERSION), SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    if (window_ == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // Apply the requested initial fullscreen state (issue #194). Done before the
    // renderer is created so the logical-size mapping is set up consistently.
    fullscreen_ = startFullscreen_ && !headless_;
    applyFullscreen();

    // Prefer a software renderer so the game runs on machines without a GPU. In
    // the low-spec profile (ADR-0008) it is required, not merely preferred.
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (renderer_ == nullptr && !lowSpec)
    {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    }
    if (renderer_ == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    // Fix the logical resolution (issue #194): the game draws in its authored
    // pixel space and SDL scales the frame to the window/display, so fullscreen
    // fills the screen without changing the drawing code or the pixel-art look
    // (nearest-neighbour, AGENTS.md 1). SDL also maps mouse events back to this
    // space, so selection stays correct when scaled.
    SDL_RenderSetLogicalSize(renderer_, screenWidth_, screenHeight_);

    // Smooth the health bars / diamonds a little. Harmless on old hardware.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    if (!font_.init(renderer_, 2))
    {
        std::fprintf(stderr, "font init failed: %s\n", SDL_GetError());
        return false;
    }

    loadLanguage();
    buildMission();
    updateWindowTitle();
    buildTiles();
    buildMapTexture();
    buildSprites();
    buildMinimap();

    // Load the campaign management screen (Milestone B). A missing campaign is
    // not fatal: the screen stays invalid and is simply skipped, so the tactical
    // game always starts (AGENTS.md §1b).
    campaign_.init(campaignPath_, exeDir_);
    campaign_.setVisible(showMap_ && campaign_.valid());

    // Center the view on the squad.
    const auto& agents = mission_.agents();
    if (!agents.empty())
    {
        centerOn(agents.front().position());
    }

    titleCardActive_ = showTitleCard_;
    titleCardTime_ = 0.0f;
    titleCardStartRequested_ = false;

    running_ = true;
    return true;
}

void Game::loadLanguage()
{
    // Load the English table first: it is both the UI language for "en" and the
    // fallback every other locale overlays onto (ADR-0016). Prefer an external
    // file (so translators can override it) and fall back to the copy embedded
    // at build time. A missing or unparsable file is not fatal: the table stays
    // empty and every lookup uses its literal fallback, so the UI renders
    // exactly today's text.
    core::i18n::StringTable english;
    std::string error;
    bool englishLoaded = false;
    for (const std::string& path : core::i18n::defaultLangPaths(exeDir_, "en"))
    {
        if (core::fileExists(path) && english.loadFromFile(path, error))
        {
            englishLoaded = true;
            break;
        }
    }
    if (!englishLoaded)
    {
        const core::Json root = core::Json::parse(kEnglishLangJson, error);
        if (english.loadFromJson(root, error))
        {
            englishLoaded = true;
        }
        else
        {
            std::fprintf(stderr, "language data error: %s\n", error.c_str());
        }
    }

    // English (or an empty request) is the fallback itself — nothing to overlay.
    if (langCode_.empty() || langCode_ == "en")
    {
        lang_ = english;
        return;
    }

    // Overlay the requested locale on the English fallback (issue #123). A
    // missing or malformed locale file is a warning, never fatal: the table is
    // still populated from English, so the game starts in English rather than
    // refusing to run.
    std::string localeError;
    if (lang_.loadForLocale(langCode_, exeDir_, english, localeError))
    {
        langOverlayLoaded_ = true;
    }
    else
    {
        std::fprintf(stderr, "language: %s\n", localeError.c_str());
    }
}

void Game::buildMission()
{
    // Content comes from data, not code (ADR-0003). `missionPath_` was already
    // resolved by main (config file, then CLI flag, then the fallback search);
    // an empty path means "use the copy embedded at build time", so the game
    // still runs as a single self-contained executable (AGENTS.md 1b).
    core::MissionData data;
    std::string error;
    bool loaded = false;
    if (!missionPath_.empty())
    {
        loaded = core::MissionData::loadFromFile(missionPath_, data, error);
        if (!loaded)
        {
            std::fprintf(stderr, "mission data error (%s): %s\n", missionPath_.c_str(),
                         error.c_str());
        }
    }
    if (!loaded)
    {
        core::Json root = core::Json::parse(kDemoMissionJson, error);
        if (!core::MissionData::fromJson(root, data, error))
        {
            std::fprintf(stderr, "embedded mission data error: %s\n", error.c_str());
        }
    }

    grid_ = core::buildGrid(data);
    mission_ = core::Mission(grid_, seed_);
    core::populateMission(data, mission_);

    // Apply a deployed squad's loadout (Milestone B, issue #312): the bought
    // weapons/gadgets replace the mission's authored defaults for the player
    // agents. An empty loadout leaves the authored squad untouched, so a normal
    // start is unchanged. The weapon ids resolve against the shared weapon
    // catalogue beside the mission file; a bad id is a content error, reported
    // rather than deploying a broken weapon.
    if (!activeLoadout_.slots.empty())
    {
        core::WeaponCatalogue loadoutWeapons;
        const std::string weaponsPath =
            core::cataloguePathForMission(missionPath_, core::kWeaponsFileName);
        if (!weaponsPath.empty())
        {
            bool found = false;
            std::string catalogueError;
            if (!loadoutWeapons.loadFromFile(weaponsPath, found, catalogueError))
            {
                std::fprintf(stderr, "weapon catalogue error (%s): %s\n", weaponsPath.c_str(),
                             catalogueError.c_str());
            }
        }
        std::string loadoutError;
        if (!core::applyLoadout(activeLoadout_, mission_, loadoutWeapons, loadoutError))
        {
            std::fprintf(stderr, "loadout error: %s\n", loadoutError.c_str());
        }
    }

    // Localise the mission's authored name through the active string table
    // (issue #142/#176): a mission gains a translation by adding
    // `mission.<id>.name` to its locale file, with no C++ or schema change. The
    // id comes from the content path, or "demo" for the embedded copy. When the
    // table has no key the authored name is kept, so the field is never blank.
    std::string missionId = core::i18n::missionIdFromPath(missionPath_);
    if (missionId.empty())
    {
        missionId = "demo"; // the embedded copy has no path of its own
    }
    const std::string localizedName =
        core::i18n::localizeMissionText(lang_, core::i18n::missionNameKey(missionId), data.name);
    missionName_ = localizedName;
    missionNameLocalized_ = !localizedName.empty() && localizedName != data.name;

    selected_.clear();
    for (std::size_t i = 0; i < mission_.agents().size(); ++i)
    {
        if (mission_.agents()[i].team() == core::Team::Player)
        {
            selected_.push_back(i);
        }
    }

    // Fresh view-only animation state per mission build.
    views_.assign(mission_.agents().size(), AgentView{});
}

void Game::updateWindowTitle()
{
    // Show the localised mission name in the OS window title (issue #176). The
    // window is absent on a headless run, so guard against a null handle.
    if (window_ == nullptr || missionName_.empty())
    {
        return;
    }
    const std::string title = "FOSSynd v" FOSSYND_VERSION " - " + missionName_;
    SDL_SetWindowTitle(window_, title.c_str());
}

void Game::applyFullscreen()
{
    // No window on a headless run (and the dummy driver would reject the call),
    // so fullscreen is a safe no-op there (issue #194).
    if (window_ == nullptr || headless_)
    {
        return;
    }
    const Uint32 flags = fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(window_, flags) != 0)
    {
        std::fprintf(stderr, "fullscreen: %s\n", SDL_GetError());
    }
}

void Game::toggleFullscreen()
{
    // Flip the logical state even headless so the toggle is unit-testable; the
    // SDL call inside applyFullscreen() is what is guarded.
    fullscreen_ = !fullscreen_;
    applyFullscreen();
}

void Game::buildSprites()
{
    // The authored sheet is baked into the binary at build time (AGENTS.md 1b),
    // so the game stays a single self-contained executable. It is data-driven
    // (assets/sprites/agents.txt, ADR-0003) but not yet hot-loadable.
    std::string error;
    if (!sprites_.build(renderer_, kAgentSpritesText, error))
    {
        // Not fatal: the renderer falls back to flat markers.
        std::fprintf(stderr, "sprite sheet error: %s\n", error.c_str());
    }
}

void Game::buildTiles()
{
    // The authored ground sheet is baked into the binary at build time (AGENTS.md
    // 1b), so the game stays a single self-contained executable. Baked at the
    // map cell size (kTileWidth x kTileHeight) so the map bake is one blit per
    // cell.
    std::string error;
    if (!tiles_.build(kGroundTilesText, static_cast<int>(kTileWidth),
                      static_cast<int>(kTileHeight), error))
    {
        // Not fatal: buildMapTexture falls back to flat diamond fills.
        std::fprintf(stderr, "tile sheet error: %s\n", error.c_str());
    }
}

void Game::buildMinimap()
{
    // One texel per tile, magnified to a readable panel. Smaller on the low-spec
    // window (320x200) so it does not eat the play area (ADR-0008, AGENTS.md 1b).
    const int scale = (screenWidth_ < 480) ? 2 : 3;
    if (!minimap_.init(renderer_, grid_, scale))
    {
        // Not fatal: the HUD simply omits the panel.
        std::fprintf(stderr, "minimap init failed: %s\n", SDL_GetError());
    }
}

void Game::startDeployedMission(const std::string& missionPath, const core::PlayerLoadout& loadout)
{
    // A deployment replaces the current mission with the campaign's target
    // (issue #312). The path is a resolved file, never the embedded fallback, so
    // the mission played is the one the territory named. The loadout is kept so
    // `restart()` replays the same squad after a loss.
    missionPath_ = missionPath;
    activeLoadout_ = loadout;
    deployedTerritory_ = campaign_.selectedTerritory();
    restart();
    campaign_.setVisible(false);
}

void Game::finishDeployment()
{
    if (deployedTerritory_ < 0)
    {
        return;
    }
    const bool won = mission_.state() == core::MissionState::Won;
    campaign_.recordDeployResult(won);
    deployedTerritory_ = -1;
    campaign_.setVisible(campaign_.valid());
}

void Game::restart()
{
    seed_ = seed_ * 6364136223846793005ull + 1442695040888963407ull;
    buildMission();
    updateWindowTitle();
    // A fresh mission runs: clear a stale pause so `R` always resumes play.
    paused_ = false;
    // Drop any banked time so the new mission starts on a clean step boundary.
    clock_.reset();
    centerOn(mission_.agents().front().position());
}

void Game::buildMapTexture()
{
    // Precompute the static map into a surface once, then upload a single
    // texture. Rendering then costs one blit per frame.
    const float halfW = kTileWidth * 0.5f;
    const float halfH = kTileHeight * 0.5f;

    mapOffsetX_ = static_cast<float>(grid_.height()) * halfW + kTileWidth;
    mapOffsetY_ = kTileHeight;

    mapSurfaceWidth_ = static_cast<int>((grid_.width() + grid_.height()) * halfW + kTileWidth);
    mapSurfaceHeight_ =
        static_cast<int>((grid_.width() + grid_.height()) * halfH + kTileHeight * 2.0f);

    if (mapTexture_ != nullptr)
    {
        SDL_DestroyTexture(mapTexture_);
    }
    mapTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STATIC, mapSurfaceWidth_, mapSurfaceHeight_);
    if (mapTexture_ == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return;
    }
    SDL_SetTextureBlendMode(mapTexture_, SDL_BLENDMODE_BLEND);

    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, mapSurfaceWidth_, mapSurfaceHeight_, 32,
                                       SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        return;
    }
    SDL_FillRect(surface, nullptr, kBackground);

    for (int y = 0; y < grid_.height(); ++y)
    {
        for (int x = 0; x < grid_.width(); ++x)
        {
            const core::Vec2 center = core::tileToWorld(static_cast<float>(x), static_cast<float>(y),
                                                        kTileWidth, kTileHeight);
            const int cx = static_cast<int>(center.x + mapOffsetX_);
            const int cy = static_cast<int>(center.y + mapOffsetY_);

            const bool walkable = grid_.isWalkable(x, y);
            const core::TileKind kind = tileKindAt(x, y);
            if (tiles_.valid())
            {
                // Authored pixel art: one blit per cell, chosen by tile state.
                tiles_.blit(surface, kind, cx, cy);
                continue;
            }

            // Fallback: the original flat diamond fills (used only when the tile
            // sheet failed to bake, so a bad sheet never stops the game).
            const bool rubble = kind == core::TileKind::Rubble;
            const std::uint32_t color = !walkable ? kTileBlocked
                                        : rubble  ? kTileRubble
                                        : (((x + y) % 2 == 0) ? kTileLight : kTileDark);
            fillDiamond(surface, cx, cy, halfW, halfH, color);

            if (!walkable)
            {
                fillDiamond(surface, cx, cy, halfW * 0.85f, halfH * 0.85f, kBlockedEdge);
                fillDiamond(surface, cx, cy, halfW * 0.7f, halfH * 0.7f, kTileBlocked);
            }
            else if (rubble)
            {
                // Scatter a few dark chunks so a breach reads even without art.
                fillDiamond(surface, cx, cy, halfW * 0.9f, halfH * 0.9f, color);
                fillDiamond(surface, cx - halfW * 0.3f, cy, halfW * 0.16f, halfH * 0.28f,
                            kTileBlocked);
                fillDiamond(surface, cx + halfW * 0.28f, cy + halfH * 0.18f, halfW * 0.13f,
                            halfH * 0.22f, kTileBlocked);
            }
            else
            {
                fillDiamond(surface, cx, cy, halfW * 0.94f, halfH * 0.94f, kGridLine);
                fillDiamond(surface, cx, cy, halfW * 0.9f, halfH * 0.9f, color);
            }
        }
    }

    SDL_UpdateTexture(mapTexture_, nullptr, surface->pixels, surface->pitch);
    SDL_FreeSurface(surface);

    // Remember the breach count this texture reflects, so update() re-bakes only
    // when the world reports a new one (issue #218).
    mapBreachEpoch_ = grid_.breachEpoch();
}

core::TileKind Game::tileKindAt(int x, int y) const
{
    if (!grid_.isWalkable(x, y))
    {
        return grid_.isDestructible(x, y) ? core::TileKind::DestructibleWall
                                          : core::TileKind::Wall;
    }
    if (grid_.isRubble(x, y))
    {
        // A breached wall: draw the rubble it left behind, not floor
        // (issue #218, core state from #217).
        return core::TileKind::Rubble;
    }
    return ((x + y) % 2 == 0) ? core::TileKind::FloorLight : core::TileKind::FloorDark;
}

core::Vec2 Game::tileToSurface(const core::Vec2& tile) const
{
    const core::Vec2 world = core::tileToWorld(tile.x, tile.y, kTileWidth, kTileHeight);
    return core::Vec2{world.x + mapOffsetX_ - cameraX_, world.y + mapOffsetY_ - cameraY_};
}

core::Vec2 Game::screenToSurface(int screenX, int screenY) const
{
    return core::Vec2{static_cast<float>(screenX) / static_cast<float>(zoom_),
                      static_cast<float>(screenY) / static_cast<float>(zoom_)};
}

core::Vec2 Game::tileToScreen(const core::Vec2& tile) const
{
    const core::Vec2 surface = tileToSurface(tile);
    return core::Vec2{surface.x * static_cast<float>(zoom_),
                      surface.y * static_cast<float>(zoom_)};
}

core::TilePos Game::screenToTile(int screenX, int screenY) const
{
    const core::Vec2 surface = screenToSurface(screenX, screenY);
    const float worldX = surface.x + cameraX_ - mapOffsetX_;
    const float worldY = surface.y + cameraY_ - mapOffsetY_;
    const core::Vec2 tile = core::worldToTile(worldX, worldY, kTileWidth, kTileHeight);
    return core::TilePos{static_cast<int>(std::floor(tile.x + 0.5f)),
                         static_cast<int>(std::floor(tile.y + 0.5f))};
}

void Game::clampZoom()
{
    zoom_ = std::max(kZoomMin, std::min(zoom_, kZoomMax));
}

void Game::setInitialZoom(int level)
{
    zoom_ = level;
    clampZoom();
}

void Game::applyZoom(int delta)
{
    const int previous = zoom_;
    zoom_ += delta;
    clampZoom();
    if (zoom_ == previous)
    {
        return;
    }
    // Keep the point at the centre of the view fixed across the change so zoom
    // feels anchored rather than jumping. The centre maps to the same surface
    // point, so the camera moves by the surface delta scaled by the new level.
    const float cx = static_cast<float>(screenWidth_) * 0.5f;
    const float cy = static_cast<float>(screenHeight_) * 0.5f;
    const core::Vec2 surface = screenToSurface(static_cast<int>(cx), static_cast<int>(cy));
    const float newSurfaceX = surface.x * static_cast<float>(previous) / static_cast<float>(zoom_);
    const float newSurfaceY = surface.y * static_cast<float>(previous) / static_cast<float>(zoom_);
    cameraX_ += surface.x - newSurfaceX;
    cameraY_ += surface.y - newSurfaceY;
    clampCamera();
}

void Game::centerOn(const core::Vec2& tile)
{
    const core::Vec2 screen = tileToScreen(tile);
    cameraX_ += (screen.x - static_cast<float>(screenWidth_) * 0.5f) / static_cast<float>(zoom_);
    cameraY_ += (screen.y - static_cast<float>(screenHeight_) * 0.5f) / static_cast<float>(zoom_);
    clampCamera();
}

void Game::clampCamera()
{
    // The visible surface region is the window divided by the zoom level, so a
    // higher zoom shows less of the surface and the camera can pan further.
    const float viewW = static_cast<float>(screenWidth_) / static_cast<float>(zoom_);
    const float viewH = static_cast<float>(screenHeight_) / static_cast<float>(zoom_);
    const float maxX = std::max(0.0f, static_cast<float>(mapSurfaceWidth_) - viewW);
    const float maxY = std::max(0.0f, static_cast<float>(mapSurfaceHeight_) - viewH);
    cameraX_ = std::max(0.0f, std::min(cameraX_, maxX));
    cameraY_ = std::max(0.0f, std::min(cameraY_, maxY));
}

int Game::agentAtScreen(int x, int y) const
{
    // Returns the index of the living player agent nearest the click, or -1.
    // Agents are ~12px wide, so accept clicks within a small radius.
    const float pickRadius = 16.0f;
    const auto& agents = mission_.agents();
    int best = -1;
    float bestDistance = pickRadius * pickRadius;
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents[i].isAlive() || agents[i].team() != core::Team::Player)
        {
            continue;
        }
        const core::Vec2 screen = tileToScreen(agents[i].position());
        const float dx = static_cast<float>(x) - screen.x;
        const float dy = static_cast<float>(y) - screen.y;
        const float distance = dx * dx + dy * dy;
        if (distance <= bestDistance)
        {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    return best;
}

void Game::selectOnly(std::size_t index)
{
    selected_.clear();
    selected_.push_back(index);
}

void Game::toggleSelect(std::size_t index)
{
    const auto it = std::find(selected_.begin(), selected_.end(), index);
    if (it != selected_.end())
    {
        selected_.erase(it);
    }
    else
    {
        selected_.push_back(index);
    }
    if (selected_.empty())
    {
        // Never leave the player with nothing selected; fall back to the agent.
        selected_.push_back(index);
    }
}

void Game::selectAllPlayers()
{
    selected_.clear();
    const auto& agents = mission_.agents();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents[i].isAlive() && agents[i].team() == core::Team::Player)
        {
            selected_.push_back(i);
        }
    }
}

void Game::selectPlayerByOrdinal(int ordinal)
{
    if (ordinal < 0)
    {
        return;
    }
    int seen = 0;
    const auto& agents = mission_.agents();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents[i].isAlive() || agents[i].team() != core::Team::Player)
        {
            continue;
        }
        if (seen == ordinal)
        {
            selectOnly(i);
            return;
        }
        ++seen;
    }
}

void Game::selectInScreenRect(int x0, int y0, int x1, int y1, bool add)
{
    // Box-select (issue #94): select every living player agent whose projected
    // screen position falls inside the axis-aligned rectangle (the two corners
    // are normalised, so any drag direction works). When `add` is set (shift-
    // drag) the enclosed agents are added to the current selection instead of
    // replacing it. An empty box therefore selects nothing.
    const int minX = std::min(x0, x1);
    const int maxX = std::max(x0, x1);
    const int minY = std::min(y0, y1);
    const int maxY = std::max(y0, y1);

    if (!add)
    {
        selected_.clear();
    }
    const auto& agents = mission_.agents();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents[i].isAlive() || agents[i].team() != core::Team::Player)
        {
            continue;
        }
        const core::Vec2 screen = tileToScreen(agents[i].position());
        const float sx = screen.x;
        const float sy = screen.y;
        if (sx < static_cast<float>(minX) || sx > static_cast<float>(maxX) ||
            sy < static_cast<float>(minY) || sy > static_cast<float>(maxY))
        {
            continue;
        }
        if (std::find(selected_.begin(), selected_.end(), i) == selected_.end())
        {
            selected_.push_back(i);
        }
    }
}

void Game::assignControlGroup(int slot)
{
    if (slot < 0 || slot >= kControlGroups)
    {
        return;
    }
    // Store a copy of the current selection. Dead members are dropped when the
    // group is recalled, so no cleanup is needed here.
    controlGroups_[slot] = selected_;
}

void Game::recallControlGroup(int slot)
{
    if (slot < 0 || slot >= kControlGroups)
    {
        return;
    }
    selected_.clear();
    const auto& agents = mission_.agents();
    for (const std::size_t index : controlGroups_[slot])
    {
        // Drop members that have died since the group was set.
        if (index < agents.size() && agents[index].isAlive() &&
            agents[index].team() == core::Team::Player)
        {
            selected_.push_back(index);
        }
    }
    if (selected_.empty())
    {
        // An empty group (or one whose members are all dead) is a no-op rather
        // than leaving the player with nothing selected.
        selectAllPlayers();
    }
}

bool Game::zoomSelfTest()
{
    // Every level must round-trip a tile through the screen boundary exactly, and
    // the on-screen spacing between two tiles must grow with the level. This is
    // the picking/order path, so a break here would misplace clicks at zoom.
    const core::Vec2 a{3.0f, 4.0f};
    const core::Vec2 b{7.0f, 9.0f};

    // Start from a known level and clamp low.
    zoom_ = kZoomMin;
    clampZoom();
    if (zoom_ != kZoomMin)
    {
        std::fprintf(stderr, "zoom self-test: low clamp failed\n");
        return false;
    }

    float previousSpan = -1.0f;
    for (int level = kZoomMin; level <= kZoomMax; ++level)
    {
        zoom_ = level;

        // Round-trip: a tile projected to the screen and mapped back must land on
        // the same tile at every zoom level.
        const core::Vec2 screenA = tileToScreen(a);
        const core::TilePos backA =
            screenToTile(static_cast<int>(std::lround(screenA.x)), static_cast<int>(std::lround(screenA.y)));
        if (backA.x != static_cast<int>(a.x) || backA.y != static_cast<int>(a.y))
        {
            std::fprintf(stderr, "zoom self-test: round-trip failed at level %d\n", level);
            return false;
        }

        // The screen distance between two fixed tiles scales with the level.
        const core::Vec2 screenB = tileToScreen(b);
        const float dx = screenB.x - screenA.x;
        const float dy = screenB.y - screenA.y;
        const float span = std::sqrt(dx * dx + dy * dy);
        if (!(span > previousSpan))
        {
            std::fprintf(stderr, "zoom self-test: spacing did not grow at level %d\n", level);
            return false;
        }
        previousSpan = span;
    }

    // The level must clamp high, not run away.
    zoom_ = kZoomMax;
    applyZoom(5);
    if (zoom_ != kZoomMax)
    {
        std::fprintf(stderr, "zoom self-test: high clamp failed\n");
        return false;
    }
    applyZoom(-100);
    if (zoom_ != kZoomMin)
    {
        std::fprintf(stderr, "zoom self-test: low clamp after over-zoom failed\n");
        return false;
    }

    zoom_ = kZoomMin;
    return true;
}

bool Game::fullscreenSelfTest()
{
    // The toggle must flip the live state and survive repeated toggles; on a
    // headless run the SDL call is a guarded no-op (issue #194). This is the
    // state machine the F11 handler drives, so a break here would leave the
    // player unable to leave fullscreen.
    fullscreen_ = false;
    toggleFullscreen();
    if (!fullscreen_)
    {
        std::fprintf(stderr, "fullscreen self-test: toggle on failed\n");
        return false;
    }
    toggleFullscreen();
    if (fullscreen_)
    {
        std::fprintf(stderr, "fullscreen self-test: toggle off failed\n");
        return false;
    }
    // An even number of toggles returns to the start.
    toggleFullscreen();
    toggleFullscreen();
    if (fullscreen_)
    {
        std::fprintf(stderr, "fullscreen self-test: round-trip failed\n");
        return false;
    }
    // Restore the requested initial state so a later render is unaffected.
    fullscreen_ = startFullscreen_ && !headless_;
    return true;
}

bool Game::pauseSelfTest()
{
    // A paused update() must advance no simulation time, and an unpaused one
    // must advance some — so a broken pause guard (sim keeps running, or never
    // runs) fails the gate (issue #210).
    if (mission_.state() != core::MissionState::Running)
    {
        std::fprintf(stderr, "pause self-test: mission is not running\n");
        return false;
    }

    paused_ = true;
    const float before = mission_.elapsedSeconds();
    update(0.5f);
    if (mission_.elapsedSeconds() != before)
    {
        std::fprintf(stderr, "pause self-test: sim advanced while paused\n");
        return false;
    }

    // Resuming must let the sim run again (and must not burst-catch-up).
    paused_ = false;
    clock_.reset();
    update(0.5f);
    if (mission_.elapsedSeconds() <= before)
    {
        std::fprintf(stderr, "pause self-test: sim did not advance after resume\n");
        return false;
    }

    // The toggle must round-trip.
    togglePause();
    if (!paused_)
    {
        std::fprintf(stderr, "pause self-test: toggle on failed\n");
        return false;
    }
    togglePause();
    if (paused_)
    {
        std::fprintf(stderr, "pause self-test: toggle off failed\n");
        return false;
    }
    return true;
}

bool Game::gadgetSelfTest()
{
    // Drives the real hotkey handlers (issue #339) against the real Mission, so
    // the gate proves the keys are wired to the core gadgets, not merely that the
    // core API works (which tests/test_mission.cpp already covers). The view must
    // pick the selected agent and the core must spend the charge and apply the
    // effect; a charge-less selection must be a safe no-op.
    std::vector<std::size_t> players;
    for (std::size_t i = 0; i < mission_.agents().size(); ++i)
    {
        if (mission_.agents()[i].isAlive() && mission_.agents()[i].team() == core::Team::Player)
        {
            players.push_back(i);
        }
    }
    if (players.size() < 2)
    {
        std::fprintf(stderr, "gadget self-test: needs at least two player agents\n");
        return false;
    }
    const std::size_t user = players[0];
    const std::size_t ally = players[1];

    // Medkit (H): a kit on the user heals the most-wounded ally in range and is
    // spent; a second press with no kit left must not heal.
    (void)mission_.agents()[ally].applyDamage(50.0f);
    const float wounded = mission_.agents()[ally].health();
    mission_.setMedikits(user, 1);
    selectOnly(user);
    useMedkitOnSelection();
    if (mission_.medikits(user) != 0)
    {
        std::fprintf(stderr, "gadget self-test: medkit charge not consumed\n");
        return false;
    }
    if (mission_.agents()[ally].health() <= wounded)
    {
        std::fprintf(stderr, "gadget self-test: medkit did not heal the ally\n");
        return false;
    }
    const float healed = mission_.agents()[ally].health();
    useMedkitOnSelection();
    if (mission_.agents()[ally].health() != healed)
    {
        std::fprintf(stderr, "gadget self-test: medkit healed with no charge left\n");
        return false;
    }

    // Shield (S): raises on the selected agent and spends a charge; an uncharged
    // agent cannot raise one (and the other player's shield state is untouched).
    mission_.setShields(user, 1);
    selectOnly(user);
    activateShieldOnSelection();
    if (mission_.shields(user) != 0 || !mission_.shieldUp(user))
    {
        std::fprintf(stderr, "gadget self-test: shield not raised or not spent\n");
        return false;
    }
    if (mission_.shieldUp(ally))
    {
        std::fprintf(stderr, "gadget self-test: shield raised on an unselected agent\n");
        return false;
    }
    mission_.setShields(ally, 0);
    selectOnly(ally);
    activateShieldOnSelection();
    if (mission_.shieldUp(ally))
    {
        std::fprintf(stderr, "gadget self-test: shield raised with no charge\n");
        return false;
    }

    // Scanner (C): starts a timed pulse and spends a charge; an uncharged agent
    // starts no pulse.
    mission_.setScanners(user, 1);
    selectOnly(user);
    activateScannerOnSelection();
    if (mission_.scanners(user) != 0 || mission_.scannerPulse(user) <= 0.0f)
    {
        std::fprintf(stderr, "gadget self-test: scanner not started or not spent\n");
        return false;
    }
    mission_.setScanners(ally, 0);
    selectOnly(ally);
    activateScannerOnSelection();
    if (mission_.scannerPulse(ally) > 0.0f)
    {
        std::fprintf(stderr, "gadget self-test: scanner started with no charge\n");
        return false;
    }

    // Performance drugs (B): starts a timed buff and spends a dose; an uncharged
    // agent takes none.
    mission_.setDrugs(user, 1);
    selectOnly(user);
    useDrugsOnSelection();
    if (mission_.drugs(user) != 0 || mission_.drugsTimer(user) <= 0.0f)
    {
        std::fprintf(stderr, "gadget self-test: drugs not taken or not spent\n");
        return false;
    }
    mission_.setDrugs(ally, 0);
    selectOnly(ally);
    useDrugsOnSelection();
    if (mission_.drugsTimer(ally) > 0.0f)
    {
        std::fprintf(stderr, "gadget self-test: drugs taken with no dose\n");
        return false;
    }

    // Leave a clean selection behind, like the other self-tests.
    selectAllPlayers();
    return true;
}

void Game::breachAllDestructibles()
{
    for (int y = 0; y < grid_.height(); ++y)
    {
        for (int x = 0; x < grid_.width(); ++x)
        {
            if (grid_.isDestructible(x, y) && !grid_.isRubble(x, y))
            {
                grid_.damageTile(x, y, grid_.tileHitPoints(x, y));
            }
        }
    }
    // Re-bake so a subsequent render (e.g. a screenshot) shows the rubble.
    if (grid_.breachEpoch() != mapBreachEpoch_)
    {
        buildMapTexture();
    }
}

void Game::forceBreachForTest()
{
    for (int y = 0; y < grid_.height(); ++y)
    {
        for (int x = 0; x < grid_.width(); ++x)
        {
            if (grid_.isDestructible(x, y) && !grid_.isRubble(x, y))
            {
                // Break it in one hit, whatever its hit points. Deliberately does
                // not re-bake: rubbleSelfTest() must prove update() does that.
                grid_.damageTile(x, y, grid_.tileHitPoints(x, y));
                return;
            }
        }
    }
}

bool Game::rubbleSelfTest()
{
    // The map must bake as a destructible wall, then as rubble once breached,
    // and the breach must make update() re-bake the static map (issue #218). The
    // mission carries a destructible wall (buildMission), so this path is real.
    int wallX = -1;
    int wallY = -1;
    for (int y = 0; y < grid_.height() && wallX < 0; ++y)
    {
        for (int x = 0; x < grid_.width(); ++x)
        {
            if (grid_.isDestructible(x, y))
            {
                wallX = x;
                wallY = y;
                break;
            }
        }
    }
    if (wallX < 0)
    {
        std::fprintf(stderr, "rubble self-test: mission has no destructible wall\n");
        return false;
    }

    if (tileKindAt(wallX, wallY) != core::TileKind::DestructibleWall)
    {
        std::fprintf(stderr, "rubble self-test: intact wall did not bake as destructible\n");
        return false;
    }

    // Breach without re-baking, so the map is left stale. update() must notice
    // the new breach and re-bake through the real per-frame path.
    const int epochBefore = grid_.breachEpoch();
    forceBreachForTest();
    if (grid_.breachEpoch() <= epochBefore)
    {
        std::fprintf(stderr, "rubble self-test: breach did not bump the epoch\n");
        return false;
    }
    if (mapBreachEpoch_ == grid_.breachEpoch())
    {
        std::fprintf(stderr, "rubble self-test: map was not left stale by the breach\n");
        return false;
    }

    update(0.02f);
    if (mapBreachEpoch_ != grid_.breachEpoch())
    {
        std::fprintf(stderr, "rubble self-test: update() did not re-bake on the breach\n");
        return false;
    }
    if (tileKindAt(wallX, wallY) != core::TileKind::Rubble)
    {
        std::fprintf(stderr, "rubble self-test: breached wall did not bake as rubble\n");
        return false;
    }

    // A further frame with no new breach must leave the map in sync (a change
    // check, not an every-frame rebuild).
    update(0.02f);
    if (mapBreachEpoch_ != grid_.breachEpoch())
    {
        std::fprintf(stderr, "rubble self-test: map fell out of sync after a frame\n");
        return false;
    }
    return true;
}

void Game::activateScannerForSquad()
{
    for (std::size_t i = 0; i < mission_.agents().size(); ++i)
    {
        const core::Agent& agent = mission_.agents()[i];
        if (!agent.isAlive() || agent.team() != core::Team::Player)
        {
            continue;
        }
        if (mission_.scanners(i) <= 0)
        {
            mission_.setScanners(i, 1);
        }
        mission_.activateScanner(i);
    }
}

void Game::raiseShieldForSquad()
{
    for (std::size_t i = 0; i < mission_.agents().size(); ++i)
    {
        const core::Agent& agent = mission_.agents()[i];
        if (!agent.isAlive() || agent.team() != core::Team::Player)
        {
            continue;
        }
        if (mission_.shields(i) <= 0)
        {
            mission_.setShields(i, 1);
        }
        mission_.activateShield(i);
    }
}

bool Game::scannerSelfTest()
{
    // Headless guard for the scanner ping overlay (issue #323): before any scan
    // the overlay draws nothing; a live pulse yields one ring at the scanning
    // agent and one mark per enemy within range; and a civilian cannot scan (no
    // ring, no marks). The overlay decision (scannerMarks) is shared with the
    // renderer, so this guards the real draw path without a display.
    const auto& agents = mission_.agents();
    const std::vector<ScannerMark> before = scannerMarks();
    if (!before.empty())
    {
        std::fprintf(stderr, "scanner self-test: overlay drew a ping before any scan\n");
        return false;
    }

    std::size_t player = agents.size();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents[i].isAlive() && agents[i].team() == core::Team::Player)
        {
            player = i;
            break;
        }
    }
    if (player == agents.size())
    {
        std::fprintf(stderr, "scanner self-test: no living player agent\n");
        return false;
    }

    mission_.setScanners(player, 1);
    if (!mission_.activateScanner(player))
    {
        std::fprintf(stderr, "scanner self-test: could not start a scan\n");
        return false;
    }

    const std::vector<std::size_t> targets = mission_.scanTargets(player);
    const std::vector<ScannerMark> marks = scannerMarks();
    // One ring for the scanner, one mark per revealed enemy.
    if (marks.size() != targets.size() + 1)
    {
        std::fprintf(stderr, "scanner self-test: expected %zu marks, got %zu\n", targets.size() + 1,
                     marks.size());
        return false;
    }
    bool hasRing = false;
    for (const ScannerMark& mark : marks)
    {
        if (mark.ring)
        {
            hasRing = true;
            if (mark.index != player)
            {
                std::fprintf(stderr, "scanner self-test: ring is not on the scanning agent\n");
                return false;
            }
        }
    }
    if (!hasRing)
    {
        std::fprintf(stderr, "scanner self-test: no pulse ring for a live scan\n");
        return false;
    }

    // A civilian is never a scanner: activating is a no-op, so it adds no mark.
    const std::size_t civilianCount = marks.size();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents[i].isAlive() && agents[i].team() == core::Team::Civilian)
        {
            mission_.setScanners(i, 1);
            if (mission_.activateScanner(i))
            {
                std::fprintf(stderr, "scanner self-test: a civilian started a scan\n");
                return false;
            }
            if (scannerMarks().size() != civilianCount)
            {
                std::fprintf(stderr, "scanner self-test: a civilian changed the overlay\n");
                return false;
            }
            break;
        }
    }

    // Let the pulse expire through the real update path: the overlay clears.
    mission_.update(6.0f);
    if (mission_.scannerPulse(player) > 0.0f || !scannerMarks().empty())
    {
        std::fprintf(stderr, "scanner self-test: overlay survived the pulse expiry\n");
        return false;
    }
    return true;
}

bool Game::objectiveSelfTest()
{
    // Every objective kind must produce a non-empty, localised banner with no
    // unsubstituted `{placeholder}` left in it. The demo mission supplies the
    // agents the escort/persuade kinds refer to (issue #281).
    const std::size_t agentCount = mission_.agents().size();
    if (agentCount < 2)
    {
        std::fprintf(stderr, "objective self-test: mission has too few agents\n");
        return false;
    }

    const core::ObjectiveSpec saved = mission_.objective();
    const core::ObjectiveSpec::Kind kinds[] = {
        core::ObjectiveSpec::Kind::EliminateAll, core::ObjectiveSpec::Kind::ReachTile,
        core::ObjectiveSpec::Kind::Survive,      core::ObjectiveSpec::Kind::Escort,
        core::ObjectiveSpec::Kind::Destroy,      core::ObjectiveSpec::Kind::Persuade,
    };

    std::set<std::string> seen;
    bool ok = true;
    for (core::ObjectiveSpec::Kind kind : kinds)
    {
        core::ObjectiveSpec spec;
        spec.kind = kind;
        spec.tileX = 1;
        spec.tileY = 1;
        spec.seconds = 30.0f;
        spec.agentIndex = 0;
        spec.targetCount = 1;
        mission_.setObjective(spec);

        bool urgent = false;
        const std::string text = objectiveBannerText(urgent);
        if (text.empty())
        {
            std::fprintf(stderr, "objective self-test: kind '%s' rendered an empty banner\n",
                         core::objectiveKindName(kind));
            ok = false;
            continue;
        }
        if (text.find('{') != std::string::npos || text.find('}') != std::string::npos)
        {
            std::fprintf(stderr, "objective self-test: kind '%s' left a placeholder: %s\n",
                         core::objectiveKindName(kind), text.c_str());
            ok = false;
        }
        // Distinct kinds must not share a banner: a kind that fell through to the
        // default would collide with `eliminate`.
        if (!seen.insert(text).second)
        {
            std::fprintf(stderr, "objective self-test: duplicate banner for kind '%s': %s\n",
                         core::objectiveKindName(kind), text.c_str());
            ok = false;
        }
    }

    mission_.setObjective(saved);
    return ok;
}

bool Game::campaignSelfTest()
{
    if (!campaign_.selfTest())
    {
        return false;
    }
    // Also draw the R&D shop overlay headlessly so `renderShop` is exercised (the
    // logic self-test above only drives the shop keys, not the draw). Opened and
    // closed on the live screen, so the player's state is unchanged. `render` is a
    // no-op when the screen is invalid, so this is safe with a null renderer.
    const bool savedVisible = campaign_.visible();
    campaign_.setVisible(true);
    campaign_.toggleShop();
    campaign_.render(renderer_, font_, screenWidth_, screenHeight_);
    campaign_.toggleShop();
    campaign_.setVisible(savedVisible);
    return !campaign_.shopOpen();
}

bool Game::deploySelfTest()
{
    // Deploying from the map must (a) resolve the territory's mission to a file,
    // (b) start that mission on the tactical board with the chosen loadout applied
    // to the player agents, and (c) feed the mission result back to the campaign
    // and return to the map. Drives the real CampaignScreen + Game path; restores
    // the live state before returning (issue #312).
    if (!campaign_.valid())
    {
        std::fprintf(stderr, "deploy self-test: no campaign loaded\n");
        return false;
    }

    const std::string savedPath = missionPath_;
    const core::PlayerLoadout savedLoadout = activeLoadout_;
    const core::TerritoryId savedDeployed = deployedTerritory_;
    const bool savedVisible = campaign_.visible();

    bool ok = true;

    // (a) The screen resolves the selected territory's mission to a file and
    // records a pending deployment whose loadout has one entry per squad slot —
    // the exact pair the `D` key uses. (The bought-weapon mapping itself is
    // unit-tested in tests/test_campaign.cpp; this guards the engine wiring.)
    campaign_.setVisible(true);
    if (!campaign_.deploySelected())
    {
        std::fprintf(stderr, "deploy self-test: could not deploy the selected territory\n");
        return false;
    }
    const std::string missionFile = campaign_.deployedMissionPath();
    if (missionFile.empty())
    {
        std::fprintf(stderr, "deploy self-test: the deployed mission has no file\n");
        return false;
    }
    const core::TerritoryId territory = campaign_.selectedTerritory();
    if (campaign_.deployedLoadout().slots.size() !=
        static_cast<std::size_t>(core::Equipment::kSquadSlots))
    {
        std::fprintf(stderr, "deploy self-test: the pending loadout has the wrong slot count\n");
        return false;
    }

    // (b) Starting it must apply the loadout to the squad. A hand-built loadout
    // stands in for a bought one: the shared "minigun" is distinctive and the
    // demo squad does not carry it, so the weapon on the first player agent
    // proves applyLoadout ran.
    core::PlayerLoadout loadout;
    loadout.slots.push_back(core::LoadoutSlot{"minigun", "", ""});
    startDeployedMission(missionFile, loadout);
    if (campaign_.visible())
    {
        std::fprintf(stderr, "deploy self-test: the map was not hidden on deploy\n");
        ok = false;
    }
    if (deployedTerritory_ != territory)
    {
        std::fprintf(stderr, "deploy self-test: the deployment did not record the territory\n");
        ok = false;
    }
    bool sawBoughtWeapon = false;
    for (const core::Agent& agent : mission_.agents())
    {
        if (agent.team() == core::Team::Player)
        {
            sawBoughtWeapon = agent.weapon().name == "Minigun";
            break; // slot 0 is the first player agent
        }
    }
    if (!sawBoughtWeapon)
    {
        std::fprintf(stderr, "deploy self-test: the loadout was not applied to the squad\n");
        ok = false;
    }

    // (c) Ending the mission must record the result and show the map again. Kill
    // the squad and let update() observe the lost state.
    for (core::Agent& agent : mission_.agents())
    {
        if (agent.team() == core::Team::Player)
        {
            agent.applyDamage(100000.0f);
        }
    }
    update(0.5f);    // guarantees whole steps so the sim observes the dead squad
    update(0.016f);  // the next frame feeds the result back and shows the map
    if (deployedTerritory_ >= 0)
    {
        std::fprintf(stderr, "deploy self-test: the result was not fed back to the campaign\n");
        ok = false;
    }
    if (!campaign_.visible())
    {
        std::fprintf(stderr, "deploy self-test: the map did not reappear after the mission\n");
        ok = false;
    }

    // Restore the live state.
    missionPath_ = savedPath;
    activeLoadout_ = savedLoadout;
    deployedTerritory_ = savedDeployed;
    campaign_.setVisible(savedVisible);
    return ok;
}

bool Game::campaignKeySelfTest()
{
    // The management screen freezes the tactical board (update() ticks the
    // campaign instead), so a tactical hotkey must be swallowed while it is up
    // (issue #364). This drives the *real* `handleEvents` dispatch with pushed
    // SDL events, so it would catch a binding that bypasses the guard — not just
    // re-assert the guard. The observable state is the board the key would touch.
    if (!campaign_.valid())
    {
        std::fprintf(stderr, "campaign-keys self-test: no campaign loaded\n");
        return false;
    }

    const bool savedVisible = campaign_.visible();
    campaign_.setVisible(true);

    const std::vector<std::size_t> savedSelection = selected_;
    if (selected_.empty())
    {
        std::fprintf(stderr, "campaign-keys self-test: nothing is selected\n");
        campaign_.setVisible(savedVisible);
        return false;
    }
    // Narrow the selection to one agent so a leaked `A`/`1`-`8` changes its size
    // (the board starts with the whole squad selected, which would hide that).
    selectOnly(selected_.front());

    // Give a selected agent a charge of each gadget so a leaked H/S/C would spend
    // it: with no charge the core is a no-op and the leak would be invisible.
    const std::size_t user = selected_.front();
    mission_.setMedikits(user, 1);
    mission_.setShields(user, 1);
    mission_.setScanners(user, 1);
    mission_.setDrugs(user, 1);

    bool alive = true;

    // Drop any events SDL queued during init, so the first pushed key is the only
    // one dispatched and the fingerprint below reflects a quiescent board.
    {
        SDL_Event drain;
        while (SDL_PollEvent(&drain) != 0)
        {
        }
    }

    // A fingerprint of every piece of board/view state a tactical key touches:
    // the mission identity (R rebuilds it and re-seeds), pause, zoom, camera, the
    // whole selection (so a leaked select-all or ordinal is caught, not just a
    // size change), the selected agent's position, and the four gadget charges.
    auto fingerprint = [&]() {
        const core::Vec2 p = mission_.agents()[user].position();
        std::string s;
        char buf[192];
        std::snprintf(buf, sizeof(buf), "%llu|%d|%d|%.2f|%.2f|%.2f,%.2f|%d%d%d%d",
                      static_cast<unsigned long long>(seed_), paused_ ? 1 : 0, zoom_, cameraX_,
                      cameraY_, p.x, p.y, mission_.medikits(user), mission_.shields(user),
                      mission_.scanners(user), mission_.drugs(user));
        s = buf;
        for (std::size_t idx : selected_)
        {
            s += "," + std::to_string(idx);
        }
        return s;
    };

    const std::string before = fingerprint();

    // Every tactical binding that must not act behind the map. Pushed one event
    // at a time so a failure names the key, and the allow-list guard is the only
    // thing that can stop it.
    const int blockedKeys[] = {SDLK_r, SDLK_SPACE, SDLK_h, SDLK_s, SDLK_c, SDLK_a,
                               SDLK_p, SDLK_f, SDLK_1,   SDLK_5,  SDLK_LEFT, SDLK_RIGHT,
                               SDLK_UP, SDLK_DOWN};
    for (int sym : blockedKeys)
    {
        if (!pushKeyDown(sym))
        {
            std::fprintf(stderr, "campaign-keys self-test: could not push a key event\n");
            campaign_.setVisible(savedVisible);
            return false;
        }
        handleEvents(alive);
        if (!alive)
        {
            std::fprintf(stderr, "campaign-keys self-test: a tactical key quit the game\n");
            campaign_.setVisible(savedVisible);
            return false;
        }
        if (fingerprint() != before)
        {
            std::fprintf(stderr, "campaign-keys self-test: key sym %d leaked through the map\n",
                         sym);
            campaign_.setVisible(savedVisible);
            return false;
        }
    }

    // The mouse wheel zooms the tactical view; it must be inert on the map too.
    // Pushed and checked one notch at a time so a single leak cannot be cancelled
    // out by a second event.
    for (int notch : {1, -1})
    {
        if (!pushWheel(notch))
        {
            std::fprintf(stderr, "campaign-keys self-test: could not push a wheel event\n");
            campaign_.setVisible(savedVisible);
            return false;
        }
        handleEvents(alive);
        if (fingerprint() != before)
        {
            std::fprintf(stderr, "campaign-keys self-test: the wheel zoomed the frozen board\n");
            campaign_.setVisible(savedVisible);
            return false;
        }
    }

    // The allowed keys must still act: `M` leaves the map (the fix must not make
    // the screen unclosable). `D`/`+`/`-` are covered by the campaign/deploy
    // self-tests.
    if (!pushKeyDown(SDLK_m))
    {
        std::fprintf(stderr, "campaign-keys self-test: could not push the M key\n");
        campaign_.setVisible(savedVisible);
        return false;
    }
    handleEvents(alive);
    if (campaign_.visible())
    {
        std::fprintf(stderr, "campaign-keys self-test: M no longer leaves the map\n");
        campaign_.setVisible(savedVisible);
        return false;
    }
    campaign_.setVisible(true);

    // Leave the live state as it was found.
    mission_.setMedikits(user, 0);
    mission_.setShields(user, 0);
    mission_.setScanners(user, 0);
    mission_.setDrugs(user, 0);
    selected_ = savedSelection;
    campaign_.setVisible(savedVisible);
    return true;
}

bool Game::languageSelfTest() const
{
    // The table must have loaded (from the external file or the embedded copy).
    if (lang_.size() == 0)
    {
        std::fprintf(stderr, "language self-test: string table is empty\n");
        return false;
    }

    // Every key the HUD/title card reads must resolve to a non-empty string — a
    // missing or blank key would silently fall back to the literal and hide a
    // broken en.json.
    static const char* const kRequiredKeys[] = {
        "app.title",           "app.version",          "app.tagline",
        "title.prompt",        "title.controls.move",  "title.controls.attack",
        "title.controls.select", "title.controls.misc", "title.footer",
        "hud.objective.eliminate", "hud.objective.reach", "hud.objective.survive",
        "hud.objective.escort", "hud.objective.destroy", "hud.objective.persuade",
        "hud.controls",        "hud.followers",        "hud.collateral",
        "hud.squad",           "hud.agentRow",         "hud.winSub",
        "hud.loseSub",         "state.won",            "state.lost",
    };
    for (const char* key : kRequiredKeys)
    {
        if (!lang_.has(key) || lang_.get(key, "").empty())
        {
            std::fprintf(stderr, "language self-test: missing key '%s'\n", key);
            return false;
        }
    }

    // An unknown key must fall back to the literal, never blank.
    if (lang_.get("no.such.key", "FALLBACK") != "FALLBACK")
    {
        std::fprintf(stderr, "language self-test: fallback lookup mismatch\n");
        return false;
    }

    // A format token must be substituted away by the real render helper.
    const std::string row = hudstyle::substitute(lang_.get("hud.agentRow", "{marker}{name}"),
                                       {{"marker", "> "}, {"name", "Ada"}, {"weapon", "Gun"},
                                        {"hp", "100"}});
    if (row.find('{') != std::string::npos || row.find("Ada") == std::string::npos)
    {
        std::fprintf(stderr, "language self-test: substitution failed ('%s')\n", row.c_str());
        return false;
    }
    return true;
}

bool Game::selectionSelfTest()
{
    auto livingPlayers = [this]() {
        std::vector<std::size_t> alive;
        const auto& agents = mission_.agents();
        for (std::size_t i = 0; i < agents.size(); ++i)
        {
            if (agents[i].isAlive() && agents[i].team() == core::Team::Player)
            {
                alive.push_back(i);
            }
        }
        return alive;
    };
    auto sorted = [](std::vector<std::size_t> values) {
        std::sort(values.begin(), values.end());
        return values;
    };

    const std::vector<std::size_t> players = livingPlayers();
    if (players.size() < 2)
    {
        std::fprintf(stderr, "selection self-test: needs at least two player agents\n");
        return false;
    }
    const std::size_t first = players[0];
    const std::size_t second = players[1];

    // selectAllPlayers selects every living player agent.
    selectAllPlayers();
    if (sorted(selected_) != sorted(players))
    {
        std::fprintf(stderr, "selection self-test: selectAllPlayers mismatch\n");
        return false;
    }

    // A control group round-trips: assign the current selection, change the
    // selection, then recall the group and get the original back.
    selectOnly(first);
    assignControlGroup(0);
    selectAllPlayers();
    recallControlGroup(0);
    if (selected_.size() != 1 || selected_[0] != first)
    {
        std::fprintf(stderr, "selection self-test: group recall mismatch\n");
        return false;
    }

    // Recalling an unassigned group is a safe no-op that still leaves a valid
    // selection.
    recallControlGroup(kControlGroups - 1);
    if (selected_.empty())
    {
        std::fprintf(stderr, "selection self-test: empty group left nothing selected\n");
        return false;
    }

    // Direct selection by ordinal maps to the nth living player agent.
    selectPlayerByOrdinal(1);
    if (selected_.size() != 1 || selected_[0] != second)
    {
        std::fprintf(stderr, "selection self-test: ordinal selection mismatch\n");
        return false;
    }

    // A group member that dies is dropped on recall, never reselected.
    selectOnly(second);
    assignControlGroup(1);
    (void)mission_.agents()[second].applyDamage(mission_.agents()[second].maxHealth() + 1.0f);
    if (mission_.agents()[second].isAlive())
    {
        std::fprintf(stderr, "selection self-test: failed to kill test agent\n");
        return false;
    }
    recallControlGroup(1);
    if (std::find(selected_.begin(), selected_.end(), second) != selected_.end())
    {
        std::fprintf(stderr, "selection self-test: recalled a dead group member\n");
        return false;
    }

    // Leave a clean selection behind.
    selectAllPlayers();
    return true;
}

bool Game::boxSelectSelfTest()
{
    // Exercises the real screen-space rectangle helper the drag path uses
    // (selectInScreenRect), which is otherwise only reachable from the
    // interactive mouse handler.
    auto livingPlayers = [this]() {
        std::vector<std::size_t> alive;
        const auto& agents = mission_.agents();
        for (std::size_t i = 0; i < agents.size(); ++i)
        {
            if (agents[i].isAlive() && agents[i].team() == core::Team::Player)
            {
                alive.push_back(i);
            }
        }
        return alive;
    };
    auto sorted = [](std::vector<std::size_t> values) {
        std::sort(values.begin(), values.end());
        return values;
    };
    // The set the helper must produce: every living player whose projected
    // position lies inside the normalised rectangle.
    auto expectedInRect = [this, &livingPlayers](int x0, int y0, int x1, int y1) {
        const int minX = std::min(x0, x1);
        const int maxX = std::max(x0, x1);
        const int minY = std::min(y0, y1);
        const int maxY = std::max(y0, y1);
        std::vector<std::size_t> out;
        const auto& agents = mission_.agents();
        for (std::size_t i : livingPlayers())
        {
            const core::Vec2 screen = tileToScreen(agents[i].position());
            if (screen.x >= static_cast<float>(minX) && screen.x <= static_cast<float>(maxX) &&
                screen.y >= static_cast<float>(minY) && screen.y <= static_cast<float>(maxY))
            {
                out.push_back(i);
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    };

    const std::vector<std::size_t> players = livingPlayers();
    if (players.empty())
    {
        std::fprintf(stderr, "box-select self-test: no living player agents\n");
        return false;
    }
    const std::size_t first = players.front();

    // An empty box (a region with no agent) selects nothing.
    selectInScreenRect(-1000, -1000, -900, -900, false);
    if (!selected_.empty())
    {
        std::fprintf(stderr, "box-select self-test: empty box selected something\n");
        return false;
    }

    // A box enclosing every player selects exactly the players.
    int minX = 100000;
    int minY = 100000;
    int maxX = -100000;
    int maxY = -100000;
    for (std::size_t i : players)
    {
        const core::Vec2 screen = tileToScreen(mission_.agents()[i].position());
        minX = std::min(minX, static_cast<int>(screen.x));
        minY = std::min(minY, static_cast<int>(screen.y));
        maxX = std::max(maxX, static_cast<int>(screen.x));
        maxY = std::max(maxY, static_cast<int>(screen.y));
    }
    const int allX0 = minX - 4;
    const int allY0 = minY - 4;
    const int allX1 = maxX + 4;
    const int allY1 = maxY + 4;
    selectInScreenRect(allX0, allY0, allX1, allY1, false);
    if (sorted(selected_) != sorted(players))
    {
        std::fprintf(stderr, "box-select self-test: enclosing box missed players\n");
        return false;
    }

    // Dragging in any direction works: reversed corners select the same set.
    selectInScreenRect(allX1, allY1, allX0, allY0, false);
    if (sorted(selected_) != sorted(players))
    {
        std::fprintf(stderr, "box-select self-test: reversed corners differ\n");
        return false;
    }

    // A tight box around one player selects exactly the enclosed set (which
    // includes that player and is a subset of all players).
    const core::Vec2 firstScreen = tileToScreen(mission_.agents()[first].position());
    const int fx = static_cast<int>(firstScreen.x);
    const int fy = static_cast<int>(firstScreen.y);
    selectInScreenRect(fx - 1, fy - 1, fx + 1, fy + 1, false);
    const std::vector<std::size_t> tight = expectedInRect(fx - 1, fy - 1, fx + 1, fy + 1);
    if (sorted(selected_) != tight)
    {
        std::fprintf(stderr, "box-select self-test: tight box mismatch\n");
        return false;
    }
    if (std::find(selected_.begin(), selected_.end(), first) == selected_.end())
    {
        std::fprintf(stderr, "box-select self-test: tight box dropped its own agent\n");
        return false;
    }

    // Shift-drag adds: a prior single selection survives, unioned with the box.
    selectOnly(first);
    selectInScreenRect(allX0, allY0, allX1, allY1, true);
    if (sorted(selected_) != sorted(players))
    {
        std::fprintf(stderr, "box-select self-test: additive box did not union\n");
        return false;
    }

    // An empty additive box leaves the existing selection untouched.
    selectOnly(first);
    selectInScreenRect(-1000, -1000, -900, -900, true);
    if (selected_.size() != 1 || selected_[0] != first)
    {
        std::fprintf(stderr, "box-select self-test: empty additive box changed selection\n");
        return false;
    }

    // Leave a clean selection behind.
    selectAllPlayers();
    return true;
}

bool Game::waypointSelfTest()
{
    // Exercise the shift-click queue through the real order path
    // (orderSelectedMove -> Mission::queueMove), which is otherwise only
    // reachable from the interactive mouse handler.
    const auto& agents = mission_.agents();
    std::size_t player = agents.size();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents[i].isAlive() && agents[i].team() == core::Team::Player)
        {
            player = i;
            break;
        }
    }
    if (player == agents.size())
    {
        std::fprintf(stderr, "waypoint self-test: no living player agent\n");
        return false;
    }

    // Find two distinct walkable tiles to use as the first and second waypoint.
    const core::TilePos start = agents[player].tile();
    core::TilePos first{-1, -1};
    core::TilePos second{-1, -1};
    for (int y = 0; y < grid_.height() && first.x < 0; ++y)
    {
        for (int x = 0; x < grid_.width(); ++x)
        {
            if (!grid_.isWalkable(x, y) || (x == start.x && y == start.y))
            {
                continue;
            }
            if (first.x < 0)
            {
                first = core::TilePos{x, y};
            }
            else if (x != first.x || y != first.y)
            {
                second = core::TilePos{x, y};
                break;
            }
        }
    }
    if (second.x < 0)
    {
        std::fprintf(stderr, "waypoint self-test: need two distinct walkable tiles\n");
        return false;
    }

    selectOnly(player);

    // A plain move replaces the order and clears any queue.
    orderSelectedMove(first, false);
    if (mission_.moveQueueSize(player) != 0)
    {
        std::fprintf(stderr, "waypoint self-test: plain move left a non-empty queue\n");
        return false;
    }

    // A shift-click appends: the first append queues a waypoint behind the
    // standing move order, and the second append adds another.
    orderSelectedMove(second, true);
    if (mission_.moveQueueSize(player) != 1)
    {
        std::fprintf(stderr, "waypoint self-test: first append did not queue a waypoint\n");
        return false;
    }
    orderSelectedMove(first, true);
    if (mission_.moveQueueSize(player) != 2)
    {
        std::fprintf(stderr, "waypoint self-test: second append did not grow the queue\n");
        return false;
    }

    // A fresh plain move supersedes the whole queued route.
    orderSelectedMove(second, false);
    if (mission_.moveQueueSize(player) != 0)
    {
        std::fprintf(stderr, "waypoint self-test: plain move did not clear the queue\n");
        return false;
    }

    // An out-of-bounds or blocked goal is ignored, leaving no queue behind.
    const core::TilePos outside{-1, -1};
    orderSelectedMove(outside, true);
    if (mission_.moveQueueSize(player) != 0)
    {
        std::fprintf(stderr, "waypoint self-test: invalid goal queued a waypoint\n");
        return false;
    }

    // Leave a clean order behind so a later screenshot/run is unaffected.
    mission_.clearMoveQueue(player);
    selectAllPlayers();
    return true;
}

bool Game::waypointPathSelfTest()
{
    // Headless guard for the queued-route overlay (issue #89). Builds a real
    // queued route through the order path, then checks the exact screen-space
    // geometry the renderer would draw.
    const auto& agents = mission_.agents();
    std::size_t player = agents.size();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents[i].isAlive() && agents[i].team() == core::Team::Player)
        {
            player = i;
            break;
        }
    }
    if (player == agents.size())
    {
        std::fprintf(stderr, "waypoint-path self-test: no living player agent\n");
        return false;
    }

    selectOnly(player);

    // No queue yet: the overlay draws nothing.
    if (!waypointRoute(player).empty())
    {
        std::fprintf(stderr, "waypoint-path self-test: drew a route with an empty queue\n");
        return false;
    }

    // Find two distinct walkable tiles to queue.
    const core::TilePos start = agents[player].tile();
    core::TilePos first{-1, -1};
    core::TilePos second{-1, -1};
    for (int y = 0; y < grid_.height() && first.x < 0; ++y)
    {
        for (int x = 0; x < grid_.width(); ++x)
        {
            if (!grid_.isWalkable(x, y) || (x == start.x && y == start.y))
            {
                continue;
            }
            if (first.x < 0)
            {
                first = core::TilePos{x, y};
            }
            else if (x != first.x || y != first.y)
            {
                second = core::TilePos{x, y};
                break;
            }
        }
    }
    if (second.x < 0)
    {
        std::fprintf(stderr, "waypoint-path self-test: need two distinct walkable tiles\n");
        return false;
    }

    orderSelectedMove(first, false); // a standing move (no queue)
    orderSelectedMove(second, true); // shift-click: queue `second` behind it
    orderSelectedMove(first, true);  // shift-click: queue `first` after that

    const std::vector<core::Vec2> route = waypointRoute(player);
    // agent tile + two queued waypoints
    if (route.size() != 3)
    {
        std::fprintf(stderr, "waypoint-path self-test: expected 3 route points, got %zu\n",
                     route.size());
        return false;
    }

    const core::Vec2 wantStart =
        tileToScreen(core::Vec2{static_cast<float>(start.x), static_cast<float>(start.y)});
    const core::Vec2 wantFirst =
        tileToScreen(core::Vec2{static_cast<float>(second.x), static_cast<float>(second.y)});
    const core::Vec2 wantSecond =
        tileToScreen(core::Vec2{static_cast<float>(first.x), static_cast<float>(first.y)});
    if (route[0].x != wantStart.x || route[0].y != wantStart.y ||
        route[1].x != wantFirst.x || route[1].y != wantFirst.y ||
        route[2].x != wantSecond.x || route[2].y != wantSecond.y)
    {
        std::fprintf(stderr, "waypoint-path self-test: route does not match the queue order\n");
        return false;
    }

    // A non-selected agent's queue is not part of the overlay, but waypointRoute
    // is per-agent: an unqueued agent still reports nothing.
    std::size_t other = agents.size();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (i != player && agents[i].isAlive() && agents[i].team() == core::Team::Player)
        {
            other = i;
            break;
        }
    }
    if (other != agents.size() && !waypointRoute(other).empty())
    {
        std::fprintf(stderr, "waypoint-path self-test: unqueued agent reported a route\n");
        return false;
    }

    // A fresh plain move clears the queue, so the overlay disappears.
    orderSelectedMove(second, false);
    if (!waypointRoute(player).empty())
    {
        std::fprintf(stderr, "waypoint-path self-test: route survived a cleared queue\n");
        return false;
    }

    // Leave a clean order behind.
    mission_.clearMoveQueue(player);
    selectAllPlayers();
    return true;
}

int Game::enemyAtScreen(int x, int y) const
{
    // Nearest living enemy agent to the click, or -1 (for attack orders).
    const float pickRadius = 16.0f;
    const auto& agents = mission_.agents();
    int best = -1;
    float bestDistance = pickRadius * pickRadius;
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents[i].isAlive() || agents[i].team() != core::Team::Enemy)
        {
            continue;
        }
        const core::Vec2 screen = tileToScreen(agents[i].position());
        const float dx = static_cast<float>(x) - screen.x;
        const float dy = static_cast<float>(y) - screen.y;
        const float distance = dx * dx + dy * dy;
        if (distance <= bestDistance)
        {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    return best;
}

void Game::orderAttackOn(int targetIndex)
{
    if (targetIndex < 0)
    {
        return;
    }
    const std::size_t target = static_cast<std::size_t>(targetIndex);
    for (const std::size_t index : selected_)
    {
        mission_.orderAttack(index, target);
    }
}

void Game::orderSelectedMove(const core::TilePos& goal, bool append)
{
    if (!grid_.inBounds(goal.x, goal.y) || !grid_.isWalkable(goal.x, goal.y))
    {
        return;
    }
    // A lone agent goes exactly where you clicked; a group spreads over a small
    // 2-wide formation so agents do not stack on one another.
    for (std::size_t i = 0; i < selected_.size(); ++i)
    {
        core::TilePos target = goal;
        if (selected_.size() > 1)
        {
            const core::TilePos offset{static_cast<int>(i % 2) - 1, static_cast<int>(i / 2)};
            const core::TilePos candidate{goal.x + offset.x, goal.y + offset.y};
            if (grid_.inBounds(candidate.x, candidate.y) && grid_.isWalkable(candidate.x, candidate.y))
            {
                target = candidate;
            }
        }
        if (append)
        {
            mission_.queueMove(selected_[i], target);
        }
        else
        {
            mission_.orderMove(selected_[i], target);
        }
    }
}

void Game::persuadeNearestCivilian()
{
    for (const std::size_t index : selected_)
    {
        mission_.persuade(index);
    }
}

void Game::freeSelectedFollowers()
{
    for (const std::size_t index : selected_)
    {
        mission_.freeFollowersOf(index);
    }
}

void Game::useMedkitOnSelection()
{
    for (const std::size_t index : selected_)
    {
        mission_.useMedikit(index);
    }
}

void Game::activateShieldOnSelection()
{
    for (const std::size_t index : selected_)
    {
        mission_.activateShield(index);
    }
}

void Game::activateScannerOnSelection()
{
    for (const std::size_t index : selected_)
    {
        mission_.activateScanner(index);
    }
}

void Game::useDrugsOnSelection()
{
    for (const std::size_t index : selected_)
    {
        mission_.useDrugs(index);
    }
}

void Game::fillDiamond(SDL_Surface* surface, int centerX, int centerY, float halfW, float halfH,
                       std::uint32_t color)
{
    const int minY = static_cast<int>(centerY - halfH);
    const int maxY = static_cast<int>(centerY + halfH);

    for (int py = minY; py <= maxY; ++py)
    {
        const float dy = (static_cast<float>(py) - static_cast<float>(centerY)) / halfH;
        const float span = 1.0f - std::fabs(dy);
        if (span <= 0.0f)
        {
            continue;
        }
        const int x0 = static_cast<int>(static_cast<float>(centerX) - halfW * span);
        const int x1 = static_cast<int>(static_cast<float>(centerX) + halfW * span);
        for (int px = x0; px <= x1; ++px)
        {
            setPixel(surface, px, py, color);
        }
    }
}

void Game::fillDiamondWorld(const core::Vec2& worldCenter, float radius, std::uint8_t r,
                            std::uint8_t g, std::uint8_t b, std::uint8_t alpha)
{
    // A small isometric diamond footprint, drawn as a few horizontal spans.
    // `worldCenter` is already in world pixels (tileToWorld output), so apply the
    // zoom here directly rather than going back through tileToScreen.
    const float z = static_cast<float>(zoom_);
    const float halfW = radius * (kTileWidth * 0.5f) * z;
    const float halfH = radius * (kTileHeight * 0.5f) * z;
    const core::Vec2 screen = core::Vec2{(worldCenter.x + mapOffsetX_ - cameraX_) * z,
                                         (worldCenter.y + mapOffsetY_ - cameraY_) * z};
    SDL_SetRenderDrawColor(renderer_, r, g, b, alpha);
    const int minY = static_cast<int>(screen.y - halfH);
    const int maxY = static_cast<int>(screen.y + halfH);
    for (int py = minY; py <= maxY; ++py)
    {
        const float dy = (static_cast<float>(py) - screen.y) / halfH;
        const float span = 1.0f - std::fabs(dy);
        if (span <= 0.0f)
        {
            continue;
        }
        SDL_Rect line;
        line.x = static_cast<int>(screen.x - halfW * span);
        line.w = static_cast<int>(2.0f * halfW * span) + 1;
        line.y = py;
        line.h = 1;
        SDL_RenderFillRect(renderer_, &line);
    }
}

void Game::strokeDiamondWorld(const core::Vec2& worldCenter, float radius, std::uint8_t r,
                              std::uint8_t g, std::uint8_t b, std::uint8_t alpha)
{
    // A hollow isometric diamond outline (the scanner pulse ring). For each
    // screen row across the diamond, draw the two edge pixels instead of the
    // full span, so the ring reads as an outline and does not hide the board.
    const float z = static_cast<float>(zoom_);
    const float halfW = radius * (kTileWidth * 0.5f) * z;
    const float halfH = radius * (kTileHeight * 0.5f) * z;
    const core::Vec2 screen = core::Vec2{(worldCenter.x + mapOffsetX_ - cameraX_) * z,
                                         (worldCenter.y + mapOffsetY_ - cameraY_) * z};
    SDL_SetRenderDrawColor(renderer_, r, g, b, alpha);
    const int minY = static_cast<int>(screen.y - halfH);
    const int maxY = static_cast<int>(screen.y + halfH);
    for (int py = minY; py <= maxY; ++py)
    {
        const float dy = (static_cast<float>(py) - screen.y) / halfH;
        const float span = 1.0f - std::fabs(dy);
        if (span <= 0.0f)
        {
            continue;
        }
        const float half = halfW * span;
        SDL_Rect left{static_cast<int>(screen.x - half), py, 2, 1};
        SDL_Rect right{static_cast<int>(screen.x + half) - 1, py, 2, 1};
        SDL_RenderFillRect(renderer_, &left);
        SDL_RenderFillRect(renderer_, &right);
    }
}

void Game::handleEvents(bool& running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        // While the title card is up, the first key or click starts the mission
        // instead of issuing orders.
        if (titleCardActive_)
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running = false;
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11)
                {
                    // Fullscreen toggle works on the title card too (issue #194).
                    toggleFullscreen();
                }
                else
                {
                    titleCardStartRequested_ = true;
                }
            }
            continue;
        }

        switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_KEYDOWN:
            // The campaign management screen can own a key: `B` opens the R&D shop,
            // and while the shop is open it is modal (navigation + action). This
            // runs before the global bindings so `M`/`D`/`-`/`+` cannot fire behind
            // the shop.
            if (campaign_.visible() && campaign_.handleKey(event.key.keysym.sym))
            {
                break;
            }
            // While the map is up the tactical board is frozen (update() ticks the
            // campaign instead), so swallow any tactical binding that does not make
            // sense on the map (issue #364): a restart, pause, gadget use, selection
            // or pan would otherwise mutate or replace the frozen board behind the
            // player's back. `handleKey` above already took the screen's own keys.
            if (campaign_.visible() && !mapScreenAllowsKey(event.key.keysym.sym))
            {
                break;
            }
            switch (event.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                running = false;
                break;
            case SDLK_F11:
                // Toggle fullscreen/windowed at runtime (issue #194).
                toggleFullscreen();
                break;
            case SDLK_m:
                // Toggle the campaign management screen (Milestone B). Only when a
                // campaign actually loaded, so `M` is inert without campaign data.
                if (campaign_.valid())
                {
                    campaign_.toggle();
                }
                break;
            case SDLK_d:
                // Deploy the selected territory's mission from the map: start it
                // on the tactical board with the bought squad (issue #312). Ignore
                // the key while a deployment is already in progress, so peeking at
                // the map mid-mission cannot abandon the live mission.
                if (campaign_.visible() && deployedTerritory_ < 0 &&
                    campaign_.deploySelected() && campaign_.hasDeployed())
                {
                    startDeployedMission(campaign_.deployedMissionPath(),
                                         campaign_.deployedLoadout());
                }
                break;
            case SDLK_SPACE:
                // Pause/resume the simulation (issue #210).
                togglePause();
                break;
            case SDLK_r:
                restart();
                break;
            case SDLK_a:
                selectAllPlayers();
                break;
            case SDLK_p:
                persuadeNearestCivilian();
                break;
            case SDLK_f:
                freeSelectedFollowers();
                break;
            case SDLK_h:
                // Use a medikit on the selection (issue #339).
                useMedkitOnSelection();
                break;
            case SDLK_s:
                // Raise a shield on the selection (issue #339).
                activateShieldOnSelection();
                break;
            case SDLK_c:
                // Run a scanner pulse for the selection (issue #339).
                activateScannerOnSelection();
                break;
            case SDLK_b:
                // Take performance drugs on the selection (issue #339).
                useDrugsOnSelection();
                break;
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
            {
                // Ctrl+1..4 stores the current selection as a control group;
                // 1..4 recalls that group (issue #33).
                const int slot = event.key.keysym.sym - SDLK_1;
                if ((event.key.keysym.mod & KMOD_CTRL) != 0)
                {
                    assignControlGroup(slot);
                }
                else
                {
                    recallControlGroup(slot);
                }
                break;
            }
            case SDLK_5:
            case SDLK_6:
            case SDLK_7:
            case SDLK_8:
            {
                // 5..8 select the 1st..4th living player agent directly (the
                // pre-#33 meaning of 1..4, moved so it does not clash with the
                // control groups).
                selectPlayerByOrdinal(event.key.keysym.sym - SDLK_5);
                break;
            }
            case SDLK_LEFT:
                cameraX_ -= kTileWidth / static_cast<float>(zoom_);
                clampCamera();
                break;
            case SDLK_RIGHT:
                cameraX_ += kTileWidth / static_cast<float>(zoom_);
                clampCamera();
                break;
            case SDLK_UP:
                cameraY_ -= kTileHeight / static_cast<float>(zoom_);
                clampCamera();
                break;
            case SDLK_DOWN:
                cameraY_ += kTileHeight / static_cast<float>(zoom_);
                clampCamera();
                break;
            case SDLK_PLUS:
            case SDLK_EQUALS:
            case SDLK_KP_PLUS:
                // On the management screen `+` raises the selected territory's
                // tax; in the tactical view it zooms in (issue #122).
                if (campaign_.visible())
                {
                    campaign_.adjustTax(1);
                }
                else
                {
                    applyZoom(1);
                }
                break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
                // On the management screen `-` lowers the selected territory's
                // tax; in the tactical view it zooms out (issue #122).
                if (campaign_.visible())
                {
                    campaign_.adjustTax(-1);
                }
                else
                {
                    applyZoom(-1);
                }
                break;
            default:
                break;
            }
            break;
        case SDL_MOUSEWHEEL:
            // Wheel up zooms in, wheel down zooms out (issue #122). Zoom is a
            // tactical-view binding, so it must not act on the frozen board while
            // the management screen is up (issue #364).
            if (!campaign_.visible())
            {
                applyZoom(event.wheel.y > 0 ? 1 : (event.wheel.y < 0 ? -1 : 0));
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)
            {
                // On the management screen a left click selects a territory node;
                // the tactical order path below is skipped entirely.
                if (campaign_.visible())
                {
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        campaign_.handleClick(event.button.x, event.button.y, screenWidth_,
                                               screenHeight_);
                    }
                    break;
                }

                const bool right = event.button.button == SDL_BUTTON_RIGHT;
                const bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

                // Right-click, or clicking an enemy, is an attack order. These
                // resolve on press; they are never drags.
                const int enemy = enemyAtScreen(event.button.x, event.button.y);
                if (right || (enemy >= 0 && !shift))
                {
                    orderAttackOn(enemy);
                    break;
                }

                // A left press might be a click or the start of a box-select
                // drag (issue #94). Record it and defer the click action to
                // release, so a drag past the threshold selects a rectangle
                // instead of ordering a move at the press point.
                dragging_ = true;
                dragAdd_ = shift;
                dragFromAgent_ = agentAtScreen(event.button.x, event.button.y) >= 0;
                dragStartX_ = event.button.x;
                dragStartY_ = event.button.y;
                dragCurrentX_ = event.button.x;
                dragCurrentY_ = event.button.y;
            }
            break;
        case SDL_MOUSEMOTION:
            if (dragging_)
            {
                dragCurrentX_ = event.motion.x;
                dragCurrentY_ = event.motion.y;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (dragging_ && event.button.button == SDL_BUTTON_LEFT)
            {
                // A drag that spanned the `M` toggle must not resolve into a
                // tactical box-select or move order on the frozen board (#364).
                if (campaign_.visible())
                {
                    dragging_ = false;
                    break;
                }
                const int x0 = dragStartX_;
                const int y0 = dragStartY_;
                // The button-up event carries the release point (dragCurrentX_/Y_
                // tracks motion only so the marquee can be drawn mid-drag).
                const int x1 = event.button.x;
                const int y1 = event.button.y;
                const bool moved =
                    std::abs(x1 - x0) >= kDragThreshold || std::abs(y1 - y0) >= kDragThreshold;
                const bool add = dragAdd_;
                const bool fromAgent = dragFromAgent_;
                dragging_ = false;

                if (moved)
                {
                    // A real drag: box-select the enclosed player agents.
                    selectInScreenRect(x0, y0, x1, y1, add);
                }
                else if (fromAgent)
                {
                    // A click on an agent selects it (or shift-toggles it).
                    const int picked = agentAtScreen(x0, y0);
                    if (picked >= 0 && !add)
                    {
                        selectOnly(static_cast<std::size_t>(picked));
                    }
                    else if (picked >= 0)
                    {
                        toggleSelect(static_cast<std::size_t>(picked));
                    }
                }
                else
                {
                    // Clicking the ground orders the current selection there. A
                    // shift-click appends the waypoint to the queue instead of
                    // replacing the order, so a squad can be walked along a path
                    // (issue #75).
                    orderSelectedMove(screenToTile(x0, y0), add);
                }
            }
            break;
        default:
            break;
        }
    }
}

void Game::update(float frameSeconds)
{
    if (titleCardActive_)
    {
        titleCardTime_ += frameSeconds;
        // A click/keypress starts the mission, as does a few seconds of
        // inactivity, so an unattended run never stalls on the card.
        if (titleCardStartRequested_ || titleCardTime_ >= 6.0f)
        {
            titleCardActive_ = false;
        }
        return;
    }

    // Pause (issue #210): freeze the simulation without touching the clock, so
    // resuming drops the banked time (below) instead of firing a catch-up burst.
    // The view still runs, so a paused frame redraws (e.g. a selection box).
    if (paused_)
    {
        return;
    }

    // A deployed campaign mission that has ended: record the win/lose on the
    // campaign, then show the map again so the loop closes (issue #312). Done
    // once — `deployedTerritory_` is cleared — so `R`/restart or a later frame
    // does not record the same result twice. Auto-play (attract mode) never
    // deploys, so this cannot fire there.
    if (deployedTerritory_ >= 0 && mission_.state() != core::MissionState::Running)
    {
        finishDeployment();
        return;
    }

    // Campaign management screen (Milestone B): while it is up the tactical
    // simulation is frozen (the player is on the map, not the board) and the
    // campaign clock advances instead. The economy and research tick on real
    // elapsed time, not the tactical fixed step.
    if (campaign_.visible())
    {
        campaign_.update(frameSeconds);
        return;
    }

    // Gameplay ticks on a fixed step so outcomes never depend on frame rate
    // (AGENTS.md golden rule 4). The clock drops the remainder beyond its stall
    // cap, so a long frame cannot trigger an unbounded catch-up burst. Orders
    // and the simulation share the same fixed step; the renderer then draws
    // between steps using `clock_.alpha()` (see renderAgents()).
    const int steps = clock_.advance(frameSeconds);
    for (int i = 0; i < steps; ++i)
    {
        if (autoPlay_)
        {
            updateAutoPlay(clock_.step());
        }
        mission_.update(clock_.step());
    }

    // Camera smoothing and the walk cycle are view-only, so they follow real
    // elapsed time rather than the step count.
    if (autoPlay_)
    {
        followSquad(frameSeconds);
    }
    // A breach changes the static map, so re-bake it once the world reports one
    // (issue #218). Cheap per frame — a counter compare — and a full bake only
    // when a tile actually breaks.
    if (grid_.breachEpoch() != mapBreachEpoch_)
    {
        buildMapTexture();
    }
    updateAgentViews(frameSeconds);
    updateShieldSnapshot(frameSeconds);
}

void Game::updateAgentViews(float dt)
{
    // Presentation-only: derive a facing direction and a walk phase from how each
    // agent's position changed this frame. Recovers from a restart (the view list
    // is rebuilt, so `initialized` is false) without an explicit reset.
    const auto& agents = mission_.agents();
    if (views_.size() != agents.size())
    {
        views_.assign(agents.size(), AgentView{});
    }

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        const core::Agent& agent = agents[i];
        AgentView& view = views_[i];
        if (!view.initialized)
        {
            view.initialized = true;
            view.lastPosition = agent.position();
            continue;
        }

        const core::Vec2 delta = agent.position() - view.lastPosition;
        view.lastPosition = agent.position();
        const float moved = length(delta);

        if (moved > 1e-4f)
        {
            view.facingX = delta.x / moved;
            view.facingY = delta.y / moved;
            view.walkTimer += dt;
            const float phase = 1.0f / kWalkCadence;
            while (view.walkTimer >= phase)
            {
                view.walkTimer -= phase;
                view.walkPhase = view.walkPhase >= 0.5f ? 0.0f : 1.0f;
            }
        }
        else
        {
            view.walkTimer = 0.0f;
            view.walkPhase = 0.0f;
        }
    }
}

void Game::updateAutoPlay(float dt)
{
    autoPlayTimer_ -= dt;
    if (mission_.state() != core::MissionState::Running)
    {
        // Auto-restart after the mission ends, for a looping demo.
        autoPlayTimer_ -= dt;
        if (autoPlayTimer_ <= 0.0f)
        {
            restart();
            autoPlayTimer_ = 0.0f;
        }
        return;
    }
    if (autoPlayTimer_ > 0.0f)
    {
        return;
    }
    autoPlayTimer_ = 0.75f;

    // Every agent advances on the nearest enemy unless already engaged.
    const auto& agents = mission_.agents();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        const core::Agent& agent = agents[i];
        if (!agent.isAlive() || agent.team() != core::Team::Player)
        {
            continue;
        }
        int best = -1;
        float bestDistance = 0.0f;
        for (std::size_t j = 0; j < agents.size(); ++j)
        {
            if (!agents[j].isAlive() || agents[j].team() != core::Team::Enemy)
            {
                continue;
            }
            const float dx = agents[j].position().x - agent.position().x;
            const float dy = agents[j].position().y - agent.position().y;
            const float distance = dx * dx + dy * dy;
            if (best == -1 || distance < bestDistance)
            {
                best = static_cast<int>(j);
                bestDistance = distance;
            }
        }
        if (best < 0)
        {
            continue;
        }
        const core::Agent& target = agents[static_cast<std::size_t>(best)];
        const float range = agent.weapon().range;
        const float dx = target.position().x - agent.position().x;
        const float dy = target.position().y - agent.position().y;
        if (dx * dx + dy * dy > range * range * 0.64f)
        {
            mission_.orderMove(i, target.tile());
        }
        else
        {
            // In range: focus fire on the chosen enemy (also draws the reticle).
            mission_.orderAttack(i, static_cast<std::size_t>(best));
        }
    }
}

void Game::followSquad(float dt)
{
    // Smoothly keep the camera on the living members of the squad.
    const auto& agents = mission_.agents();
    core::Vec2 centroid{0.0f, 0.0f};
    int count = 0;
    for (const core::Agent& agent : agents)
    {
        if (agent.isAlive() && agent.team() == core::Team::Player)
        {
            centroid = centroid + agent.position();
            ++count;
        }
    }
    if (count == 0)
    {
        return;
    }
    centroid = centroid * (1.0f / static_cast<float>(count));

    const core::Vec2 wantScreen = tileToScreen(centroid);
    // Screen offsets are magnified by the zoom, so divide back out to get the
    // camera move in surface pixels.
    const float z = static_cast<float>(zoom_);
    const float wantX = cameraX_ + (wantScreen.x - static_cast<float>(screenWidth_) * 0.5f) / z;
    const float wantY = cameraY_ + (wantScreen.y - static_cast<float>(screenHeight_) * 0.5f) / z;

    const float t = std::min(1.0f, dt * 4.0f);
    cameraX_ += (wantX - cameraX_) * t;
    cameraY_ += (wantY - cameraY_) * t;
    clampCamera();
}

void Game::drawSpriteFrame(const SpriteSheet& sheet, int frameIndex, int screenAnchorX,
                           int screenAnchorY, std::uint8_t flash)
{
    if (!sheet.valid())
    {
        return;
    }
    const SDL_Rect src = sheet.frameRect(frameIndex);
    // The authored cell is magnified by kSpriteScale, and the whole world pass by
    // zoom_ (issue #122). Integer factors keep the pixels crisp.
    const int scale = kSpriteScale * zoom_;
    SDL_Rect dst;
    dst.w = src.w * scale;
    dst.h = src.h * scale;
    dst.x = screenAnchorX - sheet.anchorX(frameIndex) * scale;
    dst.y = screenAnchorY - sheet.anchorY(frameIndex) * scale;
    SDL_RenderCopy(renderer_, sheet.texture(), &src, &dst);

    if (flash > 0)
    {
        // A brief white veil on a hit. Blended, so it reads as a flash rather
        // than replacing the sprite (the flat markers did the same with colour).
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, 255, 245, 220, flash);
        SDL_RenderFillRect(renderer_, &dst);
    }
}

void Game::renderAgents()
{
    const auto& agents = mission_.agents();
    const bool useSprites = sprites_.valid();

    // One painter's-algorithm pass over everything standing on the board: agents
    // and static props are drawn back-to-front by screen Y, so nearer things
    // overlap farther things. This is the depth-sorted sprite pass (Phase 2).
    // Between fixed steps the view draws at the interpolated position so motion
    // stays smooth on any refresh rate; alpha is 0 when the sim is paused.
    const float alpha = clock_.alpha();
    struct Drawable
    {
        float depth;
        std::size_t agentIndex;
        bool isDecor;
        bool isVehicle;
        int tileX;
        int tileY;
        int frame;
        bool wreck;
        core::Vec2 drawPos;
    };
    std::vector<Drawable> items;
    items.reserve(agents.size() + mission_.vehicles().size() + 64);

    // Hijackable vehicles are scenery on the board (issue #363): drawn in the
    // same back-to-front pass at their interpolated position, so an agent beside
    // a car overlaps correctly. A wreck is still drawn; it never leaves the map.
    for (const VehicleDraw& vd : vehicleDraws())
    {
        const float depth = tileToScreen(vd.drawPos).y;
        items.push_back(Drawable{depth, 0, false, true, 0, 0, vd.frame, vd.wreck, vd.drawPos});
    }

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents[i].isAlive())
        {
            continue;
        }
        const core::Vec2 drawPos = agents[i].interpolatedPosition(alpha);
        const float depth = tileToScreen(drawPos).y;
        items.push_back(Drawable{depth, i, false, false, 0, 0, 0, false, drawPos});
    }
    if (useSprites)
    {
        for (int y = 0; y < grid_.height(); ++y)
        {
            for (int x = 0; x < grid_.width(); ++x)
            {
                if (grid_.decor(x, y) == core::Decor::None)
                {
                    continue;
                }
                const core::Vec2 decorPos{static_cast<float>(x), static_cast<float>(y)};
                const float depth = tileToScreen(decorPos).y;
                items.push_back(Drawable{depth, 0, true, false, x, y, 0, false, decorPos});
            }
        }
    }

    std::sort(items.begin(), items.end(), [](const Drawable& a, const Drawable& b) {
        return a.depth < b.depth;
    });

    for (const Drawable& item : items)
    {
        if (item.isDecor)
        {
            const core::Vec2 screen = tileToScreen(item.drawPos);
            const AgentSprites::Prop prop = grid_.decor(item.tileX, item.tileY) == core::Decor::Barrel
                                                ? AgentSprites::Barrel
                                                : AgentSprites::Crate;
            drawSpriteFrame(sprites_.prop(prop), 0, static_cast<int>(screen.x),
                            static_cast<int>(screen.y));
            continue;
        }

        if (item.isVehicle)
        {
            const core::Vec2 screen = tileToScreen(item.drawPos);
            const core::Vec2 world =
                core::tileToWorld(item.drawPos.x, item.drawPos.y, kTileWidth, kTileHeight);
            const int sx = static_cast<int>(screen.x);
            const int sy = static_cast<int>(screen.y);

            // A drop shadow grounds the vehicle on its tile, like an agent's.
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 80);
            const int shadowW = 30 * zoom_;
            const int shadowH = 11 * zoom_;
            SDL_Rect shadow{sx - shadowW / 2, sy - shadowH / 2 + 2, shadowW, shadowH};
            SDL_RenderFillRect(renderer_, &shadow);

            if (useSprites)
            {
                const AgentSprites::Vehicle kind = item.frame == 0 ? AgentSprites::VehicleCar
                                                                   : AgentSprites::VehicleTrain;
                drawSpriteFrame(sprites_.vehicle(kind), 0, sx, sy);
                if (item.wreck)
                {
                    // A wreck reads as a charred, darkened shell — a black veil
                    // over the intact sprite (a sprite has no wreck frame yet).
                    const SDL_Rect src = sprites_.vehicle(kind).frameRect(0);
                    const int scale = kSpriteScale * zoom_;
                    SDL_Rect rect{sx - sprites_.vehicle(kind).anchorX(0) * scale,
                                  sy - sprites_.vehicle(kind).anchorY(0) * scale, src.w * scale,
                                  src.h * scale};
                    SDL_SetRenderDrawColor(renderer_, 20, 18, 16, 150);
                    SDL_RenderFillRect(renderer_, &rect);
                }
            }
            else
            {
                // Fallback: a flat steel diamond, dark grey once destroyed.
                if (item.wreck)
                {
                    fillDiamondWorld(world, 0.5f, hudstyle::kVehicleWreck[0], hudstyle::kVehicleWreck[1],
                                     hudstyle::kVehicleWreck[2]);
                }
                else
                {
                    fillDiamondWorld(world, 0.5f, hudstyle::kVehicleBody[0], hudstyle::kVehicleBody[1],
                                     hudstyle::kVehicleBody[2]);
                }
            }
            continue;
        }

        const std::size_t index = item.agentIndex;
        const core::Agent& agent = agents[index];
        const bool isSelected =
            std::find(selected_.begin(), selected_.end(), index) != selected_.end();
        const core::Vec2 drawPos = item.drawPos;
        const core::Vec2 world =
            core::tileToWorld(drawPos.x, drawPos.y, kTileWidth, kTileHeight);
        const core::Vec2 screen = tileToScreen(drawPos);
        const int sx = static_cast<int>(screen.x);
        const int sy = static_cast<int>(screen.y);
        const bool civilian = agent.team() == core::Team::Civilian;

        // A soft drop shadow grounds every sprite on its tile.
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 80);
        const int shadowW = 22 * zoom_;
        const int shadowH = 9 * zoom_;
        SDL_Rect shadow{sx - shadowW / 2, sy - shadowH / 2 + 1, shadowW, shadowH};
        SDL_RenderFillRect(renderer_, &shadow);

        // The selection ring sits on the tile, under the sprite.
        if (isSelected && !civilian)
        {
            fillDiamondWorld(world, 0.5f, 250, 250, 250);
            fillDiamondWorld(world, 0.44f, hudstyle::kPlayerBodySelected[0], hudstyle::kPlayerBodySelected[1],
                             hudstyle::kPlayerBodySelected[2]);
        }

        if (useSprites)
        {
            const AgentView& view = index < views_.size() ? views_[index] : AgentView{};
            const bool back = view.facingY < -0.15f;
            const bool step = view.walkPhase >= 0.5f;
            AgentSprites::Role role = AgentSprites::Enemy;
            if (agent.team() == core::Team::Player)
            {
                role = AgentSprites::Player;
            }
            else if (civilian)
            {
                role = agent.isFollower() ? AgentSprites::Follower : AgentSprites::Civilian;
            }
            const std::uint8_t flash =
                agent.hitFlash() > 0.0f ? 150 : 0;
            drawSpriteFrame(sprites_.sheet(role), AgentSprites::frameIndex(back, step), sx, sy,
                            flash);
        }
        else if (civilian)
        {
            // Fallback: flat diamonds, as before the sprite pass.
            if (agent.isFollower())
            {
                fillDiamondWorld(world, 0.28f, hudstyle::kFollowerBody[0], hudstyle::kFollowerBody[1], hudstyle::kFollowerBody[2]);
            }
            else
            {
                fillDiamondWorld(world, 0.26f, hudstyle::kCivilianBody[0], hudstyle::kCivilianBody[1], hudstyle::kCivilianBody[2]);
            }
        }
        else if (agent.team() == core::Team::Enemy)
        {
            if (agent.hitFlash() > 0.0f)
            {
                fillDiamondWorld(world, 0.34f, hudstyle::kEnemyBodyFlash[0], hudstyle::kEnemyBodyFlash[1],
                                 hudstyle::kEnemyBodyFlash[2]);
            }
            else
            {
                fillDiamondWorld(world, 0.34f, hudstyle::kEnemyBody[0], hudstyle::kEnemyBody[1], hudstyle::kEnemyBody[2]);
            }
        }
        else if (isSelected)
        {
            fillDiamondWorld(world, 0.36f, hudstyle::kPlayerBodySelected[0], hudstyle::kPlayerBodySelected[1],
                             hudstyle::kPlayerBodySelected[2]);
        }
        else
        {
            fillDiamondWorld(world, 0.34f, hudstyle::kPlayerBody[0], hudstyle::kPlayerBody[1], hudstyle::kPlayerBody[2]);
        }

        // Bystanders have no health bar or reticle.
        if (civilian)
        {
            continue;
        }

        // Health bar above the agent (players and enemies alike). Kept in screen
        // space (not sorted), so it always reads clearly on top of the sprite.
        // Scaled with the zoom so it stays proportional to the sprite.
        const int barW = 20 * zoom_;
        const int barH = 3 * zoom_;
        const int barX = sx - barW / 2;
        const int barY = sy - 52 * zoom_;

        SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 255);
        SDL_Rect backRect{barX - 1, barY - 1, barW + 2, barH + 2};
        SDL_RenderFillRect(renderer_, &backRect);

        const float fraction = agent.healthFraction();
        const int filled = static_cast<int>(fraction * static_cast<float>(barW));
        SDL_SetRenderDrawColor(renderer_, fraction > 0.5f ? 90 : 235,
                               fraction > 0.5f ? 210 : 90, fraction > 0.5f ? 110 : 70, 255);
        SDL_Rect fore{barX, barY, filled, barH};
        SDL_RenderFillRect(renderer_, &fore);
    }

    // Attack-order reticles: any agent a selected squadmate has focused fire on.
    // Drawn last so they always read on top of the sprites.
    for (const core::Agent& agent : agents)
    {
        const std::size_t target = static_cast<std::size_t>(&agent - agents.data());
        if (!agent.isAlive())
        {
            continue;
        }
        bool focused = false;
        for (std::size_t index : selected_)
        {
            if (agents[index].orderedTarget() == static_cast<int>(target))
            {
                focused = true;
                break;
            }
        }
        if (!focused)
        {
            continue;
        }
        const core::Vec2 reticlePos = agent.interpolatedPosition(alpha);
        const core::Vec2 world =
            core::tileToWorld(reticlePos.x, reticlePos.y, kTileWidth, kTileHeight);
        // A white outer ring with a red core reads clearly as "engaged".
        fillDiamondWorld(world, 0.6f, 250, 250, 250);
        fillDiamondWorld(world, 0.52f, 235, 90, 70);
        fillDiamondWorld(world, 0.34f,
                         agent.team() == core::Team::Enemy ? hudstyle::kEnemyBody[0] : hudstyle::kPlayerBody[0],
                         agent.team() == core::Team::Enemy ? hudstyle::kEnemyBody[1] : hudstyle::kPlayerBody[1],
                         agent.team() == core::Team::Enemy ? hudstyle::kEnemyBody[2] : hudstyle::kPlayerBody[2]);
    }
}

std::vector<core::Vec2> Game::waypointRoute(std::size_t agentIndex) const
{
    const auto& agents = mission_.agents();
    if (agentIndex >= agents.size())
    {
        return {};
    }
    const core::Agent& agent = agents[agentIndex];
    if (!agent.isAlive() || !agent.hasQueuedMoves())
    {
        return {};
    }

    // The route starts where the agent stands, then visits each queued
    // waypoint in order. Drawn from the tile centre, matching the agents' own
    // tiles, so the line reads as a plan rather than following the sprite.
    std::vector<core::Vec2> route;
    route.reserve(agent.moveQueue().size() + 1);
    route.push_back(tileToScreen(core::Vec2{static_cast<float>(agent.tile().x),
                                            static_cast<float>(agent.tile().y)}));
    for (const core::TilePos& wp : agent.moveQueue())
    {
        route.push_back(tileToScreen(core::Vec2{static_cast<float>(wp.x),
                                                static_cast<float>(wp.y)}));
    }
    return route;
}

void Game::renderWaypointPath()
{
    // Queued-route overlay (issue #89): for every selected agent with a
    // non-empty shift-click queue, draw a polyline through the waypoints and a
    // numbered dot on each one. View-only: it reads the core queue and never
    // writes it (golden rule 4).
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    for (std::size_t index : selected_)
    {
        const std::vector<core::Vec2> route = waypointRoute(index);
        if (route.size() < 2)
        {
            continue; // nothing queued: no overlay
        }

        SDL_SetRenderDrawColor(renderer_, hudstyle::kWaypointLine[0], hudstyle::kWaypointLine[1],
                               hudstyle::kWaypointLine[2], 200);
        for (std::size_t i = 1; i < route.size(); ++i)
        {
            // Integer Bresenham keeps the line cheap and crisp on the software
            // renderer (no anti-aliasing needed for the pixel-art look).
            int x0 = static_cast<int>(route[i - 1].x);
            int y0 = static_cast<int>(route[i - 1].y);
            const int x1 = static_cast<int>(route[i].x);
            const int y1 = static_cast<int>(route[i].y);
            const int dx = std::abs(x1 - x0);
            const int sx = x0 < x1 ? 1 : -1;
            const int dy = -std::abs(y1 - y0);
            const int sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            while (true)
            {
                SDL_RenderDrawPoint(renderer_, x0, y0);
                if (x0 == x1 && y0 == y1)
                {
                    break;
                }
                const int e2 = 2 * err;
                if (e2 >= dy)
                {
                    err += dy;
                    x0 += sx;
                }
                if (e2 <= dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }

        // Numbered dots on each queued waypoint (skip the agent's own tile, the
        // route's first point) so the order is readable.
        for (std::size_t i = 1; i < route.size(); ++i)
        {
            const int cx = static_cast<int>(route[i].x);
            const int cy = static_cast<int>(route[i].y);
            SDL_SetRenderDrawColor(renderer_, hudstyle::kWaypointDot[0], hudstyle::kWaypointDot[1],
                                   hudstyle::kWaypointDot[2], 235);
            SDL_Rect dot{cx - 3, cy - 3, 7, 7};
            SDL_RenderFillRect(renderer_, &dot);
            const std::string label = std::to_string(i);
            font_.draw(renderer_, cx - font_.textWidth(label) / 2, cy - font_.lineHeight() / 2,
                       label, hudstyle::kHudText);
        }
    }
}

void Game::renderScannerPing()
{
    // Scanner ping overlay (issue #323): for every agent with a live pulse, draw
    // a sensor ring at its tile and a highlight on each enemy the scan revealed.
    // View-only: it reads `Mission::scannerPulse`/`scanTargets` and never writes
    // them, so the reveal cannot change a combat outcome (golden rule 4).
    const std::vector<ScannerMark> marks = scannerMarks();
    if (marks.empty())
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    for (const ScannerMark& mark : marks)
    {
        if (mark.ring)
        {
            // The pulse ring around the scanning agent: a hollow diamond outline
            // sized to the scan radius, so the sweep reads without hiding the
            // agent it is centred on.
            strokeDiamondWorld(mark.world, mark.radius, hudstyle::kScannerPing[0],
                               hudstyle::kScannerPing[1], hudstyle::kScannerPing[2]);
            continue;
        }

        // A marked enemy: a bright crosshair that reads on top of any sprite.
        const int cx = static_cast<int>(mark.screen.x);
        const int cy = static_cast<int>(mark.screen.y);
        const int arm = 9 * zoom_;
        SDL_SetRenderDrawColor(renderer_, hudstyle::kScannerPing[0], hudstyle::kScannerPing[1],
                               hudstyle::kScannerPing[2], 220);
        SDL_Rect horizontal{cx - arm, cy - 1, arm * 2 + 1, 2};
        SDL_Rect vertical{cx - 1, cy - arm, 2, arm * 2 + 1};
        SDL_RenderFillRect(renderer_, &horizontal);
        SDL_RenderFillRect(renderer_, &vertical);
    }
}

std::vector<Game::ScannerMark> Game::scannerMarks() const
{
    std::vector<ScannerMark> marks;
    const auto& agents = mission_.agents();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (mission_.scannerPulse(i) <= 0.0f)
        {
            continue;
        }
        const core::Vec2 position = agents[i].interpolatedPosition(clock_.alpha());
        const core::Vec2 screen = tileToScreen(position);
        const core::Vec2 world =
            core::tileToWorld(position.x, position.y, kTileWidth, kTileHeight);
        // A ring drawn as a diamond footprint: the horizontal half-width factor
        // (1.6 tiles) makes the sweep read against the isometric board.
        const ScannerMark ring{i, screen, world, 1.6f, true};
        marks.push_back(ring);
        for (std::size_t target : mission_.scanTargets(i))
        {
            const core::Vec2 markPos = agents[target].interpolatedPosition(clock_.alpha());
            const ScannerMark mark{target, tileToScreen(markPos), core::Vec2{0.0f, 0.0f}, 0.0f,
                                   false};
            marks.push_back(mark);
        }
    }
    return marks;
}

void Game::renderShield()
{
    // Raised personal shield overlay (issue #375): a translucent dome over each
    // agent with a shield up, with a brighter rim and a flash while the absorb
    // burst is fading. View-only: it reads `Mission::shieldUp` and never writes
    // it (golden rule 4).
    const std::vector<ShieldMark> marks = shieldMarks();
    if (marks.empty())
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    for (const ShieldMark& mark : marks)
    {
        const bool absorbing = mark.absorbing;
        const int field = absorbing ? 150 : kShieldFieldAlpha;
        fillDiamondWorld(mark.world, 0.5f, hudstyle::kShieldField[0], hudstyle::kShieldField[1],
                         hudstyle::kShieldField[2], static_cast<std::uint8_t>(field));
        strokeDiamondWorld(mark.world, 0.5f, hudstyle::kShieldField[0], hudstyle::kShieldField[1],
                           hudstyle::kShieldField[2],
                           static_cast<std::uint8_t>(absorbing ? 255 : kShieldRimAlpha));
    }
}

std::vector<Game::ShieldMark> Game::shieldMarks() const
{
    // The shield marks the overlay should draw, from the live mission state
    // (issue #375). A dome only for a raised shield; `absorbing` is true while
    // the per-agent absorb burst is still fading, so a shield that did its job
    // leaves a visible cue. View-only: pure read of core (golden rule 4).
    std::vector<ShieldMark> marks;
    const auto& agents = mission_.agents();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        const bool up = mission_.shieldUp(i);
        const bool flash = i < shieldAbsorbFlash_.size() && shieldAbsorbFlash_[i] > 0.0f;
        if (!up && !flash)
        {
            continue;
        }
        const core::Vec2 position = agents[i].interpolatedPosition(clock_.alpha());
        const core::Vec2 world =
            core::tileToWorld(position.x, position.y, kTileWidth, kTileHeight);
        marks.push_back(ShieldMark{i, world, flash});
    }
    return marks;
}

void Game::updateShieldSnapshot(float dt)
{
    // Spot the frame a raised personal shield absorbs a hit (issue #375): the
    // shield has no timer, so the only way it leaves `shieldUp` is by absorbing
    // a hit. When that happens, start a short burst so the dome flashes instead
    // of vanishing silently. View-only; it never feeds the simulation.
    const auto& agents = mission_.agents();
    if (shieldAbsorbFlash_.size() != agents.size())
    {
        shieldAbsorbFlash_.assign(agents.size(), 0.0f);
        shieldUpPrev_.assign(agents.size(), 0);
        shieldSnapshotValid_ = false;
    }
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (shieldAbsorbFlash_[i] > 0.0f)
        {
            shieldAbsorbFlash_[i] = std::max(0.0f, shieldAbsorbFlash_[i] - dt);
        }
        const bool up = mission_.shieldUp(i);
        if (shieldSnapshotValid_ && shieldUpPrev_[i] != 0 && !up)
        {
            shieldAbsorbFlash_[i] = kShieldAbsorbFlashSeconds;
        }
        shieldUpPrev_[i] = up ? 1 : 0;
    }
    shieldSnapshotValid_ = true;
}

bool Game::shieldSelfTest()
{
    // Headless guard for the shield overlay (issue #375): drive the shared
    // shieldMarks() decision the render pass uses, and prove the invariants a
    // broken overlay would violate — no dome before a shield is raised, one dome
    // on the shielded combatant, and the dome staying while a re-raise is a
    // no-op. The absorb burst (a raised shield that drops) is covered by the
    // core absorb tests and the `--shield` screenshot, not here.
    const auto& agents = mission_.agents();

    // No shield up yet: the overlay must draw nothing. Take the snapshot first so
    // `absorbing` cannot leak from the initial state.
    updateShieldSnapshot(0.0f);
    if (!shieldMarks().empty())
    {
        std::fprintf(stderr, "shield self-test: a dome was drawn with no shield up\n");
        return false;
    }

    std::size_t user = agents.size();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents[i].isAlive() && agents[i].team() == core::Team::Player)
        {
            user = i;
            break;
        }
    }
    if (user == agents.size())
    {
        std::fprintf(stderr, "shield self-test: no living player agent\n");
        return false;
    }

    // Raise a shield on a living combatant: exactly one dome, not absorbing.
    mission_.setShields(user, 1);
    if (!mission_.activateShield(user))
    {
        std::fprintf(stderr, "shield self-test: could not raise a shield\n");
        return false;
    }
    updateShieldSnapshot(0.0f);
    std::vector<ShieldMark> marks = shieldMarks();
    if (marks.size() != 1 || marks[0].index != user)
    {
        std::fprintf(stderr, "shield self-test: expected one dome on the shielded agent\n");
        return false;
    }
    if (marks[0].absorbing)
    {
        std::fprintf(stderr, "shield self-test: a freshly raised shield read as absorbing\n");
        return false;
    }

    // A charge spent on an already-raised shield is a no-op, so the dome stays.
    mission_.setShields(user, 1);
    mission_.activateShield(user);
    if (shieldMarks().size() != 1)
    {
        std::fprintf(stderr, "shield self-test: a second raise removed the dome\n");
        return false;
    }

    return true;
}

std::vector<Game::VehicleDraw> Game::vehicleDraws() const
{
    // The draw decision for every vehicle in the mission (issue #363). A
    // destroyed vehicle is still drawn, as a wreck: unlike a dead agent it stays
    // on the board. Pure read of core (golden rule 4).
    std::vector<VehicleDraw> draws;
    const auto& vehicles = mission_.vehicles();
    draws.reserve(vehicles.size());
    const float alpha = clock_.alpha();
    for (std::size_t i = 0; i < vehicles.size(); ++i)
    {
        const core::Vehicle& vehicle = vehicles[i];
        const int frame =
            vehicle.kind() == core::VehicleKind::Train ? AgentSprites::VehicleTrain
                                                       : AgentSprites::VehicleCar;
        draws.push_back(VehicleDraw{vehicle.interpolatedPosition(alpha), static_cast<int>(i), frame,
                                    vehicle.isDestroyed()});
    }
    return draws;
}

bool Game::vehicleSelfTest()
{
    // Headless guard for vehicle rendering (issue #363): drive the shared
    // vehicleDraws() decision the depth-sorted pass uses, on a board with one car
    // and one train, and prove the invariants a broken draw pass would violate.
    addVehiclesForDemo();

    const std::vector<VehicleDraw> draws = vehicleDraws();
    if (draws.size() != 2)
    {
        std::fprintf(stderr, "vehicle self-test: expected 2 draws, got %zu\n", draws.size());
        return false;
    }

    bool sawCar = false;
    bool sawTrain = false;
    for (const VehicleDraw& draw : draws)
    {
        if (draw.wreck)
        {
            std::fprintf(stderr, "vehicle self-test: intact vehicle drawn as a wreck\n");
            return false;
        }
        if (draw.frame == AgentSprites::VehicleCar)
        {
            sawCar = true;
        }
        else if (draw.frame == AgentSprites::VehicleTrain)
        {
            sawTrain = true;
        }
        else
        {
            std::fprintf(stderr, "vehicle self-test: unknown vehicle frame %d\n", draw.frame);
            return false;
        }
    }
    if (!sawCar || !sawTrain)
    {
        std::fprintf(stderr, "vehicle self-test: a kind did not map to its own sprite\n");
        return false;
    }

    // A destroyed vehicle must still draw — as a wreck, not by disappearing.
    core::Vehicle& car = mission_.vehicles().front();
    car.applyDamage(car.maxHealth() * 2.0f);
    if (!car.isDestroyed())
    {
        std::fprintf(stderr, "vehicle self-test: could not destroy the vehicle\n");
        return false;
    }
    const std::vector<VehicleDraw> after = vehicleDraws();
    if (after.size() != 2)
    {
        std::fprintf(stderr, "vehicle self-test: a wreck left the draw pass\n");
        return false;
    }
    if (!after.front().wreck)
    {
        std::fprintf(stderr, "vehicle self-test: destroyed vehicle not flagged as a wreck\n");
        return false;
    }
    return true;
}

void Game::addVehiclesForDemo()
{
    // One car and one train on walkable tiles near the board centre (issue #363),
    // so the demo art lands in view rather than on the map edge. A
    // demo/verification hook: keeps the art visible without a mission file that
    // ships vehicles (that mission-data change is a Tactical fragment).
    const int cx = grid_.width() / 2;
    const int cy = grid_.height() / 2;
    // Walk the board in rings of increasing Manhattan distance from the centre
    // and take the first two free walkable tiles. Deterministic, no RNG.
    int placed = 0;
    const int maxRing = grid_.width() + grid_.height();
    for (int ring = 0; ring < maxRing && placed < 2; ++ring)
    {
        for (int y = 0; y < grid_.height() && placed < 2; ++y)
        {
            for (int x = 0; x < grid_.width() && placed < 2; ++x)
            {
                if (std::abs(x - cx) + std::abs(y - cy) != ring)
                {
                    continue;
                }
                if (!grid_.isWalkable(x, y))
                {
                    continue;
                }
                bool occupied = false;
                for (const core::Vehicle& existing : mission_.vehicles())
                {
                    if (existing.tile().x == x && existing.tile().y == y)
                    {
                        occupied = true;
                        break;
                    }
                }
                if (occupied)
                {
                    continue;
                }
                if (placed == 0)
                {
                    mission_.addVehicle(core::makeCar(static_cast<float>(x), static_cast<float>(y)));
                }
                else
                {
                    mission_.addVehicle(core::makeTrain(static_cast<float>(x), static_cast<float>(y)));
                }
                ++placed;
            }
        }
    }
}

void Game::renderSelectionBox()
{
    // Drag marquee (issue #94). View-only: draws the rectangle the player is
    // dragging so the pending box-selection is visible. Skipped until the drag
    // has actually started (the interactive path only shows a box past the
    // threshold) and while the title card is up.
    if (!dragging_ || titleCardActive_)
    {
        return;
    }
    if (std::abs(dragCurrentX_ - dragStartX_) < kDragThreshold &&
        std::abs(dragCurrentY_ - dragStartY_) < kDragThreshold)
    {
        return;
    }

    const int minX = std::min(dragStartX_, dragCurrentX_);
    const int maxX = std::max(dragStartX_, dragCurrentX_);
    const int minY = std::min(dragStartY_, dragCurrentY_);
    const int maxY = std::max(dragStartY_, dragCurrentY_);
    const SDL_Rect box{minX, minY, maxX - minX, maxY - minY};

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, hudstyle::kSelectionBox[0], hudstyle::kSelectionBox[1],
                           hudstyle::kSelectionBox[2], 60);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, hudstyle::kSelectionBox[0], hudstyle::kSelectionBox[1],
                           hudstyle::kSelectionBox[2], 220);
    SDL_RenderDrawRect(renderer_, &box);
}

bool Game::checkMinimap()
{
    if (!minimap_.valid())
    {
        return false;
    }
    minimap_.updateBlips(mission_, selected_);
    return minimap_.verifyBlips(mission_);
}

void Game::renderMinimap()
{
    if (!minimap_.valid())
    {
        return;
    }
    // Top-right corner, below the top HUD bar.
    const int margin = 8;
    const int x = screenWidth_ - minimap_.width() - margin;
    const int y = font_.lineHeight() + margin + margin / 2;
    minimap_.updateBlips(mission_, selected_);
    minimap_.render(renderer_, x, y);
}

void Game::renderCampaignScreen()
{
    if (!campaign_.visible())
    {
        return;
    }
    campaign_.render(renderer_, font_, screenWidth_, screenHeight_);
}

void Game::render()
{
    SDL_SetRenderDrawColor(renderer_, 12, 13, 12, 255);
    SDL_RenderClear(renderer_);

    if (mapTexture_ != nullptr)
    {
        // Zoom (issue #122): scale the whole surface by `zoom_`. Integer
        // destination offsets keep the pixel grid crisp; SDL's default
        // nearest-neighbour sampling preserves the pixel-art look (AGENTS.md §1).
        SDL_Rect dst;
        dst.x = -static_cast<int>(cameraX_ * static_cast<float>(zoom_));
        dst.y = -static_cast<int>(cameraY_ * static_cast<float>(zoom_));
        dst.w = mapSurfaceWidth_ * zoom_;
        dst.h = mapSurfaceHeight_ * zoom_;
        SDL_RenderCopy(renderer_, mapTexture_, nullptr, &dst);
    }

    renderAgents();
    renderWaypointPath();
    renderScannerPing();
    renderShield();
    renderSelectionBox();
    if (titleCardActive_)
    {
        renderTitleCard();
    }
    else
    {
        renderHud();
        renderMinimap();
    }
    // The campaign management screen draws over everything when it is up
    // (Milestone B). A no-op when hidden or no campaign loaded.
    renderCampaignScreen();

    SDL_RenderPresent(renderer_);
}

bool Game::saveScreenshot(const std::string& path)
{
    render();

    SDL_Surface* shot = SDL_CreateRGBSurfaceWithFormat(0, screenWidth_, screenHeight_, 32,
                                                       SDL_PIXELFORMAT_ARGB8888);
    if (shot == nullptr)
    {
        return false;
    }

    bool saved = false;
    if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, shot->pixels,
                             shot->pitch) == 0)
    {
        saved = SDL_SaveBMP(shot, path.c_str()) == 0;
    }
    SDL_FreeSurface(shot);
    return saved;
}

int Game::run(int maxFrames)
{
    if (!running_)
    {
        return 1;
    }

    std::uint32_t previous = SDL_GetTicks();
    int frame = 0;

    while (running_)
    {
        bool alive = true;
        handleEvents(alive);
        running_ = running_ && alive;

        const std::uint32_t now = SDL_GetTicks();
        float dt = static_cast<float>(now - previous) / 1000.0f;
        previous = now;
        // Clamp to avoid a huge catch-up step after a stall.
        dt = std::min(dt, 0.1f);

        update(dt);
        render();

        ++frame;
        if (maxFrames > 0 && frame >= maxFrames)
        {
            break;
        }
        SDL_Delay(1);
    }

    return 0;
}

} // namespace fossynd::game
