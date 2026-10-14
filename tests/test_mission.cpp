/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/sim/Mission.h"

using namespace fossynd::core;

namespace
{

Agent makeAgent(float x, float y, Team team, float health = 100.0f, Weapon weapon = makePistol())
{
    Agent agent(x, y, 2.0f);
    agent.setTeam(team);
    agent.setMaxHealth(health);
    agent.setWeapon(std::move(weapon));
    return agent;
}

// Runs the mission until it ends or `maxSeconds` elapse.
void runUntilDone(Mission& mission, float maxSeconds, float dt = 0.05f)
{
    for (float t = 0.0f; t < maxSeconds && mission.state() == MissionState::Running; t += dt)
    {
        mission.update(dt);
    }
}

} // namespace

FOSSYND_TEST(Mission, DamageKillsAgent)
{
    Agent agent(0.0f, 0.0f, 2.0f);
    agent.setMaxHealth(20.0f);
    FOSSYND_CHECK(agent.isAlive());
    FOSSYND_CHECK_NEAR(agent.applyDamage(15.0f), 15.0f);
    FOSSYND_CHECK(agent.isAlive());
    FOSSYND_CHECK_NEAR(agent.healthFraction(), 0.25f);
    agent.applyDamage(100.0f);
    FOSSYND_CHECK(!agent.isAlive());
    FOSSYND_CHECK_NEAR(agent.healthFraction(), 0.0f);
}

FOSSYND_TEST(Mission, DamageOnDeadAgentIsZero)
{
    Agent agent(0.0f, 0.0f, 2.0f);
    agent.setMaxHealth(10.0f);
    agent.applyDamage(10.0f);
    FOSSYND_CHECK(!agent.isAlive());
    FOSSYND_CHECK_NEAR(agent.applyDamage(5.0f), 0.0f);
}

FOSSYND_TEST(Mission, PlayerWinsWhenAllEnemiesDie)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 1u);
    Agent hero = makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 1000.0f, 5.0f, 10.0f, 1.0f});
    Agent foe = makeAgent(3.0f, 2.0f, Team::Enemy, 30.0f);
    const std::size_t heroIndex = mission.addAgent(std::move(hero));
    mission.addAgent(std::move(foe));

    mission.orderMove(heroIndex, TilePos{4, 2});
    runUntilDone(mission, 5.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK_EQ(mission.playerKills(), 1);
}

FOSSYND_TEST(Mission, PlayerLosesWhenSquadDies)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 2u);
    Agent hero = makeAgent(2.0f, 2.0f, Team::Player, 20.0f);
    Agent foe = makeAgent(3.0f, 2.0f, Team::Enemy, 100.0f, Weapon{"Test", 1000.0f, 5.0f, 10.0f, 1.0f});
    mission.addAgent(std::move(hero));
    mission.addAgent(std::move(foe));

    runUntilDone(mission, 5.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Lost);
    FOSSYND_CHECK_EQ(mission.enemyKills(), 1);
}

FOSSYND_TEST(Mission, EnemyClosesToWeaponRange)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 3u);
    // Player far from the enemy, holding fire (range 1) so it does not shoot.
    Agent hero = makeAgent(1.0f, 1.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f});
    Agent foe = makeAgent(1.0f, 11.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 3.0f, 10.0f, 0.0f});
    mission.addAgent(std::move(hero));
    mission.addAgent(std::move(foe));

    const float startDistance = mission.agents()[1].position().y -
                                mission.agents()[0].position().y;
    for (int i = 0; i < 200; ++i)
    {
        mission.update(0.05f);
    }
    const float endDistance = mission.agents()[1].position().y - mission.agents()[0].position().y;

    FOSSYND_CHECK(endDistance < startDistance);
    FOSSYND_CHECK(mission.state() == MissionState::Running);
}

FOSSYND_TEST(Mission, SameSeedProducesIdenticalOutcome)
{
    auto runScenario = [](std::uint64_t seed) {
        TileGrid grid(20, 20);
        Mission mission(grid, seed);
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
        mission.addAgent(makeAgent(8.0f, 8.0f, Team::Enemy, 100.0f));
        mission.orderMove(0, TilePos{6, 6});
        runUntilDone(mission, 6.0f);
        return std::make_pair(mission.state(), mission.playerKills());
    };

    const auto a = runScenario(777u);
    const auto b = runScenario(777u);
    FOSSYND_CHECK_EQ(a.first, b.first);
    FOSSYND_CHECK_EQ(a.second, b.second);
}

FOSSYND_TEST(Mission, NoOrdersStalemateStaysRunning)
{
    TileGrid grid(40, 40);
    Mission mission(grid, 5u);
    // Player out of the enemy's sight range; nobody can reach the other.
    mission.addAgent(makeAgent(1.0f, 1.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(35.0f, 35.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));

    for (int i = 0; i < 200; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.state() == MissionState::Running);
}

FOSSYND_TEST(Mission, AttackOrderFocusesFireOnChosenTarget)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 3u);
    // The hero is far from both enemies; the near one is closer, so auto-acquire
    // would pick it. The order must make the hero shoot the far one first.
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f,
                               Weapon{"Test", 1000.0f, 20.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 30.0f));  // near, index 1
    mission.addAgent(makeAgent(16.0f, 2.0f, Team::Enemy, 30.0f)); // far, index 2

    mission.orderAttack(0, 2);
    FOSSYND_CHECK_EQ(mission.agents()[0].orderedTarget(), 2);

    for (int i = 0; i < 40 && mission.agents()[2].isAlive(); ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(!mission.agents()[2].isAlive()); // far target eliminated first
    FOSSYND_CHECK(mission.agents()[1].isAlive());  // near target still standing
}

FOSSYND_TEST(Mission, MoveOrderClearsAttackOrder)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 4u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f));

    mission.orderAttack(0, 1);
    FOSSYND_CHECK_EQ(mission.agents()[0].orderedTarget(), 1);

    mission.orderMove(0, TilePos{3, 3});
    FOSSYND_CHECK_EQ(mission.agents()[0].orderedTarget(), -1);
}

FOSSYND_TEST(Mission, DeadTargetFallsBackToNearestEnemy)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 6u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f,
                               Weapon{"Test", 1000.0f, 20.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 30.0f));   // near, index 1
    mission.addAgent(makeAgent(16.0f, 2.0f, Team::Enemy, 1000.0f)); // far, index 2

    // Order fire on the near enemy until it dies, then the hero should continue
    // firing at the surviving enemy rather than stalling.
    mission.orderAttack(0, 1);
    for (int i = 0; i < 60 && mission.agents()[1].isAlive(); ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(!mission.agents()[1].isAlive());

    const float farHealthBefore = mission.agents()[2].health();
    for (int i = 0; i < 40; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.agents()[2].health() < farHealthBefore);
}


FOSSYND_TEST(Mission, CivilianIsNotAutoTargeted)
{
    // The civilian is the closest agent to the player, but combat must ignore it
    // and target the enemy. If civilians were auto-targeted the civilian would
    // die first and be counted as a kill.
    TileGrid grid(30, 30);
    Mission mission(grid, 11u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"Test", 1000.0f, 10.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian, 100.0f));
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    runUntilDone(mission, 10.0f);

    FOSSYND_CHECK(mission.state() == MissionState::Won);
    FOSSYND_CHECK(mission.agents()[1].isAlive());
    FOSSYND_CHECK_EQ(mission.civilianKills(), 0);
    FOSSYND_CHECK_EQ(mission.playerKills(), 1);
}

FOSSYND_TEST(Mission, StrayFireCanKillCivilians)
{
    // The player always misses (accuracy 0), so every shot is stray fire. A
    // bystander standing next to the target is hit as collateral.
    TileGrid grid(30, 30);
    Mission mission(grid, 12u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"Wild", 1000.0f, 20.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    mission.addAgent(makeAgent(6.5f, 2.0f, Team::Civilian, 100.0f));

    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }

    FOSSYND_CHECK(!mission.agents()[2].isAlive());
    FOSSYND_CHECK_EQ(mission.civilianKills(), 1);
    // The enemy is untouched: stray fire only ever harms civilians.
    FOSSYND_CHECK(mission.agents()[1].isAlive());
}

FOSSYND_TEST(Mission, CiviliansFleeGunfire)
{
    TileGrid grid(40, 40);
    Mission mission(grid, 13u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"Wild", 5.0f, 20.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    mission.addAgent(makeAgent(5.0f, 5.0f, Team::Civilian, 100.0f));  // near the fire
    mission.addAgent(makeAgent(30.0f, 30.0f, Team::Civilian, 100.0f)); // far away

    const Vec2 nearStart = mission.agents()[2].position();
    const Vec2 farStart = mission.agents()[3].position();

    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }

    // The bystander near the shooting runs; the distant one does not.
    FOSSYND_CHECK(length(mission.agents()[2].position() - nearStart) > 1.0f);
    FOSSYND_CHECK_NEAR(length(mission.agents()[3].position() - farStart), 0.0f);
}

FOSSYND_TEST(Mission, EnemyPatrolsAssignedRoute)
{
    // No target in sight (the player is far away and unarmed), so an enemy with
    // a patrol route should walk it instead of standing still.
    TileGrid grid(40, 40);
    Mission mission(grid, 20u);
    mission.addAgent(makeAgent(1.0f, 1.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    Agent patrol = makeAgent(30.0f, 30.0f, Team::Enemy, 100.0f,
                             Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f});
    patrol.setPatrolRoute({TilePos{35, 30}, TilePos{35, 35}, TilePos{30, 35}});
    const std::size_t patrolIndex = mission.addAgent(std::move(patrol));

    const Vec2 start = mission.agents()[patrolIndex].position();
    for (int i = 0; i < 60; ++i) // 3 seconds
    {
        mission.update(0.05f);
    }

    // It moved a real distance and is in the Patrol state.
    FOSSYND_CHECK(length(mission.agents()[patrolIndex].position() - start) > 2.0f);
    FOSSYND_CHECK(mission.agents()[patrolIndex].aiState() == AiState::Patrol);
}

FOSSYND_TEST(Mission, WoundedEnemyRetreats)
{
    // An enemy at low health near a player should break off and move away.
    TileGrid grid(40, 40);
    Mission mission(grid, 21u);
    mission.addAgent(makeAgent(5.0f, 5.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    const std::size_t foeIndex =
        mission.addAgent(makeAgent(8.0f, 5.0f, Team::Enemy, 100.0f,
                                   Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    // Wound it below the retreat threshold (35% of 100).
    mission.agents()[foeIndex].applyDamage(80.0f);

    const float startDistance =
        length(mission.agents()[foeIndex].position() - mission.agents()[0].position());
    for (int i = 0; i < 40; ++i) // 2 seconds
    {
        mission.update(0.05f);
    }
    const float endDistance =
        length(mission.agents()[foeIndex].position() - mission.agents()[0].position());

    FOSSYND_CHECK(mission.agents()[foeIndex].aiState() == AiState::Retreat);
    FOSSYND_CHECK(endDistance > startDistance + 2.0f);
}

FOSSYND_TEST(Mission, HealthyEnemyDoesNotRetreat)
{
    // The same setup at full health must engage, not retreat: the state machine
    // keys off health, not proximity.
    TileGrid grid(40, 40);
    Mission mission(grid, 22u);
    mission.addAgent(makeAgent(5.0f, 5.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    const std::size_t foeIndex =
        mission.addAgent(makeAgent(8.0f, 5.0f, Team::Enemy, 100.0f,
                                   Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.agents()[foeIndex].aiState() == AiState::Engage);
}

FOSSYND_TEST(Mission, RetreatingEnemyHoldsFire)
{
    // A retreating enemy is breaking off: even with a lethal weapon in range it
    // must not fire while withdrawing.
    TileGrid grid(40, 40);
    Mission mission(grid, 25u);
    mission.addAgent(makeAgent(5.0f, 5.0f, Team::Player, 1000.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    const std::size_t foeIndex = mission.addAgent(
        makeAgent(7.0f, 5.0f, Team::Enemy, 100.0f, Weapon{"Test", 1000.0f, 20.0f, 10.0f, 1.0f}));
    mission.agents()[foeIndex].applyDamage(80.0f); // 20 health -> retreat

    const float playerHealthBefore = mission.agents()[0].health();
    for (int i = 0; i < 60; ++i) // 3 seconds
    {
        mission.update(0.05f);
    }

    FOSSYND_CHECK(mission.agents()[foeIndex].aiState() == AiState::Retreat);
    FOSSYND_CHECK_NEAR(mission.agents()[0].health(), playerHealthBefore);
}

FOSSYND_TEST(Mission, EngagedEnemyAlertsNearbyAllies)
{
    // E0 sees the player and calls backup. E1 is within the alert radius of E0
    // but too far from the player to have acquired it on its own, so its order
    // can only have come from the alert.
    TileGrid grid(40, 40);
    Mission mission(grid, 23u);
    const std::size_t playerIndex = mission.addAgent(
        makeAgent(5.0f, 5.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    const std::size_t e0 = mission.addAgent(
        makeAgent(11.0f, 5.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    const std::size_t e1 = mission.addAgent(
        makeAgent(18.0f, 5.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    // Sanity: E1 starts with no order and cannot see the player (distance 13).
    FOSSYND_CHECK_EQ(mission.agents()[e1].orderedTarget(), -1);

    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }

    // E0 engaged the player and both now focus the player.
    FOSSYND_CHECK_EQ(mission.agents()[e0].aiState(), AiState::Engage);
    FOSSYND_CHECK_EQ(mission.agents()[e0].orderedTarget(), static_cast<int>(playerIndex));
    FOSSYND_CHECK_EQ(mission.agents()[e1].orderedTarget(), static_cast<int>(playerIndex));
}

FOSSYND_TEST(Mission, AlertAlliesOnlyReachesWithinRadius)
{
    TileGrid grid(40, 40);
    Mission mission(grid, 24u);
    mission.addAgent(makeAgent(5.0f, 5.0f, Team::Player, 100.0f));
    const std::size_t caller =
        mission.addAgent(makeAgent(9.0f, 5.0f, Team::Enemy, 100.0f));
    mission.addAgent(makeAgent(11.0f, 5.0f, Team::Enemy, 100.0f));  // 2 tiles: in
    mission.addAgent(makeAgent(30.0f, 5.0f, Team::Enemy, 100.0f));  // 21 tiles: out

    const std::size_t alerted = mission.alertAllies(caller, 8.0f);
    FOSSYND_CHECK_EQ(alerted, static_cast<std::size_t>(1));
    FOSSYND_CHECK_EQ(mission.agents()[2].orderedTarget(), 0);
    FOSSYND_CHECK_EQ(mission.agents()[3].orderedTarget(), -1);
}



// --- Objectives beyond "eliminate all" (issue #30) ------------------------

FOSSYND_TEST(Mission, DefaultObjectiveIsEliminateAll)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    FOSSYND_CHECK(mission.objective().kind == ObjectiveSpec::Kind::EliminateAll);
}

FOSSYND_TEST(Mission, ReachTileWinsWhenAPlayerStandsOnTheTile)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 4u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::ReachTile;
    objective.tileX = 4;
    objective.tileY = 2;
    mission.setObjective(objective);

    // A harmless enemy far away so the mission does not end by elimination.
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));

    mission.update(0.05f);
    FOSSYND_CHECK(mission.state() == MissionState::Running);
    FOSSYND_CHECK(mission.agents()[0].tile() != (TilePos{4, 2}));

    mission.orderMove(0, TilePos{4, 2});
    runUntilDone(mission, 10.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    // The enemy is still alive: the reach objective is what ended it.
    FOSSYND_CHECK_EQ(mission.aliveCount(Team::Enemy), static_cast<std::size_t>(1));
}

FOSSYND_TEST(Mission, ReachTileRequiresTheExactTile)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 5u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::ReachTile;
    objective.tileX = 10;
    objective.tileY = 10;
    mission.setObjective(objective);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));

    mission.orderMove(0, TilePos{4, 4});
    runUntilDone(mission, 6.0f);
    FOSSYND_CHECK(mission.state() == MissionState::Running);
}

FOSSYND_TEST(Mission, SurviveWinsAfterTheTimeElapses)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 6u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Survive;
    objective.seconds = 2.0f;
    mission.setObjective(objective);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));

    runUntilDone(mission, 10.0f);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK(mission.elapsedSeconds() >= 2.0f);
    FOSSYND_CHECK_EQ(mission.aliveCount(Team::Enemy), static_cast<std::size_t>(1));
}

FOSSYND_TEST(Mission, SurviveDoesNotWinEarly)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 7u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Survive;
    objective.seconds = 1.0f;
    mission.setObjective(objective);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));

    for (int i = 0; i < 10; ++i) // 0.5s total
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.state() == MissionState::Running);
}

FOSSYND_TEST(Mission, SquadDeathLosesEvenWithReachObjective)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 8u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::ReachTile;
    objective.tileX = 10;
    objective.tileY = 10;
    mission.setObjective(objective);
    // Weak player right next to a lethal enemy; it dies before reaching anything.
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Player, 10.0f));
    mission.addAgent(makeAgent(4.0f, 2.0f, Team::Enemy, 100.0f, Weapon{"Test", 1000.0f, 5.0f, 10.0f, 1.0f}));

    runUntilDone(mission, 5.0f);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Lost);
}

FOSSYND_TEST(Mission, ElapsedSecondsIsFrozenAfterTheMissionEnds)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 9u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Survive;
    objective.seconds = 0.5f;
    mission.setObjective(objective);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));

    runUntilDone(mission, 5.0f);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    const float wonAt = mission.elapsedSeconds();
    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_NEAR(mission.elapsedSeconds(), wonAt);
}
