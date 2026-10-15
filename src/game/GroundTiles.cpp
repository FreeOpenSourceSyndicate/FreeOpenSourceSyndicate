/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/GroundTiles.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace fossynd::game
{

GroundTiles::~GroundTiles()
{
    destroy();
}

void GroundTiles::destroy()
{
    for (SDL_Surface*& surface : surfaces_)
    {
        if (surface != nullptr)
        {
            SDL_FreeSurface(surface);
            surface = nullptr;
        }
    }
    valid_ = false;
}

bool GroundTiles::build(const std::string& text, int cellWidth, int cellHeight, std::string& error)
{
    destroy();

    core::TileSprites tiles;
    if (!core::bakeTileSprites(text, tiles, error))
    {
        return false;
    }

    const int nativeW = tiles.sheet.frame(0).width;
    const int nativeH = tiles.sheet.frame(0).height;
    // Nearest-neighbour, integer factor: the tiles are authored at the original
    // chunky resolution and scaled up whole (AGENTS.md 1, golden rule 8).
    const int scaleX = (nativeW > 0) ? cellWidth / nativeW : 0;
    const int scaleY = (nativeH > 0) ? cellHeight / nativeH : 0;
    if (nativeW <= 0 || nativeH <= 0 || scaleX < 1 || scaleY < 1)
    {
        error = "tile art is larger than a map cell";
        return false;
    }

    const int outW = nativeW * scaleX;
    const int outH = nativeH * scaleY;

    for (int i = 0; i < static_cast<int>(core::TileKind::Count); ++i)
    {
        const core::BakedFrame& frame = tiles.sheet.frames[static_cast<std::size_t>(i)];
        SDL_Surface* surface =
            SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_ARGB8888);
        if (surface == nullptr)
        {
            error = std::string("tile surface failed: ") + SDL_GetError();
            destroy();
            return false;
        }

        if (SDL_LockSurface(surface) != 0)
        {
            error = std::string("tile surface lock failed: ") + SDL_GetError();
            SDL_FreeSurface(surface);
            destroy();
            return false;
        }
        auto* pixels = static_cast<std::uint32_t*>(surface->pixels);
        const int pitchPixels = surface->pitch / static_cast<int>(sizeof(std::uint32_t));
        for (int sy = 0; sy < nativeH; ++sy)
        {
            for (int sx = 0; sx < nativeW; ++sx)
            {
                const std::size_t si =
                    (static_cast<std::size_t>(sy) * static_cast<std::size_t>(nativeW) +
                     static_cast<std::size_t>(sx)) *
                    4u;
                const std::uint32_t color =
                    SDL_MapRGBA(surface->format, frame.rgba[si + 0], frame.rgba[si + 1],
                                frame.rgba[si + 2], frame.rgba[si + 3]);
                for (int dy = 0; dy < scaleY; ++dy)
                {
                    const int py = sy * scaleY + dy;
                    std::uint32_t* row = pixels + static_cast<std::size_t>(py) * pitchPixels;
                    for (int dx = 0; dx < scaleX; ++dx)
                    {
                        row[sx * scaleX + dx] = color;
                    }
                }
            }
        }
        SDL_UnlockSurface(surface);
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
        surfaces_[i] = surface;
    }

    valid_ = true;
    return true;
}

void GroundTiles::blit(SDL_Surface* dst, core::TileKind kind, int centerX, int centerY) const
{
    const int index = static_cast<int>(kind);
    SDL_Surface* surface = surfaces_[index];
    if (dst == nullptr || surface == nullptr)
    {
        return;
    }
    SDL_Rect at{centerX - surface->w / 2, centerY - surface->h / 2, 0, 0};
    SDL_BlitSurface(surface, nullptr, dst, &at);
}

} // namespace fossynd::game
