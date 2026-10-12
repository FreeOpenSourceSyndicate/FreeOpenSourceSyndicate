/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <vector>

#include "core/math/Vec2.h"
#include "core/world/TileGrid.h"

namespace foss::core
{

// A squad member: a cyborg agent that moves along a path at a fixed speed.
//
// Engine-agnostic. Positions are in tile space (fractional), so the renderer
// converts them to screen coordinates. Movement is driven by a fixed timestep.
class Agent
{
public:
    Agent() = default;
    Agent(float tileX, float tileY, float tilesPerSecond);

    // Replaces the current path with one computed by A* to `goal`. If no path
    // exists, the agent stops and the path is cleared. Returns true if a path
    // was found.
    bool moveTo(const TileGrid& grid, TilePos goal);

    // Advances the agent along its path. `dt` is in seconds.
    void update(float dt);

    bool hasPath() const { return !path_.empty(); }
    const std::vector<TilePos>& path() const { return path_; }

    Vec2 position() const { return position_; }
    void setPosition(float tileX, float tileY);

    float speed() const { return tilesPerSecond_; }
    void setSpeed(float tilesPerSecond) { tilesPerSecond_ = tilesPerSecond; }

private:
    Vec2 position_;
    float tilesPerSecond_ = 2.0f;
    std::vector<TilePos> path_;
    std::size_t nextWaypoint_ = 0;
};

} // namespace foss::core
