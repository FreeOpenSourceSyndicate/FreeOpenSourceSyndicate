/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/math/Vec2.h"
#include "core/sim/Weapon.h"
#include "core/world/TileGrid.h"

namespace foss::core
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

    // Replaces the current path with one computed by A* to `goal`. If no path
    // exists, the agent stops and the path is cleared. Returns true if a path
    // was found.
    bool moveTo(const TileGrid& grid, TilePos goal);

    // Clears the current path (the agent stops at its current position).
    void stop();

    // Advances the agent along its path and decays timers. `dt` is in seconds.
    void update(float dt);

    bool hasPath() const { return !path_.empty(); }
    const std::vector<TilePos>& path() const { return path_; }

    Vec2 position() const { return position_; }
    void setPosition(float tileX, float tileY);

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

private:
    Vec2 position_;
    float tilesPerSecond_ = 2.0f;
    std::vector<TilePos> path_;
    std::size_t nextWaypoint_ = 0;

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
};

} // namespace foss::core
