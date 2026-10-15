/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/world/LineOfSight.h"
#include "core/world/Pathfinding.h"
#include <algorithm>


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

FOSSYND_TEST(Pathfinding, NearestMatchesFindPathWhenGoalReachable)
{
    // With a reachable goal, findPathToNearest must be exactly findPathDetailed —
    // same outcome, same path — so adopting it never changes a normal move.
    TileGrid grid(7, 7);
    for (int y = 0; y < 7; ++y)
    {
        if (y != 3)
        {
            grid.setWalkable(3, y, false);
        }
    }

    const PathResult direct = findPathDetailed(grid, TilePos{0, 0}, TilePos{6, 0}, true);
    const PathResult nearest = findPathToNearest(grid, TilePos{0, 0}, TilePos{6, 0}, true);
    FOSSYND_CHECK(nearest.outcome == PathOutcome::Found);
    FOSSYND_CHECK(nearest.path == direct.path);
}

FOSSYND_TEST(Pathfinding, NearestStopsAtTileBesideBlockedGoal)
{
    // A wall down the middle with one gap: a goal inside the wall is unwalkable,
    // so the squad should stop on the walkable tile nearest it — the one just
    // before the wall — not drop the order.
    TileGrid grid(9, 5);
    for (int y = 0; y < 5; ++y)
    {
        grid.setWalkable(4, y, false);
    }

    // Goal is a blocked tile in the wall; nearest reachable tiles are (3, 2) and
    // (5, 2). Tie broken by lowest tile index -> (3, 2).
    const PathResult result =
        findPathToNearest(grid, TilePos{0, 2}, TilePos{4, 2}, /*allowDiagonal=*/false);
    FOSSYND_CHECK(result.outcome == PathOutcome::Found);
    FOSSYND_CHECK(!result.path.empty());
    FOSSYND_CHECK(result.path.front() == (TilePos{0, 2}));
    FOSSYND_CHECK(result.path.back() == (TilePos{3, 2}));
}

FOSSYND_TEST(Pathfinding, NearestFallsBackOnUnreachableGoal)
{
    // A goal sealed off from `start`: the fallback walks as close as it can —
    // the reachable tile adjacent to the barrier on the start's side.
    TileGrid grid(6, 3);
    for (int y = 0; y < 3; ++y)
    {
        grid.setWalkable(3, y, false); // full wall, no gap
    }

    const PathResult result =
        findPathToNearest(grid, TilePos{0, 1}, TilePos{5, 1}, /*allowDiagonal=*/false);
    FOSSYND_CHECK(result.outcome == PathOutcome::Found);
    FOSSYND_CHECK(!result.path.empty());
    FOSSYND_CHECK(result.path.front() == (TilePos{0, 1}));
    FOSSYND_CHECK(result.path.back() == (TilePos{2, 1}));
    // Every step is walkable, i.e. the route never enters the wall.
    for (const TilePos& tile : result.path)
    {
        FOSSYND_CHECK(grid.isWalkable(tile.x, tile.y));
    }
}

FOSSYND_TEST(Pathfinding, NearestReturnsNoPathWhenNowhereToGo)
{
    // `start` sealed in by walls (or out of bounds): there is genuinely nowhere to
    // move, so this is NoPath rather than a degenerate single-tile path.
    TileGrid grid(5, 5);
    grid.setWalkable(1, 0, false);
    grid.setWalkable(0, 1, false);
    grid.setWalkable(1, 1, false);

    const PathResult enclosed =
        findPathToNearest(grid, TilePos{0, 0}, TilePos{4, 4}, /*allowDiagonal=*/false);
    FOSSYND_CHECK(enclosed.outcome == PathOutcome::NoPath);
    FOSSYND_CHECK(enclosed.path.empty());

    // An out-of-bounds start cannot move anywhere either.
    FOSSYND_CHECK(findPathToNearest(grid, TilePos{-1, 0}, TilePos{4, 4}).outcome ==
                  PathOutcome::NoPath);
    FOSSYND_CHECK(findPathToNearest(grid, TilePos{0, 0}, TilePos{9, 9}).outcome ==
                  PathOutcome::NoPath);
}

FOSSYND_TEST(Pathfinding, NearestNeverReturnsStartWhenGoalUnreachable)
{
    // The fallback exists so the squad *moves*; returning just `start` would read
    // as "you are already there". With an open board it must pick a real step
    // toward an unreachable (out-of-bounds) goal.
    TileGrid grid(5, 5);
    const PathResult result =
        findPathToNearest(grid, TilePos{2, 2}, TilePos{99, 99}, /*allowDiagonal=*/false);
    FOSSYND_CHECK(result.outcome == PathOutcome::Found);
    FOSSYND_CHECK(result.path.size() >= 2);
    FOSSYND_CHECK(result.path.front() == (TilePos{2, 2}));
    FOSSYND_CHECK(result.path.back() != (TilePos{2, 2}));
}

FOSSYND_TEST(Pathfinding, NearestIsDeterministic)
{
    // Ties and the capped scan must resolve the same way every call.
    TileGrid grid(20, 20);
    for (int y = 0; y < 20; ++y)
    {
        grid.setWalkable(10, y, false);
    }

    const PathResult first = findPathToNearest(grid, TilePos{0, 0}, TilePos{19, 19}, false);
    const PathResult second = findPathToNearest(grid, TilePos{0, 0}, TilePos{19, 19}, false);
    FOSSYND_CHECK(first.outcome == second.outcome);
    FOSSYND_CHECK(first.path == second.path);
}

FOSSYND_TEST(Pathfinding, DestroyingATileOpensTheRoute)
{
    // A full wall separates start from goal, so there is no path. Marking one
    // wall tile destructible and breaking it must let findPath cross (#157).
    TileGrid grid(5, 5);
    blockColumn(grid, 2, 0, 4);

    FOSSYND_CHECK(
        findPathDetailed(grid, TilePos{0, 2}, TilePos{4, 2}, false).outcome == PathOutcome::NoPath);

    grid.setDestructible(2, 2, true, 2);
    FOSSYND_CHECK(!grid.damageTile(2, 2, 1)); // still standing after one hit
    FOSSYND_CHECK(
        findPathDetailed(grid, TilePos{0, 2}, TilePos{4, 2}, false).outcome == PathOutcome::NoPath);

    FOSSYND_CHECK(grid.damageTile(2, 2, 1)); // broken
    const PathResult after = findPathDetailed(grid, TilePos{0, 2}, TilePos{4, 2}, false);
    FOSSYND_CHECK(after.outcome == PathOutcome::Found);
    // Guard the size before indexing so a regression fails cleanly.
    FOSSYND_CHECK(!after.path.empty());
    if (after.path.empty())
    {
        return;
    }
    FOSSYND_CHECK(after.path.front() == (TilePos{0, 2}));
    FOSSYND_CHECK(after.path.back() == (TilePos{4, 2}));
}

FOSSYND_TEST(Reachability, OpenGridReachesEveryTile)
{
    TileGrid grid(4, 3);
    const std::vector<TilePos> tiles = reachableTiles(grid, TilePos{0, 0});
    FOSSYND_CHECK_EQ(static_cast<int>(tiles.size()), 12);
    // Row-major, start included, and every tile is the one it says it is.
    FOSSYND_CHECK(tiles.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(tiles.back() == (TilePos{3, 2}));
    for (const TilePos& t : tiles)
    {
        FOSSYND_CHECK(grid.isWalkable(t.x, t.y));
    }
}

FOSSYND_TEST(Reachability, WallSealsOffARegion)
{
    TileGrid grid(5, 5);
    blockColumn(grid, 2, 0, 4); // a full wall: the right side is sealed off

    const std::vector<TilePos> tiles = reachableTiles(grid, TilePos{0, 0});
    // Only the 2 columns left of the wall (2x5) are reachable.
    FOSSYND_CHECK_EQ(static_cast<int>(tiles.size()), 10);
    for (const TilePos& t : tiles)
    {
        FOSSYND_CHECK(t.x < 2);
    }

    FOSSYND_CHECK(areConnected(grid, TilePos{0, 0}, TilePos{1, 4}));
    FOSSYND_CHECK(!areConnected(grid, TilePos{0, 0}, TilePos{4, 4}));
}

FOSSYND_TEST(Reachability, SealedObjectiveIsNotConnected)
{
    // A room walled off except for one tile that is itself blocked: the objective
    // inside cannot be reached, which is what mission validation must detect.
    TileGrid grid(5, 5);
    blockColumn(grid, 2, 0, 4);
    grid.setWalkable(2, 2, true); // doorway...
    grid.setWalkable(3, 2, false); // ...but the tile just past it is blocked

    FOSSYND_CHECK(!areConnected(grid, TilePos{0, 2}, TilePos{4, 2}));
    FOSSYND_CHECK(!areConnected(grid, TilePos{0, 2}, TilePos{3, 2}));

    // Reopening the blocked tile connects them, proving the query tracks the grid.
    grid.setWalkable(3, 2, true);
    FOSSYND_CHECK(areConnected(grid, TilePos{0, 2}, TilePos{4, 2}));
}

FOSSYND_TEST(Reachability, DiagonalCannotCutACorner)
{
    // Two blocked tiles meeting at a corner: a diagonal squeeze between them must
    // not count as connected, matching findPath's corner rule.
    TileGrid grid(3, 3);
    grid.setWalkable(1, 0, false);
    grid.setWalkable(0, 1, false);

    FOSSYND_CHECK(!areConnected(grid, TilePos{0, 0}, TilePos{1, 1}, /*allowDiagonal=*/true));

    // With the corner open, the diagonal move is allowed.
    grid.setWalkable(1, 0, true);
    FOSSYND_CHECK(areConnected(grid, TilePos{0, 0}, TilePos{1, 1}, /*allowDiagonal=*/true));
}

FOSSYND_TEST(Reachability, NodeCapBoundsTheScan)
{
    // A 1-tile-wide corridor: the flood must walk it one tile per expansion, so a
    // small budget reaches only the near tiles and a full budget reaches the end.
    TileGrid grid(10, 1);
    const int small = static_cast<int>(reachableTiles(grid, TilePos{0, 0}, false, 3).size());
    const int full = static_cast<int>(reachableTiles(grid, TilePos{0, 0}, false, 100).size());
    FOSSYND_CHECK(small < full);
    FOSSYND_CHECK_EQ(full, 10);
    // The budget only ever shrinks the set; it never invents tiles.
    FOSSYND_CHECK(small >= 1);
}

FOSSYND_TEST(Reachability, StartNotWalkableIsEmpty)
{
    TileGrid grid(5, 5);
    grid.setWalkable(2, 2, false);

    FOSSYND_CHECK(reachableTiles(grid, TilePos{2, 2}).empty());
    FOSSYND_CHECK(reachableTiles(grid, TilePos{-1, 0}).empty());
    FOSSYND_CHECK(reachableTiles(grid, TilePos{99, 99}).empty());

    FOSSYND_CHECK(!areConnected(grid, TilePos{2, 2}, TilePos{0, 0}));
    FOSSYND_CHECK(!areConnected(grid, TilePos{0, 0}, TilePos{2, 2}));
    FOSSYND_CHECK(!areConnected(grid, TilePos{-1, 0}, TilePos{0, 0}));
}

FOSSYND_TEST(Reachability, MatchesFindPath)
{
    // The reachable set must be exactly the tiles findPath can produce a route to:
    // a tile is reachable iff a path to it is Found.
    TileGrid grid(8, 6);
    blockColumn(grid, 4, 0, 3); // wall with a gap at y=4 and y=5
    grid.setWalkable(2, 2, false); // a lone obstacle inside the left region

    const std::vector<TilePos> tiles = reachableTiles(grid, TilePos{0, 0});
    for (int y = 0; y < 6; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            const bool inSet = std::find(tiles.begin(), tiles.end(), (TilePos{x, y})) != tiles.end();
            const bool pathFound =
                findPathDetailed(grid, TilePos{0, 0}, TilePos{x, y}, false).outcome ==
                PathOutcome::Found;
            FOSSYND_CHECK_EQ(inSet, pathFound);
        }
    }
}

FOSSYND_TEST(Reachability, IsDeterministic)
{
    TileGrid grid(12, 12);
    for (int y = 0; y < 12; ++y)
    {
        grid.setWalkable(6, y, false);
    }
    grid.setWalkable(6, 7, true); // one gap

    const std::vector<TilePos> first = reachableTiles(grid, TilePos{0, 0});
    const std::vector<TilePos> second = reachableTiles(grid, TilePos{0, 0});
    FOSSYND_CHECK(first == second);
}
