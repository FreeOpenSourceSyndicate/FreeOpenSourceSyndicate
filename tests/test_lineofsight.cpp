/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"

#include "core/world/LineOfSight.h"

using namespace fossynd::core;

FOSSYND_TEST(LineOfSight, SameTileSeesItself)
{
    TileGrid grid(4, 4);
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{1, 1}, TilePos{1, 1}));
}

FOSSYND_TEST(LineOfSight, AdjacentTilesSeeEachOther)
{
    TileGrid grid(4, 4);
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{1, 1}, TilePos{2, 1}));
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{1, 1}, TilePos{1, 2}));
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{1, 1}, TilePos{2, 2}));
}

FOSSYND_TEST(LineOfSight, OpenGridSeesEverywhere)
{
    TileGrid grid(6, 6);
    for (int ax = 0; ax < 6; ++ax)
    {
        for (int ay = 0; ay < 6; ++ay)
        {
            for (int bx = 0; bx < 6; ++bx)
            {
                for (int by = 0; by < 6; ++by)
                {
                    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{ax, ay}, TilePos{bx, by}));
                }
            }
        }
    }
}

FOSSYND_TEST(LineOfSight, WallOnTheLineBlocks)
{
    TileGrid grid(5, 5);
    grid.setWalkable(2, 2, false);
    FOSSYND_CHECK(!hasLineOfSight(grid, TilePos{0, 2}, TilePos{4, 2}));
    FOSSYND_CHECK(!hasLineOfSight(grid, TilePos{0, 0}, TilePos{4, 4}));
}

FOSSYND_TEST(LineOfSight, WallOffTheLineDoesNotBlock)
{
    TileGrid grid(5, 5);
    grid.setWalkable(0, 3, false); // not between the two tiles below
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{0, 0}, TilePos{4, 0}));
}

FOSSYND_TEST(LineOfSight, BlockedTargetTileIsNotVisible)
{
    TileGrid grid(5, 5);
    grid.setWalkable(4, 0, false);
    FOSSYND_CHECK(!hasLineOfSight(grid, TilePos{0, 0}, TilePos{4, 0}));
}

FOSSYND_TEST(LineOfSight, ObserverTileIsNotTested)
{
    // An agent stands on a walkable tile in practice, but the observer's own
    // tile must never be what blocks the line.
    TileGrid grid(3, 1);
    grid.setWalkable(0, 0, false);
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{0, 0}, TilePos{2, 0}));
}

FOSSYND_TEST(LineOfSight, DiagonalCornerSqueezeIsBlocked)
{
    TileGrid grid(3, 3);
    grid.setWalkable(1, 0, false);
    grid.setWalkable(0, 1, false);
    // The diagonal from (0,0) to (1,1) threads exactly between the two walls.
    FOSSYND_CHECK(!hasLineOfSight(grid, TilePos{0, 0}, TilePos{1, 1}));
}

FOSSYND_TEST(LineOfSight, SingleDiagonalWallBlocks)
{
    TileGrid grid(3, 3);
    grid.setWalkable(1, 1, false);
    // A wall on the diagonal itself is on the line.
    FOSSYND_CHECK(!hasLineOfSight(grid, TilePos{0, 0}, TilePos{2, 2}));
}

FOSSYND_TEST(LineOfSight, ClearDiagonalIsVisible)
{
    TileGrid grid(3, 3);
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{0, 0}, TilePos{2, 2}));
}

FOSSYND_TEST(LineOfSight, WallWithAGapIsVisibleThroughTheGap)
{
    TileGrid grid(5, 5);
    for (int y = 0; y <= 3; ++y)
    {
        grid.setWalkable(2, y, false); // wall at x=2, gap at y=4
    }
    FOSSYND_CHECK(!hasLineOfSight(grid, TilePos{0, 2}, TilePos{4, 2}));
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{0, 4}, TilePos{4, 4}));
}

FOSSYND_TEST(LineOfSight, IsSymmetric)
{
    TileGrid grid(6, 6);
    grid.setWalkable(3, 1, false);
    grid.setWalkable(3, 2, false);
    grid.setWalkable(2, 4, false);
    const TilePos points[] = {{0, 0}, {5, 5}, {0, 5}, {5, 0}, {3, 3}, {4, 0}, {1, 4}};
    for (const TilePos& a : points)
    {
        for (const TilePos& b : points)
        {
            FOSSYND_CHECK_EQ(hasLineOfSight(grid, a, b), hasLineOfSight(grid, b, a));
        }
    }
}
