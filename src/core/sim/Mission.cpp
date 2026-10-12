/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/sim/Mission.h"

#include <cmath>

namespace foss::core
{
namespace
{

// How often an AI agent re-evaluates its target and path.
constexpr float kThinkInterval = 0.4f;

// Distance (in tiles) at which an enemy is considered "in sight" for chasing.
constexpr float kSightRange = 12.0f;

// Accuracy multiplier applied at maximum range; accuracy falls linearly from
// full at point-blank to this fraction at max range.
constexpr float kAccuracyAtMaxRange = 0.35f;

} // namespace

Mission::Mission(const TileGrid& grid, std::uint64_t seed)
    : grid_(&grid)
    , rng_(seed)
{
}

std::size_t Mission::addAgent(Agent agent)
{
    // Stagger the first think so AI decisions spread across frames.
    if (agent.team() == Team::Enemy)
    {
        agent.setThinkTimer(agent.speed() * 0.05f + 0.1f);
    }
    agents_.push_back(std::move(agent));
    return agents_.size() - 1;
}

void Mission::orderMove(std::size_t index, TilePos goal)
{
    if (index >= agents_.size())
    {
        return;
    }
    Agent& agent = agents_[index];
    if (!agent.isAlive())
    {
        return;
    }
    if (grid_ != nullptr)
    {
        agent.moveTo(*grid_, goal);
    }
}

std::size_t Mission::aliveCount(Team team) const
{
    std::size_t count = 0;
    for (const Agent& agent : agents_)
    {
        if (agent.isAlive() && agent.team() == team)
        {
            ++count;
        }
    }
    return count;
}

float Mission::distanceBetween(std::size_t a, std::size_t b) const
{
    const Vec2 delta = agents_[a].position() - agents_[b].position();
    return length(delta);
}

int Mission::nearestEnemy(std::size_t self, Team enemyTeam) const
{
    int best = -1;
    float bestDistance = 0.0f;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (i == self || !agents_[i].isAlive() || agents_[i].team() != enemyTeam)
        {
            continue;
        }
        const float distance = distanceBetween(self, i);
        if (best == -1 || distance < bestDistance)
        {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    return best;
}

void Mission::update(float dt)
{
    if (state_ != MissionState::Running || dt <= 0.0f)
    {
        return;
    }

    for (Agent& agent : agents_)
    {
        agent.update(dt);
    }

    updateAI(dt);
    resolveFire();
    evaluateOutcome();
}

void Mission::updateAI(float dt)
{
    (void)dt;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        Agent& agent = agents_[i];
        if (!agent.isAlive() || agent.team() != Team::Enemy)
        {
            continue;
        }
        if (agent.thinkTimer() > 0.0f)
        {
            continue;
        }
        agent.setThinkTimer(kThinkInterval);

        const int target = nearestEnemy(i, Team::Player);
        if (target < 0 || grid_ == nullptr)
        {
            continue;
        }

        const float distance = distanceBetween(i, static_cast<std::size_t>(target));
        if (distance > kSightRange)
        {
            agent.stop();
            continue;
        }

        const float weaponRange = agent.weapon().range;
        if (distance > weaponRange * 0.9f)
        {
            // Close to within weapon range: step toward the target.
            agent.moveTo(*grid_, agents_[static_cast<std::size_t>(target)].tile());
        }
        else
        {
            // In range: hold position and fire.
            agent.stop();
        }
    }
}

void Mission::resolveFire()
{
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        Agent& attacker = agents_[i];
        if (!attacker.isAlive() || !attacker.canFire())
        {
            continue;
        }

        const Team enemyTeam = attacker.team() == Team::Player ? Team::Enemy : Team::Player;
        const int target = nearestEnemy(i, enemyTeam);
        if (target < 0)
        {
            continue;
        }

        const float distance = distanceBetween(i, static_cast<std::size_t>(target));
        const float range = attacker.weapon().range;
        if (distance > range)
        {
            continue;
        }

        attacker.beginFireCooldown();

        // Accuracy falls off linearly with distance; nearer shots hit more.
        const float falloff = 1.0f - (distance / range);
        const float hitChance =
            attacker.weapon().accuracy * (kAccuracyAtMaxRange + (1.0f - kAccuracyAtMaxRange) * falloff);
        if (!rng_.chance(hitChance))
        {
            continue;
        }

        Agent& victim = agents_[static_cast<std::size_t>(target)];
        victim.applyDamage(attacker.weapon().damage);
        if (!victim.isAlive())
        {
            if (victim.team() == Team::Enemy)
            {
                ++playerKills_;
            }
            else
            {
                ++enemyKills_;
            }
        }
    }
}

void Mission::evaluateOutcome()
{
    if (state_ != MissionState::Running)
    {
        return;
    }
    if (aliveCount(Team::Enemy) == 0)
    {
        state_ = MissionState::Won;
    }
    else if (aliveCount(Team::Player) == 0)
    {
        state_ = MissionState::Lost;
    }
}

const char* Mission::stateName() const
{
    switch (state_)
    {
    case MissionState::Running:
        return "IN PROGRESS";
    case MissionState::Won:
        return "MISSION COMPLETE";
    case MissionState::Lost:
        return "SQUAD LOST";
    }
    return "?";
}

} // namespace foss::core
