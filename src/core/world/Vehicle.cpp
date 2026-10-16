/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/world/Vehicle.h"

#include <algorithm>

#include "core/world/Pathfinding.h"

namespace fossynd::core
{

Vehicle::Vehicle(VehicleKind kind, float tileX, float tileY, float tilesPerSecond)
    : kind_(kind)
    , position_{tileX, tileY}
    , previousPosition_{tileX, tileY}
    , tilesPerSecond_(tilesPerSecond)
{
}

const char* Vehicle::kindName() const
{
    return kind_ == VehicleKind::Train ? "Train" : "Car";
}

void Vehicle::setPosition(float tileX, float tileY)
{
    position_ = Vec2{tileX, tileY};
    // A teleport (spawn, restart) must not interpolate across the map, so the
    // previous position snaps to the new one.
    previousPosition_ = position_;
}

Vec2 Vehicle::interpolatedPosition(float alpha) const
{
    const float t = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
    return previousPosition_ + (position_ - previousPosition_) * t;
}

TilePos Vehicle::tile() const
{
    return TilePos{static_cast<int>(position_.x + 0.5f), static_cast<int>(position_.y + 0.5f)};
}

bool Vehicle::driveTo(const TileGrid& grid, TilePos goal)
{
    // Same search as `Agent::moveTo`: 8-way A* + string-pulling, falling back to
    // the nearest reachable tile when the goal cannot be reached, so a drive
    // order to an unreachable tile still moves the vehicle toward it. As in
    // `Agent::moveTo`, the first waypoint (the vehicle's own tile centre) is kept
    // so an off-centre vehicle cannot clip a wall corner its path never crossed.
    const TilePos start = tile();
    const PathResult found = findPathToNearest(grid, start, goal, /*allowDiagonal=*/true);
    if (!found.found())
    {
        path_.clear();
        nextWaypoint_ = 0;
        return false;
    }

    // Same route as the agent (see `Agent::moveTo`): re-issuing an unchanged route
    // keeps the current progress instead of restarting at the tile centre, so a
    // per-tick AI re-issue cannot oscillate the vehicle back to the centre.
    const bool sameRoute = path_ == found.path;
    path_ = found.path;
    if (!sameRoute)
    {
        nextWaypoint_ = 0;
    }
    return true;
}

void Vehicle::stop()
{
    path_.clear();
    nextWaypoint_ = 0;
}

void Vehicle::update(float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }

    // Remember where we started this step so the renderer can interpolate
    // between fixed steps without the simulation reading the blend.
    previousPosition_ = position_;

    fireCooldown_ = std::max(0.0f, fireCooldown_ - dt);

    // A wreck does not move.
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

bool Vehicle::hijack(int agentIndex)
{
    if (agentIndex < 0 || isDestroyed())
    {
        return false;
    }
    // One driver at a time: a second agent cannot take a car that is already
    // being driven. Re-hijacking with the same index is a harmless no-op.
    if (isOccupied() && driver_ != agentIndex)
    {
        return false;
    }
    driver_ = agentIndex;
    return true;
}

void Vehicle::setMaxHealth(float maxHealth)
{
    maxHealth_ = std::max(1.0f, maxHealth);
    health_ = maxHealth_;
}

float Vehicle::healthFraction() const
{
    if (maxHealth_ <= 0.0f)
    {
        return 0.0f;
    }
    return std::max(0.0f, std::min(1.0f, health_ / maxHealth_));
}

float Vehicle::applyDamage(float amount)
{
    if (amount <= 0.0f || !isAlive())
    {
        return 0.0f;
    }
    const float dealt = std::min(amount, health_);
    health_ -= dealt;
    if (!isAlive())
    {
        // A wreck cannot be driven or fired from, so stop it and put the driver
        // out. Mission decides what happens to the agent itself.
        stop();
        driver_ = -1;
    }
    return dealt;
}

Vehicle makeCar(float tileX, float tileY)
{
    Vehicle car(VehicleKind::Car, tileX, tileY, kCarSpeed);
    car.setName("Car");
    car.setMaxHealth(kCarMaxHealth);
    car.setMountedWeapon(makeUzi());
    return car;
}

Vehicle makeTrain(float tileX, float tileY)
{
    Vehicle train(VehicleKind::Train, tileX, tileY, kTrainSpeed);
    train.setName("Train");
    train.setMaxHealth(kTrainMaxHealth);
    train.setMountedWeapon(makeRifle());
    return train;
}

} // namespace fossynd::core
