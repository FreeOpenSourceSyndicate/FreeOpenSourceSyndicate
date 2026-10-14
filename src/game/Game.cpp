/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "core/data/MissionData.h"
#include "core/math/Isometric.h"
#include "game/EmbeddedMission.h"
#include "game/EmbeddedSprites.h"

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

// Agent body colours.
constexpr std::uint8_t kPlayerBody[3] = {40, 170, 60};
constexpr std::uint8_t kPlayerBodySelected[3] = {90, 230, 130};
constexpr std::uint8_t kEnemyBody[3] = {200, 55, 45};
constexpr std::uint8_t kEnemyBodyFlash[3] = {255, 210, 90};
constexpr std::uint8_t kCivilianBody[3] = {205, 200, 180};
constexpr std::uint8_t kFollowerBody[3] = {90, 205, 230};

constexpr SDL_Color kHudText{225, 235, 225, 255};
constexpr SDL_Color kHudGood{120, 230, 140, 255};
constexpr SDL_Color kHudBad{235, 110, 90, 255};
constexpr SDL_Color kHudDim{150, 160, 150, 255};

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

    buildMission();
    buildMapTexture();
    buildSprites();

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

void Game::restart()
{
    seed_ = seed_ * 6364136223846793005ull + 1442695040888963407ull;
    buildMission();
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
                // Map number keys to the Nth living player agent.
                const int wanted = event.key.keysym.sym - SDLK_1;
                int seen = 0;
                const auto& agents = mission_.agents();
                for (std::size_t i = 0; i < agents.size(); ++i)
                {
                    if (!agents[i].isAlive() || agents[i].team() != core::Team::Player)
                    {
                        continue;
                    }
                    if (seen == wanted)
                    {
                        selectOnly(i);
                        break;
                    }
                    ++seen;
                }
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

void Game::update(float dt)
{
    if (titleCardActive_)
    {
        titleCardTime_ += dt;
        // A click/keypress starts the mission, as does a few seconds of
        // inactivity, so an unattended run never stalls on the card.
        if (titleCardStartRequested_ || titleCardTime_ >= 6.0f)
        {
            titleCardActive_ = false;
        }
        return;
    }
    if (autoPlay_)
    {
        updateAutoPlay(dt);
        followSquad(dt);
    }
    mission_.update(dt);
    updateAgentViews(dt);
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
    struct Drawable
    {
        float depth;
        std::size_t agentIndex;
        bool isDecor;
        int tileX;
        int tileY;
    };
    std::vector<Drawable> items;
    items.reserve(agents.size() + 64);

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents[i].isAlive())
        {
            continue;
        }
        const float depth = tileToScreen(agents[i].position()).y;
        items.push_back(Drawable{depth, i, false, 0, 0});
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
                const float depth = tileToScreen(core::Vec2{static_cast<float>(x),
                                                            static_cast<float>(y)}).y;
                items.push_back(Drawable{depth, 0, true, x, y});
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
            const core::Vec2 screen = tileToScreen(core::Vec2{static_cast<float>(item.tileX),
                                                              static_cast<float>(item.tileY)});
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
        const core::Vec2 world =
            core::tileToWorld(agent.position().x, agent.position().y, kTileWidth, kTileHeight);
        const core::Vec2 screen = tileToScreen(agent.position());
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
            fillDiamondWorld(world, 0.44f, kPlayerBodySelected[0], kPlayerBodySelected[1],
                             kPlayerBodySelected[2]);
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
                fillDiamondWorld(world, 0.28f, kFollowerBody[0], kFollowerBody[1], kFollowerBody[2]);
            }
            else
            {
                fillDiamondWorld(world, 0.26f, kCivilianBody[0], kCivilianBody[1], kCivilianBody[2]);
            }
        }
        else if (agent.team() == core::Team::Enemy)
        {
            if (agent.hitFlash() > 0.0f)
            {
                fillDiamondWorld(world, 0.34f, kEnemyBodyFlash[0], kEnemyBodyFlash[1],
                                 kEnemyBodyFlash[2]);
            }
            else
            {
                fillDiamondWorld(world, 0.34f, kEnemyBody[0], kEnemyBody[1], kEnemyBody[2]);
            }
        }
        else if (isSelected)
        {
            fillDiamondWorld(world, 0.36f, kPlayerBodySelected[0], kPlayerBodySelected[1],
                             kPlayerBodySelected[2]);
        }
        else
        {
            fillDiamondWorld(world, 0.34f, kPlayerBody[0], kPlayerBody[1], kPlayerBody[2]);
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
        const core::Vec2 world = core::tileToWorld(agent.position().x, agent.position().y, kTileWidth,
                                                   kTileHeight);
        // A white outer ring with a red core reads clearly as "engaged".
        fillDiamondWorld(world, 0.6f, 250, 250, 250);
        fillDiamondWorld(world, 0.52f, 235, 90, 70);
        fillDiamondWorld(world, 0.34f,
                         agent.team() == core::Team::Enemy ? kEnemyBody[0] : kPlayerBody[0],
                         agent.team() == core::Team::Enemy ? kEnemyBody[1] : kPlayerBody[1],
                         agent.team() == core::Team::Enemy ? kEnemyBody[2] : kPlayerBody[2]);
    }
}

void Game::renderHud()
{
    const int margin = 8;
    const int lineH = font_.lineHeight();

    // Semi-transparent top bar.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 170);
    SDL_Rect topBar{0, 0, screenWidth_, lineH + margin};
    SDL_RenderFillRect(renderer_, &topBar);

    font_.draw(renderer_, margin, margin / 2, "FOSSYND v" FOSSYND_VERSION, kHudText);
    std::string objective;
    switch (mission_.objective().kind)
    {
    case core::ObjectiveSpec::Kind::ReachTile:
        objective = "OBJECTIVE: REACH TILE (" + std::to_string(mission_.objective().tileX) + "," +
                    std::to_string(mission_.objective().tileY) + ")";
        break;
    case core::ObjectiveSpec::Kind::Survive:
    {
        const int remaining =
            static_cast<int>(mission_.objective().seconds - mission_.elapsedSeconds() + 0.999f);
        objective = "OBJECTIVE: SURVIVE " + std::to_string(remaining > 0 ? remaining : 0) + "s";
        break;
    }
    case core::ObjectiveSpec::Kind::EliminateAll:
    default:
        objective = "OBJECTIVE: ELIMINATE ALL " +
                    std::to_string(mission_.aliveCount(core::Team::Enemy)) + " ENEMY AGENTS";
        break;
    }
    objective +=
        "   [L] MOVE   [CLICK ENEMY/RIGHT] ATTACK   [1-4] SELECT   [A]LL   [P] PERSUADE   "
        "[F] FREE   [R] RESTART";
    std::size_t followers = 0;
    for (std::size_t i = 0; i < mission_.agents().size(); ++i)
    {
        followers += mission_.followerCount(i);
    }
    if (followers > 0)
    {
        objective = "FOLLOWERS: " + std::to_string(followers) + "   " + objective;
    }
    if (mission_.civilianKills() > 0)
    {
        objective = "COLLATERAL: " + std::to_string(mission_.civilianKills()) + "   " + objective;
    }
    font_.draw(renderer_, screenWidth_ - font_.textWidth(objective) - margin, margin / 2, objective,
               kHudDim);

    // Squad panel (bottom-left).
    const auto& agents = mission_.agents();
    int panelLines = 1;
    for (const core::Agent& agent : agents)
    {
        if (agent.team() == core::Team::Player)
        {
            ++panelLines;
        }
    }
    const int panelH = lineH * panelLines + margin;
    const int panelY = screenHeight_ - panelH;

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 170);
    SDL_Rect panel{0, panelY, 320, panelH};
    SDL_RenderFillRect(renderer_, &panel);

    int y = panelY + margin / 2;
    font_.draw(renderer_, margin, y, "SQUAD", kHudText);
    y += lineH;
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        const core::Agent& agent = agents[i];
        if (agent.team() != core::Team::Player)
        {
            continue;
        }
        const bool isSelected =
            std::find(selected_.begin(), selected_.end(), i) != selected_.end();
        const int hp = static_cast<int>(std::ceil(agent.healthFraction() * 100.0f));
        const std::string marker = isSelected ? "> " : "  ";
        std::string row =
            marker + agent.name() + "  " + agent.weapon().name + "  HP " + std::to_string(hp);
        const SDL_Color color = agent.isAlive() ? (isSelected ? kHudText : kHudDim) : kHudDim;
        font_.draw(renderer_, margin, y, row, color);
        y += lineH;
    }

    // Centre banner when the mission ends.
    if (mission_.state() != core::MissionState::Running)
    {
        const bool won = mission_.state() == core::MissionState::Won;
        const std::string message = mission_.stateName();
        const std::string sub = won ? "PRESS R TO RUN IT AGAIN" : "PRESS R TO RETRY";
        const int cx = screenWidth_ / 2;

        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 190);
        SDL_Rect banner{0, screenHeight_ / 2 - lineH * 2, screenWidth_, lineH * 4};
        SDL_RenderFillRect(renderer_, &banner);

        const SDL_Color color = won ? kHudGood : kHudBad;
        font_.draw(renderer_, cx - font_.textWidth(message) / 2, screenHeight_ / 2 - lineH * 3 / 2,
                   message, color);
        font_.draw(renderer_, cx - font_.textWidth(sub) / 2, screenHeight_ / 2 + lineH / 2, sub,
                   kHudText);
    }
}

void Game::drawTitleDiamond(float cx, float cy, float radius, std::uint8_t r, std::uint8_t g,
                            std::uint8_t b)
{
    // A flat isometric diamond drawn straight into the renderer. `radius` is in
    // the same 0..1 world units as fillDiamondWorld, so the title card reuses the
    // in-game marker shape and reads as the same art.
    const float halfW = radius * (kTileWidth * 0.5f);
    const float halfH = radius * (kTileHeight * 0.5f);
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    const int minY = static_cast<int>(cy - halfH);
    const int maxY = static_cast<int>(cy + halfH);
    for (int py = minY; py <= maxY; ++py)
    {
        const float dy = (static_cast<float>(py) - cy) / halfH;
        const float span = 1.0f - std::fabs(dy);
        if (span <= 0.0f)
        {
            continue;
        }
        SDL_Rect line;
        line.x = static_cast<int>(cx - halfW * span);
        line.w = static_cast<int>(2.0f * halfW * span) + 1;
        line.y = py;
        line.h = 1;
        SDL_RenderFillRect(renderer_, &line);
    }
}

void Game::renderTitleCard()
{
    const int w = screenWidth_;
    const int h = screenHeight_;
    const int lineH = font_.lineHeight();

    // Dark, slightly translucent backdrop so the map stays faintly visible.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 6, 7, 9, 232);
    SDL_Rect full{0, 0, w, h};
    SDL_RenderFillRect(renderer_, &full);

    // A row of squad diamonds (the in-game player marker) as the badge.
    const float cx = static_cast<float>(w) * 0.5f;
    const float badgeY = static_cast<float>(h) * 0.24f;
    const float spacing = static_cast<float>(w) * 0.05f;
    drawTitleDiamond(cx - spacing * 1.5f, badgeY, 0.42f, kPlayerBody[0], kPlayerBody[1],
                     kPlayerBody[2]);
    drawTitleDiamond(cx - spacing * 0.5f, badgeY - lineH, 0.42f, kPlayerBodySelected[0],
                     kPlayerBodySelected[1], kPlayerBodySelected[2]);
    drawTitleDiamond(cx + spacing * 0.5f, badgeY, 0.42f, kPlayerBody[0], kPlayerBody[1],
                     kPlayerBody[2]);
    drawTitleDiamond(cx + spacing * 1.5f, badgeY - lineH, 0.42f, kPlayerBodySelected[0],
                     kPlayerBodySelected[1], kPlayerBodySelected[2]);

    // Title and version, centred.
    const std::string title = "FOSSYND";
    const std::string version = std::string("v") + FOSSYND_VERSION;
    const std::string subtitle = "SMART ENEMIES";

    int y = static_cast<int>(static_cast<float>(h) * 0.40f);
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(title) / 2, y, title, kHudText);
    y += lineH + 2;
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(subtitle) / 2, y, subtitle,
               kHudGood);
    y += lineH + 2;
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(version) / 2, y, version, kHudDim);

    // A blinking prompt (driven by the elapsed time, no RNG).
    const bool blinkOn = std::fmod(titleCardTime_, 1.0f) < 0.6f;
    if (blinkOn)
    {
        const std::string prompt = "PRESS ANY KEY";
        y += lineH * 2;
        font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(prompt) / 2, y, prompt,
                   kHudText);
    }

    // Controls, centred lower down.
    const char* controls[] = {
        "CLICK / L  MOVE THE SQUAD",
        "RIGHT-CLICK  FOCUS FIRE",
        "1-4 / A  SELECT AGENTS",
        "ARROWS  SCROLL   R  RESTART   ESC  QUIT",
    };
    int cy = static_cast<int>(static_cast<float>(h) * 0.68f);
    for (const char* line : controls)
    {
        font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(line) / 2, cy, line, kHudDim);
        cy += lineH;
    }

    // Small footer tag, so a screenshot self-identifies the build.
    const std::string footer = "FOSSYND v" FOSSYND_VERSION "  -  FREE OPEN SOURCE SYNDICATE";
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(footer) / 2, h - lineH - 6, footer,
               kHudDim);
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
