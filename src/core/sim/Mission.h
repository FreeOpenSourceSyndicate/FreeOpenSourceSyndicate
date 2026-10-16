/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/math/Vec2.h"
#include "core/sim/Agent.h"
#include "core/sim/Cybernetic.h"
#include "core/sim/Objective.h"
#include "core/util/Rng.h"
#include "core/world/TileGrid.h"
#include "core/world/Vehicle.h"

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
// reach a target tile, survive for a set time, escort a named agent to a tile,
// breach a target tile, or recruit a number of followers. See
// `core/sim/Objective.h`.
// Lose condition: every player agent is dead (and, for an escort, the escorted
// unit dying).
class Mission
{
public:
    explicit Mission(TileGrid& grid, std::uint64_t seed = 0xC0FFEEu);

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

    // --- Shield --------------------------------------------------------------

    // Sets how many shield charges the agent at `index` carries (clamped to
    // >= 0). Used by mission setup and tests. No-op for an invalid index.
    void setShields(std::size_t index, int count);

    // Number of shield charges the agent at `index` carries (0 if the index is
    // invalid).
    int shields(std::size_t index) const;

    // Spends one shield charge on the agent at `index` to raise a shield that
    // absorbs the next incoming hit. Returns true when a shield was raised, or
    // false if there is no charge, the index is invalid, the agent is not a
    // living combatant, or a shield is already raised (a charge is never wasted
    // re-raising a shield that is already up).
    bool activateShield(std::size_t index);

    // True when the agent at `index` currently has a shield raised.
    bool shieldUp(std::size_t index) const;

    // --- Scanner -------------------------------------------------------------

    // Sets how many scanner charges the agent at `index` carries (clamped to
    // >= 0). Used by mission setup and tests. No-op for an invalid index.
    void setScanners(std::size_t index, int count);

    // Number of scanner charges the agent at `index` carries (0 if the index is
    // invalid).
    int scanners(std::size_t index) const;

    // Spends one scanner charge on the agent at `index` to ping the surrounding
    // area: living enemies within `kScannerRadius` tiles are marked for
    // `kScannerPulseSeconds`. Returns true when a scan started, or false if
    // there is no charge, the index is invalid, the user is not a living
    // combatant, or a scan is already running (a charge is never wasted on a
    // re-scan). Deterministic; no RNG.
    bool activateScanner(std::size_t index);

    // Remaining seconds of the agent's active scan pulse (0 when none). The
    // render lane draws the ping from this and `scanTargets`.
    float scannerPulse(std::size_t index) const;

    // Living enemy agents currently marked by the agent's active scan, in
    // ascending index order. Empty when no scan is running. The sim never uses
    // the marks (enemies are not made visible to AI); this is the reveal the
    // view draws. Deterministic; no RNG.
    std::vector<std::size_t> scanTargets(std::size_t index) const;

    // --- Performance drugs ---------------------------------------------------

    // Sets how many drug doses the agent at `index` carries (clamped to >= 0).
    // Used by mission setup and tests. No-op for an invalid index.
    void setDrugs(std::size_t index, int count);

    // Number of drug doses the agent at `index` carries (0 if the index is
    // invalid).
    int drugs(std::size_t index) const;

    // Spends one dose on the agent at `index` to raise a temporary buff for
    // `kDrugDurationSeconds`: movement speed is multiplied by
    // `kDrugSpeedMultiplier` and hit chance by `kDrugAccuracyMultiplier`.
    // Returns true when the buff started, or false if there is no dose, the
    // index is invalid, the user is not a living combatant, or a buff is
    // already active (a dose is never wasted re-dosing).
    bool useDrugs(std::size_t index);

    // Remaining seconds of the agent's active drug buff (0 when none).
    float drugsTimer(std::size_t index) const;

    // --- cybernetic upgrades -------------------------------------------------

    // Applies a cybernetic upgrade to the agent at `index`: its `effect`
    // multiplies the agent's movement speed, weapon hit chance, and maximum
    // health, and sets its sight range. Applied once at mission load, so a
    // bought upgrade is a permanent, passive stat change (Milestone A). Returns
    // false (no change) for an invalid index, a civilian, or an unknown id; the
    // caller is `applyLoadout` (plus tests). Deterministic; no RNG.
    //
    // The accuracy multiplier is clamped to 1.0 so an upgrade can never make a
    // shot certain, and health is applied via `Agent::setMaxHealth` (which also
    // fills the extra health, so the upgrade is a clean gain, not a wound).
    bool applyCybernetic(std::size_t index, const std::string& id);

    // How far (tiles) the agent at `index` can see an enemy to acquire it. This
    // is the perception stat the eyes upgrade raises; it delegates to the agent's
    // `Agent::sightRange()` (World's contract, default `kDefaultSightRange`).
    // Returns `kDefaultSightRange` for an invalid index. Read by `seesTarget`;
    // presentation never uses it.
    float sightRange(std::size_t index) const;

    // Sets the agent's sight range (tiles), delegating to
    // `Agent::setSightRange` (negatives clamp to 0). A no-op for an invalid
    // index. Used by mission setup (an author who sets `sightRange` on an
    // `AgentSpec`) and tests.
    void setSightRange(std::size_t index, float range);

    // --- vehicles ------------------------------------------------------------
    //
    // The `core::Vehicle` entity (`core/world/Vehicle.h`, World area) is
    // deliberately policy-free: it knows how to drive a path and fire a mounted
    // weapon, but not who may take the wheel or what a hit does. `Mission` owns
    // that policy here, mirroring how it owns combat for `Agent`.

    // Adds a vehicle and returns its index (mirrors `addAgent`).
    std::size_t addVehicle(Vehicle vehicle);

    // The vehicles in this mission.
    std::vector<Vehicle>& vehicles() { return vehicles_; }
    const std::vector<Vehicle>& vehicles() const { return vehicles_; }

    // Orders the agent at `agentIndex` to hijack the vehicle at `vehicleIndex`.
    // Legal only for a living, non-driving combatant standing within
    // `kHijackRadius` tiles of the vehicle's tile (so "enter the tile" and
    // "adjacent" both count); the vehicle must be intact and undriven. On
    // success the agent becomes the driver and its position is folded onto the
    // vehicle's (the driver rides along). Returns false and changes nothing
    // otherwise. Deterministic; no RNG.
    bool orderHijack(std::size_t vehicleIndex, std::size_t agentIndex);

    // Ejects the driver of the vehicle at `vehicleIndex` (if any) and places it
    // on the vehicle's tile. A no-op for an invalid index or an empty vehicle.
    void ejectVehicle(std::size_t vehicleIndex);

    // Orders the vehicle at `vehicleIndex` to drive to `goal` via
    // `Vehicle::driveTo`. Only an occupied, intact vehicle may be driven (its
    // driver issues the order); returns false and changes nothing otherwise.
    bool orderDrive(std::size_t vehicleIndex, TilePos goal);

    // Orders the vehicle at `vehicleIndex` to focus its mounted fire on the
    // agent at `targetIndex` (any living agent; typically an enemy). Any target
    // may be named, matching `orderAttack`. A no-op for an invalid vehicle.
    void orderVehicleAttack(std::size_t vehicleIndex, std::size_t targetIndex);

    // Index of the agent driving the vehicle at `vehicleIndex`, or -1 when it is
    // unoccupied or the index is invalid.
    int vehicleDriver(std::size_t vehicleIndex) const;

    // Index of the vehicle whose tile holds the agent at `agentIndex`, or -1
    // when it is not in a vehicle.
    int vehicleOf(std::size_t agentIndex) const;

    // True when the agent at `agentIndex` is currently driving a vehicle. A
    // driving agent is carried by its vehicle: it is skipped by AI and personal
    // fire, and only the vehicle's mounted weapon fires.
    bool isDriving(std::size_t agentIndex) const;

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

    // Best cover tile for `index` against `attacker`: a walkable neighbour that
    // the attacker cannot see (hasClearShot false). Prefers the tile furthest
    // from the attacker; ties break by direction order. Returns false when no
    // neighbour is usable cover. Deterministic; no RNG (issue #120).
    bool findCoverTile(std::size_t index, std::size_t attacker, TilePos& out) const;
    void updateFollower(std::size_t index);
    void resolveFire();

    // Advances every vehicle, then folds each driver's position onto its
    // vehicle so the driver rides along. Called once per update. Deterministic.
    void updateVehicles(float dt);

    // The driver's own position follows its vehicle; an agent whose vehicle was
    // destroyed (and which is therefore no longer driving) is left where the
    // wreck stopped.
    void syncDrivers();

    // Mounted fire for every intact, occupied vehicle: honours an explicit
    // `vehicleTargets_` order or auto-acquires the nearest living enemy, applies
    // the same range/line-of-sight rules as an agent, and funnels damage through
    // `damageAgent`/`applyAreaDamage` (so a shield still covers the hit).
    // Deterministic; no RNG beyond the shared hit-chance roll.
    void resolveVehicleFire();

    // The single point where weapon damage reaches a vehicle (from an explosive
    // blast; an agent's own weapon never targets a vehicle). Returns the damage
    // dealt. Kept beside `damageAgent` so both entity kinds have one funnel.
    float damageVehicle(std::size_t index, float amount);

    // Starts the agent's next queued waypoint when it has no active path.
    // Called once per update for every agent. Deterministic; no RNG.
    void advanceMoveQueue(std::size_t index);

    // A missed shot near `target` can wound a civilian bystander. Deterministic;
    // never affects combatants, so combat outcomes are unchanged.
    void applyStrayFire(std::size_t attacker, std::size_t target, float damage);

    // The first blocked tile on the straight line between two tiles, or
    // `found = false` when the line is clear. Used by `resolveFire` to find the
    // wall a shot lands on when the target itself is not directly hittable. The
    // two endpoints are never returned; the walk matches `hasLineOfSight`, so a
    // tile is reported here exactly when it is what blocks the line.
    TilePos firstWallOnLine(TilePos from, TilePos to, bool& found) const;

    // Damages every other living agent within `radius` tiles of `target`'s tile
    // (excluding the attacker and the primary target). Used by explosive weapons
    // with a non-zero area radius (issue #79).
    void applyAreaDamage(std::size_t attacker, std::size_t target, float damage, float radius);

    // The single point where weapon damage reaches an agent. A raised shield
    // absorbs the hit entirely and drops; otherwise the damage is applied
    // normally. Returns the damage actually dealt (0 when a shield absorbed it).
    float damageAgent(std::size_t index, float amount);

    void evaluateOutcome();

    // True if any living player agent stands on the reach objective's tile.
    bool anyPlayerOnTile() const;

    // Escort: true when the designated agent (`objective_.agentIndex`) is alive
    // and standing on the target tile.
    bool escortedAgentOnTile() const;

    // Escort: true when the designated agent exists and is dead, i.e. the unit
    // the mission was to protect has been lost (an unconditional loss).
    bool escortUnitLost() const;

    // Destroy: true when the target tile has been breached (destructible rubble,
    // issue #217).
    bool targetTileBreached() const;

    // Persuade: true when the player's agents have recruited at least
    // `objective_.targetCount` living followers in total.
    bool followerTargetMet() const;

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

    TileGrid* grid_ = nullptr;
    std::vector<Agent> agents_;
    std::vector<Vehicle> vehicles_;
    // Explicit mounted-fire target per vehicle (agent index, or -1 for
    // auto-acquire), kept in step with `vehicles_`. Parallel to how an agent's
    // `orderedTarget` works, but held on the Mission so `core::Vehicle` stays
    // policy-free.
    std::vector<int> vehicleTargets_;
    std::vector<int> medikits_;
    std::vector<int> shields_;   // charges carried, sized to agents_
    std::vector<bool> shieldUp_; // raised shield per agent, sized to agents_
    std::vector<int> scanners_;      // charges carried, sized to agents_
    std::vector<float> scannerTimer_;// active scan pulse, sized to agents_
    std::vector<int> drugs_;         // doses carried, sized to agents_
    std::vector<float> drugsTimer_;  // active drug buff, sized to agents_
    Rng rng_;
    ObjectiveSpec objective_;
    float elapsedSeconds_ = 0.0f;
    MissionState state_ = MissionState::Running;
    int playerKills_ = 0;
    int enemyKills_ = 0;
    int civilianKills_ = 0;
};

} // namespace fossynd::core
