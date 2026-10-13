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

// Civilians: how close gunfire must be before a bystander panics and flees, and
// how far/fast they run. Fleeing uses no RNG, so it stays deterministic.
constexpr float kCivilianPanicRange = 5.0f;
constexpr float kCivilianFleeDistance = 6.0f;
constexpr float kCivilianFleeSpeed = 3.2f;

// A missed shot can still catch a bystander within this many tiles of the
// intended target. Deterministic: no extra RNG draw, so combat outcomes are
// unaffected (only civilians are ever hit by stray fire).
constexpr float kStrayRadius = 1.5f;
constexpr float kStrayDamageScale = 0.5f;

// The opposing combatant team for a given team. Civilians have no enemy.
Team opposingTeam(Team team)
{
    return team == Team::Player ? Team::Enemy : Team::Player;
}

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
    // A move order cancels any standing attack order.
    agent.clearOrderedTarget();
    if (grid_ != nullptr)
    {
        agent.moveTo(*grid_, goal);
    }
}

void Mission::orderAttack(std::size_t index, std::size_t targetIndex)
{
    if (index >= agents_.size() || targetIndex >= agents_.size())
    {
        return;
    }
    Agent& agent = agents_[index];
    Agent& target = agents_[targetIndex];
    if (!agent.isAlive() || !target.isAlive() || index == targetIndex)
    {
        return;
    }
    agent.setOrderedTarget(static_cast<int>(targetIndex));

    // Close to within weapon range if not already there.
    if (grid_ != nullptr)
    {
        const float distance = distanceBetween(index, targetIndex);
        if (distance > agent.weapon().range * 0.9f)
        {
            agent.moveTo(*grid_, target.tile());
        }
        else
        {
            agent.stop();
        }
    }
}

void Mission::clearOrder(std::size_t index)
{
    if (index < agents_.size())
    {
        agents_[index].clearOrderedTarget();
    }
}

int Mission::nearestEnemyTo(float tileX, float tileY, Team enemyTeam) const
{
    int best = -1;
    float bestDistance = 0.0f;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        const Agent& agent = agents_[i];
        if (!agent.isAlive() || agent.team() != enemyTeam)
        {
            continue;
        }
        const Vec2 delta = agent.position() - Vec2{tileX, tileY};
        const float distance = length(delta);
        if (best == -1 || distance < bestDistance)
        {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    return best;
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
        if (!agent.isAlive())
        {
            continue;
        }
        if (agent.thinkTimer() > 0.0f)
        {
            continue;
        }

        if (agent.team() == Team::Civilian)
        {
            agent.setThinkTimer(kThinkInterval);
            updateCivilian(i);
            continue;
        }
        if (agent.team() != Team::Enemy)
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

void Mission::updateCivilian(std::size_t index)
{
    Agent& civilian = agents_[index];
    if (grid_ == nullptr)
    {
        return;
    }

    // Panic if any living combatant has fired recently nearby.
    const Vec2 self = civilian.position();
    bool panicking = false;
    for (const Agent& other : agents_)
    {
        if (!other.isAlive() || !other.isCombatant())
        {
            continue;
        }
        if (other.gunfireTimer() <= 0.0f)
        {
            continue; // has not fired recently
        }
        if (length(other.position() - self) <= kCivilianPanicRange)
        {
            panicking = true;
            break;
        }
    }

    if (!panicking)
    {
        return;
    }

    civilian.setSpeed(kCivilianFleeSpeed);

    // Flee directly away from the nearest combatant, to the first walkable tile
    // in that direction. Deterministic and cheap: no pathfinding.
    int nearest = -1;
    float nearestDistance = 0.0f;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        const Agent& other = agents_[i];
        if (!other.isAlive() || !other.isCombatant())
        {
            continue;
        }
        const float distance = length(other.position() - self);
        if (nearest < 0 || distance < nearestDistance)
        {
            nearest = static_cast<int>(i);
            nearestDistance = distance;
        }
    }
    if (nearest < 0)
    {
        return;
    }

    const Vec2 away = normalized(self - agents_[static_cast<std::size_t>(nearest)].position());
    const int stepX = away.x >= 0.0f ? 1 : -1;
    const int stepY = away.y >= 0.0f ? 1 : -1;

    const TilePos here = civilian.tile();
    for (int dist = static_cast<int>(kCivilianFleeDistance); dist >= 2; --dist)
    {
        const int gx = here.x + stepX * dist;
        const int gy = here.y + stepY * dist;
        if (grid_->inBounds(gx, gy) && grid_->isWalkable(gx, gy))
        {
            civilian.moveTo(*grid_, TilePos{gx, gy});
            return;
        }
    }
}

void Mission::resolveFire()
{
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        Agent& attacker = agents_[i];
        if (!attacker.isAlive() || !attacker.canAttack() || !attacker.canFire())
        {
            continue;
        }

        const Team enemyTeam = opposingTeam(attacker.team());

        // Honour an explicit attack order if it still points at a living enemy;
        // otherwise auto-acquire the nearest enemy.
        int target = -1;
        const int ordered = attacker.orderedTarget();
        if (ordered >= 0 && static_cast<std::size_t>(ordered) < agents_.size())
        {
            const Agent& candidate = agents_[static_cast<std::size_t>(ordered)];
            if (candidate.isAlive() && candidate.team() == enemyTeam)
            {
                target = ordered;
            }
        }
        if (target < 0)
        {
            target = nearestEnemy(i, enemyTeam);
        }
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
            // A miss can still catch a bystander near the target: stray fire.
            applyStrayFire(i, static_cast<std::size_t>(target), attacker.weapon().damage);
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
            else if (victim.team() == Team::Player)
            {
                ++enemyKills_;
            }
        }
    }
}

void Mission::applyStrayFire(std::size_t attacker, std::size_t target, float damage)
{
    const Vec2 impact = agents_[target].position();
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        Agent& bystander = agents_[i];
        if (i == attacker || !bystander.isAlive() || bystander.team() != Team::Civilian)
        {
            continue;
        }
        if (length(bystander.position() - impact) > kStrayRadius)
        {
            continue;
        }
        bystander.applyDamage(damage * kStrayDamageScale);
        if (!bystander.isAlive() && agents_[attacker].team() == Team::Player)
        {
            ++civilianKills_;
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
