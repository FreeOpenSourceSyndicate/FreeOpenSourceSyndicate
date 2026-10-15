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

FOSSYND_TEST(TileGrid, TilesAreNotDestructibleByDefault)
{
    TileGrid grid(3, 3);
    FOSSYND_CHECK(!grid.isDestructible(1, 1));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 0);
    // Damage on a plain tile is a no-op and never opens it.
    FOSSYND_CHECK(!grid.damageTile(1, 1, 5));
    FOSSYND_CHECK(grid.isWalkable(1, 1));
}

FOSSYND_TEST(TileGrid, DestructibleTileTakesDamageBeforeBreaking)
{
    TileGrid grid(3, 3);
    grid.setWalkable(1, 1, false);
    grid.setDecor(1, 1, Decor::Crate);
    grid.setDestructible(1, 1, true, 3);
    FOSSYND_CHECK(grid.isDestructible(1, 1));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 3);

    // Partial damage keeps the tile blocked, destructible, and decorated.
    FOSSYND_CHECK(!grid.damageTile(1, 1, 2));
    FOSSYND_CHECK(!grid.isWalkable(1, 1));
    FOSSYND_CHECK(grid.isDestructible(1, 1));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 1);
    FOSSYND_CHECK(grid.decor(1, 1) == Decor::Crate);

    // The final point breaks it: walkable, no longer destructible, decor gone.
    FOSSYND_CHECK(grid.damageTile(1, 1, 1));
    FOSSYND_CHECK(grid.isWalkable(1, 1));
    FOSSYND_CHECK(!grid.isDestructible(1, 1));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 0);
    FOSSYND_CHECK(grid.decor(1, 1) == Decor::None);

    // A second hit does nothing once it is open.
    FOSSYND_CHECK(!grid.damageTile(1, 1, 1));
}

FOSSYND_TEST(TileGrid, DestructibleDefaultsToOneHitPoint)
{
    TileGrid grid(2, 2);
    grid.setWalkable(0, 0, false);
    grid.setDestructible(0, 0, true); // no explicit hit points
    FOSSYND_CHECK_EQ(grid.tileHitPoints(0, 0), 1);
    FOSSYND_CHECK(grid.damageTile(0, 0, 1));
    FOSSYND_CHECK(grid.isWalkable(0, 0));

    // A non-positive hit-point request falls back to the default, not to 0.
    grid.setWalkable(1, 1, false);
    grid.setDestructible(1, 1, true, 0);
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 1);
}

FOSSYND_TEST(TileGrid, DamageIgnoresInvalidInput)
{
    TileGrid grid(3, 3);
    grid.setWalkable(1, 1, false);
    grid.setDestructible(1, 1, true, 5);

    FOSSYND_CHECK(!grid.damageTile(1, 1, 0));   // no damage
    FOSSYND_CHECK(!grid.damageTile(1, 1, -3));  // negative damage
    FOSSYND_CHECK_EQ(grid.tileHitPoints(1, 1), 5);
    FOSSYND_CHECK(!grid.isWalkable(1, 1));

    // Out of bounds: no crash, no effect.
    FOSSYND_CHECK(!grid.damageTile(-1, 0, 5));
    FOSSYND_CHECK(!grid.damageTile(9, 9, 5));
    FOSSYND_CHECK(!grid.isDestructible(-1, 0));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(9, 9), 0);
}

FOSSYND_TEST(TileGrid, ClearingDestructibleMarkResetsHitPoints)
{
    TileGrid grid(2, 2);
    grid.setWalkable(0, 0, false);
    grid.setDestructible(0, 0, true, 4);
    FOSSYND_CHECK_EQ(grid.tileHitPoints(0, 0), 4);

    grid.setDestructible(0, 0, false);
    FOSSYND_CHECK(!grid.isDestructible(0, 0));
    FOSSYND_CHECK_EQ(grid.tileHitPoints(0, 0), 0);
    FOSSYND_CHECK(!grid.damageTile(0, 0, 10));
    FOSSYND_CHECK(!grid.isWalkable(0, 0)); // still blocked, just not breakable
}
