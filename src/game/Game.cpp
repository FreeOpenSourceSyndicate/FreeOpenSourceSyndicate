/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/math/Isometric.h"

namespace foss::game
{
namespace
{

constexpr std::uint32_t rgba(int r, int g, int b, int a = 255)
{
    return static_cast<std::uint32_t>((a << 24) | (b << 16) | (g << 8) | r);
}

// Palette kept deliberately small and flat (prototype art, no external assets).
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

bool Game::init(int width, int height, bool headless)
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

    window_ = SDL_CreateWindow("Free Open Source Syndicate", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    if (window_ == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // Prefer a software renderer so the game runs on machines without a GPU.
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (renderer_ == nullptr)
    {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    }
    if (renderer_ == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    buildMap();
    buildMapTexture();
    spawnAgents();

    // Center the view on the squad's first agent.
    if (!agents_.empty())
    {
        const core::Vec2 focus = tileToScreen(agents_.front().position());
        cameraX_ += focus.x - static_cast<float>(screenWidth_) * 0.5f;
        cameraY_ += focus.y - static_cast<float>(screenHeight_) * 0.5f;
    }

    running_ = true;
    return true;
}

void Game::buildMap()
{
    for (int y = 0; y < kMapHeight; ++y)
    {
        for (int x = 0; x < kMapWidth; ++x)
        {
            grid_.setWalkable(x, y, true);
            grid_.setTileType(x, y, (x + y) % 2);
        }
    }

    // A simple wall layout: a border of buildings plus an interior block.
    for (int x = 4; x <= 8; ++x)
    {
        grid_.setWalkable(x, 6, false);
    }
    for (int y = 10; y <= 15; ++y)
    {
        grid_.setWalkable(12, y, false);
    }
    for (int x = 15; x <= 18; ++x)
    {
        grid_.setWalkable(x, 12, false);
    }
}

void Game::buildMapTexture()
{
    // Precompute the static map into a surface once, then upload a single
    // texture. Rendering then costs one blit per frame.
    const float halfW = kTileWidth * 0.5f;
    const float halfH = kTileHeight * 0.5f;

    mapOffsetX_ = static_cast<float>(kMapHeight) * halfW + kTileWidth;
    mapOffsetY_ = kTileHeight;

    mapSurfaceWidth_ = static_cast<int>((kMapWidth + kMapHeight) * halfW + kTileWidth);
    mapSurfaceHeight_ = static_cast<int>((kMapWidth + kMapHeight) * halfH + kTileHeight * 2.0f);

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

    for (int y = 0; y < kMapHeight; ++y)
    {
        for (int x = 0; x < kMapWidth; ++x)
        {
            const core::Vec2 center = core::tileToWorld(static_cast<float>(x), static_cast<float>(y),
                                                        kTileWidth, kTileHeight);
            const int cx = static_cast<int>(center.x + mapOffsetX_);
            const int cy = static_cast<int>(center.y + mapOffsetY_);

            const bool walkable = grid_.isWalkable(x, y);
            std::uint32_t color = walkable ? (((x + y) % 2 == 0) ? kTileLight : kTileDark)
                                           : kTileBlocked;
            fillDiamond(surface, cx, cy, halfW, halfH, color);

            if (!walkable)
            {
                // Outline blocked tiles so walls read clearly.
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

void Game::spawnAgents()
{
    // Four agents, in the spirit of a Syndicate squad. For now they stand at
    // fixed posts; the player orders the selected one around with the mouse.
    agents_.clear();
    agents_.emplace_back(2.0f, 2.0f, 3.0f);
    agents_.emplace_back(3.0f, 2.0f, 3.0f);
    agents_.emplace_back(2.0f, 3.0f, 3.0f);
    agents_.emplace_back(3.0f, 3.0f, 3.0f);
    selected_ = 0;
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

void Game::handleEvents(bool& running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
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
            case SDLK_TAB:
                if (!agents_.empty())
                {
                    selected_ = (selected_ + 1) % static_cast<int>(agents_.size());
                }
                break;
            case SDLK_LEFT:
                cameraX_ -= kTileWidth;
                break;
            case SDLK_RIGHT:
                cameraX_ += kTileWidth;
                break;
            case SDLK_UP:
                cameraY_ -= kTileHeight;
                break;
            case SDLK_DOWN:
                cameraY_ += kTileHeight;
                break;
            default:
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT && !agents_.empty())
            {
                const core::TilePos goal = screenToTile(event.button.x, event.button.y);
                if (grid_.inBounds(goal.x, goal.y) && grid_.isWalkable(goal.x, goal.y))
                {
                    agents_[static_cast<std::size_t>(selected_)].moveTo(grid_, goal);
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
    for (core::Agent& agent : agents_)
    {
        agent.update(dt);
    }
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

    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        const core::Vec2 screen = tileToScreen(agents_[i].position());
        const bool isSelected = static_cast<int>(i) == selected_;

        SDL_SetRenderDrawColor(renderer_, isSelected ? 90 : 40, isSelected ? 220 : 140,
                               isSelected ? 120 : 40, 255);
        SDL_Rect body;
        body.w = 12;
        body.h = 12;
        body.x = static_cast<int>(screen.x) - body.w / 2;
        body.y = static_cast<int>(screen.y) - body.h / 2;
        SDL_RenderFillRect(renderer_, &body);

        if (isSelected)
        {
            SDL_SetRenderDrawColor(renderer_, 250, 250, 250, 255);
            SDL_Rect marker;
            marker.w = 20;
            marker.h = 20;
            marker.x = static_cast<int>(screen.x) - marker.w / 2;
            marker.y = static_cast<int>(screen.y) - marker.h / 2;
            SDL_RenderDrawRect(renderer_, &marker);
        }
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

} // namespace foss::game
