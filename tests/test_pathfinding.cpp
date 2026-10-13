/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/world/Pathfinding.h"

using namespace fossynd::core;

namespace
{
void blockColumn(TileGrid& grid, int x, int fromY, int toY)
{
    for (int y = fromY; y <= toY; ++y)
    {
        grid.setWalkable(x, y, false);
    }
}
} // namespace

FOSSYND_TEST(Pathfinding, StraightLineOnOpenGrid)
{
    TileGrid grid(5, 5);
    const auto path = findPath(grid, TilePos{0, 0}, TilePos{4, 0});
    FOSSYND_CHECK_EQ(static_cast<int>(path.size()), 5);
    FOSSYND_CHECK(path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(path.back() == (TilePos{4, 0}));
}

FOSSYND_TEST(Pathfinding, StartEqualsGoalIsSingleTile)
{
    TileGrid grid(5, 5);
    const auto path = findPath(grid, TilePos{2, 2}, TilePos{2, 2});
    FOSSYND_CHECK_EQ(static_cast<int>(path.size()), 1);
    FOSSYND_CHECK(path.front() == (TilePos{2, 2}));
}

FOSSYND_TEST(Pathfinding, RoutesAroundWall)
{
    TileGrid grid(5, 5);
    blockColumn(grid, 2, 0, 3); // wall from y=0..3, gap at y=4

    const auto path = findPath(grid, TilePos{0, 0}, TilePos{4, 0});
    FOSSYND_CHECK(!path.empty());
    FOSSYND_CHECK(path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(path.back() == (TilePos{4, 0}));

    // No step may land on a blocked tile.
    for (const auto& step : path)
    {
        FOSSYND_CHECK(grid.isWalkable(step.x, step.y));
    }
    // The path must be forced down to the gap row.
    bool usedGap = false;
    for (const auto& step : path)
    {
        if (step.x == 2 && step.y == 4)
        {
            usedGap = true;
        }
    }
    FOSSYND_CHECK(usedGap);
}

FOSSYND_TEST(Pathfinding, UnreachableGoalReturnsEmpty)
{
    TileGrid grid(5, 5);
    blockColumn(grid, 2, 0, 4); // full wall, no gap

    const auto path = findPath(grid, TilePos{0, 0}, TilePos{4, 0});
    FOSSYND_CHECK(path.empty());
}

FOSSYND_TEST(Pathfinding, BlockedGoalReturnsEmpty)
{
    TileGrid grid(5, 5);
    grid.setWalkable(4, 4, false);
    const auto path = findPath(grid, TilePos{0, 0}, TilePos{4, 4});
    FOSSYND_CHECK(path.empty());
}

FOSSYND_TEST(Pathfinding, OutOfBoundsReturnsEmpty)
{
    TileGrid grid(5, 5);
    FOSSYND_CHECK(findPath(grid, TilePos{-1, 0}, TilePos{4, 4}).empty());
    FOSSYND_CHECK(findPath(grid, TilePos{0, 0}, TilePos{9, 9}).empty());
}

FOSSYND_TEST(Pathfinding, DiagonalDoesNotCutCorners)
{
    TileGrid grid(3, 3);
    grid.setWalkable(1, 0, false);
    grid.setWalkable(0, 1, false);

    // From (0,0) to (1,1) diagonally: both orthogonal neighbours are blocked,
    // so the direct diagonal must not be taken.
    const auto path = findPath(grid, TilePos{0, 0}, TilePos{1, 1}, /*allowDiagonal=*/true);
    FOSSYND_CHECK(path.empty());
}

FOSSYND_TEST(Pathfinding, DiagonalTakesShortcutWhenClear)
{
    TileGrid grid(4, 4);
    const auto path = findPath(grid, TilePos{0, 0}, TilePos{3, 3}, /*allowDiagonal=*/true);
    FOSSYND_CHECK_EQ(static_cast<int>(path.size()), 4); // 3 diagonal steps + start
    FOSSYND_CHECK(path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(path.back() == (TilePos{3, 3}));
}
