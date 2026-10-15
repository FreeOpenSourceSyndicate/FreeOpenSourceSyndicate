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

// Hit points a destructible tile gets when none is given explicitly.
inline constexpr int kDefaultTileHitPoints = 1;

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

    // --- destructible tiles (issue #157) ---------------------------------
    //
    // A destructible tile is a blocked tile that weapon fire can break. When
    // its hit points reach zero it becomes walkable and any decor is cleared,
    // so `findPath`/`findPathToNearest` route through the opening on the next
    // search. Tiles are not destructible by default.

    // Marks a tile destructible (or clears the mark), giving it `hitPoints`
    // when marked. Out-of-bounds writes are ignored.
    void setDestructible(int x, int y, bool destructible,
                         int hitPoints = kDefaultTileHitPoints);
    bool isDestructible(int x, int y) const;

    // Remaining hit points of a destructible tile, else 0.
    int tileHitPoints(int x, int y) const;

    // Applies `amount` damage to a destructible tile. Returns true when this
    // call destroyed it (the tile is now walkable). A no-op returning false for
    // an out-of-bounds or non-destructible tile, or when `amount <= 0`.
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
