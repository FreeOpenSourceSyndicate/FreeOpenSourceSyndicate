/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/sim/Agent.h"

#include <algorithm>

#include "core/world/Pathfinding.h"

namespace foss::core
{

Agent::Agent(float tileX, float tileY, float tilesPerSecond)
    : position_{tileX, tileY}
    , tilesPerSecond_(tilesPerSecond)
{
}

void Agent::setPosition(float tileX, float tileY)
{
    position_ = Vec2{tileX, tileY};
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

TilePos Agent::tile() const
{
    return TilePos{static_cast<int>(position_.x + 0.5f), static_cast<int>(position_.y + 0.5f)};
}

bool Agent::moveTo(const TileGrid& grid, TilePos goal)
{
    // Path from the agent's current tile. The first waypoint is usually the
    // tile we are standing on; skipping it avoids an immediate zero-distance
    // step.
    const TilePos start = tile();
    std::vector<TilePos> found = findPath(grid, start, goal);
    if (found.empty())
    {
        path_.clear();
        nextWaypoint_ = 0;
        return false;
    }

    path_ = std::move(found);
    nextWaypoint_ = path_.size() > 1 ? 1 : 0;
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

    fireCooldown_ = std::max(0.0f, fireCooldown_ - dt);
    hitFlash_ = std::max(0.0f, hitFlash_ - dt);
    thinkTimer_ = std::max(0.0f, thinkTimer_ - dt);

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

} // namespace foss::core
