/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/math/Vec2.h"
#include "core/sim/ComplianceBeacon.h"
#include "core/sim/Weapon.h"
#include "core/world/TileGrid.h"

namespace fossynd::core
{

// Which side an agent fights for.
enum class Team
{
    Player,
    Enemy,
    // A neutral bystander: not a combatant, does not count toward win/lose, but
    // can be killed as collateral damage (see Mission::civilianKills).
    Civilian,
};

// What a combatant is currently doing, as decided by the mission AI. Kept on the
// agent so it is inspectable (and unit-testable) without reaching into Mission.
enum class AiState
{
    // No target and no route: hold position.
    Idle,
    // No target in sight, walking the assigned patrol route.
    Patrol,
    // A target is in sight: close to weapon range and fire.
    Engage,
    // Badly wounded: break off and move away from the threat.
    Retreat,
};

// A squad member: a cyborg agent that moves along a path at a fixed speed and
// carries a weapon.
//
// Engine-agnostic. Positions are in tile space (fractional), so the renderer
// converts them to screen coordinates. Movement is driven by a fixed timestep.
class Agent
{
public:
    Agent() = default;
    Agent(float tileX, float tileY, float tilesPerSecond);

    // --- movement ---------------------------------------------------------

    // Replaces the current path with one computed by A* to `goal`. When `goal`
    // is unreachable (blocked, out of bounds, or abandoned by the node cap) the
    // agent instead walks to the reachable tile nearest it, so a click on an
    // unreachable tile still moves the squad toward it. The path is cleared only
    // when there is genuinely nowhere to go (an enclosed or out-of-bounds start).
    // Returns true if a path was set.
    bool moveTo(const TileGrid& grid, TilePos goal);

    // Clears the current path (the agent stops at its current position).
    void stop();

    // Advances the agent along its path and decays timers. `dt` is in seconds.
    void update(float dt);

    bool hasPath() const { return !path_.empty(); }
    const std::vector<TilePos>& path() const { return path_; }

    // --- queued move orders -----------------------------------------------

    // Waypoints queued *after* the current path (the view's shift-click route).
    // The mission starts the next one when the current path finishes. Distinct
    // from `patrolRoute_`, which is AI-owned and looped; this is player-ordered
    // and consumed once.
    const std::vector<TilePos>& moveQueue() const { return moveQueue_; }
    bool hasQueuedMoves() const { return !moveQueue_.empty(); }
    void pushMoveWaypoint(TilePos goal) { moveQueue_.push_back(goal); }

    // Removes and returns the next queued waypoint. Call only when
    // hasQueuedMoves() is true.
    TilePos popMoveWaypoint()
    {
        const TilePos goal = moveQueue_.front();
        moveQueue_.erase(moveQueue_.begin());
        return goal;
    }
    void clearMoveQueue() { moveQueue_.clear(); }

    Vec2 position() const { return position_; }
    void setPosition(float tileX, float tileY);

    // Position at the start of the most recent update(). The renderer uses it
    // with interpolatedPosition() to draw smooth motion between fixed steps
    // (AGENTS.md golden rule 4). Never read by the simulation itself.
    Vec2 previousPosition() const { return previousPosition_; }

    // Linear blend from the previous step's position to the current one:
    // alpha 0 gives previousPosition(), alpha 1 gives position(). The result is
    // presentation-only and must not feed back into gameplay.
    Vec2 interpolatedPosition(float alpha) const;

    float speed() const { return tilesPerSecond_; }
    void setSpeed(float tilesPerSecond) { tilesPerSecond_ = tilesPerSecond; }

    TilePos tile() const;

    // --- identity ---------------------------------------------------------

    Team team() const { return team_; }
    void setTeam(Team team) { team_ = team; }

    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // --- combat -----------------------------------------------------------

    float health() const { return health_; }
    float maxHealth() const { return maxHealth_; }
    void setMaxHealth(float maxHealth);
    bool isAlive() const { return health_ > 0.0f; }

    // Fraction of health remaining in [0, 1] (1 for a full-health agent).
    float healthFraction() const;

    // Applies damage and returns the amount actually dealt. Sets the hit-flash
    // timer used by the renderer. Damage never raises health above max.
    float applyDamage(float amount);

    // Restores health and returns the amount actually healed. Never raises
    // health above `maxHealth`; a dead agent is not healed (returns 0). Used by
    // gadgets such as the medikit.
    float heal(float amount);

    const Weapon& weapon() const { return weapon_; }
    void setWeapon(Weapon weapon) { weapon_ = std::move(weapon); }

    // True when the weapon's cooldown has elapsed.
    bool canFire() const { return fireCooldown_ <= 0.0f; }
    void beginFireCooldown()
    {
        fireCooldown_ = weapon_.cooldown();
        gunfireTimer_ = 0.75f;
    }
    float fireCooldown() const { return fireCooldown_; }

    // Counts down after the agent fires; lets nearby civilians notice gunfire
    // even between their (slower) decision ticks.
    float gunfireTimer() const { return gunfireTimer_; }

    // Counts down since the agent last took damage; used for a damage flash.
    float hitFlash() const { return hitFlash_; }
    void flashHit() { hitFlash_ = 0.25f; }

    // Counts down to the next AI re-path decision.
    float thinkTimer() const { return thinkTimer_; }
    void setThinkTimer(float seconds) { thinkTimer_ = seconds; }

    // --- orders -----------------------------------------------------------

    // True when the agent is a combatant (player or enemy). Civilians never fire
    // and are never auto-targeted.
    bool isCombatant() const { return team_ != Team::Civilian; }

    // An explicit attack order: the agent index to focus fire on, or -1 for
    // "no order" (the mission then auto-acquires the nearest enemy). The index
    // is validated by Mission each step, so a stale index is harmless.
    int orderedTarget() const { return orderedTarget_; }
    void setOrderedTarget(int index) { orderedTarget_ = index; }
    void clearOrderedTarget() { orderedTarget_ = -1; }

    // True when the agent is alive and armed.
    bool canAttack() const { return isAlive() && isCombatant(); }

    // --- AI state ---------------------------------------------------------

    // What the mission AI is currently doing with this agent. Set by
    // Mission::updateAI; meaningful only for combatants.
    AiState aiState() const { return aiState_; }
    void setAiState(AiState state) { aiState_ = state; }

    // Patrol route in tile coordinates, walked in order and looped. Empty means
    // "no patrol". Deterministic: no RNG is involved in following it.
    const std::vector<TilePos>& patrolRoute() const { return patrolRoute_; }
    void setPatrolRoute(std::vector<TilePos> route)
    {
        patrolRoute_ = std::move(route);
        patrolIndex_ = 0;
    }
    bool hasPatrolRoute() const { return !patrolRoute_.empty(); }

    // Index of the next patrol waypoint to head for.
    std::size_t patrolIndex() const { return patrolIndex_; }
    void setPatrolIndex(std::size_t index) { patrolIndex_ = index; }

    // --- persuasion -------------------------------------------------------

    // True when the agent is a civilian recruited as a follower: it keeps
    // Team::Civilian (so it is never a valid combat target) but follows a player
    // agent. Followers never fire and never count toward win/lose.
    bool isFollower() const { return followerLeader_ >= 0; }

    // Index of the player agent being followed, or -1 when not following.
    int followerLeader() const { return followerLeader_; }
    void setFollowerLeader(int leader) { followerLeader_ = leader; }

    // Seconds until the ComplianceBeacon can be used again.
    float persuadeCooldown() const { return persuadeCooldown_; }
    void beginPersuadeCooldown() { persuadeCooldown_ = beacon_.cooldown(); }

    const ComplianceBeacon& beacon() const { return beacon_; }
    void setComplianceBeacon(ComplianceBeacon device) { beacon_ = std::move(device); }

private:
    Vec2 position_;
    Vec2 previousPosition_;
    float tilesPerSecond_ = 2.0f;
    std::vector<TilePos> path_;
    std::size_t nextWaypoint_ = 0;
    // Player-ordered waypoints queued after `path_` (see moveQueue() above).
    std::vector<TilePos> moveQueue_;

    std::string name_ = "Agent";
    Team team_ = Team::Player;
    float maxHealth_ = 100.0f;
    float health_ = 100.0f;
    Weapon weapon_ = makePistol();

    float fireCooldown_ = 0.0f;
    float gunfireTimer_ = 0.0f;
    float hitFlash_ = 0.0f;
    float thinkTimer_ = 0.0f;
    int orderedTarget_ = -1;

    AiState aiState_ = AiState::Idle;
    std::vector<TilePos> patrolRoute_;
    std::size_t patrolIndex_ = 0;

    ComplianceBeacon beacon_ = makeComplianceBeacon();
    float persuadeCooldown_ = 0.0f;
    int followerLeader_ = -1;
};

} // namespace fossynd::core
