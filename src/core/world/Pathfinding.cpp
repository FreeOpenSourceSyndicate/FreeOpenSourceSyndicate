/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/world/Pathfinding.h"

#include <algorithm>
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
    return path;
}

std::vector<TilePos> smoothPath(const TileGrid& grid, const std::vector<TilePos>& path)
{
    if (path.size() <= 2)
    {
        return path;
    }

    // Greedy string-pull: from the last kept waypoint, keep the furthest
    // waypoint still reachable by a single clear straight segment. Every
    // retained segment is therefore clear, and no waypoint is invented.
    std::vector<TilePos> result;
    result.push_back(path.front());

    std::size_t anchor = 0;
    while (anchor + 1 < path.size())
    {
        std::size_t best = anchor + 1;
        for (std::size_t i = path.size(); i-- > anchor + 2;)
        {
            if (hasLineOfSight(grid, path[anchor], path[i]))
            {
                best = i;
                break;
            }
        }
        result.push_back(path[best]);
        anchor = best;
    }

    return result;
}

std::vector<TilePos> findSmoothPath(const TileGrid& grid, TilePos start, TilePos goal, bool allowDiagonal)
{
    return smoothPath(grid, findPath(grid, start, goal, allowDiagonal));
}

} // namespace fossynd::core
