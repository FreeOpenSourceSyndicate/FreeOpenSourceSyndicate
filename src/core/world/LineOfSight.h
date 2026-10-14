/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdlib>

#include "core/math/Vec2.h"
#include "core/world/TileGrid.h"

namespace fossynd::core
{

// True when a straight line between two tiles crosses no blocked tile.
//
// Used for sight and fire: without it, combatants would see and shoot through
// walls. Deterministic and RNG-free, so it never perturbs a mission replay.
//
// The walk is a "supercover" line: it visits every tile the segment touches,
// not just the tiles a plain Bresenham line lands on, so a one-tile-thick wall
// cannot slip between two sample points.
//
// The observer's tile (`from`) is not tested — agents stand on walkable tiles —
// but every other tile on the segment, including the target's, is. A diagonal
// step that squeezes exactly between two blocked orthogonal neighbours is
// blocked too, so a sight line cannot thread a one-tile corner.
inline bool hasLineOfSight(const TileGrid& grid, TilePos from, TilePos to)
{
    int x = from.x;
    int y = from.y;
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    const int nx = std::abs(dx);
    const int ny = std::abs(dy);
    const int signX = (dx > 0) - (dx < 0);
    const int signY = (dy > 0) - (dy < 0);

    int ix = 0;
    int iy = 0;
    while (ix < nx || iy < ny)
    {
        if ((1 + 2 * ix) * ny == (1 + 2 * iy) * nx)
        {
            // The segment passes exactly through the shared corner of four
            // tiles, so it touches both orthogonal neighbours as well as the
            // diagonal one: any blocked tile among them blocks the sight line.
            // This is what stops an agent peeking through a one-tile corner.
            if (!grid.isWalkable(x + signX, y) || !grid.isWalkable(x, y + signY) ||
                !grid.isWalkable(x + signX, y + signY))
            {
                return false;
            }
            x += signX;
            y += signY;
            ++ix;
            ++iy;
        }
        else if ((1 + 2 * ix) * ny < (1 + 2 * iy) * nx)
        {
            x += signX;
            ++ix;
        }
        else
        {
            y += signY;
            ++iy;
        }

        if (!grid.isWalkable(x, y))
        {
            return false;
        }
    }
    return true;
}

} // namespace fossynd::core
