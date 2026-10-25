/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/sim/Mission.h"

#include <algorithm>
#include <cmath>

#include "core/world/LineOfSight.h"

namespace fossynd::core
{
namespace
{

// How often an AI agent re-evaluates its target and path.
constexpr float kThinkInterval = 0.4f;

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
// A combatant under fire looks for cover among its eight neighbouring tiles
// (issue #120). Only a tile the shooter cannot see counts, so the enemy must be
// flanked to be dislodged. See Mission::updateCombatantAI.
constexpr int kCoverNeighbours[8][2] = {{1, 0},  {-1, 0}, {0, 1},  {0, -1},
                                        {1, 1},  {1, -1}, {-1, 1}, {-1, -1}};
// How close (tiles) an agent must get to a patrol waypoint to advance to the
// next one.
constexpr float kPatrolWaypointRadius = 0.6f;

// Medikit: range (tiles) within which a medikit can heal a team-mate (or the
// user itself), and the health restored per use. Balance lives here until a
// data format is fixed (ADR-0003).
constexpr float kMedikitRadius = 3.0f;
constexpr float kMedikitHealAmount = 40.0f;

// Scanner: the radius (tiles) a scan reveals living enemies within, and how
// long the ping lasts. The reveal is presentation-only (the sim never uses it),
// so it changes no combat outcome. Deterministic; no RNG.
constexpr float kScannerRadius = 8.0f;
constexpr float kScannerPulseSeconds = 5.0f;

// Performance drugs: how long the buff lasts and what it multiplies. Speed is
// movement only (not fire rate); accuracy is the hit chance before the range
// falloff, clamped to 1.0. Balance lives here until a data format is fixed
// (ADR-0003).
constexpr float kDrugDurationSeconds = 8.0f;
constexpr float kDrugSpeedMultiplier = 1.5f;
constexpr float kDrugAccuracyMultiplier = 1.35f;

// Regulated drug bars (ADR-0043): how fast the neutral point drifts toward the
// current level (per second). A sustained high hold lets the body adapt, so the
// effect shrinks and dropping the bar becomes a penalty. The level itself never
// drifts — the player regulates it. Deterministic; no RNG.
constexpr float kDrugNormalisationRate = 0.25f;

// Adrenalin (S2). The effect scales movement speed by `1 + k * effect` (so it is
// the base speed at effect 0, faster above, slower below), and the AI think
// cadence by `1 - k * effect` (shorter = faster reaction above 0, longer below).
// The panic threshold falls linearly with the effect; at effect <= 0 it is
// infinite, so a calm agent never panics. All deterministic; no RNG.
constexpr float kAdrenalinSpeedGain = 0.5f;    // ±50% at full effect
constexpr float kAdrenalinReactionGain = 0.5f; // ±50% cadence at full effect
constexpr float kAdrenalinAccuracyLoss = 0.4f; // -40% hit chance at full effect
constexpr float kPanicThresholdBase = 40.0f;   // threat units at effect 0+
constexpr float kPanicThresholdDrop = 35.0f;   // threshold at effect 1
constexpr float kPanicClearFraction = 0.7f;    // hysteresis: clear below 70%
// Threat-model tuning. A weapon's base threat is `damage * (1 + area)` (a
// blast weapon projects more); situational modifiers are multiplied in.
constexpr float kThreatBlastWeight = 1.0f;
constexpr float kThreatRangeTiles = 12.0f;      // distance falloff scale
constexpr float kThreatTargetingMe = 1.5f;
constexpr float kThreatTargetingAlly = 1.25f;
constexpr float kThreatApproaching = 1.25f;
constexpr float kThreatHasSeenMe = 1.25f;
constexpr float kThreatApproachSpeed = 0.05f;   // tiles/s that counts as "towards me"

// Intelligence (S3). The effect scales the felt threat (recognition): below 1 as
// the effect falls (a low-intelligence agent under-detects threats, so it panics
// less), above 1 as it rises. It also scales how strongly the target picker
// prefers a hittable target, and at effect > 0 the agent recognises an
// `assassinate` mission target as valid. All deterministic; no RNG.
constexpr float kRecognitionGain = 0.6f;      // ±60% felt threat at full effect
constexpr float kTargetPreferenceGain = 1.0f; // hittability weight at full effect

// Physical (S4). The effect scales max health, movement speed, carrying capacity,
// and grants slow self-healing while positive. Health and speed are *recomputed*
// from their base each tick (never multiplied in place), so the normalisation
// drift cannot corrupt the base stat. Healing is `kPhysicalHealPerSecond * effect`
// health per second, capped at max; effect <= 0 heals nothing. All deterministic;
// no RNG.
constexpr float kPhysicalHealthGain = 0.4f;    // ±40% max health at full effect
constexpr float kPhysicalSpeedGain = 0.3f;     // ±30% speed at full effect
constexpr float kPhysicalCarryGain = 1.0f;     // capacity 0..2 (base 1) at full effect
constexpr float kPhysicalHealPerSecond = 2.0f; // hp/s at full effect (positive only)

// Vehicles: how close (tiles) an agent must be to a vehicle's tile to hijack it,
// so both "stand on the tile" and "step up to it" work. A destroyed vehicle is
// a wreck (occupants are thrown clear), so it damages agents within this blast
// radius by a fraction of its maximum health.
constexpr float kHijackRadius = 1.5f;
constexpr float kVehicleWreckRadius = 1.5f;
constexpr float kVehicleWreckDamageScale = 0.5f;

// Time bomb (issue #449). A player-armed fused charge: it detonates after a
// fixed fuse, with a blast wider and stronger than a rifle round, so it opens a
// room the way a rocket does. The AI does not arm bombs yet.
constexpr float kTimeBombFuseSeconds = 3.0f;
constexpr float kTimeBombRadius = 2.0f;
constexpr float kTimeBombDamage = 60.0f;

// A sentinel attacker index for a blast whose source is gone (a time bomb
// outliving its owner). It is never a valid agent index, so `detonate` excludes
// no agent by identity and simply hurts everyone in range — the correct
// behaviour for an orphaned charge.
constexpr std::size_t kNoAttacker = static_cast<std::size_t>(-1);

// The opposing combatant team for a given team. Civilians have no enemy.
Team opposingTeam(Team team)
{
    return team == Team::Player ? Team::Enemy : Team::Player;
}

} // namespace

Mission::Mission(TileGrid& grid, std::uint64_t seed)
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
    medikits_.push_back(0);
    shields_.push_back(0);
    shieldUp_.push_back(false);
    scanners_.push_back(0);
    scannerTimer_.push_back(0.0f);
    drugs_.push_back(0);
    drugsTimer_.push_back(0.0f);
    drugStates_.push_back(DrugState{});
    // The adrenalin base is the agent's speed with no adrenalin effect applied.
    // `applyCybernetic` refreshes it if a legs upgrade later changes the speed.
    // It is also the shared pre-drug speed base for the physical bar (S4), so the
    // two bars compose: speed is always recomputed from this base.
    adrenalinBaseSpeed_.push_back(agents_.back().speed());
    physicalBaseMaxHealth_.push_back(agents_.back().maxHealth());
    panicking_.push_back(false);
    return agents_.size() - 1;
}

std::size_t Mission::addVehicle(Vehicle vehicle)
{
    vehicles_.push_back(std::move(vehicle));
    vehicleTargets_.push_back(-1);
    return vehicles_.size() - 1;
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
    // A panicking agent ignores the player's orders until it settles (ADR-0043).
    if (panicking_[index])
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
    // A panicking agent ignores the player's orders until it settles (ADR-0043).
    if (panicking_[index])
    {
        return;
    }
    agent.setOrderedTarget(static_cast<int>(targetIndex));
    // An attack order supersedes any queued route.
    agent.clearMoveQueue();

    // Close to within weapon range if not already there. An agent that is in
    // range by distance must also be able to *do* something from where it
    // stands: it may hold position only with a clear shot, or when the only
    // thing between it and the target is a destructible wall it can chip
    // through (matching `resolveFire`). Otherwise it keeps moving, so the path
    // routes around the obstacle. Stopping with neither would freeze the agent
    // (it cannot shoot through a solid wall) and deadlock the mission (#229).
    if (grid_ != nullptr)
    {
        const float distance = distanceBetween(index, targetIndex);
        const bool inRange = distance <= agent.weapon().range * 0.9f;
        const bool clearShot = hasClearShot(index, targetIndex);
        bool canBreach = false;
        if (!clearShot)
        {
            bool hitWall = false;
            const TilePos wall = firstWallOnLine(agent.tile(), target.tile(), hitWall);
            canBreach = hitWall && grid_->isDestructible(wall.x, wall.y);
        }
        if (inRange && (clearShot || canBreach))
        {
            agent.stop();
        }
        else
        {
            agent.moveTo(*grid_, target.tile());
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

void Mission::setMedikits(std::size_t index, int count)
{
    if (index >= agents_.size())
    {
        return;
    }
    medikits_[index] = count < 0 ? 0 : count;
}

int Mission::medikits(std::size_t index) const
{
    return index < medikits_.size() ? medikits_[index] : 0;
}

float Mission::useMedikit(std::size_t index)
{
    if (index >= agents_.size() || medikits_[index] <= 0)
    {
        return 0.0f;
    }
    Agent& user = agents_[index];
    if (!user.isAlive() || !user.isCombatant())
    {
        return 0.0f;
    }

    // Heal the most-wounded living team-mate (or the user) in range. Ties break
    // by agent index, so the choice is deterministic; no RNG.
    int best = -1;
    float bestFraction = 1.0f;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        const Agent& candidate = agents_[i];
        if (!candidate.isAlive() || candidate.team() != user.team())
        {
            continue;
        }
        if (candidate.healthFraction() >= 1.0f)
        {
            continue;
        }
        if (length(candidate.position() - user.position()) > kMedikitRadius)
        {
            continue;
        }
        if (candidate.healthFraction() < bestFraction)
        {
            best = static_cast<int>(i);
            bestFraction = candidate.healthFraction();
        }
    }
    if (best < 0)
    {
        // No wounded target in range: do not waste a kit.
        return 0.0f;
    }

    const float healed = agents_[static_cast<std::size_t>(best)].heal(kMedikitHealAmount);
    if (healed > 0.0f)
    {
        --medikits_[index];
    }
    return healed;
}

void Mission::setShields(std::size_t index, int count)
{
    if (index >= agents_.size())
    {
        return;
    }
    shields_[index] = count < 0 ? 0 : count;
}

int Mission::shields(std::size_t index) const
{
    return index < shields_.size() ? shields_[index] : 0;
}

bool Mission::activateShield(std::size_t index)
{
    if (index >= agents_.size() || shields_[index] <= 0)
    {
        return false;
    }
    Agent& user = agents_[index];
    if (!user.isAlive() || !user.isCombatant())
    {
        return false;
    }
    if (shieldUp_[index])
    {
        // Already shielded: do not burn another charge.
        return false;
    }
    --shields_[index];
    shieldUp_[index] = true;
    return true;
}

bool Mission::shieldUp(std::size_t index) const
{
    return index < shieldUp_.size() && shieldUp_[index];
}

void Mission::setScanners(std::size_t index, int count)
{
    if (index >= agents_.size())
    {
        return;
    }
    scanners_[index] = count < 0 ? 0 : count;
}

int Mission::scanners(std::size_t index) const
{
    return index < scanners_.size() ? scanners_[index] : 0;
}

bool Mission::activateScanner(std::size_t index)
{
    if (index >= agents_.size() || scanners_[index] <= 0)
    {
        return false;
    }
    const Agent& user = agents_[index];
    if (!user.isAlive() || !user.isCombatant())
    {
        return false;
    }
    if (scannerTimer_[index] > 0.0f)
    {
        // A scan is already running: do not burn another charge.
        return false;
    }
    --scanners_[index];
    scannerTimer_[index] = kScannerPulseSeconds;
    return true;
}

float Mission::scannerPulse(std::size_t index) const
{
    return index < scannerTimer_.size() ? scannerTimer_[index] : 0.0f;
}

std::vector<std::size_t> Mission::scanTargets(std::size_t index) const
{
    std::vector<std::size_t> targets;
    if (index >= agents_.size() || scannerTimer_[index] <= 0.0f)
    {
        return targets;
    }
    const Team enemyTeam = opposingTeam(agents_[index].team());
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (!agents_[i].isAlive() || agents_[i].team() != enemyTeam)
        {
            continue;
        }
        if (distanceBetween(index, i) <= kScannerRadius)
        {
            targets.push_back(i);
        }
    }
    return targets;
}

void Mission::setDrugs(std::size_t index, int count)
{
    if (index >= agents_.size())
    {
        return;
    }
    drugs_[index] = count < 0 ? 0 : count;
}

int Mission::drugs(std::size_t index) const
{
    return index < drugs_.size() ? drugs_[index] : 0;
}

bool Mission::useDrugs(std::size_t index)
{
    if (index >= agents_.size() || drugs_[index] <= 0)
    {
        return false;
    }
    Agent& user = agents_[index];
    if (!user.isAlive() || !user.isCombatant())
    {
        return false;
    }
    if (drugsTimer_[index] > 0.0f)
    {
        // Already boosted: do not burn another dose.
        return false;
    }
    --drugs_[index];
    drugsTimer_[index] = kDrugDurationSeconds;
    // The buff is folded into the adrenalin base speed, not multiplied onto the
    // live speed: the adrenalin path recomputes speed from that base every tick,
    // so a one-off multiply here would be overwritten (and the expiry divide
    // would then corrupt the base). Folding it in keeps both systems composing.
    adrenalinBaseSpeed_[index] *= kDrugSpeedMultiplier;
    user.setSpeed(user.speed() * kDrugSpeedMultiplier);
    return true;
}

float Mission::drugsTimer(std::size_t index) const
{
    return index < drugsTimer_.size() ? drugsTimer_[index] : 0.0f;
}

void Mission::setDrugLevel(std::size_t index, Drug drug, float level)
{
    if (index >= drugStates_.size())
    {
        return;
    }
    // Clamp to the bar's ceiling, not just [0, 1]: a bar cannot be regulated
    // above what the metagame sold (ADR-0043, #518). The ceiling defaults to
    // `kDrugMaxLevel`, so an unregulated mission is unchanged.
    const float cap = drugCeiling(index, drug);
    drugStates_[index].level[drugSlot(drug)] = std::clamp(level, 0.0f, cap);
}

void Mission::setDrugCeiling(std::size_t index, Drug drug, float ceiling)
{
    if (index >= drugStates_.size())
    {
        return;
    }
    const float cap = std::clamp(ceiling, 0.0f, kDrugMaxLevel);
    const std::size_t slot = drugSlot(drug);
    drugStates_[index].ceiling[slot] = cap;
    // A ceiling that drops below the current level pulls the level down with it,
    // so `level <= ceiling` holds after every call (the invariant `setDrugLevel`
    // relies on).
    drugStates_[index].level[slot] = std::min(drugStates_[index].level[slot], cap);
}

float Mission::drugCeiling(std::size_t index, Drug drug) const
{
    return index < drugStates_.size() ? drugStates_[index].ceiling[drugSlot(drug)] : 0.0f;
}

float Mission::drugLevel(std::size_t index, Drug drug) const
{
    return index < drugStates_.size() ? drugStates_[index].level[drugSlot(drug)] : 0.0f;
}

float Mission::drugNeutral(std::size_t index, Drug drug) const
{
    return index < drugStates_.size() ? drugStates_[index].neutral[drugSlot(drug)] : 0.0f;
}

float Mission::drugEffect(std::size_t index, Drug drug) const
{
    return std::clamp(drugLevel(index, drug) - drugNeutral(index, drug), -1.0f, 1.0f);
}

float Mission::threatLevel(std::size_t index) const
{
    if (index >= agents_.size() || !agents_[index].isCombatant())
    {
        return 0.0f;
    }
    const Weapon& weapon = agents_[index].weapon();
    // A blast weapon projects threat over its whole radius, so it counts for
    // more than its raw damage suggests.
    return weapon.damage * (1.0f + kThreatBlastWeight * weapon.areaRadius);
}

float Mission::threatTo(std::size_t index) const
{
    if (index >= agents_.size() || !agents_[index].isCombatant())
    {
        return 0.0f;
    }
    const Agent& self = agents_[index];
    const Team enemyTeam = opposingTeam(self.team());
    float total = 0.0f;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        const Agent& other = agents_[i];
        if (i == index || !other.isAlive() || other.team() != enemyTeam)
        {
            continue;
        }
        // Only what the agent can actually see contributes threat.
        if (!seesTarget(index, i))
        {
            continue;
        }
        const float base = threatLevel(i);
        const float distance = distanceBetween(index, i);
        // Distance: a threat at zero range is double a threat at the falloff
        // scale, and never below 1x within sight.
        const float distanceModifier = 1.0f + (1.0f - std::min(1.0f, distance / kThreatRangeTiles));

        // Is it aiming at me, or at one of my team-mates? "At me" is worse; a
        // team-mate being targeted is worse the closer that team-mate is to me
        // (the fire is landing near me), so that modifier is distance-weighted.
        const int ordered = other.orderedTarget();
        float targetingModifier = 1.0f;
        if (ordered == static_cast<int>(index))
        {
            targetingModifier = kThreatTargetingMe;
        }
        else if (ordered >= 0 && static_cast<std::size_t>(ordered) < agents_.size() &&
                 agents_[static_cast<std::size_t>(ordered)].team() == self.team())
        {
            const float allyDistance = distanceBetween(index, static_cast<std::size_t>(ordered));
            const float closeness = 1.0f - std::min(1.0f, allyDistance / kThreatRangeTiles);
            targetingModifier = 1.0f + (kThreatTargetingAlly - 1.0f) * closeness;
        }

        // Has it seen me? The relationship is asymmetric: I can see a threat that
        // has not noticed me yet, which is less dangerous than one that has.
        float seenModifier = 1.0f;
        if (seesTarget(i, index))
        {
            seenModifier = kThreatHasSeenMe;
        }

        // Is it closing on me? The delta of its distance to me over its last step
        // (the movement the sim just applied) — deterministic, no RNG.
        float approachingModifier = 1.0f;
        const float previousDistance =
            length(other.previousPosition() - self.position());
        if (previousDistance - distance > kThreatApproachSpeed)
        {
            approachingModifier = kThreatApproaching;
        }

        total += base * distanceModifier * targetingModifier * seenModifier * approachingModifier;
    }
    // Recognition (S3): intelligence scales how much of the raw threat the agent
    // actually perceives. A low-intelligence agent under-detects (panics less),
    // a high one detects more. At effect 0 this is 1.0, so S2 is unchanged.
    return total * recognitionScale(index);
}

float Mission::recognitionScale(std::size_t index) const
{
    const float effect = drugEffect(index, Drug::Intelligence);
    const float scale = 1.0f + kRecognitionGain * effect;
    return scale < 0.0f ? 0.0f : scale;
}

bool Mission::isAssassinationTarget(std::size_t index) const
{
    return objective_.kind == ObjectiveSpec::Kind::Assassinate && objective_.agentIndex >= 0 &&
           static_cast<std::size_t>(objective_.agentIndex) == index;
}

int Mission::selectTarget(std::size_t self, Team enemyTeam) const
{
    if (self >= agents_.size() || !agents_[self].isCombatant())
    {
        return -1;
    }
    // At effect <= 0 this is exactly `nearestEnemy`, so a low-intelligence agent
    // (and every S2 mission, where intelligence sits at neutral) is unchanged.
    const float effect = drugEffect(self, Drug::Intelligence);
    if (effect <= 0.0f)
    {
        return nearestEnemy(self, enemyTeam);
    }

    const Agent& attacker = agents_[self];
    const float range = attacker.weapon().range;
    const float weight = effect * kTargetPreferenceGain;
    int best = -1;
    float bestScore = 0.0f;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (i == self || !agents_[i].isAlive())
        {
            continue;
        }
        // A mission target (assassinate) is recognised as a valid target at high
        // intelligence even if it is a civilian, so the objective stays
        // completable; otherwise only the enemy team qualifies. Only the player
        // pursues the objective, so an enemy never diverts to it, and a unit on
        // the agent's own team is never a target even if the objective names it.
        const bool sameTeam = agents_[i].team() == agents_[self].team();
        const bool missionTarget = agents_[self].team() == Team::Player &&
                                   isAssassinationTarget(i) && !sameTeam;
        if (agents_[i].team() != enemyTeam && !missionTarget)
        {
            continue;
        }
        // A wall blocks acquisition, matching `nearestEnemy`.
        if (!hasClearShot(self, i))
        {
            continue;
        }
        const float distance = distanceBetween(self, i);
        // A nearer target scores higher; so does one it can actually hit — in
        // range and with no team-mate in the line (a shot it would hold fire on).
        float score = 1.0f / (1.0f + distance);
        if (distance <= range && !wouldHitFriendInBlast(self, i))
        {
            score += weight;
        }
        if (best == -1 || score > bestScore)
        {
            best = static_cast<int>(i);
            bestScore = score;
        }
    }
    return best;
}

bool Mission::wouldHitFriendInBlast(std::size_t self, std::size_t targetIndex) const
{
    if (self >= agents_.size() || targetIndex >= agents_.size())
    {
        return false;
    }
    const float radius = agents_[self].weapon().areaRadius;
    if (radius <= 0.0f)
    {
        return false; // a single-target weapon catches no one else
    }
    const Vec2 impact = agents_[targetIndex].position();
    const Team friendlyTeam = agents_[self].team();
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (i == self || i == targetIndex || !agents_[i].isAlive())
        {
            continue;
        }
        const bool friendly = agents_[i].team() == friendlyTeam;
        // A civilian is a bystander to protect; a mission target (assassinate) is
        // never protected, so the objective stays completable.
        const bool bystander =
            agents_[i].team() == Team::Civilian && !isAssassinationTarget(i);
        if (!friendly && !bystander)
        {
            continue;
        }
        if (length(agents_[i].position() - impact) <= radius)
        {
            return true;
        }
    }
    return false;
}

bool Mission::shouldTakeAreaShot(std::size_t self, std::size_t targetIndex) const
{
    if (self >= agents_.size())
    {
        return true;
    }
    // Only a high-intelligence agent is careful; at or below neutral it always
    // takes the shot, so S2 and the shipped missions are unchanged.
    if (drugEffect(self, Drug::Intelligence) <= 0.0f)
    {
        return true;
    }
    return !wouldHitFriendInBlast(self, targetIndex);
}

float Mission::reactionInterval(std::size_t index) const
{
    if (index >= agents_.size())
    {
        return kThinkInterval;
    }
    const float effect = drugEffect(index, Drug::Adrenalin);
    // Higher adrenalin -> shorter interval (faster reaction); the clamp keeps it
    // positive so a full low bar cannot produce a zero or negative cadence.
    return kThinkInterval * std::max(0.2f, 1.0f - kAdrenalinReactionGain * effect);
}

bool Mission::isPanicking(std::size_t index) const
{
    return index < panicking_.size() && panicking_[index];
}

float Mission::adrenalinAccuracyScale(std::size_t index) const
{
    if (index >= agents_.size())
    {
        return 1.0f;
    }
    // Symmetric about effect 0: high adrenalin loses precision, low gains it.
    return std::max(0.0f, 1.0f - kAdrenalinAccuracyLoss * drugEffect(index, Drug::Adrenalin));
}

void Mission::applyDrugSpeed(std::size_t index)
{
    if (index >= agents_.size() || index >= adrenalinBaseSpeed_.size())
    {
        return;
    }
    // Recompute from the base every time: the effects drift as the bars
    // normalise, so multiplying the *current* speed would compound the drift.
    // Adrenalin and physical compose (both scale the shared pre-drug base).
    const float adrenalin = drugEffect(index, Drug::Adrenalin);
    const float physical = drugEffect(index, Drug::Physical);
    const float scale = (1.0f + kAdrenalinSpeedGain * adrenalin) *
                        (1.0f + kPhysicalSpeedGain * physical);
    agents_[index].setSpeed(std::max(0.1f, adrenalinBaseSpeed_[index] * scale));
}

float Mission::carryCapacity(std::size_t index) const
{
    if (index >= agents_.size())
    {
        return 0.0f;
    }
    const float effect = drugEffect(index, Drug::Physical);
    const float capacity = 1.0f + kPhysicalCarryGain * effect;
    return capacity < 0.0f ? 0.0f : capacity;
}

void Mission::applyPhysicalStats(std::size_t index, float dt)
{
    if (index >= agents_.size() || index >= physicalBaseMaxHealth_.size())
    {
        return;
    }
    Agent& agent = agents_[index];
    if (!agent.isAlive() || !agent.isCombatant())
    {
        return;
    }
    const float effect = drugEffect(index, Drug::Physical);
    // Recompute the maximum health from its base, preserving the health fraction
    // so the drift cannot grant free healing (or a free wound) each tick.
    agent.setMaxHealthPreservingFraction(
        physicalBaseMaxHealth_[index] * (1.0f + kPhysicalHealthGain * effect));
    // Slow self-healing while the effect is positive; zero effect heals nothing.
    if (effect > 0.0f)
    {
        agent.heal(kPhysicalHealPerSecond * effect * dt);
    }
}

void Mission::updatePanic(std::size_t index)
{
    if (index >= agents_.size() || index >= panicking_.size())
    {
        return;
    }
    Agent& agent = agents_[index];
    if (!agent.isAlive() || !agent.isCombatant())
    {
        panicking_[index] = false;
        return;
    }
    const float effect = drugEffect(index, Drug::Adrenalin);
    // A calm agent (effect <= 0) never panics: the threshold is effectively
    // infinite. This is the "low adrenalin = rational AI" half of ADR-0043.
    if (effect <= 0.0f)
    {
        panicking_[index] = false;
        return;
    }
    const float threat = threatTo(index);
    const float threshold = kPanicThresholdBase - kPanicThresholdDrop * effect;
    if (!panicking_[index])
    {
        if (threat > threshold)
        {
            panicking_[index] = true;
        }
    }
    else if (threat < threshold * kPanicClearFraction)
    {
        // Hysteresis: settle only once the threat has clearly fallen, so panic
        // does not flicker on and off around the threshold.
        panicking_[index] = false;
    }
    if (panicking_[index])
    {
        // Order-ignoring flee reaction: drop the player's order and run from the
        // nearest visible threat. Reuses AiState::Retreat so the existing
        // step-away behaviour and the "hold fire" rule apply unchanged.
        agent.setAiState(AiState::Retreat);
        agent.clearOrderedTarget();
        agent.clearMoveQueue();
        // Flee the nearest threat the agent can actually see (the panic trigger
        // only counted visible ones), not merely the nearest with line of sight.
        int threatIndex = -1;
        float bestDistance = 0.0f;
        const Team enemyTeam = opposingTeam(agent.team());
        for (std::size_t j = 0; j < agents_.size(); ++j)
        {
            if (j == index || !agents_[j].isAlive() || agents_[j].team() != enemyTeam ||
                !seesTarget(index, j))
            {
                continue;
            }
            const float distance = distanceBetween(index, j);
            if (threatIndex < 0 || distance < bestDistance)
            {
                threatIndex = static_cast<int>(j);
                bestDistance = distance;
            }
        }
        if (threatIndex < 0)
        {
            agent.stop();
        }
        else if (!agent.hasPath())
        {
            // Re-path only when the current flee path is exhausted, so panic does
            // not run an A* search every tick.
            stepAwayFrom(index, static_cast<std::size_t>(threatIndex));
        }
    }
}

bool Mission::applyCybernetic(std::size_t index, const std::string& id)
{
    if (index >= agents_.size())
    {
        return false;
    }
    CyberneticEffect effect;
    if (!cyberneticEffectForId(id, effect))
    {
        return false;
    }
    Agent& agent = agents_[index];
    if (!agent.isAlive() || !agent.isCombatant())
    {
        // A passive upgrade on a civilian or a dead agent is meaningless.
        return false;
    }
    // Movement speed (legs). Multiplied in directly, as the drug buff does.
    if (effect.speedMultiplier != 1.0f)
    {
        agent.setSpeed(agent.speed() * effect.speedMultiplier);
        // Keep the adrenalin base in step, so the legs upgrade and the live
        // adrenalin effect compose instead of one overwriting the other.
        adrenalinBaseSpeed_[index] = agent.speed();
    }
    // Maximum health (chest). setMaxHealth refills to the new maximum, so the
    // upgrade is a clean gain rather than a wound. Keep the physical base (S4) in
    // step so the chest upgrade and the live physical effect compose.
    if (effect.healthMultiplier != 1.0f)
    {
        agent.setMaxHealth(agent.maxHealth() * effect.healthMultiplier);
        physicalBaseMaxHealth_[index] = agent.maxHealth();
    }
    // Hit chance (arms). Clamped to 1.0: an upgrade must never make a shot
    // certain. The weapon is copied, not mutated in place, because `weapon()`
    // returns a const reference (the Weapon is a value on the agent).
    if (effect.accuracyMultiplier != 1.0f)
    {
        Weapon weapon = agent.weapon();
        weapon.accuracy = std::min(1.0f, weapon.accuracy * effect.accuracyMultiplier);
        agent.setWeapon(std::move(weapon));
    }
    // Sight range / perception (eyes). Delegates to the agent's own
    // `sightRange()` (World's per-agent perception contract, added in #337 for
    // this upgrade), so there is one source of truth for the stat.
    if (effect.sightMultiplier != 1.0f)
    {
        agent.setSightRange(agent.sightRange() * effect.sightMultiplier);
    }
    return true;
}

float Mission::sightRange(std::size_t index) const
{
    return index < agents_.size() ? agents_[index].sightRange() : kDefaultSightRange;
}

void Mission::setSightRange(std::size_t index, float range)
{
    if (index >= agents_.size())
    {
        return;
    }
    agents_[index].setSightRange(range);
}

// --- vehicles ---------------------------------------------------------------

int Mission::vehicleOf(std::size_t agentIndex) const
{
    for (std::size_t v = 0; v < vehicles_.size(); ++v)
    {
        if (vehicles_[v].driver() == static_cast<int>(agentIndex))
        {
            return static_cast<int>(v);
        }
    }
    return -1;
}

bool Mission::isDriving(std::size_t agentIndex) const
{
    return vehicleOf(agentIndex) >= 0;
}

int Mission::vehicleDriver(std::size_t vehicleIndex) const
{
    if (vehicleIndex >= vehicles_.size())
    {
        return -1;
    }
    return vehicles_[vehicleIndex].driver();
}

bool Mission::orderHijack(std::size_t vehicleIndex, std::size_t agentIndex)
{
    if (vehicleIndex >= vehicles_.size() || agentIndex >= agents_.size())
    {
        return false;
    }
    Vehicle& vehicle = vehicles_[vehicleIndex];
    Agent& agent = agents_[agentIndex];
    // Only a living combatant can drive, and only one vehicle at a time.
    if (!agent.isAlive() || !agent.isCombatant() || isDriving(agentIndex))
    {
        return false;
    }
    // The agent must be at the vehicle (its tile or an adjacent one).
    if (length(agent.position() - vehicle.position()) > kHijackRadius)
    {
        return false;
    }
    if (!vehicle.hijack(static_cast<int>(agentIndex)))
    {
        return false;
    }
    // The driver rides in the vehicle: fold its position onto the vehicle's so
    // the two never drift apart. `updateVehicles` keeps them in step.
    agent.stop();
    agent.clearMoveQueue();
    agent.clearOrderedTarget();
    agent.setPosition(vehicle.position().x, vehicle.position().y);
    return true;
}

void Mission::ejectVehicle(std::size_t vehicleIndex)
{
    if (vehicleIndex >= vehicles_.size())
    {
        return;
    }
    Vehicle& vehicle = vehicles_[vehicleIndex];
    const int driver = vehicle.driver();
    if (driver < 0)
    {
        return;
    }
    vehicle.eject();
    if (static_cast<std::size_t>(driver) < agents_.size())
    {
        agents_[static_cast<std::size_t>(driver)].setPosition(vehicle.position().x,
                                                             vehicle.position().y);
    }
}

bool Mission::orderDrive(std::size_t vehicleIndex, TilePos goal)
{
    if (vehicleIndex >= vehicles_.size() || grid_ == nullptr)
    {
        return false;
    }
    Vehicle& vehicle = vehicles_[vehicleIndex];
    // Only an occupied, intact vehicle can be driven (its driver is at the
    // wheel). An empty or wrecked vehicle ignores a drive order.
    if (!vehicle.isAlive() || !vehicle.isOccupied())
    {
        return false;
    }
    return vehicle.driveTo(*grid_, goal);
}

void Mission::orderVehicleAttack(std::size_t vehicleIndex, std::size_t targetIndex)
{
    if (vehicleIndex >= vehicles_.size() || targetIndex >= agents_.size())
    {
        return;
    }
    vehicleTargets_[vehicleIndex] = static_cast<int>(targetIndex);
}

float Mission::damageAgent(std::size_t index, float amount)
{
    if (index >= agents_.size())
    {
        return 0.0f;
    }
    // A raised shield absorbs the whole hit and drops. It is a one-hit buffer,
    // so the absorbed damage is reported as 0 (nothing reached the agent).
    if (shieldUp_[index])
    {
        shieldUp_[index] = false;
        return 0.0f;
    }
    Agent& victim = agents_[index];
    const float before = victim.health();
    victim.applyDamage(amount);
    return before - victim.health();
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

bool Mission::hasClearShot(std::size_t a, std::size_t b) const
{
    if (grid_ == nullptr)
    {
        return true;
    }
    return hasLineOfSight(*grid_, agents_[a].tile(), agents_[b].tile());
}

// Walks the same supercover line `hasLineOfSight` uses and returns the first
// blocked tile, so a wall the shot lands on can be identified. A plain
// Bresenham walk would report a different tile at a corner crossing, so this
// mirrors `hasLineOfSight` step for step: the tile reported is always the one
// that actually blocks the line. Out-of-bounds neighbours are blocked (matching
// `TileGrid::isWalkable`), so a shot can also strike the map edge.
TilePos Mission::firstWallOnLine(TilePos from, TilePos to, bool& found) const
{
    found = false;
    if (grid_ == nullptr)
    {
        return TilePos{0, 0};
    }

    int x = from.x;
    int y = from.y;
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    const int nx = std::abs(dx);
    const int ny = std::abs(dy);
    const int signX = (dx > 0) - (dx < 0);
    const int signY = (dy > 0) - (dy < 0);

    int ix = 0;
    int iy = 0;
    while (ix < nx || iy < ny)
    {
        if ((1 + 2 * ix) * ny == (1 + 2 * iy) * nx)
        {
            // Diagonal corner: the segment touches both orthogonal neighbours as
            // well as the diagonal one (same check as `hasLineOfSight`).
            if (!grid_->isWalkable(x + signX, y))
            {
                found = true;
                return TilePos{x + signX, y};
            }
            if (!grid_->isWalkable(x, y + signY))
            {
                found = true;
                return TilePos{x, y + signY};
            }
            x += signX;
            y += signY;
            ++ix;
            ++iy;
        }
        else if ((1 + 2 * ix) * ny < (1 + 2 * iy) * nx)
        {
            x += signX;
            ++ix;
        }
        else
        {
            y += signY;
            ++iy;
        }

        if (!grid_->isWalkable(x, y))
        {
            found = true;
            return TilePos{x, y};
        }
    }
    return TilePos{0, 0};
}

bool Mission::seesTarget(std::size_t observer, std::size_t target) const
{
    return distanceBetween(observer, target) <= sightRange(observer) && hasClearShot(observer, target);
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
        // A wall blocks acquisition: an enemy it cannot see is not a target.
        if (!hasClearShot(self, i))
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

    // Gadget timers. A drug buff multiplies speed on activation and divides it
    // back out here on expiry, so the movement code itself is untouched. The
    // scanner pulse is presentation-only; the sim never reads its marks.
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (scannerTimer_[i] > 0.0f)
        {
            scannerTimer_[i] = std::max(0.0f, scannerTimer_[i] - dt);
        }
        if (drugsTimer_[i] > 0.0f)
        {
            drugsTimer_[i] = std::max(0.0f, drugsTimer_[i] - dt);
            if (drugsTimer_[i] <= 0.0f)
            {
                // Restore the base too, since the adrenalin path recomputes the
                // live speed from it every tick.
                adrenalinBaseSpeed_[i] /= kDrugSpeedMultiplier;
                agents_[i].setSpeed(agents_[i].speed() / kDrugSpeedMultiplier);
            }
        }
    }

    // Drug-bar normalisation (ADR-0043): the neutral point drifts toward the
    // current level, so a held high bar adapts away and dropping it becomes a
    // penalty. The level is player-regulated and never drifts. Pure state, no
    // behavioural effect yet (S2-S4 read `drugEffect`).
    const float normalisationStep = kDrugNormalisationRate * dt;
    for (DrugState& state : drugStates_)
    {
        for (int bar = 0; bar < kDrugCount; ++bar)
        {
            const float delta = state.level[bar] - state.neutral[bar];
            const float step = std::min(std::abs(delta), normalisationStep);
            state.neutral[bar] += delta > 0.0f ? step : -step;
        }
    }

    // Adrenalin (S2) and physical (S4): recompute each combatant's speed and
    // physical stats from their bases and the live effects, then evaluate panic.
    // Runs *after* the normalisation drift, so the stats match the effect the
    // player will see this tick, and *before* `updateAI`, so a panicking agent's
    // flee reaction wins over its AI decision and over any player order issued
    // since the last tick.
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (!agents_[i].isAlive() || !agents_[i].isCombatant())
        {
            continue;
        }
        applyDrugSpeed(i);
        applyPhysicalStats(i, dt);
        updatePanic(i);
    }

    elapsedSeconds_ += dt;
    updateAI(dt);
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        advanceMoveQueue(i);
    }
    resolveFire();
    updateDetonations(dt);
    updateVehicles(dt);
    resolveVehicleFire();
    updateRetrieval();
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
        // A driver is carried by its vehicle: it does not walk or decide for
        // itself, and the vehicle's mounted weapon is its only gun.
        if (isDriving(i))
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
        // A panicking agent ignores its AI as well as the player's orders: the
        // flee reaction `updatePanic` set stands until the panic settles.
        agent.setThinkTimer(reactionInterval(i));
        if (panicking_[i])
        {
            continue;
        }
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

    const int target = selectTarget(index, Team::Player);
    if (target < 0)
    {
        // No target in sight (none reachable without a wall in the way): patrol
        // the assigned route, or hold position.
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

    // Only engage once the target is within sight range; otherwise keep
    // patrolling rather than chasing across the whole map.
    if (!seesTarget(index, targetIndex))
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

    // Under fire: step behind nearby cover instead of trading shots in the open
    // (issue #120). A combatant counts as under fire when it has just been hit
    // or when the shooter is still firing (the shooter's gunfire timer), so the
    // decision survives the slower think interval while the fight continues.
    // Cover holds until the shooter stops or loses the line; the agent then
    // re-engages or slides to another covered neighbour. Deterministic; no RNG.
    const Agent& shooter = agents_[targetIndex];
    const bool underFire = agent.hitFlash() > 0.0f || shooter.gunfireTimer() > 0.0f;
    TilePos cover{};
    if (underFire && findCoverTile(index, targetIndex, cover))
    {
        agent.setAiState(AiState::Engage);
        agent.moveTo(*grid_, cover);
        return;
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

bool Mission::findCoverTile(std::size_t index, std::size_t attacker, TilePos& out) const
{
    if (grid_ == nullptr || attacker >= agents_.size())
    {
        return false;
    }

    const TilePos here = agents_[index].tile();
    const Vec2 threat = agents_[attacker].position();
    bool found = false;
    float best = 0.0f;
    for (const auto& dir : kCoverNeighbours)
    {
        const int nx = here.x + dir[0];
        const int ny = here.y + dir[1];
        if (!grid_->inBounds(nx, ny) || !grid_->isWalkable(nx, ny))
        {
            continue;
        }
        // A tile the shooter can still see is not cover.
        if (hasLineOfSight(*grid_, TilePos{nx, ny}, agents_[attacker].tile()))
        {
            continue;
        }
        // Prefer the cover tile furthest from the shooter, so the enemy also
        // gains distance. Ties keep the first direction found (deterministic).
        const Vec2 candidate{static_cast<float>(nx), static_cast<float>(ny)};
        const float dist = length(candidate - threat);
        if (!found || dist > best)
        {
            found = true;
            best = dist;
            out = TilePos{nx, ny};
        }
    }
    return found;
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
    // An explicit order that points through a wall is not a target to broadcast.
    if (target < 0 || !hasClearShot(source, static_cast<std::size_t>(target)))
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
        // A driver fires the mounted weapon instead (resolveVehicleFire), not a
        // personal one.
        if (isDriving(i))
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
            target = selectTarget(i, enemyTeam);
        }
        if (target < 0)
        {
            continue;
        }

        const std::size_t targetIndex = static_cast<std::size_t>(target);
        const float distance = distanceBetween(i, targetIndex);
        const float range = attacker.weapon().range;
        if (distance > range)
        {
            continue;
        }

        // A wall blocks fire even on an explicit order: no shooting through it.
        // A shot into a wall lands on it, so a *destructible* wall is chipped
        // while a solid one simply absorbs the round (exactly the no-op it was
        // before). A destructible hit burns the normal fire cooldown, so wall
        // damage is rate-limited like any other shot instead of being applied
        // every frame.
        if (!hasClearShot(i, targetIndex))
        {
            bool hitWall = false;
            const TilePos wall =
                firstWallOnLine(attacker.tile(), agents_[targetIndex].tile(), hitWall);
            if (hitWall && grid_->isDestructible(wall.x, wall.y))
            {
                attacker.beginFireCooldown();
                grid_->damageTile(wall.x, wall.y, static_cast<int>(attacker.weapon().damage));
            }
            continue;
        }

        // Intelligence (S3): a high-intelligence agent holds fire on an area shot
        // that would catch a team-mate or a civilian in the blast (unless the
        // mission's target *is* that civilian). A neutral-or-low agent takes it,
        // so this changes no S2/shipped-mission outcome. Deterministic; no RNG.
        if (!shouldTakeAreaShot(i, targetIndex))
        {
            continue;
        }

        attacker.beginFireCooldown();

        // Accuracy falls off linearly with distance; nearer shots hit more. A
        // performance-drug buff raises the base chance (clamped to 1.0); a high
        // adrenalin effect *lowers* it (high = low precision, ADR-0043).
        const float falloff = 1.0f - (distance / range);
        float baseAccuracy = drugsTimer_[i] > 0.0f
                                 ? std::min(1.0f, attacker.weapon().accuracy * kDrugAccuracyMultiplier)
                                 : attacker.weapon().accuracy;
        baseAccuracy = std::clamp(baseAccuracy * adrenalinAccuracyScale(i), 0.0f, 1.0f);
        const float hitChance =
            baseAccuracy * (kAccuracyAtMaxRange + (1.0f - kAccuracyAtMaxRange) * falloff);
        if (!rng_.chance(hitChance))
        {
            // A miss can still catch a bystander near the target: stray fire.
            applyStrayFire(i, targetIndex, attacker.weapon().damage);
            continue;
        }

        resolveWeaponHit(i, targetIndex, attacker.weapon());
    }
}

void Mission::breachTilesInBlast(const Vec2& impact, float radius, float damage)
{
    if (grid_ == nullptr || radius <= 0.0f || damage <= 0.0f)
    {
        return;
    }
    // A tile's centre is at (x, y) in tile space, so it is in the blast when its
    // centre lies within `radius` of the impact. The scan is bounded to the
    // radius box (and the board); squared distance avoids a square root.
    const float r2 = radius * radius;
    const int minX = std::max(0, static_cast<int>(impact.x - radius));
    const int maxX = std::min(grid_->width() - 1, static_cast<int>(impact.x + radius));
    const int minY = std::max(0, static_cast<int>(impact.y - radius));
    const int maxY = std::min(grid_->height() - 1, static_cast<int>(impact.y + radius));
    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            // `isDestructible` is false once a tile has been breached, so an
            // already-open tile is never re-chipped.
            const float dx = static_cast<float>(x) - impact.x;
            const float dy = static_cast<float>(y) - impact.y;
            if (dx * dx + dy * dy <= r2 && grid_->isDestructible(x, y))
            {
                grid_->damageTile(x, y, static_cast<int>(damage));
            }
        }
    }
}

void Mission::applyAreaDamage(std::size_t attacker, std::size_t target, float damage, float radius)
{
    detonate(agents_[target].position(), radius, damage, attacker, target);
}

void Mission::resolveWeaponHit(std::size_t attacker, std::size_t targetIndex, const Weapon& weapon)
{
    // A fused weapon does not damage on impact: it arms a bomb at the target's
    // tile that detonates after the fuse, through the same area-damage path.
    // Owned by the shooter, so a shooter can never stack two bombs.
    if (weapon.fuseSeconds > 0.0f)
    {
        armTimeBomb(attacker, agents_[targetIndex].tile(), weapon.areaRadius, weapon.damage);
        return;
    }

    Agent& victim = agents_[targetIndex];
    damageAgent(targetIndex, weapon.damage);
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

    // Explosive weapons also damage everyone else caught in the blast. The
    // primary target is excluded so it is hit exactly once (issue #79).
    if (weapon.areaRadius > 0.0f)
    {
        applyAreaDamage(attacker, targetIndex, weapon.damage, weapon.areaRadius);
    }
}

void Mission::detonate(const Vec2& impact, float radius, float damage, std::size_t attacker,
                       std::size_t exclude)
{
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        Agent& bystander = agents_[i];
        if (i == attacker || i == exclude || !bystander.isAlive())
        {
            continue;
        }
        if (length(bystander.position() - impact) > radius)
        {
            continue;
        }
        damageAgent(i, damage);
        if (!bystander.isAlive())
        {
            if (bystander.team() == Team::Enemy)
            {
                ++playerKills_;
            }
            else if (bystander.team() == Team::Player)
            {
                ++enemyKills_;
            }
            else if (bystander.team() == Team::Civilian)
            {
                ++civilianKills_;
            }
        }
    }

    // An explosive blast also wrecks vehicles caught in it. A destroyed vehicle
    // ejects its driver (handled by Vehicle::applyDamage) and throws a secondary
    // blast that hurts nearby agents, so a car is cover that can backfire.
    for (std::size_t v = 0; v < vehicles_.size(); ++v)
    {
        Vehicle& vehicle = vehicles_[v];
        if (!vehicle.isAlive() || length(vehicle.position() - impact) > radius)
        {
            continue;
        }
        const float maxHealth = vehicle.maxHealth();
        if (damageVehicle(v, damage) <= 0.0f || vehicle.isAlive())
        {
            continue;
        }
        // The wreck detonates: agents near it take a fraction of its size in
        // damage, and the blast breaches destructible walls the wreck touches.
        breachTilesInBlast(vehicle.position(), kVehicleWreckRadius,
                           maxHealth * kVehicleWreckDamageScale);
        for (std::size_t i = 0; i < agents_.size(); ++i)
        {
            Agent& agent = agents_[i];
            if (!agent.isAlive() ||
                length(agent.position() - vehicle.position()) > kVehicleWreckRadius)
            {
                continue;
            }
            damageAgent(i, maxHealth * kVehicleWreckDamageScale);
        }
    }

    // The explosion itself breaches destructible walls in its radius, not just
    // the one tile a blocked shot would have landed on (#185).
    breachTilesInBlast(impact, radius, damage);
}

bool Mission::armTimeBomb(std::size_t owner, TilePos tile, float radius, float damage)
{
    if (owner >= agents_.size())
    {
        return false;
    }
    const Agent& agent = agents_[owner];
    // A bomb is a combatant's action: not a civilian, not a corpse, not someone
    // already behind a wheel. One live bomb per agent, so a charge is never
    // stacked on the same spot.
    if (!agent.canAttack() || isDriving(owner))
    {
        return false;
    }
    for (const Detonation& armed : detonations_)
    {
        if (armed.owner == owner)
        {
            return false;
        }
    }
    Detonation bomb;
    bomb.impact = Vec2{static_cast<float>(tile.x), static_cast<float>(tile.y)};
    bomb.radius = radius;
    bomb.damage = damage;
    bomb.fuse = kTimeBombFuseSeconds;
    bomb.owner = owner;
    detonations_.push_back(bomb);
    return true;
}

bool Mission::orderTimeBomb(std::size_t index, TilePos tile)
{
    return armTimeBomb(index, tile, kTimeBombRadius, kTimeBombDamage);
}

void Mission::updateDetonations(float dt)
{
    // Tick every fuse first, then detonate the ones that have run out, so a
    // single `update` cannot both arm and fire a bomb (order is fixed and
    // deterministic). Iterating front-to-back and erasing the fired entries
    // keeps the remaining order stable.
    for (Detonation& bomb : detonations_)
    {
        bomb.fuse -= dt;
    }
    for (std::size_t i = 0; i < detonations_.size();)
    {
        if (detonations_[i].fuse > 0.0f)
        {
            ++i;
            continue;
        }
        const Detonation& bomb = detonations_[i];
        // The owner may be dead or gone by now: the blast is attributed to the
        // sentinel, so it excludes nobody and hurts everyone in range.
        detonate(bomb.impact, bomb.radius, bomb.damage, kNoAttacker, kNoAttacker);
        detonations_.erase(detonations_.begin() + static_cast<std::ptrdiff_t>(i));
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
        damageAgent(i, damage * kStrayDamageScale);
        if (!bystander.isAlive() && agents_[attacker].team() == Team::Player)
        {
            ++civilianKills_;
        }
    }
}

void Mission::updateVehicles(float dt)
{
    for (Vehicle& vehicle : vehicles_)
    {
        vehicle.update(dt);
    }
    syncDrivers();
}

void Mission::syncDrivers()
{
    for (const Vehicle& vehicle : vehicles_)
    {
        const int driver = vehicle.driver();
        if (driver < 0 || static_cast<std::size_t>(driver) >= agents_.size())
        {
            continue;
        }
        Agent& agent = agents_[static_cast<std::size_t>(driver)];
        // The driver rides in the vehicle, so its position is the vehicle's.
        agent.setPosition(vehicle.position().x, vehicle.position().y);
    }
}

float Mission::damageVehicle(std::size_t index, float amount)
{
    if (index >= vehicles_.size())
    {
        return 0.0f;
    }
    // `Vehicle::applyDamage` stops a wreck and puts the driver out; Mission owns
    // what the agent does next, which is nothing until the next order.
    return vehicles_[index].applyDamage(amount);
}

void Mission::resolveVehicleFire()
{
    for (std::size_t v = 0; v < vehicles_.size(); ++v)
    {
        Vehicle& vehicle = vehicles_[v];
        const int driver = vehicle.driver();
        if (!vehicle.isAlive() || !vehicle.canFire())
        {
            continue;
        }
        if (driver < 0 || static_cast<std::size_t>(driver) >= agents_.size())
        {
            continue;
        }
        const Agent& gunner = agents_[static_cast<std::size_t>(driver)];
        const Team enemyTeam = opposingTeam(gunner.team());

        // Honour an explicit mounted-fire order if it still points at a living
        // agent on the opposing team; otherwise auto-acquire the nearest enemy.
        int target = -1;
        const int ordered = vehicleTargets_[v];
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
            // Reuse the agent acquisition, but from the vehicle's tile.
            target = nearestEnemyTo(vehicle.position().x, vehicle.position().y, enemyTeam);
        }
        if (target < 0)
        {
            continue;
        }

        const std::size_t targetIndex = static_cast<std::size_t>(target);
        // A vehicle needs a clear line to its target, exactly as an agent does.
        if (grid_ != nullptr &&
            !hasLineOfSight(*grid_, vehicle.tile(), agents_[targetIndex].tile()))
        {
            continue;
        }
        const float distance = length(agents_[targetIndex].position() - vehicle.position());
        const float range = vehicle.mountedWeapon().range;
        if (distance > range)
        {
            continue;
        }

        vehicle.beginFireCooldown();

        const float falloff = 1.0f - (distance / range);
        const float hitChance =
            vehicle.mountedWeapon().accuracy *
            (kAccuracyAtMaxRange + (1.0f - kAccuracyAtMaxRange) * falloff);
        if (!rng_.chance(hitChance))
        {
            applyStrayFire(static_cast<std::size_t>(driver), targetIndex,
                           vehicle.mountedWeapon().damage);
            continue;
        }

        Agent& victim = agents_[targetIndex];
        damageAgent(targetIndex, vehicle.mountedWeapon().damage);
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

        // A mounted weapon is a gun, not a charge: the time bomb is an agent's
        // personal weapon, so this path never arms one (and a driver could not
        // plant one anyway). Blast radius still applies as before.
        const float areaRadius = vehicle.mountedWeapon().areaRadius;
        if (areaRadius > 0.0f)
        {
            applyAreaDamage(static_cast<std::size_t>(driver), targetIndex,
                            vehicle.mountedWeapon().damage, areaRadius);
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
    case ObjectiveSpec::Kind::Escort:
        // Losing the escorted unit is an unconditional defeat, whether it was
        // killed or simply never reached the exit.
        if (escortUnitLost())
        {
            state_ = MissionState::Lost;
            return;
        }
        if (escortedAgentOnTile())
        {
            state_ = MissionState::Won;
            return;
        }
        break;
    case ObjectiveSpec::Kind::Destroy:
        if (targetTileBreached())
        {
            state_ = MissionState::Won;
            return;
        }
        break;
    case ObjectiveSpec::Kind::Persuade:
        if (followerTargetMet())
        {
            state_ = MissionState::Won;
            return;
        }
        break;
    case ObjectiveSpec::Kind::Assassinate:
        if (assassinationTargetDead())
        {
            state_ = MissionState::Won;
            return;
        }
        break;
    case ObjectiveSpec::Kind::Retrieve:
        if (itemDelivered())
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

bool Mission::escortedAgentOnTile() const
{
    const int index = objective_.agentIndex;
    if (index < 0 || static_cast<std::size_t>(index) >= agents_.size())
    {
        return false;
    }
    const Agent& agent = agents_[static_cast<std::size_t>(index)];
    const TilePos goal{objective_.tileX, objective_.tileY};
    return agent.isAlive() && agent.tile() == goal;
}

bool Mission::escortUnitLost() const
{
    const int index = objective_.agentIndex;
    if (index < 0 || static_cast<std::size_t>(index) >= agents_.size())
    {
        // A missing escorted unit can never be delivered; the loader rejects
        // this, so it only guards a hand-built spec.
        return true;
    }
    return !agents_[static_cast<std::size_t>(index)].isAlive();
}

bool Mission::targetTileBreached() const
{
    if (grid_ == nullptr)
    {
        return false;
    }
    return grid_->isRubble(objective_.tileX, objective_.tileY);
}

bool Mission::followerTargetMet() const
{
    const int target = objective_.targetCount < 1 ? 1 : objective_.targetCount;
    std::size_t total = 0;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (agents_[i].team() == Team::Player)
        {
            total += followerCount(i);
        }
    }
    return total >= static_cast<std::size_t>(target);
}

bool Mission::assassinationTargetDead() const
{
    const int index = objective_.agentIndex;
    if (index < 0 || static_cast<std::size_t>(index) >= agents_.size())
    {
        // A missing target can never be killed; the loader rejects this, so it
        // only guards a hand-built spec.
        return false;
    }
    return !agents_[static_cast<std::size_t>(index)].isAlive();
}

bool Mission::itemDelivered() const
{
    if (!itemRetrieved_)
    {
        return false;
    }
    const TilePos exit{objective_.tileX, objective_.tileY};
    for (const Agent& agent : agents_)
    {
        if (agent.isAlive() && agent.team() == Team::Player && agent.tile() == exit)
        {
            return true;
        }
    }
    return false;
}

void Mission::updateRetrieval()
{
    if (objective_.kind != ObjectiveSpec::Kind::Retrieve || itemRetrieved_)
    {
        return;
    }
    const TilePos item{objective_.itemX, objective_.itemY};
    for (const Agent& agent : agents_)
    {
        if (agent.isAlive() && agent.team() == Team::Player && agent.tile() == item)
        {
            itemRetrieved_ = true;
            return;
        }
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

} // namespace fossynd::core
