/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/math/Vec2.h"
#include "core/sim/Agent.h"
#include "core/util/Rng.h"
#include "core/world/TileGrid.h"

namespace foss::core
{

enum class MissionState
{
    Running,
    Won,
    Lost,
};

// A single tactical mission: a set of agents on a grid, an enemy AI, automatic
// weapon fire, and a win/lose condition.
//
// The whole thing is engine-agnostic and deterministic: given the same seed and
// the same sequence of player orders, `update` produces identical results. That
// makes it testable and replayable.
//
// Win condition (demo): eliminate every enemy agent.
// Lose condition: every player agent is dead.
class Mission
{
public:
    explicit Mission(const TileGrid& grid, std::uint64_t seed = 0xC0FFEEu);

    // Adds an agent and returns its index. The agent's initial think timer is
    // staggered so AI decisions do not all land on the same frame.
    std::size_t addAgent(Agent agent);

    // Issues a move order to the agent at `index` (no-op if dead or invalid).
    void orderMove(std::size_t index, TilePos goal);

    // Advances the simulation by `dt` seconds: movement, AI, weapon fire,
    // damage, and win/lose evaluation.
    void update(float dt);

    MissionState state() const { return state_; }
    const char* stateName() const;

    std::vector<Agent>& agents() { return agents_; }
    const std::vector<Agent>& agents() const { return agents_; }

    std::size_t aliveCount(Team team) const;
    int playerKills() const { return playerKills_; }
    int enemyKills() const { return enemyKills_; }

    Rng& rng() { return rng_; }

private:
    void updateAI(float dt);
    void resolveFire();
    void evaluateOutcome();

    // Index of the nearest living agent on `enemyTeam`, or -1.
    int nearestEnemy(std::size_t self, Team enemyTeam) const;

    // Straight-line distance between two agents, in tiles.
    float distanceBetween(std::size_t a, std::size_t b) const;

    const TileGrid* grid_ = nullptr;
    std::vector<Agent> agents_;
    Rng rng_;
    MissionState state_ = MissionState::Running;
    int playerKills_ = 0;
    int enemyKills_ = 0;
};

} // namespace foss::core
