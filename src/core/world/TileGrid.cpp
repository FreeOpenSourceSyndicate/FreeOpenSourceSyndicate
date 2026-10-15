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
    destructible_.assign(count, 0);
    rubble_.assign(count, 0);
    hitPoints_.assign(count, 0);
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

void TileGrid::setDestructible(int x, int y, bool destructible, int hitPoints)
{
    if (!inBounds(x, y))
    {
        return;
    }
    const std::size_t i = index(x, y);
    destructible_[i] = destructible ? 1 : 0;
    hitPoints_[i] = destructible ? (hitPoints > 0 ? hitPoints : kDefaultTileHitPoints) : 0;
    if (destructible)
    {
        // (Re)authoring an intact destructible wall clears any earlier breach, so
        // a rebuilt wall is not still drawn as rubble.
        rubble_[i] = 0;
    }
}

bool TileGrid::isDestructible(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return false;
    }
    return destructible_[index(x, y)] != 0;
}

int TileGrid::tileHitPoints(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return 0;
    }
    return hitPoints_[index(x, y)];
}

bool TileGrid::damageTile(int x, int y, int amount)
{
    if (!inBounds(x, y) || amount <= 0 || destructible_[index(x, y)] == 0)
    {
        return false;
    }
    const std::size_t i = index(x, y);
    hitPoints_[i] -= amount;
    if (hitPoints_[i] > 0)
    {
        return false;
    }
    // Destroyed: open the tile so pathfinding and line of sight pass through,
    // and drop the decor that was sitting on it. The breach is remembered so the
    // view draws rubble rather than a plain floor tile (issue #217).
    hitPoints_[i] = 0;
    destructible_[i] = 0;
    walkable_[i] = 1;
    rubble_[i] = 1;
    ++breachEpoch_;
    decor_[i] = static_cast<std::uint8_t>(Decor::None);
    return true;
}

bool TileGrid::isRubble(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return false;
    }
    return rubble_[index(x, y)] != 0;
}

} // namespace fossynd::core
