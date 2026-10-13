/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
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
