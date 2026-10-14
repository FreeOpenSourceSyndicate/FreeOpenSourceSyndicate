/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/math/Vec2.h"
#include "core/sim/Agent.h"
#include "core/sim/Objective.h"
#include "core/util/Rng.h"
#include "core/world/TileGrid.h"

namespace fossynd::core
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
// Win condition: data-driven (`ObjectiveSpec`) — eliminate all enemies (default),
// reach a target tile, or survive for a set time. See `core/sim/Objective.h`.
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

    // Orders the agent at `index` to focus fire on `targetIndex`. Any agent may
    // target any living agent; typically the player targets an enemy.
    void orderAttack(std::size_t index, std::size_t targetIndex);

    // Clears any explicit attack order for the agent at `index`.
    void clearOrder(std::size_t index);

    // --- queued move orders (waypoint queue) ------------------------------

    // Appends `goal` to the agent's waypoint queue. If the agent has no active
    // path it starts moving to the first queued waypoint; otherwise `goal` is
    // visited after the current path completes. Cancels any standing attack
    // order. No-op for a dead/invalid index or an unwalkable goal.
    void queueMove(std::size_t index, TilePos goal);

    // Clears the agent's waypoint queue and stops it at its current position.
    void clearMoveQueue(std::size_t index);

    // Number of waypoints still queued for the agent (0 when none). For the HUD.
    std::size_t moveQueueSize(std::size_t index) const;

    // "Call backup": orders living combatants on the same team as the agent at
    // `source` that are within `radius` tiles to focus fire on the agent's
    // current target (its ordered target if set, otherwise the nearest enemy it
    // can see). Returns the number of allies alerted. Deterministic; no RNG.
    std::size_t alertAllies(std::size_t source, float radius);

    // --- ComplianceBeacon ----------------------------------------------------

    // Uses the ComplianceBeacon from the agent at `index`. Turns the nearest
    // un-recruited civilian within range into a follower of `index` and returns
    // its index, or -1 if there is no valid target (or the device is cooling
    // down). The user is not the agent: persuasion is deterministic and never
    // touches combat targeting. Consumes a use even when it misses.
    int persuade(std::size_t index);

    // Releases every follower of the agent at `index`; they resume normal
    // civilian behaviour. Returns how many were freed.
    std::size_t freeFollowersOf(std::size_t index);

    // Number of living agents following the agent at `index`.
    std::size_t followerCount(std::size_t index) const;

    // --- Medikit -------------------------------------------------------------

    // Sets how many medikits the agent at `index` carries (clamped to >= 0).
    // Used by mission setup and tests. No-op for an invalid index.
    void setMedikits(std::size_t index, int count);

    // Number of medikits the agent at `index` carries (0 if the index is
    // invalid).
    int medikits(std::size_t index) const;

    // Uses one medikit from the agent at `index` to heal the most-wounded living
    // agent on the same team within `kMedikitRadius` tiles (the user itself or a
    // team-mate). Consumes one kit and returns the amount healed, or 0 if there
    // is no kit, the user is not a living combatant, or every agent in range is
    // already at full health (a kit is never wasted on a healthy target).
    // Deterministic: ties break by agent index.
    float useMedikit(std::size_t index);

    // Index of the nearest living agent on `enemyTeam` to (x, y) in tile space,
    // or -1 if none. Used by the view to pick a target under the cursor.
    int nearestEnemyTo(float tileX, float tileY, Team enemyTeam) const;

    // Advances the simulation by `dt` seconds: movement, AI, weapon fire,
    // damage, and win/lose evaluation.
    void update(float dt);

    // --- Objective (data-driven win condition) ---------------------------

    // Sets the mission's win condition. Call before the first `update`.
    void setObjective(ObjectiveSpec objective) { objective_ = objective; }
    const ObjectiveSpec& objective() const { return objective_; }

    // Mission seconds elapsed while Running (frozen once Won/Lost). Used by the
    // `survive` objective and available to the HUD.
    float elapsedSeconds() const { return elapsedSeconds_; }

    MissionState state() const { return state_; }
    const char* stateName() const;

    std::vector<Agent>& agents() { return agents_; }
    const std::vector<Agent>& agents() const { return agents_; }

    std::size_t aliveCount(Team team) const;
    int playerKills() const { return playerKills_; }
    int enemyKills() const { return enemyKills_; }

    // Civilians killed by player fire (collateral damage). Tracked for scoring;
    // does not affect the win/lose condition.
    int civilianKills() const { return civilianKills_; }

    Rng& rng() { return rng_; }

private:
    void updateAI(float dt);
    void updateCivilian(std::size_t index);
    void updateCombatantAI(std::size_t index);
    void updatePatrol(std::size_t index);
    bool stepAwayFrom(std::size_t index, std::size_t threat);
    void updateFollower(std::size_t index);
    void resolveFire();

    // Starts the agent's next queued waypoint when it has no active path.
    // Called once per update for every agent. Deterministic; no RNG.
    void advanceMoveQueue(std::size_t index);

    // A missed shot near `target` can wound a civilian bystander. Deterministic;
    // never affects combatants, so combat outcomes are unchanged.
    void applyStrayFire(std::size_t attacker, std::size_t target, float damage);

    // Damages every other living agent within `radius` tiles of `target`'s tile
    // (excluding the attacker and the primary target). Used by explosive weapons
    // with a non-zero area radius (issue #79).
    void applyAreaDamage(std::size_t attacker, std::size_t target, float damage, float radius);

    void evaluateOutcome();

    // True if any living player agent stands on the reach objective's tile.
    bool anyPlayerOnTile() const;

    // Index of the nearest living agent on `enemyTeam`, or -1.
    int nearestEnemy(std::size_t self, Team enemyTeam) const;

    // Straight-line distance between two agents, in tiles.
    float distanceBetween(std::size_t a, std::size_t b) const;

    // True when the straight line between the two agents' tiles is clear of
    // walls (LineOfSight). Range is not considered; a null grid is open. Used to
    // stop fire passing through walls.
    bool hasClearShot(std::size_t a, std::size_t b) const;

    // True when `observer` can see `target`: within sight range and with a clear
    // shot (hasClearShot). This is what stops combatants from acquiring targets
    // through walls.
    bool seesTarget(std::size_t observer, std::size_t target) const;

    const TileGrid* grid_ = nullptr;
    std::vector<Agent> agents_;
    std::vector<int> medikits_;
    Rng rng_;
    ObjectiveSpec objective_;
    float elapsedSeconds_ = 0.0f;
    MissionState state_ = MissionState::Running;
    int playerKills_ = 0;
    int enemyKills_ = 0;
    int civilianKills_ = 0;
};

} // namespace fossynd::core
