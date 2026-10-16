/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Minimap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fossynd::game
{
namespace
{

constexpr std::uint32_t rgba(int r, int g, int b, int a = 255)
{
    return static_cast<std::uint32_t>((a << 24) | (b << 16) | (g << 8) | r);
}

// Terrain colours, deliberately dim so the blips read on top.
constexpr std::uint32_t kWalkLight = rgba(72, 84, 72);
constexpr std::uint32_t kWalkDark = rgba(60, 71, 60);
constexpr std::uint32_t kBlocked = rgba(28, 32, 28);

// Blip colours match the in-world marker palette (Game.cpp) so the minimap and
// the board read as the same art.
constexpr std::uint8_t kPlayer[3] = {60, 220, 90};
constexpr std::uint8_t kPlayerSelected[3] = {140, 255, 170};
constexpr std::uint8_t kEnemy[3] = {225, 70, 55};
constexpr std::uint8_t kCivilian[3] = {205, 200, 180};
constexpr std::uint8_t kFollower[3] = {90, 205, 230};
// A scanner-marked enemy (issue #323) shares the in-world ping cyan.
constexpr std::uint8_t kScannerMarked[3] = {90, 235, 245};

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

Minimap::~Minimap()
{
    if (texture_ != nullptr)
    {
        SDL_DestroyTexture(texture_);
    }
}

bool Minimap::init(SDL_Renderer* renderer, const core::TileGrid& grid, int pixelScale)
{
    if (renderer == nullptr || grid.width() <= 0 || grid.height() <= 0 || pixelScale <= 0)
    {
        return false;
    }

    gridWidth_ = grid.width();
    gridHeight_ = grid.height();
    pixelScale_ = pixelScale;
    width_ = gridWidth_ * pixelScale_;
    height_ = gridHeight_ * pixelScale_;

    if (texture_ != nullptr)
    {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, gridWidth_, gridHeight_, 32,
                                                          SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr)
    {
        return false;
    }

    // One texel per tile; magnified nearest-neighbour when blitted.
    for (int y = 0; y < gridHeight_; ++y)
    {
        for (int x = 0; x < gridWidth_; ++x)
        {
            const std::uint32_t color =
                grid.isWalkable(x, y) ? (((x + y) % 2 == 0) ? kWalkLight : kWalkDark) : kBlocked;
            setPixel(surface, x, y, color);
        }
    }

    texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                 gridWidth_, gridHeight_);
    if (texture_ == nullptr)
    {
        SDL_FreeSurface(surface);
        return false;
    }
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(texture_, SDL_ScaleModeNearest);
#endif
    SDL_UpdateTexture(texture_, nullptr, surface->pixels, surface->pitch);
    SDL_FreeSurface(surface);
    return true;
}

void Minimap::updateBlips(const core::Mission& mission, const std::vector<std::size_t>& selected)
{
    blips_.clear();
    if (!valid())
    {
        return;
    }

    const auto& agents = mission.agents();

    // Which agents any active scan has revealed (issue #323): computed once so
    // the per-blip loop below does not re-run the scan for every agent.
    std::vector<bool> marked(agents.size(), false);
    for (std::size_t s = 0; s < agents.size(); ++s)
    {
        if (mission.scannerPulse(s) <= 0.0f)
        {
            continue;
        }
        for (std::size_t target : mission.scanTargets(s))
        {
            marked[target] = true;
        }
    }

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        const core::Agent& agent = agents[i];
        if (!agent.isAlive())
        {
            continue;
        }
        const core::Vec2 position = agent.position();
        const int maxX = std::max(0, width_ - pixelScale_);
        const int maxY = std::max(0, height_ - pixelScale_);
        Blip blip;
        blip.x = std::max(0, std::min(static_cast<int>(position.x * static_cast<float>(pixelScale_)),
                                       maxX));
        blip.y = std::max(0, std::min(static_cast<int>(position.y * static_cast<float>(pixelScale_)),
                                       maxY));

        const std::uint8_t* color = kCivilian;
        if (agent.team() == core::Team::Player)
        {
            color = kPlayer;
        }
        else if (agent.team() == core::Team::Enemy)
        {
            color = marked[i] ? kScannerMarked : kEnemy;
        }
        else if (agent.isFollower())
        {
            color = kFollower;
        }
        blip.r = color[0];
        blip.g = color[1];
        blip.b = color[2];
        blip.selected =
            agent.team() == core::Team::Player &&
            std::find(selected.begin(), selected.end(), i) != selected.end();
        blips_.push_back(blip);
    }
}

void Minimap::render(SDL_Renderer* renderer, int x, int y) const
{
    if (!valid() || renderer == nullptr)
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // A dark backing plate keeps the panel readable over any terrain.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
    SDL_Rect plate{x - 1, y - 1, width_ + 2, height_ + 2};
    SDL_RenderFillRect(renderer, &plate);

    SDL_Rect dst{x, y, width_, height_};
    SDL_RenderCopy(renderer, texture_, nullptr, &dst);

    for (const Blip& blip : blips_)
    {
        if (blip.selected)
        {
            // A brighter halo so the selected squad stands out at a glance.
            SDL_SetRenderDrawColor(renderer, kPlayerSelected[0], kPlayerSelected[1],
                                   kPlayerSelected[2], 255);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, blip.r, blip.g, blip.b, 255);
        }
        SDL_Rect dot{x + blip.x, y + blip.y, pixelScale_, pixelScale_};
        SDL_RenderFillRect(renderer, &dot);
    }

    SDL_SetRenderDrawColor(renderer, 90, 96, 90, 255);
    SDL_Rect border{x - 1, y - 1, width_ + 2, height_ + 2};
    SDL_RenderDrawRect(renderer, &border);
}

bool Minimap::verifyBlips(const core::Mission& mission) const
{
    if (!valid())
    {
        return false;
    }

    std::size_t living = 0;
    for (const core::Agent& agent : mission.agents())
    {
        if (agent.isAlive())
        {
            ++living;
        }
    }
    if (blips_.size() != living)
    {
        return false;
    }
    for (const Blip& blip : blips_)
    {
        if (blip.x < 0 || blip.y < 0 || blip.x + pixelScale_ > width_ ||
            blip.y + pixelScale_ > height_)
        {
            return false;
        }
    }
    return true;
}

} // namespace fossynd::game
