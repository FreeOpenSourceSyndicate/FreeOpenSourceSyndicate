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
std::vector<TilePos> findPath(const TileGrid& grid, TilePos start, TilePos goal,
                              bool allowDiagonal = false,
                              int maxExpandedNodes = kDefaultMaxExpandedNodes);

} // namespace fossynd::core
