/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <vector>

#include "core/math/Vec2.h"

namespace foss::core
{

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

private:
    std::size_t index(int x, int y) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<int> types_;
    std::vector<std::uint8_t> walkable_;
};

} // namespace foss::core
