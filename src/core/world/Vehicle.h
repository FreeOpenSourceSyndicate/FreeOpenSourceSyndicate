/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/math/Vec2.h"
#include "core/sim/Weapon.h"
#include "core/world/TileGrid.h"

namespace fossynd::core
{

// Which kind of vehicle this is. Cars drive on the open street grid; trains run
// on rails. The movement model below is shared by both for this first slice — a
// rail-fixed route (a train that can only follow its track) is a follow-up, so
// `kind()` currently only selects the default stats and the display name.
enum class VehicleKind
{
    Car,
    Train,
};

// Default stats, in code until a vehicles catalogue lands under `assets/data`
// (a Content follow-up, tracked on issue #315). Values are original; no ties to
// any existing game's data.
inline constexpr float kCarSpeed = 6.0f;      // tiles/second (an agent walks at 2.0)
inline constexpr float kTrainSpeed = 9.0f;
inline constexpr float kCarMaxHealth = 150.0f;
inline constexpr float kTrainMaxHealth = 400.0f;

// A hijackable vehicle (car or train) with a mounted weapon.
//
// Engine-agnostic: positions are in tile space (fractional), so the renderer
// converts them to screen coordinates, and movement is driven by a fixed
// timestep exactly like `Agent`. Deterministic — the same path and the same
// sequence of `update` calls produce identical positions.
//
// The entity models only the vehicle itself. It does not know about the grid's
// other occupants, does not decide who may drive, and does not resolve weapon
// damage: `hijack` records *which* agent holds the wheel and `mountedWeapon`
// says what the driver may fire, but the policy (whether a hijack is legal, and
// what a hit does) belongs to `Mission` (Tactical). That keeps the entity
// testable on its own.
//
// A hijackable vehicle is occupied by at most one agent (the driver). In the
// original, an agent who enters a car drives it and its position becomes the
// car's; folding an agent's position into the vehicle's is Mission's job, not
// this entity's.
class Vehicle
{
public:
    Vehicle() = default;
    Vehicle(VehicleKind kind, float tileX, float tileY, float tilesPerSecond);

    // --- identity ---------------------------------------------------------

    VehicleKind kind() const { return kind_; }
    void setKind(VehicleKind kind) { kind_ = kind; }

    // Display name ("Car" / "Train" by default; settable).
    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // "Car" or "Train" for the vehicle's kind, for logs and the HUD.
    const char* kindName() const;

    // --- movement (same model as Agent: A* path + fixed timestep) ---------

    // Replaces the current path with one computed by A* to `goal`. Like
    // `Agent::moveTo`, when `goal` is unreachable (blocked, out of bounds, or
    // abandoned by the node cap) the vehicle drives to the reachable tile
    // nearest it instead of dropping the order. The path is cleared only when
    // there is genuinely nowhere to go. Returns true if a path was set.
    //
    // Movement does not require a driver: a scripted or runaway vehicle can
    // follow a path too. Who is *allowed* to issue a drive order is a policy
    // decision for the caller.
    bool driveTo(const TileGrid& grid, TilePos goal);

    // Clears the current path (the vehicle stops at its current position).
    void stop();

    // Advances the vehicle along its path and decays its fire cooldown. `dt` is
    // in seconds.
    void update(float dt);

    bool hasPath() const { return !path_.empty(); }
    const std::vector<TilePos>& path() const { return path_; }

    Vec2 position() const { return position_; }
    void setPosition(float tileX, float tileY);

    // Position at the start of the most recent update(). The renderer uses it
    // with interpolatedPosition() to draw smooth motion between fixed steps
    // (AGENTS.md golden rule 4). Never read by the simulation itself.
    Vec2 previousPosition() const { return previousPosition_; }

    // Linear blend from the previous step's position to the current one:
    // alpha 0 gives previousPosition(), alpha 1 gives position().
    Vec2 interpolatedPosition(float alpha) const;

    TilePos tile() const;

    float speed() const { return tilesPerSecond_; }
    void setSpeed(float tilesPerSecond) { tilesPerSecond_ = tilesPerSecond; }

    // --- occupancy / hijack ----------------------------------------------

    // Puts the agent at `agentIndex` behind the wheel. Returns false — and
    // changes nothing — when the vehicle is destroyed or another agent is
    // already driving it, or when `agentIndex` is negative. Re-hijacking with
    // the same index is a no-op that returns true.
    bool hijack(int agentIndex);

    bool isOccupied() const { return driver_ >= 0; }

    // Index of the agent driving the vehicle, or -1 when it is unoccupied.
    int driver() const { return driver_; }

    // Removes the driver (the vehicle stays where it is). No-op when empty.
    void eject() { driver_ = -1; }

    // --- health / destruction --------------------------------------------

    float health() const { return health_; }
    float maxHealth() const { return maxHealth_; }
    void setMaxHealth(float maxHealth);
    bool isAlive() const { return health_ > 0.0f; }
    bool isDestroyed() const { return !isAlive(); }

    // Fraction of health remaining in [0, 1] (1 for a full-health vehicle).
    float healthFraction() const;

    // Applies damage and returns the amount actually dealt. Destroying the
    // vehicle also stops it and ejects the driver — a wreck cannot be driven.
    // Damage never raises health above max, and a destroyed vehicle takes none.
    float applyDamage(float amount);

    // --- mounted fire -----------------------------------------------------

    const Weapon& mountedWeapon() const { return mounted_; }
    void setMountedWeapon(Weapon weapon) { mounted_ = std::move(weapon); }

    // The mounted weapon can fire only while the vehicle is intact and occupied
    // (an empty or wrecked vehicle has no gunner).
    bool canFire() const { return isAlive() && isOccupied() && fireCooldown_ <= 0.0f; }

    void beginFireCooldown() { fireCooldown_ = mounted_.cooldown(); }
    float fireCooldown() const { return fireCooldown_; }

private:
    VehicleKind kind_ = VehicleKind::Car;
    std::string name_ = "Car";

    Vec2 position_;
    Vec2 previousPosition_;
    float tilesPerSecond_ = kCarSpeed;
    std::vector<TilePos> path_;
    std::size_t nextWaypoint_ = 0;

    int driver_ = -1;

    float maxHealth_ = kCarMaxHealth;
    float health_ = kCarMaxHealth;

    Weapon mounted_ = makeUzi();
    float fireCooldown_ = 0.0f;
};

// A car: fast, light, uzi-armed. Original values.
Vehicle makeCar(float tileX, float tileY);

// A train: heavier and faster, rifle-armed.
Vehicle makeTrain(float tileX, float tileY);

} // namespace fossynd::core
