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
std::vector<TilePos> findPath(const TileGrid& grid, TilePos start, TilePos goal,
                              bool allowDiagonal = false);

// String-pulls a path: drops intermediate waypoints that a straight, clear
// segment (core/world/LineOfSight.h) can skip, so an agent walks diagonally
// across open ground instead of staircasing along the grid. The first and last
// waypoints are always kept.
//
// The result is never longer than the input and every retained segment is clear,
// so it stays walkable. Deterministic and RNG-free. An empty or single-tile path
// is returned unchanged.
std::vector<TilePos> smoothPath(const TileGrid& grid, const std::vector<TilePos>& path);

// A* followed by smoothPath — the route an agent actually follows. Uses 8-way
// A* by default so the search can find the diagonal shortcuts the smoothing
// then relies on.
std::vector<TilePos> findSmoothPath(const TileGrid& grid, TilePos start, TilePos goal,
                                    bool allowDiagonal = true);

} // namespace fossynd::core
