/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>

#include "SDL.h"

#include "core/data/TileSprites.h"

namespace fossynd::game
{

// The authored ground tiles (assets/tiles/ground.txt), uploaded as surfaces and
// blitted into the pre-baked static map (issue #189). Each tile is a single
// isometric diamond, pre-scaled to the cell size so a map bake is one plain
// blit per cell. Owned by the view; never copied.
//
// If the sheet cannot be baked the object stays invalid and the caller falls
// back to the original flat diamond fills, so a bad sheet never stops the game
// from running.
class GroundTiles
{
public:
    GroundTiles() = default;
    ~GroundTiles();

    GroundTiles(const GroundTiles&) = delete;
    GroundTiles& operator=(const GroundTiles&) = delete;

    // Bakes `text` and pre-scales each tile to a `cellWidth` x `cellHeight`
    // diamond (nearest-neighbour, an integer factor). Returns false and sets
    // `error` on any failure, leaving the object invalid.
    bool build(const std::string& text, int cellWidth, int cellHeight, std::string& error);

    void destroy();

    bool valid() const { return valid_; }

    // Blits tile `kind` so its diamond is centred on (centerX, centerY) of `dst`.
    void blit(SDL_Surface* dst, core::TileKind kind, int centerX, int centerY) const;

private:
    SDL_Surface* surfaces_[static_cast<int>(core::TileKind::Count)] = {nullptr, nullptr, nullptr,
                                                                      nullptr, nullptr};
    bool valid_ = false;
};

} // namespace fossynd::game
