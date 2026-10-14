/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/sim/Mission.h"

#include <cmath>

namespace fossynd::core
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

// Followers: how far behind the leader they aim to trail, how close counts as
// "caught up", and how often they re-path. Deterministic: no RNG.
constexpr float kFollowerFollowDistance = 2.0f;
constexpr float kFollowerCatchUp = 1.5f;
constexpr float kFollowerSpeed = 3.0f;

// A missed shot can still catch a bystander within this many tiles of the
// intended target. Deterministic: no extra RNG draw, so combat outcomes are
// unaffected (only civilians are ever hit by stray fire).
constexpr float kStrayRadius = 1.5f;
constexpr float kStrayDamageScale = 0.5f;

// Enemy AI state machine. All transitions are pure functions of health and
// distance, so no RNG is drawn and missions stay deterministic.
// Below this fraction of health a combatant breaks off and retreats.
constexpr float kRetreatHealthFraction = 0.35f;
// When an engaged combatant first sees a target it alerts same-team allies
// within this many tiles ("call backup").
constexpr float kAlertRadius = 8.0f;
// A retreating agent aims for a walkable tile this far (tiles) from its threat.
constexpr float kRetreatDistance = 6.0f;
// How close (tiles) an agent must get to a patrol waypoint to advance to the
// next one.
constexpr float kPatrolWaypointRadius = 0.6f;

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
    // A move order cancels any standing attack order and any queued route
    // (a plain move supersedes them).
    agent.clearOrderedTarget();
    agent.clearMoveQueue();
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
    // An attack order supersedes any queued route.
    agent.clearMoveQueue();

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

void Mission::queueMove(std::size_t index, TilePos goal)
{
    if (index >= agents_.size() || grid_ == nullptr)
    {
        return;
    }
    Agent& agent = agents_[index];
    if (!agent.isAlive() || !grid_->isWalkable(goal.x, goal.y))
    {
        return;
    }
    // A queued move is a move order: it cancels any standing attack order.
    agent.clearOrderedTarget();
    if (agent.hasPath())
    {
        // A path is in progress: visit this waypoint after it completes.
        agent.pushMoveWaypoint(goal);
    }
    else
    {
        // Idle: start moving now. If no path exists, the agent stops and the
        // waypoint is dropped (moveTo returns false).
        agent.moveTo(*grid_, goal);
    }
}

void Mission::clearMoveQueue(std::size_t index)
{
    if (index >= agents_.size())
    {
        return;
    }
    agents_[index].clearMoveQueue();
    agents_[index].stop();
}

std::size_t Mission::moveQueueSize(std::size_t index) const
{
    if (index >= agents_.size())
    {
        return 0;
    }
    return agents_[index].moveQueue().size();
}

void Mission::advanceMoveQueue(std::size_t index)
{
    Agent& agent = agents_[index];
    // Only advance an agent that has a route and is not otherwise busy: an AI
    // patrol/retreat/engage path owns movement while it exists, and the queue
    // resumes when that path ends. A dead agent never moves.
    if (!agent.isAlive() || !agent.hasQueuedMoves() || agent.hasPath())
    {
        return;
    }
    const TilePos goal = agent.popMoveWaypoint();
    if (!agent.moveTo(*grid_, goal))
    {
        // Unreachable (e.g. the tile was blocked after queuing): the route is
        // broken, so drop the rest rather than retrying an impossible path.
        agent.clearMoveQueue();
    }
}

int Mission::persuade(std::size_t index)
{
    if (index >= agents_.size())
    {
        return -1;
    }
    Agent& user = agents_[index];
    if (!user.isAlive() || !user.isCombatant() || user.persuadeCooldown() > 0.0f)
    {
        return -1;
    }

    // A use is consumed even on a miss, so mashing the key is not free.
    user.beginPersuadeCooldown();

    const float range = user.beacon().range;
    int best = -1;
    float bestDistance = 0.0f;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        const Agent& candidate = agents_[i];
        if (!candidate.isAlive() || candidate.team() != Team::Civilian || candidate.isFollower())
        {
            continue;
        }
        const float distance = length(candidate.position() - user.position());
        if (distance > range)
        {
            continue;
        }
        if (best == -1 || distance < bestDistance)
        {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    if (best < 0)
    {
        return -1;
    }

    Agent& recruit = agents_[static_cast<std::size_t>(best)];
    recruit.setFollowerLeader(static_cast<int>(index));
    recruit.setSpeed(kFollowerSpeed);
    recruit.setThinkTimer(0.0f);
    recruit.stop();
    return best;
}

std::size_t Mission::freeFollowersOf(std::size_t index)
{
    if (index >= agents_.size())
    {
        return 0;
    }
    std::size_t freed = 0;
    for (Agent& agent : agents_)
    {
        if (agent.followerLeader() == static_cast<int>(index))
        {
            agent.setFollowerLeader(-1);
            agent.setSpeed(kCivilianFleeSpeed);
            ++freed;
        }
    }
    return freed;
}

std::size_t Mission::followerCount(std::size_t index) const
{
    std::size_t count = 0;
    for (const Agent& agent : agents_)
    {
        if (agent.isAlive() && agent.followerLeader() == static_cast<int>(index))
        {
            ++count;
        }
    }
    return count;
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

    elapsedSeconds_ += dt;
    updateAI(dt);
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        advanceMoveQueue(i);
    }
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
            if (agent.isFollower())
            {
                updateFollower(i);
            }
            else
            {
                updateCivilian(i);
            }
            continue;
        }
        if (agent.team() != Team::Enemy)
        {
            continue;
        }
        agent.setThinkTimer(kThinkInterval);
        updateCombatantAI(i);
    }
}

// The enemy state machine: patrol when idle, engage when a target is in sight,
// and retreat when badly wounded. Deterministic (no RNG).
void Mission::updateCombatantAI(std::size_t index)
{
    Agent& agent = agents_[index];
    if (grid_ == nullptr)
    {
        return;
    }

    // Retreat takes priority: a badly wounded combatant breaks off from the
    // nearest threat, whatever else it was doing.
    const int threat = nearestEnemy(index, opposingTeam(agent.team()));
    if (agent.healthFraction() < kRetreatHealthFraction && threat >= 0)
    {
        agent.setAiState(AiState::Retreat);
        if (!stepAwayFrom(index, static_cast<std::size_t>(threat)))
        {
            agent.stop();
        }
        return;
    }

    const int target = nearestEnemy(index, Team::Player);
    if (target < 0)
    {
        // No target in sight: patrol the assigned route, or hold position.
        if (agent.hasPatrolRoute())
        {
            updatePatrol(index);
        }
        else
        {
            agent.setAiState(AiState::Idle);
            agent.stop();
        }
        return;
    }

    const std::size_t targetIndex = static_cast<std::size_t>(target);
    const float distance = distanceBetween(index, targetIndex);

    // Only engage once the target is actually in sight; otherwise keep patrolling
    // rather than chasing across the whole map.
    if (distance > kSightRange)
    {
        if (agent.hasPatrolRoute())
        {
            updatePatrol(index);
        }
        else
        {
            agent.setAiState(AiState::Idle);
            agent.stop();
        }
        return;
    }

    // Newly engaged: call nearby allies onto the same target.
    if (agent.aiState() != AiState::Engage)
    {
        agent.setOrderedTarget(target);
        alertAllies(index, kAlertRadius);
    }
    agent.setAiState(AiState::Engage);

    const float weaponRange = agent.weapon().range;
    if (distance > weaponRange * 0.9f)
    {
        // Close to within weapon range: step toward the target.
        agent.moveTo(*grid_, agents_[targetIndex].tile());
    }
    else
    {
        // In range: hold position and fire.
        agent.stop();
    }
}

// Walks the agent along its patrol route, advancing to the next waypoint when it
// arrives and looping at the end.
void Mission::updatePatrol(std::size_t index)
{
    Agent& agent = agents_[index];
    agent.setAiState(AiState::Patrol);

    const std::vector<TilePos>& route = agent.patrolRoute();
    if (route.empty())
    {
        agent.stop();
        return;
    }

    std::size_t waypoint = agent.patrolIndex() % route.size();
    const Vec2 goal = Vec2{static_cast<float>(route[waypoint].x), static_cast<float>(route[waypoint].y)};
    if (length(goal - agent.position()) <= kPatrolWaypointRadius)
    {
        waypoint = (waypoint + 1) % route.size();
        agent.setPatrolIndex(waypoint);
    }

    const TilePos tile = route[waypoint];
    if (grid_->inBounds(tile.x, tile.y) && grid_->isWalkable(tile.x, tile.y))
    {
        agent.moveTo(*grid_, tile);
    }
    else
    {
        // A malformed route (blocked/out-of-bounds waypoint) must not stall the
        // whole patrol: skip it.
        agent.setPatrolIndex((waypoint + 1) % route.size());
    }
}

// Moves the agent to the walkable tile furthest from `threat` along the line
// away from it, falling back to nearer tiles. Deterministic; no pathfinding.
bool Mission::stepAwayFrom(std::size_t index, std::size_t threat)
{
    Agent& agent = agents_[index];
    if (grid_ == nullptr || threat >= agents_.size())
    {
        return false;
    }

    const Vec2 away = normalized(agent.position() - agents_[threat].position());
    if (away.x == 0.0f && away.y == 0.0f)
    {
        return false;
    }
    const int stepX = away.x >= 0.0f ? 1 : -1;
    const int stepY = away.y >= 0.0f ? 1 : -1;

    const TilePos here = agent.tile();
    for (int dist = static_cast<int>(kRetreatDistance); dist >= 2; --dist)
    {
        const int gx = here.x + stepX * dist;
        const int gy = here.y + stepY * dist;
        if (grid_->inBounds(gx, gy) && grid_->isWalkable(gx, gy))
        {
            agent.moveTo(*grid_, TilePos{gx, gy});
            return true;
        }
    }
    return false;
}

std::size_t Mission::alertAllies(std::size_t source, float radius)
{
    if (source >= agents_.size())
    {
        return 0;
    }
    Agent& caller = agents_[source];
    if (!caller.isAlive() || !caller.isCombatant())
    {
        return 0;
    }

    // The caller's target: its explicit order if still valid, else the nearest
    // enemy it can see.
    const Team enemyTeam = opposingTeam(caller.team());
    int target = -1;
    const int ordered = caller.orderedTarget();
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
        target = nearestEnemy(source, enemyTeam);
    }
    if (target < 0)
    {
        return 0;
    }

    std::size_t alerted = 0;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (i == source)
        {
            continue;
        }
        Agent& ally = agents_[i];
        if (!ally.isAlive() || ally.team() != caller.team() || !ally.isCombatant())
        {
            continue;
        }
        if (distanceBetween(source, i) > radius)
        {
            continue;
        }
        ally.setOrderedTarget(target);
        if (ally.aiState() != AiState::Engage)
        {
            ally.setAiState(AiState::Engage);
        }
        ++alerted;
    }
    return alerted;
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

void Mission::updateFollower(std::size_t index)
{
    Agent& follower = agents_[index];
    if (grid_ == nullptr)
    {
        return;
    }

    const int leader = follower.followerLeader();
    if (leader < 0 || static_cast<std::size_t>(leader) >= agents_.size())
    {
        // The leader is gone (invalid index); the follower stands down.
        follower.setFollowerLeader(-1);
        return;
    }

    const Agent& lead = agents_[static_cast<std::size_t>(leader)];
    if (!lead.isAlive())
    {
        follower.setFollowerLeader(-1);
        return;
    }

    // Keep a fixed distance behind the leader; once close enough, hold position
    // so followers do not jitter around the leader's tile.
    const float distance = length(lead.position() - follower.position());
    if (distance <= kFollowerCatchUp)
    {
        follower.stop();
        return;
    }

    const Vec2 toLeader = lead.position() - follower.position();
    const Vec2 direction = normalized(toLeader);
    const Vec2 goalPoint = lead.position() - direction * kFollowerFollowDistance;

    int goalX = static_cast<int>(goalPoint.x + 0.5f);
    int goalY = static_cast<int>(goalPoint.y + 0.5f);
    if (!grid_->inBounds(goalX, goalY) || !grid_->isWalkable(goalX, goalY))
    {
        goalX = lead.tile().x;
        goalY = lead.tile().y;
        if (!grid_->inBounds(goalX, goalY) || !grid_->isWalkable(goalX, goalY))
        {
            return;
        }
    }

    follower.setSpeed(kFollowerSpeed);
    follower.moveTo(*grid_, TilePos{goalX, goalY});
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
        // A wounded enemy breaks off and holds fire until it recovers (see
        // Mission::updateCombatantAI). Keyed off health, not aiState, so it also
        // applies on the first tick before the staggered AI has run.
        if (attacker.team() == Team::Enemy && attacker.healthFraction() < kRetreatHealthFraction)
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

    // Evaluate the objective's win condition first, then the universal lose
    // condition. Ordering keeps the default `eliminate` behaviour identical to
    // before this change.
    switch (objective_.kind)
    {
    case ObjectiveSpec::Kind::EliminateAll:
        if (aliveCount(Team::Enemy) == 0)
        {
            state_ = MissionState::Won;
            return;
        }
        break;
    case ObjectiveSpec::Kind::ReachTile:
        if (anyPlayerOnTile())
        {
            state_ = MissionState::Won;
            return;
        }
        break;
    case ObjectiveSpec::Kind::Survive:
        if (elapsedSeconds_ >= objective_.seconds)
        {
            state_ = MissionState::Won;
            return;
        }
        break;
    }

    if (aliveCount(Team::Player) == 0)
    {
        state_ = MissionState::Lost;
    }
}

bool Mission::anyPlayerOnTile() const
{
    const TilePos goal{objective_.tileX, objective_.tileY};
    for (const Agent& agent : agents_)
    {
        if (agent.isAlive() && agent.team() == Team::Player && agent.tile() == goal)
        {
            return true;
        }
    }
    return false;
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

} // namespace fossynd::core
