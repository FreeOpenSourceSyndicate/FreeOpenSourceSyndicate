/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/sim/Mission.h"
#include "core/world/LineOfSight.h"

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

FOSSYND_TEST(Mission, QueuedWaypointsAreVisitedInOrder)
{
    // A distant, unarmed enemy keeps the mission Running (no combat) so the
    // route can be observed. Two waypoints are queued while the first is being
    // walked; the agent must reach them in the order given.
    TileGrid grid(40, 40);
    Mission mission(grid, 30u);
    const std::size_t hero = mission.addAgent(
        makeAgent(1.0f, 1.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    mission.addAgent(makeAgent(39.0f, 39.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    mission.queueMove(hero, TilePos{1, 20});
    mission.queueMove(hero, TilePos{20, 20});
    mission.queueMove(hero, TilePos{20, 1});
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(2));

    // Tick in small steps and record each waypoint the agent reaches.
    std::vector<TilePos> reached;
    for (int i = 0; i < 1200; ++i)
    {
        mission.update(0.05f);
        const TilePos here = mission.agents()[hero].tile();
        if (reached.empty() || !(reached.back() == here))
        {
            reached.push_back(here);
        }
    }

    // The three queued goals appear in order in the visited-tile trail.
    auto indexOf = [&reached](TilePos goal) {
        for (std::size_t i = 0; i < reached.size(); ++i)
        {
            if (reached[i] == goal)
            {
                return i;
            }
        }
        return reached.size();
    };
    const std::size_t first = indexOf(TilePos{1, 20});
    const std::size_t second = indexOf(TilePos{20, 20});
    const std::size_t third = indexOf(TilePos{20, 1});
    FOSSYND_CHECK(first < reached.size());
    FOSSYND_CHECK(first < second);
    FOSSYND_CHECK(second < third);
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(0));
    FOSSYND_CHECK(mission.agents()[hero].tile() == (TilePos{20, 1}));
}

FOSSYND_TEST(Mission, OrderMoveReplacesQueuedRoute)
{
    TileGrid grid(40, 40);
    Mission mission(grid, 31u);
    const std::size_t hero = mission.addAgent(
        makeAgent(1.0f, 1.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    mission.addAgent(makeAgent(39.0f, 39.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    mission.queueMove(hero, TilePos{1, 20});
    mission.queueMove(hero, TilePos{20, 20});
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(1));

    // A plain move supersedes the queued route entirely.
    mission.orderMove(hero, TilePos{5, 5});
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(0));

    // It heads to the new goal, not the old queued waypoint.
    for (int i = 0; i < 400; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.agents()[hero].tile() == (TilePos{5, 5}));
}

FOSSYND_TEST(Mission, OrderAttackClearsQueuedRoute)
{
    TileGrid grid(40, 40);
    Mission mission(grid, 32u);
    const std::size_t hero = mission.addAgent(makeAgent(1.0f, 1.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(30.0f, 30.0f, Team::Enemy, 100.0f));

    mission.queueMove(hero, TilePos{1, 20});
    mission.queueMove(hero, TilePos{20, 20});
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(1));

    mission.orderAttack(hero, 1);
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(0));
    FOSSYND_CHECK_EQ(mission.agents()[hero].orderedTarget(), 1);
}

FOSSYND_TEST(Mission, ClearMoveQueueStopsAndEmptiesQueue)
{
    TileGrid grid(40, 40);
    Mission mission(grid, 33u);
    const std::size_t hero = mission.addAgent(
        makeAgent(1.0f, 1.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    mission.addAgent(makeAgent(39.0f, 39.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    mission.queueMove(hero, TilePos{1, 20});
    mission.queueMove(hero, TilePos{20, 20});
    mission.update(0.05f);

    mission.clearMoveQueue(hero);
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(0));
    FOSSYND_CHECK(!mission.agents()[hero].hasPath());

    // It stays put: no queued waypoint resumes the route.
    const Vec2 parked = mission.agents()[hero].position();
    for (int i = 0; i < 40; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_NEAR(length(mission.agents()[hero].position() - parked), 0.0f);
}

FOSSYND_TEST(Mission, QueueMoveRejectsDeadInvalidAndUnwalkable)
{
    TileGrid grid(20, 20);
    grid.setWalkable(5, 5, false); // a wall to try to queue onto
    Mission mission(grid, 34u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));

    // Unwalkable goal: no-op.
    mission.queueMove(hero, TilePos{5, 5});
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(0));
    FOSSYND_CHECK(!mission.agents()[hero].hasPath());

    // Out-of-range index: no-op, no crash.
    mission.queueMove(99, TilePos{3, 3});
    FOSSYND_CHECK_EQ(mission.moveQueueSize(99), static_cast<std::size_t>(0));
    mission.clearMoveQueue(99);

    // Dead agent: no-op.
    mission.agents()[hero].applyDamage(1000.0f);
    FOSSYND_CHECK(!mission.agents()[hero].isAlive());
    mission.queueMove(hero, TilePos{3, 3});
    FOSSYND_CHECK_EQ(mission.moveQueueSize(hero), static_cast<std::size_t>(0));
}

FOSSYND_TEST(Mission, WallBlocksEnemySightAndFire)
{
    // Player and a lethal enemy are six tiles apart. Without a wall the enemy
    // acquires and kills the player; with a wall between them the enemy never
    // sees the target and the player is unharmed.
    const auto runScenario = [](bool withWall) {
        TileGrid grid(20, 20);
        if (withWall)
        {
            grid.setWalkable(5, 1, false);
            grid.setWalkable(5, 2, false);
            grid.setWalkable(5, 3, false);
        }
        Mission mission(grid, 40u);
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
        mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f,
                                   Weapon{"Test", 1000.0f, 8.0f, 10.0f, 1.0f}));
        for (int i = 0; i < 60; ++i) // 3 seconds
        {
            mission.update(0.05f);
        }
        return mission;
    };

    Mission exposed = runScenario(false);
    Mission walled = runScenario(true);

    // No wall: the enemy saw the player and opened fire.
    FOSSYND_CHECK(exposed.agents()[0].health() < 100.0f);
    // Wall: the player was never hit, and the enemy never engaged.
    FOSSYND_CHECK_NEAR(walled.agents()[0].health(), 100.0f);
    FOSSYND_CHECK(walled.agents()[1].aiState() != AiState::Engage);
}

FOSSYND_TEST(Mission, ExplicitOrderDoesNotFireThroughWall)
{
    TileGrid grid(20, 20);
    grid.setWalkable(5, 2, false); // wall between the two agents
    Mission mission(grid, 41u);
    const std::size_t hero = mission.addAgent(
        makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 1000.0f, 10.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    mission.orderAttack(hero, 1);
    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }
    // Ordered fire is still blocked by the wall.
    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 100.0f);

    // Remove the wall: the same order now connects.
    grid.setWalkable(5, 2, true);
    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.agents()[1].health() < 100.0f);
}

FOSSYND_TEST(Mission, TargetAcquisitionSkipsEnemyBehindWall)
{
    // Two enemies: a nearer one behind a wall, a farther one in the open. The
    // player must target the one it can actually see.
    TileGrid grid(20, 20);
    grid.setWalkable(4, 2, false); // blocks the line to the near enemy at (6,2)
    Mission mission(grid, 42u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f,
                               Weapon{"Test", 1000.0f, 15.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f})); // near, walled
    mission.addAgent(makeAgent(2.0f, 9.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f})); // far, visible

    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }

    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 100.0f); // behind the wall: safe
    FOSSYND_CHECK(mission.agents()[2].health() < 100.0f);     // in the open: hit
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

// --- Area-of-effect weapons (issue #79) ------------------------------------
// Layout shared by the explosive tests: a player with an always-hitting weapon
// at (2,2) shoots the nearest enemy at (5,2); one enemy stands inside the blast
// (0.7 tiles away) and one far outside it (7 tiles away). The caller owns the
// grid (Mission keeps a non-owning pointer to it).
namespace
{
void addBlastSquad(Mission& mission, float areaRadius, float damage)
{
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f,
                               Weapon{"Boom", damage, 20.0f, 10.0f, 1.0f, areaRadius}));
    mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, 100.0f));  // primary target
    mission.addAgent(makeAgent(5.5f, 2.5f, Team::Enemy, 100.0f));  // inside the blast
    mission.addAgent(makeAgent(5.0f, 9.0f, Team::Enemy, 100.0f));  // well outside it
}
} // namespace

FOSSYND_TEST(Mission, ExplosiveHitDamagesBystanderButNotThePrimaryTwice)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 11u);
    addBlastSquad(mission, 2.0f, 40.0f);
    mission.update(0.05f);

    // The primary target takes exactly one hit (40 of 100), never two.
    FOSSYND_CHECK_NEAR(mission.agents()[1].healthFraction(), 0.6f);
    // The in-radius bystander is caught in the blast for the same damage.
    FOSSYND_CHECK_NEAR(mission.agents()[2].healthFraction(), 0.6f);
    // The far bystander is untouched: the blast is bounded by the radius.
    FOSSYND_CHECK_NEAR(mission.agents()[3].healthFraction(), 1.0f);
}

FOSSYND_TEST(Mission, ZeroAreaRadiusRemainsSingleTarget)
{
    // areaRadius == 0 is the default and must keep the old single-target rules.
    TileGrid grid(30, 30);
    Mission mission(grid, 11u);
    addBlastSquad(mission, 0.0f, 40.0f);
    mission.update(0.05f);

    FOSSYND_CHECK_NEAR(mission.agents()[1].healthFraction(), 0.6f); // primary hit once
    FOSSYND_CHECK_NEAR(mission.agents()[2].healthFraction(), 1.0f); // bystander untouched
    FOSSYND_CHECK_NEAR(mission.agents()[3].healthFraction(), 1.0f);
}

FOSSYND_TEST(Mission, BlastKillsCountAndCanKillCivilians)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 12u);
    // Lethal blast: one hit kills both the enemy and a civilian in the radius.
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f,
                               Weapon{"Boom", 100.0f, 20.0f, 10.0f, 1.0f, 2.0f}));
    mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, 50.0f));
    mission.addAgent(makeAgent(5.5f, 2.5f, Team::Civilian, 50.0f));

    mission.update(0.05f);

    FOSSYND_CHECK(!mission.agents()[1].isAlive());
    FOSSYND_CHECK(!mission.agents()[2].isAlive());
    FOSSYND_CHECK_EQ(mission.playerKills(), 1);
    FOSSYND_CHECK_EQ(mission.civilianKills(), 1);
}

FOSSYND_TEST(Mission, MedikitHealsMostWoundedAllyInRange)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 21u);
    // User at (2,2); two wounded allies: a graver one closer, a lighter one far.
    const std::size_t user = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Player)); // in range, graver
    mission.addAgent(makeAgent(3.5f, 2.0f, Team::Player)); // in range, lighter
    mission.addAgent(makeAgent(10.0f, 2.0f, Team::Player)); // out of range
    mission.agents()[1].applyDamage(70.0f); // 30 hp
    mission.agents()[2].applyDamage(20.0f); // 80 hp
    mission.agents()[3].applyDamage(90.0f); // 10 hp but out of range
    mission.setMedikits(user, 1);

    const float healed = mission.useMedikit(user);

    FOSSYND_CHECK_NEAR(healed, 40.0f);
    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 70.0f); // graver ally healed
    FOSSYND_CHECK_NEAR(mission.agents()[2].health(), 80.0f); // lighter ally untouched
    FOSSYND_CHECK_NEAR(mission.agents()[3].health(), 10.0f); // out of range untouched
    FOSSYND_CHECK_EQ(mission.medikits(user), 0);
}

FOSSYND_TEST(Mission, MedikitHealsTheUserWhenMostWounded)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 22u);
    const std::size_t user = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Player));
    mission.agents()[user].applyDamage(80.0f); // user is graver (20 hp)
    mission.agents()[1].applyDamage(30.0f);    // ally at 70 hp
    mission.setMedikits(user, 2);

    FOSSYND_CHECK_NEAR(mission.useMedikit(user), 40.0f);
    FOSSYND_CHECK_NEAR(mission.agents()[user].health(), 60.0f);
    FOSSYND_CHECK_EQ(mission.medikits(user), 1);
}

FOSSYND_TEST(Mission, MedikitIsNotWastedWhenAllInRangeAreHealthy)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 23u);
    const std::size_t user = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Player)); // full health
    mission.setMedikits(user, 1);

    FOSSYND_CHECK_NEAR(mission.useMedikit(user), 0.0f);
    FOSSYND_CHECK_EQ(mission.medikits(user), 1); // kit kept
}

FOSSYND_TEST(Mission, MedikitNoKitOrDeadUserIsNoOp)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 24u);
    const std::size_t user = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Player));
    mission.agents()[1].applyDamage(50.0f);

    // No kits: no-op, nothing healed.
    FOSSYND_CHECK_NEAR(mission.useMedikit(user), 0.0f);
    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 50.0f);

    // A dead user cannot use a kit.
    mission.setMedikits(user, 1);
    mission.agents()[user].applyDamage(1000.0f);
    FOSSYND_CHECK(!mission.agents()[user].isAlive());
    FOSSYND_CHECK_NEAR(mission.useMedikit(user), 0.0f);
    FOSSYND_CHECK_EQ(mission.medikits(user), 1);
}

FOSSYND_TEST(Mission, MedikitDoesNotCrossTeams)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 25u);
    const std::size_t user = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Enemy)); // wounded enemy in range
    mission.agents()[1].applyDamage(60.0f);
    mission.setMedikits(user, 1);

    FOSSYND_CHECK_NEAR(mission.useMedikit(user), 0.0f);
    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 40.0f); // enemy untouched
    FOSSYND_CHECK_EQ(mission.medikits(user), 1);
}

// --- Enemies take cover under fire (issue #120) ----------------------------

namespace
{

// A harmless, always-hitting weapon: low damage so nobody dies during the test,
// long range so the player can fire at the enemy from the start.
Weapon makeChipGun()
{
    return Weapon{"Test", 5.0f, 20.0f, 5.0f, 1.0f};
}

constexpr float kTough = 100000.0f;

bool agentIsHidden(const TileGrid& grid, const Mission& mission, std::size_t index)
{
    return !hasLineOfSight(grid, mission.agents()[0].tile(), mission.agents()[index].tile());
}

} // namespace

FOSSYND_TEST(Mission, EnemyStepsIntoCoverWhenHit)
{
    // Player (2,2) can see the enemy (5,2) along the open row y = 2. A wall at
    // (4,3) shadows the enemy's neighbours (5,3) and (6,3) without blocking the
    // player's line to (5,2), so cover is available one step away.
    TileGrid grid(20, 20);
    grid.setWalkable(4, 3, false);
    Mission mission(grid, 30u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, kTough, makeChipGun()));
    mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, kTough));

    // Before the first shot the enemy stands in the open, on the player's line.
    FOSSYND_CHECK(hasLineOfSight(grid, TilePos{2, 2}, TilePos{5, 2}));
    FOSSYND_CHECK(mission.agents()[1].tile() == (TilePos{5, 2}));

    for (int i = 0; i < 40; ++i) // 2 seconds
    {
        mission.update(0.05f);
    }

    // Hit with cover next door: the enemy has left the open tile and is no
    // longer visible to the player.
    FOSSYND_CHECK(mission.agents()[1].tile() != (TilePos{5, 2}));
    FOSSYND_CHECK(agentIsHidden(grid, mission, 1));
}

FOSSYND_TEST(Mission, EnemyHoldsCoverWhileStillUnderFire)
{
    TileGrid grid(20, 20);
    grid.setWalkable(4, 3, false);
    Mission mission(grid, 31u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, kTough, makeChipGun()));
    mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, kTough));

    for (int i = 0; i < 40; ++i)
    {
        mission.update(0.05f);
    }
    const TilePos inCover = mission.agents()[1].tile();
    FOSSYND_CHECK(agentIsHidden(grid, mission, 1));

    // Keep the shooter firing (the enemy is now hidden, so simulate the
    // sustained fire): the enemy must stay put in cover, not walk back out.
    for (int i = 0; i < 40; ++i)
    {
        mission.agents()[0].beginFireCooldown();
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.agents()[1].tile() == inCover);
    FOSSYND_CHECK(agentIsHidden(grid, mission, 1));
}

FOSSYND_TEST(Mission, EnemyWithoutCoverEngagesAsBefore)
{
    // Open ground: no cover anywhere, so the enemy behaves exactly as before
    // (closes to weapon range and fires).
    TileGrid grid(20, 20);
    Mission mission(grid, 32u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, kTough, makeChipGun()));
    mission.addAgent(makeAgent(10.0f, 2.0f, Team::Enemy, kTough));

    for (int i = 0; i < 40; ++i)
    {
        mission.update(0.05f);
    }
    // No cover exists, so the enemy stays engaged and closes the distance.
    FOSSYND_CHECK_EQ(mission.agents()[1].aiState(), AiState::Engage);
    FOSSYND_CHECK(mission.agents()[1].tile().x < 10);
}

