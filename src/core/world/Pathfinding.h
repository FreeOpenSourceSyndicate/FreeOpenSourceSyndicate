/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <vector>

#include "core/math/Vec2.h"
#include "core/world/TileGrid.h"

namespace fossynd::core
{

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
std::vector<TilePos> findPath(const TileGrid& grid, TilePos start, TilePos goal,
                              bool allowDiagonal = false);

} // namespace fossynd::core
