/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/world/TileGrid.h"

namespace fossynd::core
{

TileGrid::TileGrid(int width, int height)
    : width_(width > 0 ? width : 0)
    , height_(height > 0 ? height : 0)
{
    const auto count = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    types_.assign(count, 0);
    walkable_.assign(count, 1);
    decor_.assign(count, 0);
}

std::size_t TileGrid::index(int x, int y) const
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}

bool TileGrid::inBounds(int x, int y) const
{
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

bool TileGrid::isWalkable(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return false;
    }
    return walkable_[index(x, y)] != 0;
}

void TileGrid::setWalkable(int x, int y, bool walkable)
{
    if (!inBounds(x, y))
    {
        return;
    }
    walkable_[index(x, y)] = walkable ? 1 : 0;
}

int TileGrid::tileType(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return -1;
    }
    return types_[index(x, y)];
}

void TileGrid::setTileType(int x, int y, int type)
{
    if (!inBounds(x, y))
    {
        return;
    }
    types_[index(x, y)] = type;
}

Decor TileGrid::decor(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return Decor::None;
    }
    return static_cast<Decor>(decor_[index(x, y)]);
}

void TileGrid::setDecor(int x, int y, Decor decor)
{
    if (!inBounds(x, y))
    {
        return;
    }
    decor_[index(x, y)] = static_cast<std::uint8_t>(decor);
}

} // namespace fossynd::core
