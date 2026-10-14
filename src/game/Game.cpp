/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "core/data/ContentPaths.h"
#include "core/data/Json.h"
#include "core/data/MissionData.h"
#include "core/math/Isometric.h"
#include "game/EmbeddedLang.h"
#include "game/EmbeddedMission.h"
#include "game/EmbeddedSprites.h"
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
constexpr std::uint32_t kBackground = rgba(12, 13, 12);

void setPixel(SDL_Surface* surface, int x, int y, std::uint32_t color)
{
    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    {
        return;
    }
    auto* pixels = static_cast<std::uint32_t*>(surface->pixels);
    pixels[y * (surface->pitch / 4) + x] = color;
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

    // Smooth the health bars / diamonds a little. Harmless on old hardware.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    if (!font_.init(renderer_, 2))
    {
        std::fprintf(stderr, "font init failed: %s\n", SDL_GetError());
        return false;
    }

    loadLanguage();
    buildMission();
    buildMapTexture();
    buildSprites();
    buildMinimap();

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
    // The English table lives in assets/lang/en.json; prefer an external file
    // (so translators can override it) and fall back to the copy embedded at
    // build time (ADR-0016). A missing or unparsable file is not fatal: the
    // table stays empty and every lookup uses its literal fallback, so the UI
    // renders exactly today's text.
    std::string error;
    for (const std::string& path : core::i18n::defaultLangPaths(exeDir_, "en"))
    {
        if (core::fileExists(path) && lang_.loadFromFile(path, error))
        {
            return;
        }
    }
    const core::Json root = core::Json::parse(kEnglishLangJson, error);
    if (!lang_.loadFromJson(root, error))
    {
        std::fprintf(stderr, "language data error: %s\n", error.c_str());
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

void Game::restart()
{
    seed_ = seed_ * 6364136223846793005ull + 1442695040888963407ull;
    buildMission();
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
            const std::uint32_t color = walkable
                                            ? (((x + y) % 2 == 0) ? kTileLight : kTileDark)
                                            : kTileBlocked;
            fillDiamond(surface, cx, cy, halfW, halfH, color);

            if (!walkable)
            {
                fillDiamond(surface, cx, cy, halfW * 0.85f, halfH * 0.85f, kBlockedEdge);
                fillDiamond(surface, cx, cy, halfW * 0.7f, halfH * 0.7f, kTileBlocked);
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
}

core::Vec2 Game::tileToScreen(const core::Vec2& tile) const
{
    const core::Vec2 world = core::tileToWorld(tile.x, tile.y, kTileWidth, kTileHeight);
    return core::Vec2{world.x + mapOffsetX_ - cameraX_, world.y + mapOffsetY_ - cameraY_};
}

core::TilePos Game::screenToTile(int screenX, int screenY) const
{
    const float worldX = static_cast<float>(screenX) + cameraX_ - mapOffsetX_;
    const float worldY = static_cast<float>(screenY) + cameraY_ - mapOffsetY_;
    const core::Vec2 tile = core::worldToTile(worldX, worldY, kTileWidth, kTileHeight);
    return core::TilePos{static_cast<int>(std::floor(tile.x + 0.5f)),
                         static_cast<int>(std::floor(tile.y + 0.5f))};
}

void Game::centerOn(const core::Vec2& tile)
{
    const core::Vec2 screen = tileToScreen(tile);
    cameraX_ += screen.x - static_cast<float>(screenWidth_) * 0.5f;
    cameraY_ += screen.y - static_cast<float>(screenHeight_) * 0.5f;
    clampCamera();
}

void Game::clampCamera()
{
    const float maxX = std::max(0.0f, static_cast<float>(mapSurfaceWidth_ - screenWidth_));
    const float maxY = std::max(0.0f, static_cast<float>(mapSurfaceHeight_ - screenHeight_));
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
                            std::uint8_t g, std::uint8_t b)
{
    // A small isometric diamond footprint, drawn as a few horizontal spans.
    const float halfW = radius * (kTileWidth * 0.5f);
    const float halfH = radius * (kTileHeight * 0.5f);
    const core::Vec2 screen = core::Vec2{worldCenter.x + mapOffsetX_ - cameraX_,
                                         worldCenter.y + mapOffsetY_ - cameraY_};
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
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
            switch (event.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                running = false;
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
                cameraX_ -= kTileWidth;
                clampCamera();
                break;
            case SDLK_RIGHT:
                cameraX_ += kTileWidth;
                clampCamera();
                break;
            case SDLK_UP:
                cameraY_ -= kTileHeight;
                clampCamera();
                break;
            case SDLK_DOWN:
                cameraY_ += kTileHeight;
                clampCamera();
                break;
            default:
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)
            {
                const bool right = event.button.button == SDL_BUTTON_RIGHT;
                const bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

                // Right-click, or clicking an enemy, is an attack order.
                const int enemy = enemyAtScreen(event.button.x, event.button.y);
                if (right || (enemy >= 0 && !shift))
                {
                    orderAttackOn(enemy);
                    break;
                }

                // Otherwise it is a selection or a move order.
                const int picked = agentAtScreen(event.button.x, event.button.y);
                if (picked >= 0 && !shift)
                {
                    selectOnly(static_cast<std::size_t>(picked));
                }
                else if (picked >= 0)
                {
                    toggleSelect(static_cast<std::size_t>(picked));
                }
                else
                {
                    // Clicking the ground orders the current selection there.
                    const core::TilePos goal = screenToTile(event.button.x, event.button.y);
                    if (grid_.inBounds(goal.x, goal.y) && grid_.isWalkable(goal.x, goal.y))
                    {
                        // A lone agent goes exactly where you clicked; a group
                        // spreads over a small 2-wide formation so agents do not
                        // stack on one another.
                        for (std::size_t i = 0; i < selected_.size(); ++i)
                        {
                            core::TilePos target = goal;
                            if (selected_.size() > 1)
                            {
                                const core::TilePos offset{static_cast<int>(i % 2) - 1,
                                                           static_cast<int>(i / 2)};
                                const core::TilePos candidate{goal.x + offset.x, goal.y + offset.y};
                                if (grid_.inBounds(candidate.x, candidate.y) &&
                                    grid_.isWalkable(candidate.x, candidate.y))
                                {
                                    target = candidate;
                                }
                            }
                            mission_.orderMove(selected_[i], target);
                        }
                    }
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
    updateAgentViews(frameSeconds);
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
    const float wantX = cameraX_ + wantScreen.x - static_cast<float>(screenWidth_) * 0.5f;
    const float wantY = cameraY_ + wantScreen.y - static_cast<float>(screenHeight_) * 0.5f;

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
    SDL_Rect dst;
    dst.w = src.w * kSpriteScale;
    dst.h = src.h * kSpriteScale;
    dst.x = screenAnchorX - sheet.anchorX(frameIndex) * kSpriteScale;
    dst.y = screenAnchorY - sheet.anchorY(frameIndex) * kSpriteScale;
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
        int tileX;
        int tileY;
        core::Vec2 drawPos;
    };
    std::vector<Drawable> items;
    items.reserve(agents.size() + 64);

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents[i].isAlive())
        {
            continue;
        }
        const core::Vec2 drawPos = agents[i].interpolatedPosition(alpha);
        const float depth = tileToScreen(drawPos).y;
        items.push_back(Drawable{depth, i, false, 0, 0, drawPos});
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
                items.push_back(Drawable{depth, 0, true, x, y, decorPos});
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
        const int shadowW = 22;
        const int shadowH = 9;
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
        const int barW = 20;
        const int barH = 3;
        const int barX = sx - barW / 2;
        const int barY = sy - 52;

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

void Game::render()
{
    SDL_SetRenderDrawColor(renderer_, 12, 13, 12, 255);
    SDL_RenderClear(renderer_);

    if (mapTexture_ != nullptr)
    {
        SDL_Rect dst;
        dst.x = static_cast<int>(-cameraX_);
        dst.y = static_cast<int>(-cameraY_);
        dst.w = mapSurfaceWidth_;
        dst.h = mapSurfaceHeight_;
        SDL_RenderCopy(renderer_, mapTexture_, nullptr, &dst);
    }

    renderAgents();
    if (titleCardActive_)
    {
        renderTitleCard();
    }
    else
    {
        renderHud();
        renderMinimap();
    }

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
