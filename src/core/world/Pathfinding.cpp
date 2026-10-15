/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/world/Pathfinding.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <queue>

#include "core/world/LineOfSight.h"

namespace fossynd::core
{
namespace
{

struct OpenNode
{
    int index = 0;
    float f = 0.0f;
};

struct GreaterF
{
    bool operator()(const OpenNode& a, const OpenNode& b) const { return a.f > b.f; }
};

float distanceBetween(TilePos a, TilePos b)
{
    const float dx = static_cast<float>(b.x - a.x);
    const float dy = static_cast<float>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

// String-pulling: drop intermediate waypoints that the last kept waypoint can
// see past, so the agent walks straight between corners instead of staircasing
// across the grid. A shortcut is taken only when it actually shortens the route
// (strictly shorter than the run it replaces), so genuine staircase detours
// collapse while an already-straight run — including a pure diagonal — is left
// untouched. Endpoints are never removed, so the route still ends on the goal,
// and every retained segment is clear (checked with `hasLineOfSight`).
//
// Work is bounded by `maxProbes` `hasLineOfSight` calls — the same budget the A*
// search ran under. The rescan is otherwise unbounded by the node cap: it can
// re-test the whole tail for every anchor, so its work grows with the square of
// the waypoint count (on a 60x60 serpentine, ~1829 waypoints cost ~1.6M probes
// and ~54 ms — far more than the search that produced the path), which would
// stall a frame on the target hardware (ADR-0008). When the budget runs out the
// rest of the path is kept verbatim, so the route is the raw A* path: always
// walkable, just less smoothed. A fixed probe count, never a time budget, so the
// result stays deterministic. A realistic order spends far fewer probes than the
// budget, so its smoothing is unchanged.
std::vector<TilePos> simplifyPath(const TileGrid& grid, const std::vector<TilePos>& path,
                                  int maxProbes)
{
    if (path.size() < 3)
    {
        return path;
    }

    // Cumulative length up to each waypoint, so a run's length is O(1).
    std::vector<float> cumulative(path.size(), 0.0f);
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        cumulative[i] = cumulative[i - 1] + distanceBetween(path[i - 1], path[i]);
    }

    std::vector<TilePos> simplified;
    simplified.reserve(path.size());
    simplified.push_back(path.front());

    int probes = 0;
    std::size_t anchor = 0;
    while (anchor + 1 < path.size())
    {
        if (probes >= maxProbes)
        {
            // Budget spent: append the raw A* tail so the route still ends on the
            // goal, and stop scanning.
            for (std::size_t i = anchor + 1; i < path.size(); ++i)
            {
                simplified.push_back(path[i]);
            }
            break;
        }

        // Reach as far ahead as the anchor can still see, then keep that tile as
        // the next anchor. anchor+1 always qualifies, so this makes progress.
        std::size_t next = anchor + 1;
        for (std::size_t candidate = path.size() - 1; candidate > anchor; --candidate)
        {
            if (probes >= maxProbes)
            {
                break;
            }
            ++probes;
            if (!hasLineOfSight(grid, path[anchor], path[candidate]))
            {
                continue;
            }
            const float direct = distanceBetween(path[anchor], path[candidate]);
            const float run = cumulative[candidate] - cumulative[anchor];
            if (direct < run - 1e-4f)
            {
                next = candidate;
                break;
            }
        }
        simplified.push_back(path[next]);
        anchor = next;
    }
    return simplified;
}

constexpr float kStraightCost = 1.0f;
constexpr float kDiagonalCost = 1.41421356f;

// `cameFrom` marker for a tile the BFS never reached. `-1` means "reached, and
// is the start" (no predecessor); any other value is the predecessor index.
constexpr int kUnvisited = -2;

// Breadth-first flood of the walkable tiles reachable from `start`, honouring
// the same corner rule as `findPath`: a diagonal step is refused when either
// orthogonal neighbour is blocked, so a route can never squeeze through a
// one-tile corner. This is the single walk shared by `findPathToNearest` (which
// picks a nearest tile from the result) and `reachableTiles` (which returns the
// whole set), so both agree on exactly which tiles are reachable.
//
// Returns a row-major `cameFrom` array: `kUnvisited` for unreached tiles, `-1`
// for `start`, else the predecessor index. The scan expands at most
// `maxExpandedNodes` tiles (ADR-0008) — a fixed count, never a time budget, so
// the result is deterministic and RNG-free. Callers must pass an in-bounds,
// walkable `start` (the public entry points check this first).
std::vector<int> bfsReachable(const TileGrid& grid, TilePos start, bool allowDiagonal,
                              int maxExpandedNodes)
{
    const int w = grid.width();
    const int count = w * grid.height();
    const auto idx = [w](int x, int y) { return y * w + x; };

    std::vector<int> cameFrom(static_cast<std::size_t>(count), kUnvisited);
    std::queue<int> frontier;
    const int startIdx = idx(start.x, start.y);
    cameFrom[static_cast<std::size_t>(startIdx)] = -1;
    frontier.push(startIdx);

    const int dirs4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    const int dirs8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    int expanded = 0;
    while (!frontier.empty())
    {
        if (++expanded > maxExpandedNodes)
        {
            break; // best-so-far wins; the scan stays bounded (ADR-0008)
        }
        const int current = frontier.front();
        frontier.pop();
        const int cx = current % w;
        const int cy = current / w;

        const int dirCount = allowDiagonal ? 8 : 4;
        for (int d = 0; d < dirCount; ++d)
        {
            const int* dir = allowDiagonal ? dirs8[d] : dirs4[d];
            const int nx = cx + dir[0];
            const int ny = cy + dir[1];

            if (!grid.inBounds(nx, ny) || !grid.isWalkable(nx, ny))
            {
                continue;
            }
            const bool diagonal = dir[0] != 0 && dir[1] != 0;
            if (diagonal && (!grid.isWalkable(cx + dir[0], cy) || !grid.isWalkable(cx, cy + dir[1])))
            {
                continue; // do not cut corners through blocked tiles
            }

            const int nIdx = idx(nx, ny);
            if (cameFrom[static_cast<std::size_t>(nIdx)] != kUnvisited)
            {
                continue;
            }
            cameFrom[static_cast<std::size_t>(nIdx)] = current;
            frontier.push(nIdx);
        }
    }
    return cameFrom;
}

float heuristic(int dx, int dy, bool allowDiagonal)
{
    const float ax = static_cast<float>(std::abs(dx));
    const float ay = static_cast<float>(std::abs(dy));
    if (!allowDiagonal)
    {
        return ax + ay;
    }
    // Octile distance.
    return (ax + ay) + (kDiagonalCost - 2.0f) * std::min(ax, ay);
}

} // namespace

std::vector<TilePos> findPath(const TileGrid& grid, TilePos start, TilePos goal, bool allowDiagonal,
                              int maxExpandedNodes)
{
    return findPathDetailed(grid, start, goal, allowDiagonal, maxExpandedNodes).path;
}

PathResult findPathDetailed(const TileGrid& grid, TilePos start, TilePos goal, bool allowDiagonal,
                            int maxExpandedNodes)
{
    if (!grid.inBounds(start.x, start.y) || !grid.inBounds(goal.x, goal.y))
    {
        return {};
    }
    if (!grid.isWalkable(goal.x, goal.y))
    {
        return {};
    }

    const int w = grid.width();
    const int h = grid.height();
    const int count = w * h;
    const auto idx = [w](int x, int y) { return y * w + x; };

    std::vector<float> gScore(static_cast<std::size_t>(count), -1.0f);
    std::vector<int> cameFrom(static_cast<std::size_t>(count), -1);
    std::priority_queue<OpenNode, std::vector<OpenNode>, GreaterF> open;

    const int startIdx = idx(start.x, start.y);
    const int goalIdx = idx(goal.x, goal.y);

    gScore[static_cast<std::size_t>(startIdx)] = 0.0f;
    open.push(OpenNode{startIdx, heuristic(goal.x - start.x, goal.y - start.y, allowDiagonal)});

    const int dirs4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    const int dirs8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    int expanded = 0;
    while (!open.empty())
    {
        const OpenNode current = open.top();
        open.pop();

        if (current.index == goalIdx)
        {
            break;
        }

        // Bound the search: once the cap is hit with the goal still unpopped,
        // give up rather than expand the rest of the grid. A fixed node count,
        // not a time budget, so the outcome stays deterministic.
        if (++expanded > maxExpandedNodes)
        {
            PathResult result;
            result.outcome = PathOutcome::CapExceeded;
            return result;
        }

        const int cx = current.index % w;
        const int cy = current.index / w;

        const int dirCount = allowDiagonal ? 8 : 4;
        for (int d = 0; d < dirCount; ++d)
        {
            const int* dir = allowDiagonal ? dirs8[d] : dirs4[d];
            const int nx = cx + dir[0];
            const int ny = cy + dir[1];

            if (!grid.inBounds(nx, ny) || !grid.isWalkable(nx, ny))
            {
                continue;
            }

            const bool diagonal = dir[0] != 0 && dir[1] != 0;
            if (diagonal && (!grid.isWalkable(cx + dir[0], cy) || !grid.isWalkable(cx, cy + dir[1])))
            {
                continue; // do not cut corners through blocked tiles
            }

            const float step = diagonal ? kDiagonalCost : kStraightCost;
            const int nIdx = idx(nx, ny);
            const float tentative = gScore[static_cast<std::size_t>(current.index)] + step;

            if (gScore[static_cast<std::size_t>(nIdx)] < 0.0f || tentative < gScore[static_cast<std::size_t>(nIdx)])
            {
                gScore[static_cast<std::size_t>(nIdx)] = tentative;
                cameFrom[static_cast<std::size_t>(nIdx)] = current.index;
                const float f = tentative + heuristic(goal.x - nx, goal.y - ny, allowDiagonal);
                open.push(OpenNode{nIdx, f});
            }
        }
    }

    if (startIdx != goalIdx && cameFrom[static_cast<std::size_t>(goalIdx)] < 0)
    {
        // No route: either the goal is unreachable, or the open set drained
        // without ever reaching it. Both are "no path exists".
        return {};
    }

    std::vector<TilePos> path;
    for (int at = goalIdx; at >= 0; at = cameFrom[static_cast<std::size_t>(at)])
    {
        path.push_back(TilePos{at % w, at / w});
        if (at == startIdx)
        {
            break;
        }
    }
    std::reverse(path.begin(), path.end());

    // 4-way paths are already minimal and string-pulling cannot improve them,
    // so only the diagonal case (which produces staircases) is smoothed.
    if (allowDiagonal)
    {
        path = simplifyPath(grid, path, maxExpandedNodes);
    }

    PathResult result;
    result.outcome = PathOutcome::Found;
    result.path = std::move(path);
    return result;
}

PathResult findPathToNearest(const TileGrid& grid, TilePos start, TilePos goal, bool allowDiagonal,
                             int maxExpandedNodes)
{
    // Fast path: when the goal is genuinely reachable, this is exactly findPath.
    const PathResult direct = findPathDetailed(grid, start, goal, allowDiagonal, maxExpandedNodes);
    if (direct.outcome == PathOutcome::Found)
    {
        return direct;
    }

    // Nowhere to go: `start` must be a walkable in-bounds tile to move at all.
    if (!grid.inBounds(start.x, start.y) || !grid.isWalkable(start.x, start.y))
    {
        return {};
    }

    const int w = grid.width();
    const auto idx = [w](int x, int y) { return y * w + x; };

    // Flood the reachable region once, then pick the tile nearest the goal.
    const std::vector<int> cameFrom = bfsReachable(grid, start, allowDiagonal, maxExpandedNodes);

    const auto manhattanToGoal = [goal](int x, int y) {
        return std::abs(x - goal.x) + std::abs(y - goal.y);
    };

    // Best reachable tile other than `start`: nearest to the goal, ties broken by
    // the lowest tile index so the choice is deterministic.
    int bestIdx = -1;
    int bestDistance = 0;
    const int count = w * grid.height();
    for (int i = 0; i < count; ++i)
    {
        if (cameFrom[static_cast<std::size_t>(i)] == kUnvisited || i == idx(start.x, start.y))
        {
            continue;
        }
        const int distance = manhattanToGoal(i % w, i / w);
        if (bestIdx < 0 || distance < bestDistance ||
            (distance == bestDistance && i < bestIdx))
        {
            bestIdx = i;
            bestDistance = distance;
        }
    }

    // Nothing but `start` is reachable, so there is nowhere to move.
    if (bestIdx < 0)
    {
        return {};
    }

    std::vector<TilePos> path;
    for (int at = bestIdx; at >= 0; at = cameFrom[static_cast<std::size_t>(at)])
    {
        path.push_back(TilePos{at % w, at / w});
    }
    std::reverse(path.begin(), path.end());

    if (allowDiagonal)
    {
        path = simplifyPath(grid, path, maxExpandedNodes);
    }

    PathResult result;
    result.outcome = PathOutcome::Found;
    result.path = std::move(path);
    return result;
}

std::vector<TilePos> reachableTiles(const TileGrid& grid, TilePos start, bool allowDiagonal,
                                    int maxExpandedNodes)
{
    if (!grid.inBounds(start.x, start.y) || !grid.isWalkable(start.x, start.y))
    {
        return {};
    }

    const int w = grid.width();
    const int count = w * grid.height();
    const std::vector<int> cameFrom = bfsReachable(grid, start, allowDiagonal, maxExpandedNodes);

    // Row-major order is the natural order of `cameFrom`, and it is stable, so the
    // returned set needs no sort and compares cheaply.
    std::vector<TilePos> tiles;
    for (int i = 0; i < count; ++i)
    {
        if (cameFrom[static_cast<std::size_t>(i)] != kUnvisited)
        {
            tiles.push_back(TilePos{i % w, i / w});
        }
    }
    return tiles;
}

bool areConnected(const TileGrid& grid, TilePos a, TilePos b, bool allowDiagonal,
                  int maxExpandedNodes)
{
    if (!grid.inBounds(a.x, a.y) || !grid.isWalkable(a.x, a.y) || !grid.inBounds(b.x, b.y) ||
        !grid.isWalkable(b.x, b.y))
    {
        return false;
    }

    const int w = grid.width();
    const int bIdx = b.y * w + b.x;
    const std::vector<int> cameFrom = bfsReachable(grid, a, allowDiagonal, maxExpandedNodes);
    return cameFrom[static_cast<std::size_t>(bIdx)] != kUnvisited;
}

} // namespace fossynd::core
