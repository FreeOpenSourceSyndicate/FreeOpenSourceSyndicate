/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <vector>

#include "core/math/Vec2.h"
#include "core/world/TileGrid.h"

namespace fossynd::core
{

// The default A* node-expansion cap. Large enough that no realistic order
// approaches it — a 24x24 board has 576 tiles, so the cap is roughly a 3.5x
// margin — yet small enough to bound worst-case work to a fraction of a frame on
// the target hardware (ADR-0008). Override per call for tests or a larger map.
inline constexpr int kDefaultMaxExpandedNodes = 2048;

// The outcome of a path search. `findPath` collapses all three to an empty
// vector; `findPathDetailed` distinguishes them so a rejected order can say
// *why* it failed — "no route exists" is a level-design fact, while "the node
// cap was hit" means a route probably exists but the search was abandoned to
// bound frame time (issue #141).
enum class PathOutcome
{
    Found,       // a path was returned (path is non-empty)
    NoPath,      // no route exists: goal unreachable, out of bounds, or blocked
    CapExceeded, // the node cap was hit before the goal was popped
};

// The full result of a search: the outcome and the path (empty unless Found).
struct PathResult
{
    PathOutcome outcome = PathOutcome::NoPath;
    std::vector<TilePos> path;

    bool found() const { return outcome == PathOutcome::Found; }
};

// A* over a TileGrid with 4-way movement and Manhattan heuristic.
//
// Returns the path from `start` to `goal` inclusive. Returns an empty vector
// when no path exists. If `start == goal`, returns a single-element path.
//
// `allowDiagonal` enables 8-way movement; diagonal steps are only taken when
// both orthogonal neighbours are walkable, so agents never cut corners through
// walls.
//
// With `allowDiagonal`, the raw A* path is post-processed by string-pulling:
// waypoints are dropped when a straight, unobstructed line from the last kept
// waypoint both reaches further and covers that run in less distance, so the
// agent walks straight instead of staircasing. A shortcut is only taken when it
// is strictly shorter, so an already-straight run (including a pure diagonal) is
// left untouched. The endpoints are always kept and every retained segment is
// clear (verified with `hasLineOfSight`), so the route stays walkable. The 4-way
// path is already minimal and is returned unsmoothed. Deterministic, RNG-free.
//
// The search is bounded by `maxExpandedNodes`: when that many nodes have been
// expanded and the goal is still unpopped, the search gives up and returns an
// empty path rather than expanding the rest of the grid. This keeps a
// pathological or very large request from stalling a frame on old hardware
// (ADR-0008). The cap only ever turns a would-be path into "no path"; when it is
// not reached the result is identical to an unbounded search. Determinism is
// preserved — the cap is a fixed node count, not a time budget.
//
// This is a thin wrapper over `findPathDetailed`; use that when the caller needs
// to tell "no path exists" from "the cap was hit".
std::vector<TilePos> findPath(const TileGrid& grid, TilePos start, TilePos goal,
                              bool allowDiagonal = false,
                              int maxExpandedNodes = kDefaultMaxExpandedNodes);

// As `findPath`, but reports the outcome. `path` is non-empty exactly when
// `outcome == PathOutcome::Found`; the two failure outcomes both leave it empty,
// so existing callers see no behaviour change.
PathResult findPathDetailed(const TileGrid& grid, TilePos start, TilePos goal,
                            bool allowDiagonal = false,
                            int maxExpandedNodes = kDefaultMaxExpandedNodes);

// "Walk as close as you can" variant (issue #146): when `goal` is reachable this
// is exactly `findPathDetailed`. When it is not — blocked, out of bounds, or
// abandoned by the node cap — the result is instead a path to the walkable tile
// reachable from `start` that is nearest `goal`, so a click on an unreachable
// tile still moves the squad toward it rather than being dropped.
//
// The nearest tile is chosen by Manhattan distance to `goal`, ties broken by the
// lowest tile index so the choice is deterministic. `start` itself is not a
// candidate: a fallback only happens when `goal` could not be reached, and the
// caller wants the squad to *move*, not to be told to stand still. `NoPath` is
// therefore returned only when there is genuinely nowhere to go — `start` is not
// a walkable in-bounds tile, or nothing else is reachable from it.
//
// The breadth-first scan that finds the nearest tile is bounded by the same
// `maxExpandedNodes` budget as `findPath` (ADR-0008): if the cap is hit the best
// tile found so far wins, so the result stays cheap and deterministic. The path
// to the chosen tile is smoothed exactly like `findPathDetailed` when
// `allowDiagonal` is set.
PathResult findPathToNearest(const TileGrid& grid, TilePos start, TilePos goal,
                             bool allowDiagonal = false,
                             int maxExpandedNodes = kDefaultMaxExpandedNodes);

} // namespace fossynd::core
