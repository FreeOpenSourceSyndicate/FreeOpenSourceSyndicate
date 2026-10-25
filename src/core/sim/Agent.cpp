/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/sim/Agent.h"

#include <algorithm>

#include "core/world/Pathfinding.h"

namespace fossynd::core
{

Agent::Agent(float tileX, float tileY, float tilesPerSecond)
    : position_{tileX, tileY}
    , previousPosition_{tileX, tileY}
    , tilesPerSecond_(tilesPerSecond)
{
}

void Agent::setPosition(float tileX, float tileY)
{
    position_ = Vec2{tileX, tileY};
    // A teleport (spawn, restart) must not interpolate across the map, so the
    // previous position snaps to the new one.
    previousPosition_ = position_;
}

Vec2 Agent::interpolatedPosition(float alpha) const
{
    const float t = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
    return previousPosition_ + (position_ - previousPosition_) * t;
}

void Agent::setMaxHealth(float maxHealth)
{
    maxHealth_ = std::max(1.0f, maxHealth);
    health_ = maxHealth_;
}

float Agent::healthFraction() const
{
    if (maxHealth_ <= 0.0f)
    {
        return 0.0f;
    }
    return std::max(0.0f, std::min(1.0f, health_ / maxHealth_));
}

void Agent::setMaxHealthPreservingFraction(float maxHealth)
{
    const float fraction = healthFraction();
    maxHealth_ = std::max(1.0f, maxHealth);
    health_ = std::max(0.0f, std::min(maxHealth_, maxHealth_ * fraction));
}

float Agent::applyDamage(float amount)
{
    if (amount <= 0.0f || !isAlive())
    {
        return 0.0f;
    }
    const float dealt = std::min(amount, health_);
    health_ -= dealt;
    flashHit();
    return dealt;
}

float Agent::heal(float amount)
{
    if (amount <= 0.0f || !isAlive())
    {
        return 0.0f;
    }
    const float healed = std::min(amount, maxHealth_ - health_);
    health_ += healed;
    return healed;
}

TilePos Agent::tile() const
{
    return TilePos{static_cast<int>(position_.x + 0.5f), static_cast<int>(position_.y + 0.5f)};
}

bool Agent::moveTo(const TileGrid& grid, TilePos goal)
{
    // Path from the agent's current tile.
    //
    // The first waypoint is the agent's own tile centre and it is deliberately
    // *kept* (nextWaypoint_ = 0 the first time the route is set). A path is
    // validated centre-to-centre, but the agent physically stands at a fractional
    // position, so walking straight at the second waypoint can cut the corner of a
    // tile the centre line never touched — through a wall it was never allowed to
    // pass. Routing through the tile centre first makes every walked segment a
    // validated centre-to-centre segment, so the agent can never end up on a
    // blocked tile.
    //
    // Re-issuing the *same* route while the agent is mid-segment must not send it
    // back to that centre, though: the AI re-paths every tick while retreating
    // (`Mission::stepAwayFrom`), so resetting `nextWaypoint_` each time would
    // yank the agent back to the centre and it would oscillate in place instead of
    // retreating. Only a *changed* route (a new goal, or a new start tile after
    // the agent has crossed into one) restarts from the centre.
    //
    // 8-way A* + string-pulling (see findPathToNearest): diagonal movement lets
    // the search find shortcuts, and the smoothing collapses the remaining
    // staircases, so the agent walks straight instead of zig-zagging.
    //
    // `findPathToNearest` is exactly `findPath` when `goal` is reachable, so a
    // reachable order is unchanged. When it is not — blocked, out of bounds, or
    // abandoned by the node cap — the agent walks to the nearest reachable tile
    // instead of dropping the order, so a click on an unreachable tile still
    // moves the squad toward it (#149). It returns NoPath only when there is
    // genuinely nowhere to go (an enclosed or out-of-bounds start).
    const TilePos start = tile();
    const PathResult found = findPathToNearest(grid, start, goal, /*allowDiagonal=*/true);
    if (!found.found())
    {
        path_.clear();
        nextWaypoint_ = 0;
        return false;
    }

    const bool sameRoute = path_ == found.path;
    path_ = found.path;
    if (!sameRoute)
    {
        nextWaypoint_ = 0;
    }
    return true;
}

void Agent::stop()
{
    path_.clear();
    nextWaypoint_ = 0;
}

void Agent::update(float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }

    // Remember where we started this step so the renderer can interpolate
    // between fixed steps without the simulation reading the blend.
    previousPosition_ = position_;

    fireCooldown_ = std::max(0.0f, fireCooldown_ - dt);
    gunfireTimer_ = std::max(0.0f, gunfireTimer_ - dt);
    hitFlash_ = std::max(0.0f, hitFlash_ - dt);
    thinkTimer_ = std::max(0.0f, thinkTimer_ - dt);
    persuadeCooldown_ = std::max(0.0f, persuadeCooldown_ - dt);

    if (!isAlive() || nextWaypoint_ >= path_.size())
    {
        return;
    }

    float remaining = tilesPerSecond_ * dt;
    while (remaining > 0.0f && nextWaypoint_ < path_.size())
    {
        const Vec2 target{static_cast<float>(path_[nextWaypoint_].x),
                          static_cast<float>(path_[nextWaypoint_].y)};
        const Vec2 delta = target - position_;
        const float distance = length(delta);

        if (distance <= remaining)
        {
            position_ = target;
            remaining -= distance;
            ++nextWaypoint_;
        }
        else
        {
            position_ = position_ + normalized(delta) * remaining;
            remaining = 0.0f;
        }
    }

    if (nextWaypoint_ >= path_.size())
    {
        path_.clear();
        nextWaypoint_ = 0;
    }
}

} // namespace fossynd::core
