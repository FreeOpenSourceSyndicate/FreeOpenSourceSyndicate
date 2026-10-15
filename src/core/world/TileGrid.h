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

    // The hit points a destructible tile gets when none is given.
    static constexpr int kDefaultTileHitPoints = 20;

    // Destructible tiles (roadmap: "Destructible environment"). A blocked tile
    // may be flagged destructible and given hit points; damage reduces them and,
    // at zero, flips the tile walkable (and clears its decor), so a later path
    // search routes through the opening. Default: not destructible, so existing
    // grids are unchanged. Only blocked tiles can be destructible — a walkable
    // tile has nothing to open, so `setDestructible` on one is a no-op.
    bool isDestructible(int x, int y) const;
    void setDestructible(int x, int y, bool destructible, int hitPoints = kDefaultTileHitPoints);
    int tileHitPoints(int x, int y) const;

    // Applies `amount` damage to a destructible tile. Non-destructible,
    // walkable, and out-of-bounds tiles are safe no-ops. Non-positive `amount`
    // does nothing. When hit points reach zero the tile becomes walkable and its
    // decor is cleared. Returns true if this call opened the tile.
    bool damageTile(int x, int y, int amount);

private:
    std::size_t index(int x, int y) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<int> types_;
    std::vector<std::uint8_t> walkable_;
    std::vector<std::uint8_t> decor_;
    std::vector<std::uint8_t> destructible_;
    std::vector<int> hitPoints_;
};

} // namespace fossynd::core
