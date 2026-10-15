/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/world/Pathfinding.h"
#include "core/world/TileGrid.h"

using namespace fossynd::core;

FOSSYND_TEST(TileGrid, DefaultsToWalkable)
{
    TileGrid grid(4, 3);
    FOSSYND_CHECK_EQ(grid.width(), 4);
    FOSSYND_CHECK_EQ(grid.height(), 3);
    FOSSYND_CHECK(grid.isWalkable(0, 0));
    FOSSYND_CHECK(grid.isWalkable(3, 2));
}

FOSSYND_TEST(TileGrid, OutOfBoundsIsNotWalkable)
{
    TileGrid grid(4, 3);
    FOSSYND_CHECK(!grid.isWalkable(-1, 0));
    FOSSYND_CHECK(!grid.isWalkable(0, -1));
    FOSSYND_CHECK(!grid.isWalkable(4, 0));
    FOSSYND_CHECK(!grid.isWalkable(0, 3));
    FOSSYND_CHECK_EQ(grid.tileType(9, 9), -1);
}

FOSSYND_TEST(TileGrid, SetWalkableAndType)
{
    TileGrid grid(3, 3);
    grid.setWalkable(1, 1, false);
    FOSSYND_CHECK(!grid.isWalkable(1, 1));
    FOSSYND_CHECK(grid.isWalkable(1, 0));

    grid.setTileType(2, 2, 7);
    FOSSYND_CHECK_EQ(grid.tileType(2, 2), 7);

    // Out-of-bounds writes are ignored, not crashes.
    grid.setWalkable(-5, -5, false);
    grid.setTileType(100, 100, 3);
    FOSSYND_CHECK(grid.isWalkable(0, 0));
}

FOSSYND_TEST(TileGrid, DestructibleDefaultsToFalse)
{
    TileGrid grid(4, 4);
    grid.setWalkable(1, 1, false);

    FOSSYND_CHECK(!grid.isDestructible(1, 1));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 0);

    // Out-of-bounds queries are safe.
    FOSSYND_CHECK(!grid.isDestructible(-1, 0));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(99, 99), 0);

    // Damage on a non-destructible tile is a no-op: it stays blocked.
    FOSSYND_CHECK(!grid.damageTile(1, 1, 1000));
    FOSSYND_CHECK(!grid.isWalkable(1, 1));
}

FOSSYND_TEST(TileGrid, OnlyBlockedTilesCanBeDestructible)
{
    TileGrid grid(3, 3);

    // A walkable tile cannot be flagged destructible: there is no wall to open.
    grid.setDestructible(0, 0, true, 5);
    FOSSYND_CHECK(!grid.isDestructible(0, 0));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(0, 0), 0);

    // Blocking it first makes the flag stick.
    grid.setWalkable(0, 0, false);
    grid.setDestructible(0, 0, true, 5);
    FOSSYND_CHECK(grid.isDestructible(0, 0));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(0, 0), 5);

    // Clearing the flag also clears the hit points.
    grid.setDestructible(0, 0, false);
    FOSSYND_CHECK(!grid.isDestructible(0, 0));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(0, 0), 0);

    // Out-of-bounds writes are ignored, not crashes.
    grid.setDestructible(-3, -3, true, 9);
    FOSSYND_CHECK(!grid.isDestructible(-3, -3));
}

FOSSYND_TEST(TileGrid, DamageKeepsTileBlockedUntilZero)
{
    TileGrid grid(3, 3);
    grid.setWalkable(1, 1, false);
    grid.setDestructible(1, 1, true, 10);
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 10);

    // Damage below the threshold reduces HP but keeps the tile blocked.
    FOSSYND_CHECK(!grid.damageTile(1, 1, 4));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 6);
    FOSSYND_CHECK(!grid.isWalkable(1, 1));
    FOSSYND_CHECK(grid.isDestructible(1, 1));

    // Non-positive damage does nothing.
    FOSSYND_CHECK(!grid.damageTile(1, 1, 0));
    FOSSYND_CHECK(!grid.damageTile(1, 1, -5));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 6);

    // Reaching zero opens the tile, clears the flag and the decor, and reports it.
    grid.setDecor(1, 1, Decor::Crate);
    FOSSYND_CHECK(grid.damageTile(1, 1, 6));
    FOSSYND_CHECK(grid.isWalkable(1, 1));
    FOSSYND_CHECK(!grid.isDestructible(1, 1));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 0);
    FOSSYND_CHECK(grid.decor(1, 1) == Decor::None);

    // Further damage to the now-open tile is a no-op.
    FOSSYND_CHECK(!grid.damageTile(1, 1, 5));
    FOSSYND_CHECK(grid.isWalkable(1, 1));
}

FOSSYND_TEST(TileGrid, DamageBeyondZeroOpensOnce)
{
    TileGrid grid(3, 3);
    grid.setWalkable(2, 1, false);
    grid.setDestructible(2, 1, true, 3);

    // A single hit that exceeds the remaining HP still opens exactly once.
    FOSSYND_CHECK(grid.damageTile(2, 1, 100));
    FOSSYND_CHECK(grid.isWalkable(2, 1));
    FOSSYND_CHECK(!grid.damageTile(2, 1, 100));
}

FOSSYND_TEST(TileGrid, DamageOpensTheTileForPathfinding)
{
    // A wall column spans the full height, so the far side is unreachable until
    // the destructible tile in it is opened.
    TileGrid grid(5, 3);
    for (int y = 0; y < 3; ++y)
    {
        grid.setWalkable(2, y, false);
    }
    grid.setDestructible(2, 1, true, 4);

    const TilePos start{0, 1};
    const TilePos goal{4, 1};

    // The intact wall forces a detour (or no path when the board is tight).
    const std::vector<TilePos> blockedPath = findPath(grid, start, goal);
    FOSSYND_CHECK(blockedPath.empty() || blockedPath.size() > 5);

    // Open the middle of the wall; the direct route through it is now shortest.
    FOSSYND_CHECK(grid.damageTile(2, 1, 4));
    const std::vector<TilePos> openPath = findPath(grid, start, goal);
    FOSSYND_CHECK(!openPath.empty());
    if (!openPath.empty())
    {
        FOSSYND_CHECK_EQ(openPath.front().x, start.x);
        FOSSYND_CHECK_EQ(openPath.front().y, start.y);
        FOSSYND_CHECK_EQ(openPath.back().x, goal.x);
        FOSSYND_CHECK_EQ(openPath.back().y, goal.y);

        // The route actually crosses the breached tile.
        bool crosses = false;
        for (const TilePos& tile : openPath)
        {
            if (tile.x == 2 && tile.y == 1)
            {
                crosses = true;
            }
        }
        FOSSYND_CHECK(crosses);
    }
}
