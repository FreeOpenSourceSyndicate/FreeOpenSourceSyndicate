/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <vector>

#include "core/math/Vec2.h"

namespace fossynd::core
{

// Static board decoration drawn as a sprite on a tile. Only ever placed on
// blocked tiles, so it never competes with an agent for the same tile. Purely
// presentational; game logic ignores it.
enum class Decor : std::uint8_t
{
    None,
    Crate,
    Barrel,
};

// A single layer of walkable/blocking tiles. Engine-agnostic.
class TileGrid
{
public:
    TileGrid(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    bool inBounds(int x, int y) const;
    bool isWalkable(int x, int y) const;
    void setWalkable(int x, int y, bool walkable);

    // -1 for out-of-bounds, otherwise the tile's type id.
    int tileType(int x, int y) const;
    void setTileType(int x, int y, int type);

    // Board decoration sprite on a tile (presentational only).
    Decor decor(int x, int y) const;
    void setDecor(int x, int y, Decor decor);

private:
    std::size_t index(int x, int y) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<int> types_;
    std::vector<std::uint8_t> walkable_;
    std::vector<std::uint8_t> decor_;
};

} // namespace fossynd::core
