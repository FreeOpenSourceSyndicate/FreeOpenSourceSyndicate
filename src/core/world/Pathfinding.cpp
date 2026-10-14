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
std::vector<TilePos> simplifyPath(const TileGrid& grid, const std::vector<TilePos>& path)
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

    std::size_t anchor = 0;
    while (anchor + 1 < path.size())
    {
        // Reach as far ahead as the anchor can still see, then keep that tile as
        // the next anchor. anchor+1 always qualifies, so this makes progress.
        std::size_t next = anchor + 1;
        for (std::size_t candidate = path.size() - 1; candidate > anchor; --candidate)
        {
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

std::vector<TilePos> findPath(const TileGrid& grid, TilePos start, TilePos goal, bool allowDiagonal)
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

    while (!open.empty())
    {
        const OpenNode current = open.top();
        open.pop();

        if (current.index == goalIdx)
        {
            break;
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
        return simplifyPath(grid, path);
    }
    return path;
}

} // namespace fossynd::core
