/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/world/LineOfSight.h"
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

namespace
{
// Every consecutive pair of waypoints must have a clear straight line.
void checkEverySegmentClear(const TileGrid& grid, const std::vector<TilePos>& path)
{
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        FOSSYND_CHECK(hasLineOfSight(grid, path[i - 1], path[i]));
        FOSSYND_CHECK(grid.isWalkable(path[i].x, path[i].y));
    }
}
} // namespace

FOSSYND_TEST(Pathfinding, SmoothCollapsesStaircaseToEndpoints)
{
    // 4-way A* to a diagonal goal staircases (one step per axis). On open ground
    // string-pulling must collapse it to just the two endpoints.
    TileGrid grid(8, 8);
    const auto raw = findPath(grid, TilePos{0, 0}, TilePos{5, 5}, /*allowDiagonal=*/false);
    FOSSYND_CHECK_EQ(static_cast<int>(raw.size()), 11); // 10 axis steps + start

    const auto smooth = smoothPath(grid, raw);
    FOSSYND_CHECK_EQ(static_cast<int>(smooth.size()), 2);
    FOSSYND_CHECK(smooth.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(smooth.back() == (TilePos{5, 5}));
    checkEverySegmentClear(grid, smooth);
}

FOSSYND_TEST(Pathfinding, SmoothKeepsDetourAroundWall)
{
    // A wall with a gap forces a real detour; smoothing may shorten the straight
    // legs but must keep the corner that rounds the wall.
    TileGrid grid(7, 7);
    blockColumn(grid, 3, 0, 4); // wall at x=3, y=0..4, gap at y=5..6

    const auto raw = findPath(grid, TilePos{0, 0}, TilePos{6, 0}, /*allowDiagonal=*/true);
    const auto smooth = smoothPath(grid, raw);
    FOSSYND_CHECK(!smooth.empty());
    FOSSYND_CHECK(smooth.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(smooth.back() == (TilePos{6, 0}));
    FOSSYND_CHECK(smooth.size() <= raw.size());
    checkEverySegmentClear(grid, smooth);
    // The route must still dip down to the gap row past the wall: a straight
    // line along y=0 would cross the blocked column, so some waypoint sits at
    // y >= 5.
    bool dipped = false;
    for (const auto& step : smooth)
    {
        if (step.y >= 5)
        {
            dipped = true;
        }
    }
    FOSSYND_CHECK(dipped);
}

FOSSYND_TEST(Pathfinding, SmoothNeverLengthensAndStaysClear)
{
    // Property check on an open grid: smoothing only ever drops waypoints.
    TileGrid grid(10, 10);
    const auto raw = findPath(grid, TilePos{1, 1}, TilePos{8, 6}, /*allowDiagonal=*/false);
    const auto smooth = smoothPath(grid, raw);
    FOSSYND_CHECK(smooth.size() <= raw.size());
    FOSSYND_CHECK(smooth.front() == raw.front());
    FOSSYND_CHECK(smooth.back() == raw.back());
    checkEverySegmentClear(grid, smooth);
}

FOSSYND_TEST(Pathfinding, SmoothIsIdempotentAndHandlesShortPaths)
{
    TileGrid grid(8, 8);
    const auto raw = findPath(grid, TilePos{0, 0}, TilePos{5, 3}, /*allowDiagonal=*/false);
    const auto once = smoothPath(grid, raw);
    const auto twice = smoothPath(grid, once);
    FOSSYND_CHECK(once == twice); // smoothing a smooth path changes nothing

    FOSSYND_CHECK(smoothPath(grid, {}).empty());
    FOSSYND_CHECK_EQ(static_cast<int>(smoothPath(grid, {TilePos{2, 2}}).size()), 1);
    const std::vector<TilePos> two{TilePos{0, 0}, TilePos{1, 0}};
    FOSSYND_CHECK(smoothPath(grid, two) == two);
}

FOSSYND_TEST(Pathfinding, FindSmoothPathShortcutsOpenGround)
{
    // The one-call helper must return a directly-walkable diagonal route.
    TileGrid grid(8, 8);
    const auto path = findSmoothPath(grid, TilePos{0, 0}, TilePos{6, 6});
    FOSSYND_CHECK_EQ(static_cast<int>(path.size()), 2);
    FOSSYND_CHECK(path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(path.back() == (TilePos{6, 6}));
    checkEverySegmentClear(grid, path);
}
