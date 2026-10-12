/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <vector>

#include "core/math/Vec2.h"
#include "core/world/TileGrid.h"

namespace foss::core
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

} // namespace foss::core
