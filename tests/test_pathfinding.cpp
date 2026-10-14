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

// Every consecutive pair on the route must have a clear line, i.e. no segment
// crosses a blocked tile. This is the invariant string-pulling must preserve.
bool everySegmentClear(const TileGrid& grid, const std::vector<TilePos>& path)
{
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        if (!hasLineOfSight(grid, path[i - 1], path[i]))
        {
            return false;
        }
    }
    return true;
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

FOSSYND_TEST(Pathfinding, StringPullingStraightensStaircase)
{
    TileGrid grid(6, 6);

    // A shallow slope: A* with diagonal movement can only staircase here (the
    // run is not 45 degrees), so a two-point result is only possible if the
    // post-processing collapsed the intermediate waypoints.
    const auto path = findPath(grid, TilePos{0, 0}, TilePos{4, 1}, /*allowDiagonal=*/true);
    FOSSYND_CHECK_EQ(static_cast<int>(path.size()), 2);
    FOSSYND_CHECK(path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(path.back() == (TilePos{4, 1}));
    FOSSYND_CHECK(everySegmentClear(grid, path));
}

FOSSYND_TEST(Pathfinding, StringPullingKeepsStraightDiagonalUnchanged)
{
    TileGrid grid(6, 6);

    // An exact 45-degree run is already a straight line: smoothing must not
    // insert or drop waypoints (a shortcut would be the same length).
    const auto path = findPath(grid, TilePos{0, 0}, TilePos{4, 4}, /*allowDiagonal=*/true);
    FOSSYND_CHECK_EQ(static_cast<int>(path.size()), 5);
    FOSSYND_CHECK(path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(path.back() == (TilePos{4, 4}));
}

FOSSYND_TEST(Pathfinding, StringPullingRespectsWalls)
{
    TileGrid grid(7, 7);
    // Vertical wall at x=3 with a single gap at y=3, so the route must funnel
    // through the gap.
    for (int y = 0; y < 7; ++y)
    {
        if (y != 3)
        {
            grid.setWalkable(3, y, false);
        }
    }

    const auto path = findPath(grid, TilePos{0, 0}, TilePos{6, 0}, /*allowDiagonal=*/true);
    FOSSYND_CHECK(!path.empty());
    FOSSYND_CHECK(path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(path.back() == (TilePos{6, 0}));
    FOSSYND_CHECK(everySegmentClear(grid, path));

    // The shortcut must not jump the wall: the route still passes the gap.
    bool usedGap = false;
    for (const auto& step : path)
    {
        FOSSYND_CHECK(grid.isWalkable(step.x, step.y));
        if (step == (TilePos{3, 3}))
        {
            usedGap = true;
        }
    }
    FOSSYND_CHECK(usedGap);
}

FOSSYND_TEST(Pathfinding, StringPullingShorterOrEqualThanRaw)
{
    TileGrid grid(6, 6);

    // Smoothing never lengthens the route: the diagonal path must not be longer
    // than the 4-way path over the same grid, which is the unsmoothed baseline.
    const auto smoothed = findPath(grid, TilePos{0, 0}, TilePos{4, 1}, /*allowDiagonal=*/true);
    const auto fourWay = findPath(grid, TilePos{0, 0}, TilePos{4, 1}, /*allowDiagonal=*/false);
    FOSSYND_CHECK(smoothed.size() <= fourWay.size());
    FOSSYND_CHECK(everySegmentClear(grid, smoothed));
}

FOSSYND_TEST(Pathfinding, NodeCapBoundsWorstCaseSearch)
{
    // 100x100 with a wall at x=50 and a single gap on the bottom row: the
    // optimal route detours far, and the Manhattan heuristic ignores the wall, so
    // A* must expand several thousand nodes before it can pop the goal. That
    // exceeds the default cap (issue #121).
    TileGrid grid(100, 100);
    for (int y = 0; y < 100; ++y)
    {
        if (y != 99)
        {
            grid.setWalkable(50, y, false);
        }
    }

    // Reachable: a generous cap finds the path, so an empty result below is the
    // cap at work, not an unreachable goal.
    const auto generous = findPath(grid, TilePos{0, 0}, TilePos{99, 0}, /*allowDiagonal=*/false,
                                   /*maxExpandedNodes=*/1000000);
    FOSSYND_CHECK(!generous.empty());
    FOSSYND_CHECK(generous.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(generous.back() == (TilePos{99, 0}));

    // The shipped default bounds the same request to "no path" instead of
    // expanding the rest of the grid.
    FOSSYND_CHECK(findPath(grid, TilePos{0, 0}, TilePos{99, 0}, /*allowDiagonal=*/false).empty());

    // A tiny cap bounds it harder still, and never hangs.
    FOSSYND_CHECK(findPath(grid, TilePos{0, 0}, TilePos{99, 0}, false, /*maxExpandedNodes=*/10).empty());
}

FOSSYND_TEST(Pathfinding, NodeCapIsDeterministic)
{
    // The cap is a fixed node count, not a time budget, so a bounded search
    // returns the same thing every time.
    TileGrid grid(100, 100);
    for (int y = 0; y < 100; ++y)
    {
        if (y != 99)
        {
            grid.setWalkable(50, y, false);
        }
    }

    const auto first = findPath(grid, TilePos{0, 0}, TilePos{99, 0}, false, 100);
    const auto second = findPath(grid, TilePos{0, 0}, TilePos{99, 0}, false, 100);
    FOSSYND_CHECK(first.empty());
    FOSSYND_CHECK(first == second);
}

FOSSYND_TEST(Pathfinding, NodeCapLeavesUnderCapResultsUnchanged)
{
    // When the cap is not reached the result is identical to an unbounded search:
    // the bound only ever turns a would-be path into "no path", never reshapes a
    // path it completes.
    TileGrid grid(7, 7);
    for (int y = 0; y < 7; ++y)
    {
        if (y != 3)
        {
            grid.setWalkable(3, y, false);
        }
    }

    const auto withDefault = findPath(grid, TilePos{0, 0}, TilePos{6, 0}, /*allowDiagonal=*/true);
    const auto withHugeCap =
        findPath(grid, TilePos{0, 0}, TilePos{6, 0}, /*allowDiagonal=*/true, 1000000);
    FOSSYND_CHECK(!withDefault.empty());
    FOSSYND_CHECK(withDefault == withHugeCap);
}

FOSSYND_TEST(Pathfinding, NodeCapZeroStillResolvesStartEqualsGoal)
{
    // The goal check runs before the cap, so a degenerate request is resolved
    // even under a zero budget.
    TileGrid grid(5, 5);
    const auto path = findPath(grid, TilePos{2, 2}, TilePos{2, 2}, /*allowDiagonal=*/false,
                               /*maxExpandedNodes=*/0);
    FOSSYND_CHECK_EQ(static_cast<int>(path.size()), 1);
    FOSSYND_CHECK(path.front() == (TilePos{2, 2}));
}

FOSSYND_TEST(Pathfinding, DetailedReportsFound)
{
    // A reachable goal under the cap is "Found", and the path is non-empty and
    // ends on the goal.
    TileGrid grid(6, 6);
    const PathResult result = findPathDetailed(grid, TilePos{0, 0}, TilePos{4, 0});
    FOSSYND_CHECK(result.outcome == PathOutcome::Found);
    FOSSYND_CHECK(result.found());
    FOSSYND_CHECK(!result.path.empty());
    FOSSYND_CHECK(result.path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(result.path.back() == (TilePos{4, 0}));
}

FOSSYND_TEST(Pathfinding, DetailedDistinguishesNoPathFromCapExceeded)
{
    // The whole point of issue #141: an unreachable goal and a capped search must
    // not read the same. Build a grid where both happen for the same endpoints —
    // reachable with a generous cap, unreachable with a wall sealing the goal.
    TileGrid grid(100, 100);
    for (int y = 0; y < 100; ++y)
    {
        if (y != 99)
        {
            grid.setWalkable(50, y, false);
        }
    }

    // Reachable but expensive: the default cap is hit first -> CapExceeded.
    const PathResult capped = findPathDetailed(grid, TilePos{0, 0}, TilePos{99, 0}, false);
    FOSSYND_CHECK(capped.outcome == PathOutcome::CapExceeded);
    FOSSYND_CHECK(capped.path.empty());

    // Same endpoints, but with the last gap sealed the goal is truly
    // unreachable -> NoPath, not CapExceeded.
    TileGrid sealed = grid;
    sealed.setWalkable(50, 99, false);
    const PathResult noRoute =
        findPathDetailed(sealed, TilePos{0, 0}, TilePos{99, 0}, false, /*maxExpandedNodes=*/1000000);
    FOSSYND_CHECK(noRoute.outcome == PathOutcome::NoPath);
    FOSSYND_CHECK(noRoute.path.empty());
}

FOSSYND_TEST(Pathfinding, DetailedNoPathForOutOfBoundsAndBlockedGoal)
{
    // Out-of-bounds and blocked-goal requests are "NoPath", never "CapExceeded" —
    // the cap cannot be the reason a search never started.
    TileGrid grid(5, 5);
    FOSSYND_CHECK(findPathDetailed(grid, TilePos{-1, 0}, TilePos{4, 4}).outcome ==
                  PathOutcome::NoPath);
    FOSSYND_CHECK(findPathDetailed(grid, TilePos{0, 0}, TilePos{9, 9}).outcome ==
                  PathOutcome::NoPath);

    grid.setWalkable(4, 4, false);
    FOSSYND_CHECK(findPathDetailed(grid, TilePos{0, 0}, TilePos{4, 4}).outcome ==
                  PathOutcome::NoPath);
}

FOSSYND_TEST(Pathfinding, DetailedCapZeroResolvesStartEqualsGoalAsFound)
{
    // start == goal is resolved before the cap, so it is "Found" even at a zero
    // budget — the cap is not what stopped a search that never needed to run.
    TileGrid grid(5, 5);
    const PathResult result = findPathDetailed(grid, TilePos{2, 2}, TilePos{2, 2}, false, 0);
    FOSSYND_CHECK(result.outcome == PathOutcome::Found);
    FOSSYND_CHECK_EQ(static_cast<int>(result.path.size()), 1);
}

FOSSYND_TEST(Pathfinding, DetailedPathMatchesFindPath)
{
    // The thin wrapper must not change the returned path: findPath ==
    // findPathDetailed(...).path for a found route and for each failure mode.
    TileGrid grid(7, 7);
    for (int y = 0; y < 7; ++y)
    {
        if (y != 3)
        {
            grid.setWalkable(3, y, false);
        }
    }

    FOSSYND_CHECK(findPath(grid, TilePos{0, 0}, TilePos{6, 0}, true) ==
                  findPathDetailed(grid, TilePos{0, 0}, TilePos{6, 0}, true).path);
    FOSSYND_CHECK(findPath(grid, TilePos{0, 0}, TilePos{9, 9}).empty());
    FOSSYND_CHECK(findPathDetailed(grid, TilePos{0, 0}, TilePos{9, 9}).path.empty());
}
