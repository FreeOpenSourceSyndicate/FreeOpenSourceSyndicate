/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/MissionData.h"
#include "core/sim/Mission.h"
#include "core/sim/PlayerLoadout.h"
#include "core/world/LineOfSight.h"
#include "core/world/Pathfinding.h"
#include "core/world/Vehicle.h"

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

// --- Escort / destroy / persuade objectives (issue #281) -------------------
//
// Each new kind wins on its own condition, not by eliminating the enemy, and
// never on the wrong condition. The lose condition stays universal (squad dead)
// plus, for an escort, the escorted unit dying.

FOSSYND_TEST(Mission, EscortWinsWhenTheNamedAgentReachesTheTileAlive)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 31u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Escort;
    objective.tileX = 6;
    objective.tileY = 2;
    objective.agentIndex = 1; // the VIP is deliberately not index 0
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    // A harmless enemy far away so the mission does not end by elimination.
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));

    mission.orderMove(1, TilePos{6, 2});
    runUntilDone(mission, 10.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK_EQ(mission.agents()[1].tile(), (TilePos{6, 2}));
    FOSSYND_CHECK_EQ(mission.aliveCount(Team::Enemy), static_cast<std::size_t>(1));
}

FOSSYND_TEST(Mission, EscortDoesNotWinWhenAnotherAgentReachesTheTile)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 32u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Escort;
    objective.tileX = 6;
    objective.tileY = 2;
    objective.agentIndex = 1; // the VIP
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Player, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));

    // A different squad member walks onto the tile; the VIP stays put. Reaching
    // the tile with the wrong unit must not win.
    mission.orderMove(0, TilePos{6, 2});
    runUntilDone(mission, 8.0f);

    FOSSYND_CHECK(mission.state() == MissionState::Running);
    FOSSYND_CHECK_EQ(mission.agents()[0].tile(), (TilePos{6, 2}));
    FOSSYND_CHECK_EQ(mission.agents()[1].tile(), (TilePos{3, 2}));
}

FOSSYND_TEST(Mission, EscortLosesWhenTheNamedAgentDies)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 33u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Escort;
    objective.tileX = 18;
    objective.tileY = 2;
    objective.agentIndex = 1; // the VIP
    mission.setObjective(objective);

    // A second player far from the fight survives, so the loss is the VIP's
    // death and not the universal "squad wiped" rule.
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(10.0f, 2.0f, Team::Player, 10.0f)); // fragile VIP
    // A lethal guard adjacent to the VIP but out of range of the far survivor.
    mission.addAgent(makeAgent(11.0f, 2.0f, Team::Enemy, 100.0f, Weapon{"Test", 1000.0f, 3.0f, 10.0f, 1.0f}));

    runUntilDone(mission, 5.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Lost);
    // The squad is not wiped: it is the lost VIP that ended the mission.
    FOSSYND_CHECK(mission.aliveCount(Team::Player) >= static_cast<std::size_t>(1));
}

FOSSYND_TEST(Mission, DestroyWinsWhenTheTargetTileIsBreached)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 34u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Destroy;
    objective.tileX = 5;
    objective.tileY = 2;
    mission.setObjective(objective);

    // The structure is a one-hit-point destructible wall.
    grid.setWalkable(5, 2, false);
    grid.setDestructible(5, 2, true, 1);

    // A decoy enemy sits *behind* the wall: the explicit attack order makes the
    // shot land on the wall (a wall blocks fire), chipping it. A successful hit
    // on the wall is what breaches it, not killing the defender.
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"Breach", 50.0f, 9.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.orderAttack(0, 1);

    FOSSYND_CHECK(!grid.isRubble(5, 2));
    runUntilDone(mission, 5.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK(grid.isRubble(5, 2));
    // The win is the breach, not a kill: the enemy behind the wall is alive.
    FOSSYND_CHECK_EQ(mission.aliveCount(Team::Enemy), static_cast<std::size_t>(1));
}

FOSSYND_TEST(Mission, DestroyDoesNotWinBeforeTheTileIsBreached)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 35u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Destroy;
    objective.tileX = 5;
    objective.tileY = 2;
    mission.setObjective(objective);

    // A tough wall: a few hits chip it but do not breach it in the window below.
    grid.setWalkable(5, 2, false);
    grid.setDestructible(5, 2, true, 1000);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"Breach", 50.0f, 9.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f, Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.orderAttack(0, 1);

    for (int i = 0; i < 6; ++i) // 0.3s: several shots, not enough to breach
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.state() == MissionState::Running);
    FOSSYND_CHECK(!grid.isRubble(5, 2));
    FOSSYND_CHECK(grid.tileHitPoints(5, 2) < 1000); // it was hit, just not breached
}

FOSSYND_TEST(Mission, PersuadeWinsWhenEnoughCiviliansAreRecruited)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 36u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Persuade;
    objective.targetCount = 2;
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));    // 0
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian));  // 1, nearest
    mission.addAgent(makeAgent(3.0f, 3.0f, Team::Civilian));  // 2
    // A far, harmless enemy keeps the mission running between beacon uses.
    mission.addAgent(makeAgent(28.0f, 28.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    FOSSYND_CHECK_EQ(mission.persuade(0), 1);
    mission.update(0.05f);
    // One follower is below the target of two.
    FOSSYND_CHECK(mission.state() == MissionState::Running);

    for (int i = 0; i < 30; ++i) // wait out the beacon cooldown (1.0s)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_EQ(mission.persuade(0), 2);
    mission.update(0.05f);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK(mission.followerCount(0) >= static_cast<std::size_t>(2));
}

FOSSYND_TEST(Mission, PersuadeDoesNotWinBelowTheTarget)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 37u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Persuade;
    objective.targetCount = 3; // more than the two civilians that exist
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian));
    mission.addAgent(makeAgent(3.0f, 3.0f, Team::Civilian));
    mission.addAgent(makeAgent(28.0f, 28.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    FOSSYND_CHECK_EQ(mission.persuade(0), 1);
    for (int i = 0; i < 30; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_EQ(mission.persuade(0), 2);

    runUntilDone(mission, 3.0f);
    FOSSYND_CHECK(mission.state() == MissionState::Running);
}

FOSSYND_TEST(Mission, DestroyObjectiveIsDeterministic)
{
    auto runScenario = [](std::uint64_t seed) {
        TileGrid grid(20, 20);
        Mission mission(grid, seed);
        ObjectiveSpec objective;
        objective.kind = ObjectiveSpec::Kind::Destroy;
        objective.tileX = 5;
        objective.tileY = 2;
        mission.setObjective(objective);
        grid.setWalkable(5, 2, false);
        grid.setDestructible(5, 2, true, 1);
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"Breach", 50.0f, 9.0f, 10.0f, 1.0f}));
        mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f,
                                   Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
        mission.orderAttack(0, 1);
        runUntilDone(mission, 5.0f);
        return std::make_pair(mission.state(), grid.breachEpoch());
    };

    const auto a = runScenario(909u);
    const auto b = runScenario(909u);
    FOSSYND_CHECK_EQ(a.first, b.first);
    FOSSYND_CHECK_EQ(a.second, b.second);
    FOSSYND_CHECK_EQ(a.first, MissionState::Won);
}

// --- Assassinate / retrieve objectives (issue #444) ------------------------
//
// Two more kinds from the Definition of Done's objective set. `assassinate`
// wins on one named agent's death, not the whole garrison; `retrieve` wins when
// the item is picked up and carried to the exit tile. Each wins on its own
// condition and never on the wrong one; the universal squad-dead loss is
// unchanged.

FOSSYND_TEST(Mission, AssassinateWinsWhenTheNamedTargetDies)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 41u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Assassinate;
    objective.agentIndex = 2; // the VIP is deliberately not index 0 or 1
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"Rifle", 100.0f, 9.0f, 10.0f, 1.0f}));
    // A second, tougher enemy that must *survive*: the win is the named kill,
    // not the garrison's elimination.
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    mission.addAgent(makeAgent(4.0f, 2.0f, Team::Enemy, 40.0f)); // the target

    mission.orderAttack(0, 2);
    runUntilDone(mission, 10.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK(!mission.agents()[2].isAlive());
    // The garrison was not wiped: another enemy is still alive.
    FOSSYND_CHECK(mission.aliveCount(Team::Enemy) >= static_cast<std::size_t>(1));
}

FOSSYND_TEST(Mission, AssassinateDoesNotWinBeforeTheTargetDies)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 42u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Assassinate;
    objective.agentIndex = 1;
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    for (int i = 0; i < 40; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.state() == MissionState::Running);
    FOSSYND_CHECK(mission.agents()[1].isAlive());
}

FOSSYND_TEST(Mission, AssassinateOfAMissingTargetNeverWins)
{
    // A hand-built spec that names no agent is unsatisfiable (the loader rejects
    // it), so it must not spuriously win.
    TileGrid grid(20, 20);
    Mission mission(grid, 43u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Assassinate;
    objective.agentIndex = 99;
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.state() == MissionState::Running);
}

FOSSYND_TEST(Mission, RetrieveWinsWhenTheItemIsCarriedToTheExit)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 44u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Retrieve;
    objective.itemX = 4;
    objective.itemY = 2;
    objective.tileX = 18;
    objective.tileY = 2;
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    // A harmless far enemy keeps the mission running between the two legs.
    mission.addAgent(makeAgent(28.0f, 28.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    // Leg 1: pick the item up. The mission must not end merely on pickup.
    mission.orderMove(0, TilePos{4, 2});
    runUntilDone(mission, 8.0f);
    FOSSYND_CHECK(mission.state() == MissionState::Running);
    FOSSYND_CHECK_EQ(mission.agents()[0].tile(), (TilePos{4, 2}));

    // Leg 2: carry it to the exit.
    mission.orderMove(0, TilePos{18, 2});
    runUntilDone(mission, 20.0f);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK_EQ(mission.agents()[0].tile(), (TilePos{18, 2}));
}

FOSSYND_TEST(Mission, RetrieveDoesNotWinOnTheExitTileBeforePickup)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 45u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Retrieve;
    objective.itemX = 4;
    objective.itemY = 2;
    objective.tileX = 18;
    objective.tileY = 2;
    mission.setObjective(objective);

    mission.addAgent(makeAgent(16.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(28.0f, 28.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    // Walk straight onto the exit without ever touching the item tile.
    mission.orderMove(0, TilePos{18, 2});
    runUntilDone(mission, 6.0f);
    FOSSYND_CHECK(mission.state() == MissionState::Running);
    FOSSYND_CHECK_EQ(mission.agents()[0].tile(), (TilePos{18, 2}));
}

FOSSYND_TEST(Mission, RetrieveItemSurvivesThePickerUpAndIsCarriedByAnother)
{
    // The pickup is a squad-level latch, not tied to one unit: a second member
    // may deliver the item after the first has picked it up.
    TileGrid grid(30, 30);
    Mission mission(grid, 46u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Retrieve;
    objective.itemX = 4;
    objective.itemY = 2;
    objective.tileX = 18;
    objective.tileY = 2;
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(2.0f, 4.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(28.0f, 28.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    // Agent 0 picks the item up; agent 1 stays back and then delivers it.
    mission.orderMove(0, TilePos{4, 2});
    runUntilDone(mission, 8.0f);
    FOSSYND_CHECK(mission.state() == MissionState::Running);

    mission.orderMove(1, TilePos{18, 2});
    runUntilDone(mission, 20.0f);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
}

FOSSYND_TEST(Mission, RetrieveLosesWhenTheSquadDiesAfterPickup)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 47u);
    ObjectiveSpec objective;
    objective.kind = ObjectiveSpec::Kind::Retrieve;
    objective.itemX = 4;
    objective.itemY = 2;
    objective.tileX = 18;
    objective.tileY = 2;
    mission.setObjective(objective);

    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(28.0f, 28.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    mission.orderMove(0, TilePos{4, 2});
    runUntilDone(mission, 8.0f);
    FOSSYND_CHECK(mission.state() == MissionState::Running);

    // The universal lose condition still wins over a retrieve objective.
    mission.agents()[0].applyDamage(1000.0f);
    mission.update(0.05f);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Lost);
}

FOSSYND_TEST(Mission, RetrieveObjectiveIsDeterministic)
{
    auto runScenario = [](std::uint64_t seed) {
        TileGrid grid(30, 30);
        Mission mission(grid, seed);
        ObjectiveSpec objective;
        objective.kind = ObjectiveSpec::Kind::Retrieve;
        objective.itemX = 4;
        objective.itemY = 2;
        objective.tileX = 18;
        objective.tileY = 2;
        mission.setObjective(objective);
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"None", 0.0f, 1.0f, 10.0f, 0.0f}));
        mission.addAgent(makeAgent(28.0f, 28.0f, Team::Enemy, 100000.0f,
                                   Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
        mission.orderMove(0, TilePos{4, 2});
        for (int i = 0; i < 120; ++i)
        {
            mission.update(0.05f);
        }
        mission.orderMove(0, TilePos{18, 2});
        for (int i = 0; i < 300; ++i)
        {
            mission.update(0.05f);
        }
        return std::make_pair(mission.state(), mission.elapsedSeconds());
    };

    const auto a = runScenario(910u);
    const auto b = runScenario(910u);
    FOSSYND_CHECK_EQ(a.first, b.first);
    FOSSYND_CHECK_NEAR(a.second, b.second);
    FOSSYND_CHECK_EQ(a.first, MissionState::Won);
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


// --- destructible walls (issue #163) ---------------------------------------
//
// `TileGrid::damageTile` (#157) lets weapon fire open a blocked tile. These
// tests pin the tactical half: a shot that lands on a destructible wall chips
// it, a breach opens the route for a later path search, and a solid wall is
// unchanged. The firing weapon has accuracy 0, so it never hits an agent and
// every shot is an "impact" that lands on the wall in the line of fire — the
// scenario is fully deterministic with no RNG.

FOSSYND_TEST(Mission, FireChipsDestructibleWallUntilBreached)
{
    TileGrid grid(20, 20);
    grid.setWalkable(5, 2, false);
    grid.setDestructible(5, 2, true, 100); // 100 HP, weapon chips 10 per shot
    Mission mission(grid, 43u);
    const std::size_t hero = mission.addAgent(
        makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 10.0f, 10.0f, 2.0f, 0.0f}));
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    mission.orderAttack(hero, 1);

    // A short burst chips the wall but does not breach it.
    for (int i = 0; i < 15; ++i)
    {
        mission.update(0.05f);
    }
    const int chipped = grid.tileHitPoints(5, 2);
    FOSSYND_CHECK(chipped < 100);
    FOSSYND_CHECK(chipped > 0);
    FOSSYND_CHECK(!grid.isWalkable(5, 2));
    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 100.0f); // never shot through

    // Sustained fire breaches it: walkable, no longer destructible.
    for (int i = 0; i < 300; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(grid.isWalkable(5, 2));
    FOSSYND_CHECK(!grid.isDestructible(5, 2));
    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 100.0f);
}

FOSSYND_TEST(Mission, BreachedWallOpensTheRouteForPathfinding)
{
    TileGrid grid(8, 5);
    grid.setWalkable(4, 0, false);
    grid.setWalkable(4, 1, false);
    grid.setWalkable(4, 2, false);
    grid.setWalkable(4, 3, false);
    grid.setWalkable(4, 4, false); // full-height wall splits the board
    grid.setDestructible(4, 2, true, 5);

    // The sealed wall leaves no route across.
    FOSSYND_CHECK(findPath(grid, TilePos{1, 2}, TilePos{6, 2}).empty());

    Mission mission(grid, 44u);
    const std::size_t hero = mission.addAgent(
        makeAgent(1.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 10.0f, 10.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));
    mission.orderAttack(hero, 1);

    // One shot at (4,2) does 10 damage against 5 HP: the wall opens.
    for (int i = 0; i < 25; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(grid.isWalkable(4, 2));
    FOSSYND_CHECK(!findPath(grid, TilePos{1, 2}, TilePos{6, 2}).empty());
}

FOSSYND_TEST(Mission, FireLeavesANonDestructibleWallIntact)
{
    TileGrid grid(20, 20);
    grid.setWalkable(5, 2, false); // solid wall: never marked destructible
    Mission mission(grid, 45u);
    const std::size_t hero = mission.addAgent(
        makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 10.0f, 10.0f, 10.0f, 0.0f}));
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    mission.orderAttack(hero, 1);
    for (int i = 0; i < 60; ++i) // 3 seconds of sustained fire
    {
        mission.update(0.05f);
    }

    FOSSYND_CHECK(!grid.isWalkable(5, 2)); // unchanged
    FOSSYND_CHECK(!grid.isDestructible(5, 2));
    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 100.0f);
}

// --- explosives breach walls (issue #185) ----------------------------------
//
// An explosive weapon's blast damages everything within its area radius. It also
// opens destructible walls in that radius, so a rocket can breach a room the way
// a blocked shot chips the one tile it lands on. `breachTilesInBlast` bounds the
// chip to the blast radius, so a wall outside it is left standing.

FOSSYND_TEST(Mission, ExplosiveBlastBreachesWallsInItsRadius)
{
    TileGrid grid(30, 30);
    // The target stands in the open at (5,2); destructible walls flank the
    // impact at (5,1) and (6,2) (1 tile away, inside a 2.0 radius) and at (4,3)
    // (1.4 tiles away). A far wall at (5,6) is well outside the blast.
    grid.setWalkable(5, 1, false);
    grid.setDestructible(5, 1, true, 20);
    grid.setWalkable(6, 2, false);
    grid.setDestructible(6, 2, true, 20);
    grid.setWalkable(4, 3, false);
    grid.setDestructible(4, 3, true, 20);
    grid.setWalkable(5, 6, false);
    grid.setDestructible(5, 6, true, 20); // outside the radius

    Mission mission(grid, 61u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f,
                               Weapon{"Boom", 40.0f, 20.0f, 10.0f, 1.0f, 2.0f}));
    mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, 100.0f)); // primary target

    mission.update(0.05f);

    FOSSYND_CHECK(grid.isWalkable(5, 1));  // in radius
    FOSSYND_CHECK(grid.isWalkable(6, 2));  // in radius
    FOSSYND_CHECK(grid.isWalkable(4, 3));  // in radius (diagonal)
    FOSSYND_CHECK(!grid.isWalkable(5, 6)); // outside the blast: untouched
    FOSSYND_CHECK(grid.isDestructible(5, 6));
}

FOSSYND_TEST(Mission, ExplosiveBlastLeavesASolidWallStanding)
{
    TileGrid grid(30, 30);
    grid.setWalkable(5, 2, false); // solid wall: never marked destructible
    Mission mission(grid, 62u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f,
                               Weapon{"Boom", 40.0f, 20.0f, 10.0f, 1.0f, 2.0f}));
    mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, 100.0f));

    mission.update(0.05f);

    FOSSYND_CHECK(!grid.isWalkable(5, 2)); // a blast cannot open solid rock
    FOSSYND_CHECK(!grid.isDestructible(5, 2));
}

FOSSYND_TEST(Mission, ExplosiveBlastBreachesWallEvenWhenTargetIsInTheOpen)
{
    // The old direct-shot path only chipped a wall on the firing line; an
    // explosion must breach a wall beside the impact even with a clear shot to
    // the target.
    TileGrid grid(30, 30);
    grid.setWalkable(5, 3, false);
    grid.setDestructible(5, 3, true, 10);
    Mission mission(grid, 63u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 500.0f,
                               Weapon{"Boom", 40.0f, 20.0f, 10.0f, 1.0f, 2.0f}));
    mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, 100.0f)); // clear line

    mission.update(0.05f);

    FOSSYND_CHECK(grid.isWalkable(5, 3));
    FOSSYND_CHECK(!grid.isDestructible(5, 3));
}

// --- shield gadget (issue #211) --------------------------------------------
//
// The shield lives on the Mission (like the medikit), so the World-owned Agent
// contract is untouched. It is a one-hit buffer: a raised shield absorbs the
// next incoming hit entirely and drops, and the hit after that lands normally.

FOSSYND_TEST(Mission, ShieldAbsorbsTheNextHitThenDrops)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 51u);
    // Player holds position (no player AI) and always hits; the enemy carries a
    // long-range, zero-accuracy weapon so it neither closes nor kills anyone.
    const std::size_t hero = mission.addAgent(
        makeAgent(3.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 10.0f, 10.0f, 10.0f, 1.0f}));
    const std::size_t foe = mission.addAgent(makeAgent(4.0f, 2.0f, Team::Enemy, 100.0f,
                                                      Weapon{"None", 0.0f, 10.0f, 1.0f, 0.0f}));
    (void)hero;

    mission.setShields(foe, 1);
    FOSSYND_CHECK(mission.activateShield(foe));
    FOSSYND_CHECK(mission.shieldUp(foe));
    FOSSYND_CHECK_EQ(mission.shields(foe), 0);

    // The first hit is absorbed: the shield drops and no health is lost.
    int guard = 0;
    while (mission.shieldUp(foe) && guard++ < 200)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(!mission.shieldUp(foe));
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 100.0f);

    // The next hit lands normally.
    guard = 0;
    while (mission.agents()[foe].health() >= 100.0f && guard++ < 200)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 90.0f);
}

FOSSYND_TEST(Mission, ShieldAbsorbsAnExplosiveHit)
{
    // Area damage also funnels through the shield: the blast is absorbed whole
    // (the primary target loses nothing) and the shield drops.
    TileGrid grid(20, 20);
    Mission mission(grid, 52u);
    const std::size_t hero = mission.addAgent(
        makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 40.0f, 10.0f, 10.0f, 1.0f, 2.0f}));
    const std::size_t foe = mission.addAgent(makeAgent(4.0f, 2.0f, Team::Enemy, 100.0f,
                                                      Weapon{"None", 0.0f, 10.0f, 1.0f, 0.0f}));
    (void)hero;

    mission.setShields(foe, 1);
    FOSSYND_CHECK(mission.activateShield(foe));

    int guard = 0;
    while (mission.shieldUp(foe) && guard++ < 200)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(!mission.shieldUp(foe));
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 100.0f); // blast absorbed
}

FOSSYND_TEST(Mission, ShieldWithNoChargeOrOnACivilianIsNoOp)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 53u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const std::size_t bystander = mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian, 20.0f));
    (void)hero;

    // No charges: nothing happens, no shield is raised.
    FOSSYND_CHECK(!mission.activateShield(bystander));
    FOSSYND_CHECK(!mission.shieldUp(bystander));

    // Civilians cannot raise a shield even when handed a charge.
    mission.setShields(bystander, 1);
    FOSSYND_CHECK(!mission.activateShield(bystander));
    FOSSYND_CHECK(!mission.shieldUp(bystander));
    FOSSYND_CHECK_EQ(mission.shields(bystander), 1); // charge untouched

    // A dead agent cannot raise one either.
    const std::size_t dead = mission.addAgent(makeAgent(5.0f, 5.0f, Team::Enemy, 10.0f));
    mission.setShields(dead, 1);
    FOSSYND_CHECK(mission.activateShield(dead));
    mission.agents()[dead].applyDamage(100.0f);
    FOSSYND_CHECK(!mission.activateShield(dead));
}

FOSSYND_TEST(Mission, ShieldDoesNotStackWhileAlreadyUp)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 54u);
    const std::size_t foe = mission.addAgent(makeAgent(4.0f, 2.0f, Team::Enemy, 100.0f));

    mission.setShields(foe, 2);
    FOSSYND_CHECK(mission.activateShield(foe));
    // Re-activating while a shield is already up must not burn the second charge.
    FOSSYND_CHECK(!mission.activateShield(foe));
    FOSSYND_CHECK_EQ(mission.shields(foe), 1);
}

FOSSYND_TEST(Mission, AttackOrderHoldsPositionWithClearShot)
{
    // In range with an unobstructed line: the agent must hold position (no
    // path) and fire. Guards against the #229 fix over-eagerly moving an agent
    // that can already shoot.
    TileGrid grid(20, 20);
    Mission mission(grid, 46u);
    const std::size_t hero = mission.addAgent(
        makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 10.0f, 10.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, 1000.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    mission.orderAttack(hero, 1);
    FOSSYND_CHECK(!mission.agents()[hero].hasPath()); // in range + clear shot: holds

    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.agents()[1].health() < 1000.0f);
}

FOSSYND_TEST(Mission, AttackOrderMovesWhenWallBlocksShot)
{
    // Regression for #229: the enemy is inside weapon range by distance but a
    // solid wall blocks the line, so the agent can neither shoot from here nor
    // stand still. It must keep moving (around the wall) and eventually fire,
    // instead of freezing and deadlocking the mission.
    TileGrid grid(20, 20);
    grid.setWalkable(5, 2, false); // solid wall between the two agents
    Mission mission(grid, 47u);
    const std::size_t hero = mission.addAgent(
        makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 10.0f, 10.0f, 10.0f, 1.0f}));
    // Far enough to stay alive, unarmed so it cannot shoot back.
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 1000.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    // Sanity: the target is in range (distance 6 <= 9) but not visible.
    mission.orderAttack(hero, 1);
    FOSSYND_CHECK(mission.agents()[hero].hasPath()); // not frozen: has a route

    for (int i = 0; i < 120; ++i)
    {
        mission.update(0.05f);
    }
    // The agent rounded the wall and opened fire.
    FOSSYND_CHECK(mission.agents()[1].health() < 1000.0f);
}

// --- squad loadout (Milestone B deployment, docs/METAGAME.md §7 R1) ---------

namespace
{

// A mission with two player agents (index 0, 1) and one enemy (index 2), so the
// tests can prove slots map onto players only.
MissionData loadoutFixture()
{
    MissionData data;
    data.width = 12;
    data.height = 12;

    AgentSpec hero;
    hero.team = Team::Player;
    hero.x = 2.0f;
    hero.y = 2.0f;
    hero.weapon = Weapon{"Authored", 3.0f, 4.0f, 1.0f, 0.5f};
    data.agents.push_back(hero);

    AgentSpec mate;
    mate.team = Team::Player;
    mate.x = 3.0f;
    mate.y = 2.0f;
    mate.weapon = makePistol();
    data.agents.push_back(mate);

    AgentSpec foe;
    foe.team = Team::Enemy;
    foe.x = 8.0f;
    foe.y = 8.0f;
    foe.weapon = makePistol();
    data.agents.push_back(foe);
    return data;
}

WeaponCatalogue loadoutWeapons()
{
    WeaponCatalogue weapons;
    std::string error;
    const Json root = Json::parse(
        R"({"weapons":[{"id":"rifle","name":"Rifle","damage":9,"range":9,"fireRate":4,"accuracy":0.7}]})",
        error);
    FOSSYND_CHECK(weapons.loadFromJson(root, error));
    return weapons;
}

} // namespace

FOSSYND_TEST(Mission, LoadoutOverridesPlayerWeapon)
{
    const MissionData data = loadoutFixture();
    TileGrid grid = buildGrid(data);
    Mission mission(grid, 5u);
    populateMission(data, mission);

    PlayerLoadout loadout;
    loadout.slots.resize(1);
    loadout.slots[0].weaponId = "rifle";

    std::string error;
    const WeaponCatalogue weapons = loadoutWeapons();
    FOSSYND_CHECK(applyLoadout(loadout, mission, weapons, error));
    FOSSYND_CHECK(error.empty());

    // Slot 0 (the first player) now carries the bought weapon...
    FOSSYND_CHECK_EQ(mission.agents()[0].weapon().name, std::string("Rifle"));
    // ...and it is the weapon the sim would fire (not a copy left behind).
    FOSSYND_CHECK_NEAR(mission.agents()[0].weapon().damage, 9.0f);
    // Slot 1 keeps the authored default, and the enemy is never touched.
    FOSSYND_CHECK_EQ(mission.agents()[1].weapon().name, std::string("Pistol"));
    FOSSYND_CHECK_EQ(mission.agents()[2].weapon().name, std::string("Pistol"));
}

FOSSYND_TEST(Mission, LoadoutWeaponIsWhatTheSimFires)
{
    // The acceptance bar: the override is not just stored, it is the weapon the
    // simulation actually fires. One player against one enemy, so the only shot
    // that can land is the player's: the enemy drops by exactly the rifle's
    // damage (9) on the first hit, not the authored pistol's (10).
    MissionData data;
    data.width = 12;
    data.height = 12;
    AgentSpec hero;
    hero.team = Team::Player;
    hero.x = 2.0f;
    hero.y = 2.0f;
    hero.weapon = Weapon{"Authored", 10.0f, 4.0f, 1.0f, 0.9f};
    data.agents.push_back(hero);
    AgentSpec foe;
    foe.team = Team::Enemy;
    foe.x = 4.0f;
    foe.y = 2.0f;
    foe.health = 1000.0f;
    foe.weapon = Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}; // cannot shoot back
    data.agents.push_back(foe);

    TileGrid grid = buildGrid(data);
    Mission mission(grid, 9u);
    populateMission(data, mission);

    PlayerLoadout loadout;
    loadout.slots.resize(1);
    loadout.slots[0].weaponId = "rifle";
    std::string error;
    const WeaponCatalogue weapons = loadoutWeapons();
    FOSSYND_CHECK(applyLoadout(loadout, mission, weapons, error));

    for (int i = 0; i < 200 && mission.agents()[1].health() >= 1000.0f; ++i)
    {
        mission.update(0.05f);
    }
    // Exactly the rifle's damage (9), not the authored pistol's (10).
    FOSSYND_CHECK_NEAR(mission.agents()[1].health(), 1000.0f - 9.0f);
}

FOSSYND_TEST(Mission, EmptyLoadoutKeepsAuthoredDefaults)
{
    const MissionData data = loadoutFixture();
    TileGrid grid = buildGrid(data);
    Mission mission(grid, 5u);
    populateMission(data, mission);

    PlayerLoadout loadout;
    loadout.slots.resize(2); // two empty slots

    std::string error;
    const WeaponCatalogue weapons = loadoutWeapons();
    FOSSYND_CHECK(applyLoadout(loadout, mission, weapons, error));
    FOSSYND_CHECK(error.empty());

    FOSSYND_CHECK_EQ(mission.agents()[0].weapon().name, std::string("Authored"));
    FOSSYND_CHECK_EQ(mission.agents()[1].weapon().name, std::string("Pistol"));
    FOSSYND_CHECK_EQ(mission.medikits(0), 0);
    FOSSYND_CHECK_EQ(mission.shields(0), 0);
}

FOSSYND_TEST(Mission, LoadoutGrantsGadgets)
{
    const MissionData data = loadoutFixture();
    TileGrid grid = buildGrid(data);
    Mission mission(grid, 5u);
    populateMission(data, mission);

    PlayerLoadout loadout;
    loadout.slots.resize(2);
    loadout.slots[0].gadgetId = "medkit";
    loadout.slots[1].gadgetId = "shield";

    std::string error;
    const WeaponCatalogue weapons = loadoutWeapons();
    FOSSYND_CHECK(applyLoadout(loadout, mission, weapons, error));
    FOSSYND_CHECK(error.empty());

    FOSSYND_CHECK_EQ(mission.medikits(0), kGadgetCharges);
    FOSSYND_CHECK_EQ(mission.shields(1), kGadgetCharges);
    // The gadget is real: the medikit heals a wounded slot-0 agent.
    mission.agents()[0].applyDamage(30.0f);
    FOSSYND_CHECK_NEAR(mission.useMedikit(0), 30.0f);
}

FOSSYND_TEST(Mission, LoadoutRejectsUnknownIds)
{
    const MissionData data = loadoutFixture();
    TileGrid grid = buildGrid(data);
    Mission mission(grid, 5u);
    populateMission(data, mission);

    const WeaponCatalogue weapons = loadoutWeapons();
    std::string error;

    PlayerLoadout badWeapon;
    badWeapon.slots.resize(1);
    badWeapon.slots[0].weaponId = "death-ray";
    FOSSYND_CHECK(!applyLoadout(badWeapon, mission, weapons, error));
    FOSSYND_CHECK(!error.empty());
    // Nothing was applied: the authored weapon is intact.
    FOSSYND_CHECK_EQ(mission.agents()[0].weapon().name, std::string("Authored"));

    error.clear();
    PlayerLoadout badGadget;
    badGadget.slots.resize(1);
    badGadget.slots[0].gadgetId = "jetpack";
    FOSSYND_CHECK(!applyLoadout(badGadget, mission, weapons, error));
    FOSSYND_CHECK(!error.empty());
    FOSSYND_CHECK_EQ(mission.medikits(0), 0);
    FOSSYND_CHECK_EQ(mission.shields(0), 0);
}

FOSSYND_TEST(Mission, LoadoutIgnoresSlotsBeyondSquad)
{
    // A four-slot loadout against a one-player mission must not walk off the
    // agent list (the deployed squad may be smaller than the bought one).
    MissionData data = loadoutFixture();
    data.agents.erase(data.agents.begin() + 1); // drop the second player
    TileGrid grid = buildGrid(data);
    Mission mission(grid, 5u);
    populateMission(data, mission);

    PlayerLoadout loadout;
    loadout.slots.resize(PlayerLoadout::kMaxSlots);
    loadout.slots[0].weaponId = "rifle";
    loadout.slots[1].weaponId = "rifle";
    loadout.slots[2].weaponId = "rifle";
    loadout.slots[3].weaponId = "rifle";

    std::string error;
    const WeaponCatalogue weapons = loadoutWeapons();
    FOSSYND_CHECK(applyLoadout(loadout, mission, weapons, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(mission.agents()[0].weapon().name, std::string("Rifle"));
    // The enemy (now the last agent) kept its authored weapon.
    FOSSYND_CHECK_EQ(mission.agents()[1].weapon().name, std::string("Pistol"));
}

FOSSYND_TEST(Mission, GadgetVocabularyRejectsUnknown)
{
    FOSSYND_CHECK_EQ(gadgetChargesForId("medkit"), kGadgetCharges);
    FOSSYND_CHECK_EQ(gadgetChargesForId("shield"), kGadgetCharges);
    FOSSYND_CHECK_EQ(gadgetChargesForId("scanner"), kGadgetCharges);
    FOSSYND_CHECK_EQ(gadgetChargesForId("drugs"), kGadgetCharges);
    FOSSYND_CHECK_EQ(gadgetChargesForId(""), kUnknownGadget);
    FOSSYND_CHECK_EQ(gadgetChargesForId("jetpack"), kUnknownGadget);
}

FOSSYND_TEST(Mission, LoadoutGrantsScannerAndDrugs)
{
    // The two new gadget ids (#314) deploy through the same loadout path as the
    // medkit/shield: a charge lands on the mapped player agent.
    TileGrid grid(10, 10);
    Mission mission(grid, 7u);
    mission.addAgent(makeAgent(1.0f, 1.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(2.0f, 1.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(8.0f, 8.0f, Team::Enemy, 100.0f));

    PlayerLoadout loadout;
    loadout.slots.resize(2);
    loadout.slots[0].gadgetId = "scanner";
    loadout.slots[1].gadgetId = "drugs";

    std::string error;
    const WeaponCatalogue weapons = loadoutWeapons();
    FOSSYND_CHECK(applyLoadout(loadout, mission, weapons, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(mission.scanners(0), 1);
    FOSSYND_CHECK_EQ(mission.drugs(1), 1);
}

// --- scanner & performance-drug gadgets (Milestone A, issue #314) -------------

FOSSYND_TEST(Mission, ScannerMarksEnemiesInRadiusAndConsumesACharge)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 61u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    // One enemy inside the scan radius, one far outside it.
    const std::size_t nearFoe = mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100.0f));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f));

    mission.setScanners(hero, 1);
    FOSSYND_CHECK_EQ(mission.scanners(hero), 1);
    FOSSYND_CHECK(mission.activateScanner(hero));
    FOSSYND_CHECK_EQ(mission.scanners(hero), 0);
    FOSSYND_CHECK(mission.scannerPulse(hero) > 0.0f);

    const std::vector<std::size_t> targets = mission.scanTargets(hero);
    FOSSYND_CHECK_EQ(targets.size(), static_cast<std::size_t>(1));
    if (!targets.empty())
    {
        FOSSYND_CHECK_EQ(targets[0], nearFoe);
    }
}

FOSSYND_TEST(Mission, ScannerConsumesAChargeAndMarksNothingWhenNoEnemyInRange)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 62u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f));

    mission.setScanners(hero, 1);
    FOSSYND_CHECK(mission.activateScanner(hero));
    FOSSYND_CHECK_EQ(mission.scanners(hero), 0);
    FOSSYND_CHECK(mission.scanTargets(hero).empty());
}

FOSSYND_TEST(Mission, ScannerNoChargeOrCivilianOrAlreadyScanningIsNoOp)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 63u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const std::size_t bystander = mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian, 20.0f));

    // No charge: nothing happens.
    FOSSYND_CHECK(!mission.activateScanner(bystander));
    FOSSYND_CHECK_NEAR(mission.scannerPulse(bystander), 0.0f);

    // A civilian cannot scan even when handed a charge.
    mission.setScanners(bystander, 1);
    FOSSYND_CHECK(!mission.activateScanner(bystander));
    FOSSYND_CHECK_EQ(mission.scanners(bystander), 1); // charge untouched

    // A dead agent cannot scan.
    const std::size_t dead = mission.addAgent(makeAgent(5.0f, 5.0f, Team::Enemy, 10.0f));
    mission.setScanners(dead, 1);
    FOSSYND_CHECK(mission.activateScanner(dead));
    mission.agents()[dead].applyDamage(100.0f);
    FOSSYND_CHECK(!mission.activateScanner(dead));

    // Re-scanning while a pulse is active must not burn the second charge.
    const std::size_t scout = mission.addAgent(makeAgent(8.0f, 8.0f, Team::Player, 100.0f));
    mission.setScanners(scout, 2);
    FOSSYND_CHECK(mission.activateScanner(scout));
    FOSSYND_CHECK(!mission.activateScanner(scout));
    FOSSYND_CHECK_EQ(mission.scanners(scout), 1);
}

FOSSYND_TEST(Mission, ScannerPulseExpiresAndClearsTheMarks)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 64u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 1000.0f));
    // Zero-damage weapons on both sides keep the enemy alive through the pulse
    // and keep the mission Running (an empty enemy list would win immediately).
    mission.agents()[0].setWeapon(Weapon{"None", 0.0f, 10.0f, 1.0f, 0.0f});
    mission.agents()[1].setWeapon(Weapon{"None", 0.0f, 10.0f, 1.0f, 0.0f});

    mission.setScanners(hero, 1);
    FOSSYND_CHECK(mission.activateScanner(hero));
    FOSSYND_CHECK(!mission.scanTargets(hero).empty());

    // Run past the pulse duration: the marks clear.
    for (int i = 0; i < 200; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_NEAR(mission.scannerPulse(hero), 0.0f);
    FOSSYND_CHECK(mission.scanTargets(hero).empty());
}

FOSSYND_TEST(Mission, DrugsBoostSpeedThenWearOff)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 65u);
    const std::size_t hero = mission.addAgent(
        makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 10.0f, 10.0f, 1.0f, 0.5f}));
    // A distant enemy keeps the mission Running so the buff timer can tick.
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100.0f));

    const float baseSpeed = mission.agents()[hero].speed();
    mission.setDrugs(hero, 1);
    FOSSYND_CHECK_EQ(mission.drugs(hero), 1);
    FOSSYND_CHECK(mission.useDrugs(hero));
    FOSSYND_CHECK_EQ(mission.drugs(hero), 0);
    FOSSYND_CHECK(mission.drugsTimer(hero) > 0.0f);
    // The buff raises speed by the documented multiplier.
    FOSSYND_CHECK(mission.agents()[hero].speed() > baseSpeed);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].speed(), baseSpeed * 1.5f);

    // After the duration the speed is restored exactly.
    for (int i = 0; i < 200; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_NEAR(mission.drugsTimer(hero), 0.0f);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].speed(), baseSpeed);
}

FOSSYND_TEST(Mission, DrugsNoDoseOrCivilianOrAlreadyBoostedIsNoOp)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 66u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const std::size_t bystander = mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian, 20.0f));

    FOSSYND_CHECK(!mission.useDrugs(bystander));

    mission.setDrugs(bystander, 1);
    const float civilianSpeed = mission.agents()[bystander].speed();
    FOSSYND_CHECK(!mission.useDrugs(bystander)); // civilians cannot dose
    FOSSYND_CHECK_EQ(mission.drugs(bystander), 1);
    FOSSYND_CHECK_NEAR(mission.agents()[bystander].speed(), civilianSpeed);

    // A second dose while boosted is refused and does not stack the speed.
    mission.setDrugs(hero, 2);
    const float baseSpeed = mission.agents()[hero].speed();
    FOSSYND_CHECK(mission.useDrugs(hero));
    const float boosted = mission.agents()[hero].speed();
    FOSSYND_CHECK(!mission.useDrugs(hero));
    FOSSYND_CHECK_EQ(mission.drugs(hero), 1);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].speed(), boosted);
    FOSSYND_CHECK(boosted > baseSpeed);
}

FOSSYND_TEST(Mission, DrugsRaiseHitChance)
{
    // The buff raises the base accuracy, so over a fixed seed the boosted
    // shooter lands more hits than an identical unbuffed one. The sample stays
    // inside the 8 s buff window and is large (140 shots) so the gap is far
    // larger than the seed's noise.
    TileGrid grid(30, 5);
    Mission buffed(grid, 77u);
    const std::size_t shooter = buffed.addAgent(
        makeAgent(1.0f, 2.0f, Team::Player, 1000.0f, Weapon{"Test", 1.0f, 20.0f, 10.0f, 0.7f}));
    const std::size_t dummy = buffed.addAgent(makeAgent(10.0f, 2.0f, Team::Enemy, 100000.0f,
                                                        Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    buffed.setDrugs(shooter, 1);
    FOSSYND_CHECK(buffed.useDrugs(shooter));

    Mission plain(grid, 77u);
    plain.addAgent(
        makeAgent(1.0f, 2.0f, Team::Player, 1000.0f, Weapon{"Test", 1.0f, 20.0f, 10.0f, 0.7f}));
    const std::size_t dummy2 = plain.addAgent(makeAgent(10.0f, 2.0f, Team::Enemy, 100000.0f,
                                                        Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    for (int i = 0; i < 140; ++i)
    {
        buffed.update(0.05f);
        plain.update(0.05f);
    }
    const float buffedDamage = 100000.0f - buffed.agents()[dummy].health();
    const float plainDamage = 100000.0f - plain.agents()[dummy2].health();
    FOSSYND_CHECK(buffedDamage > plainDamage);
}

// --- regulated drug bars (ADR-0043, issue #496) ------------------------------

FOSSYND_TEST(Mission, DrugBarsStartAtNeutralWithZeroEffect)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 90u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));

    for (Drug drug : {Drug::Adrenalin, Drug::Intelligence, Drug::Physical})
    {
        FOSSYND_CHECK_NEAR(mission.drugLevel(hero, drug), kDrugNeutralStart);
        FOSSYND_CHECK_NEAR(mission.drugNeutral(hero, drug), kDrugNeutralStart);
        FOSSYND_CHECK_NEAR(mission.drugEffect(hero, drug), 0.0f);
    }
}

FOSSYND_TEST(Mission, DrugLevelClampsAndInvalidIndexIsNoOp)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 91u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));

    mission.setDrugLevel(hero, Drug::Physical, 2.5f);
    FOSSYND_CHECK_NEAR(mission.drugLevel(hero, Drug::Physical), 1.0f);
    mission.setDrugLevel(hero, Drug::Physical, -3.0f);
    FOSSYND_CHECK_NEAR(mission.drugLevel(hero, Drug::Physical), 0.0f);
    mission.setDrugLevel(hero, Drug::Physical, 0.75f);
    FOSSYND_CHECK_NEAR(mission.drugLevel(hero, Drug::Physical), 0.75f);

    // An out-of-range index is a safe no-op and reads back the sentinel 0.
    const std::size_t bad = 99u;
    mission.setDrugLevel(bad, Drug::Adrenalin, 1.0f);
    FOSSYND_CHECK_NEAR(mission.drugLevel(bad, Drug::Adrenalin), 0.0f);
    FOSSYND_CHECK_NEAR(mission.drugNeutral(bad, Drug::Adrenalin), 0.0f);
    FOSSYND_CHECK_NEAR(mission.drugEffect(bad, Drug::Adrenalin), 0.0f);
}

FOSSYND_TEST(Mission, DrugNeutralDriftsTowardTheHeldLevelAndShrinksTheEffect)
{
    // The normalisation property: hold a bar high and the body adapts, so the
    // *effect* shrinks even though the player has not touched the level.
    TileGrid grid(20, 20);
    Mission mission(grid, 92u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    // A distant enemy keeps the mission Running (an empty enemy list would win).
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    mission.setDrugLevel(hero, Drug::Adrenalin, 1.0f);
    FOSSYND_CHECK_NEAR(mission.drugEffect(hero, Drug::Adrenalin), 0.5f);

    // One second of drift at kDrugNormalisationRate (0.25/s): neutral rises by
    // 0.25, so the effect falls from 0.5 to 0.25. The level is untouched.
    for (int i = 0; i < 20; ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_NEAR(mission.drugLevel(hero, Drug::Adrenalin), 1.0f);
    FOSSYND_CHECK_NEAR(mission.drugNeutral(hero, Drug::Adrenalin), 0.75f);
    FOSSYND_CHECK_NEAR(mission.drugEffect(hero, Drug::Adrenalin), 0.25f);
}

FOSSYND_TEST(Mission, DroppingABarAfterAHighHoldGivesANegativeEffect)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 93u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));

    // Hold high long enough for the neutral point to climb, then drop the bar:
    // the agent is now *below* its adapted level, so the effect is negative.
    mission.setDrugLevel(hero, Drug::Physical, 1.0f);
    for (int i = 0; i < 40; ++i) // 2 s: neutral rises by 0.5 toward 1.0
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.drugNeutral(hero, Drug::Physical) > kDrugNeutralStart);

    mission.setDrugLevel(hero, Drug::Physical, 0.0f);
    FOSSYND_CHECK(mission.drugEffect(hero, Drug::Physical) < 0.0f);
}

FOSSYND_TEST(Mission, AddAgentGrowsTheDrugStateWithTheRoster)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 94u);
    const std::size_t first = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    mission.setDrugLevel(first, Drug::Intelligence, 0.9f);

    // A second agent starts fresh at neutral; the first keeps its own bar.
    const std::size_t second = mission.addAgent(makeAgent(3.0f, 2.0f, Team::Player, 100.0f));
    FOSSYND_CHECK_NEAR(mission.drugLevel(second, Drug::Intelligence), kDrugNeutralStart);
    FOSSYND_CHECK_NEAR(mission.drugLevel(first, Drug::Intelligence), 0.9f);

    // A civilian added later is kept in sync too (no crash, sane defaults).
    const std::size_t bystander = mission.addAgent(makeAgent(4.0f, 2.0f, Team::Civilian, 20.0f));
    FOSSYND_CHECK_NEAR(mission.drugLevel(bystander, Drug::Physical), kDrugNeutralStart);
}

// --- adrenalin behaviour (ADR-0043, S2, issue #497) --------------------------

namespace
{
// A high-damage weapon so a single visible enemy clears the panic threshold.
Weapon makeCannon() { return Weapon{"Cannon", 40.0f, 8.0f, 1.0f, 0.8f}; }
} // namespace

FOSSYND_TEST(Mission, ThreatIsZeroWithoutVisibleThreats)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 100u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));

    FOSSYND_CHECK_NEAR(mission.threatTo(hero), 0.0f);
    // A non-combatant projects no threat of its own and feels none.
    const std::size_t bystander = mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian, 20.0f));
    FOSSYND_CHECK_NEAR(mission.threatTo(bystander), 0.0f);
    FOSSYND_CHECK_NEAR(mission.threatLevel(bystander), 0.0f);
}

FOSSYND_TEST(Mission, ThreatRisesWithProximityAndIsDeterministic)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 101u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const std::size_t far = mission.addAgent(makeAgent(2.0f, 9.0f, Team::Enemy, 100.0f,
                                                       makePistol()));
    const float farThreat = mission.threatTo(hero);
    FOSSYND_CHECK(farThreat > 0.0f);
    // Pure and deterministic: repeated reads agree exactly (no RNG).
    FOSSYND_CHECK_NEAR(mission.threatTo(hero), farThreat);

    // The same enemy moved closer raises the felt threat.
    mission.agents()[far].setPosition(2.0f, 3.0f);
    const float nearThreat = mission.threatTo(hero);
    FOSSYND_CHECK(nearThreat > farThreat);

    // A wall between them removes the threat entirely (must be visible).
    mission.agents()[far].setPosition(2.0f, 9.0f);
    for (int y = 3; y < 9; ++y)
    {
        grid.setWalkable(2, y, false);
    }
    FOSSYND_CHECK_NEAR(mission.threatTo(hero), 0.0f);
}

FOSSYND_TEST(Mission, ThreatRisesWhenTargetedAndWhenTargetingAnAlly)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 102u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const std::size_t mate = mission.addAgent(makeAgent(2.0f, 4.0f, Team::Player, 100.0f));
    const std::size_t foe = mission.addAgent(makeAgent(2.0f, 9.0f, Team::Enemy, 100.0f,
                                                       makePistol()));

    const float baseline = mission.threatTo(hero);
    FOSSYND_CHECK(baseline > 0.0f);

    // The enemy aims at the ally: a team-mate under fire raises the threat.
    mission.agents()[foe].setOrderedTarget(static_cast<int>(mate));
    const float allyTargeted = mission.threatTo(hero);
    FOSSYND_CHECK(allyTargeted > baseline);

    // The enemy aims at me: worse than aiming at an ally.
    mission.agents()[foe].setOrderedTarget(static_cast<int>(hero));
    const float meTargeted = mission.threatTo(hero);
    FOSSYND_CHECK(meTargeted > allyTargeted);
}

FOSSYND_TEST(Mission, ThreatIsLowerWhenTheEnemyHasNotSeenMe)
{
    // "Has seen me" is asymmetric: a threat that has not noticed me is less
    // dangerous than one that has, and an agent can see further than a myopic
    // enemy's sight range (per-agent sight ranges, issue #333).
    TileGrid grid(20, 20);
    Mission mission(grid, 108u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const std::size_t foe = mission.addAgent(makeAgent(2.0f, 8.0f, Team::Enemy, 100.0f,
                                                       makePistol()));

    const float seen = mission.threatTo(hero);
    FOSSYND_CHECK(seen > 0.0f);
    // Shrink the enemy's sight so it can no longer see the hero, but the hero can
    // still see it: the felt threat drops.
    mission.setSightRange(foe, 1.0f);
    const float unseen = mission.threatTo(hero);
    FOSSYND_CHECK(unseen > 0.0f);
    FOSSYND_CHECK(unseen < seen);
}

FOSSYND_TEST(Mission, CalmAdrenalinNeverPanicsButHighAdrenalinDoes)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 103u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100000.0f));
    mission.addAgent(makeAgent(2.0f, 3.0f, Team::Enemy, 100000.0f, makeCannon()));

    // Adrenalin level == neutral => effect 0 => never panics, whatever the threat.
    mission.setDrugLevel(hero, Drug::Adrenalin, kDrugNeutralStart);
    FOSSYND_CHECK_NEAR(mission.drugEffect(hero, Drug::Adrenalin), 0.0f);
    FOSSYND_CHECK(mission.threatTo(hero) > 0.0f);
    mission.update(0.05f);
    FOSSYND_CHECK(!mission.isPanicking(hero));

    // High adrenalin (effect 0.5) under the same threat => panic.
    mission.setDrugLevel(hero, Drug::Adrenalin, 1.0f);
    FOSSYND_CHECK_NEAR(mission.drugEffect(hero, Drug::Adrenalin), 0.5f);
    mission.update(0.05f);
    FOSSYND_CHECK(mission.isPanicking(hero));
}

FOSSYND_TEST(Mission, PanickingAgentIgnoresOrdersAndSettlesWhenThreatClears)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 104u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100000.0f));
    const std::size_t foe = mission.addAgent(makeAgent(2.0f, 3.0f, Team::Enemy, 100000.0f,
                                                       makeCannon()));
    mission.setDrugLevel(hero, Drug::Adrenalin, 1.0f);
    mission.update(0.05f);
    FOSSYND_CHECK(mission.isPanicking(hero));
    FOSSYND_CHECK_EQ(mission.agents()[hero].aiState(), AiState::Retreat);

    // An order while panicking is dropped: the flee path is untouched.
    const std::vector<TilePos> fleePath = mission.agents()[hero].path();
    mission.orderMove(hero, TilePos{15, 15});
    FOSSYND_CHECK(mission.agents()[hero].path() == fleePath);
    FOSSYND_CHECK(mission.isPanicking(hero));

    // Once the threat is gone the panic clears (with hysteresis) and orders work
    // again.
    mission.agents()[foe].applyDamage(1.0e6f);
    mission.update(0.05f);
    FOSSYND_CHECK(!mission.isPanicking(hero));
    mission.orderMove(hero, TilePos{15, 15});
    FOSSYND_CHECK(!mission.agents()[hero].path().empty());
    FOSSYND_CHECK(mission.agents()[hero].path().back() == (TilePos{15, 15}));
}

FOSSYND_TEST(Mission, AdrenalinMovesSpeedReactionAndAccuracyInDocumentedDirections)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 105u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    // A harmless distant enemy keeps the mission Running (no living enemies would
    // end it and make later updates no-ops) and adds no meaningful threat.
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    const float baseSpeed = mission.agents()[hero].speed();

    // High adrenalin: faster, shorter reaction cadence, less precise.
    mission.setDrugLevel(hero, Drug::Adrenalin, 1.0f);
    mission.update(0.05f);
    FOSSYND_CHECK(mission.agents()[hero].speed() > baseSpeed);
    FOSSYND_CHECK(mission.reactionInterval(hero) < 0.4f);
    FOSSYND_CHECK(mission.adrenalinAccuracyScale(hero) < 1.0f);

    // Low adrenalin: the mirror — slower, slower to react, more precise.
    mission.setDrugLevel(hero, Drug::Adrenalin, 0.0f);
    mission.update(0.05f);
    FOSSYND_CHECK(mission.agents()[hero].speed() < baseSpeed);
    FOSSYND_CHECK(mission.reactionInterval(hero) > 0.4f);
    FOSSYND_CHECK(mission.adrenalinAccuracyScale(hero) > 1.0f);
}

FOSSYND_TEST(Mission, AdrenalinSpeedIsRecomputedNotCompoundedByNormalisation)
{
    // The normalisation drift changes the effect every tick; a multiply-in-place
    // implementation would compound it and run the speed away. The speed must
    // stay within the documented band [base, base*(1+gain)] as the neutral point
    // adapts to a held-high bar.
    TileGrid grid(20, 20);
    Mission mission(grid, 106u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    const float baseSpeed = mission.agents()[hero].speed();

    mission.setDrugLevel(hero, Drug::Adrenalin, 1.0f);
    for (int i = 0; i < 200; ++i)
    {
        mission.update(0.05f);
        const float speed = mission.agents()[hero].speed();
        FOSSYND_CHECK(speed >= baseSpeed * 0.999f);
        FOSSYND_CHECK(speed <= baseSpeed * 1.5f + 0.001f);
    }
}

FOSSYND_TEST(Mission, AdrenalinComposesWithTheLegacyDrugBuffAndLegsUpgrade)
{
    // The adrenalin path recomputes speed from `adrenalinBaseSpeed_` every tick.
    // A legacy one-off speed buff (the `useDrugs` gadget) and a legs cybernetic
    // must therefore fold into that base, or the recompute would erase them.
    TileGrid grid(20, 20);
    Mission mission(grid, 107u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    mission.addAgent(makeAgent(18.0f, 18.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    const float baseSpeed = mission.agents()[hero].speed();

    // The legs upgrade raises the base; a subsequent tick must keep it.
    FOSSYND_CHECK(mission.applyCybernetic(hero, "legs"));
    const float legsSpeed = mission.agents()[hero].speed();
    FOSSYND_CHECK(legsSpeed > baseSpeed);
    mission.update(0.05f);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].speed(), legsSpeed);

    // The legacy buff stacks on top of the legs base and survives ticks.
    mission.setDrugs(hero, 1);
    FOSSYND_CHECK(mission.useDrugs(hero));
    FOSSYND_CHECK_NEAR(mission.agents()[hero].speed(), legsSpeed * 1.5f);
    mission.update(0.05f);
    FOSSYND_CHECK(mission.agents()[hero].speed() > legsSpeed);
}

// --- cybernetic upgrades (Milestone A, issue #313) ---------------------------

FOSSYND_TEST(Mission, CyberneticVocabularyRejectsUnknown)
{
    CyberneticEffect effect;
    FOSSYND_CHECK(cyberneticEffectForId("legs", effect));
    FOSSYND_CHECK_NEAR(effect.speedMultiplier, 1.20f);
    FOSSYND_CHECK(cyberneticEffectForId("arms", effect));
    FOSSYND_CHECK_NEAR(effect.accuracyMultiplier, 1.15f);
    FOSSYND_CHECK(cyberneticEffectForId("chest", effect));
    FOSSYND_CHECK_NEAR(effect.healthMultiplier, 1.25f);
    FOSSYND_CHECK(cyberneticEffectForId("eyes", effect));
    FOSSYND_CHECK_NEAR(effect.sightMultiplier, 1.30f);
    FOSSYND_CHECK(cyberneticEffectForId("targeting-implant", effect));
    FOSSYND_CHECK_NEAR(effect.accuracyMultiplier, 1.10f);
    FOSSYND_CHECK(!cyberneticEffectForId("jetpack", effect));
    FOSSYND_CHECK(!cyberneticEffectForId("", effect));
}

FOSSYND_TEST(Mission, CyberneticLegsArmsChestChangeStats)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 72u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const float baseSpeed = mission.agents()[hero].speed();
    const float baseAccuracy = mission.agents()[hero].weapon().accuracy;
    const float baseHealth = mission.agents()[hero].maxHealth();

    FOSSYND_CHECK(mission.applyCybernetic(hero, "legs"));
    FOSSYND_CHECK(mission.applyCybernetic(hero, "arms"));
    FOSSYND_CHECK(mission.applyCybernetic(hero, "chest"));

    FOSSYND_CHECK_NEAR(mission.agents()[hero].speed(), baseSpeed * 1.20f);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].weapon().accuracy, baseAccuracy * 1.15f);
    // A chest upgrade raises the maximum *and* fills the extra health, so it is
    // a clean gain, not a wound.
    FOSSYND_CHECK_NEAR(mission.agents()[hero].maxHealth(), baseHealth * 1.25f);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].health(), baseHealth * 1.25f);
}

FOSSYND_TEST(Mission, CyberneticEyesRaiseSightRange)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 71u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));

    // A mission's authored sight range is the default until an eyes upgrade.
    FOSSYND_CHECK_NEAR(mission.sightRange(hero), kDefaultSightRange);
    FOSSYND_CHECK(mission.applyCybernetic(hero, "eyes"));
    FOSSYND_CHECK_NEAR(mission.sightRange(hero), kDefaultSightRange * 1.30f);

    // The setter lets an author seed a different base for a mission.
    mission.setSightRange(hero, 20.0f);
    FOSSYND_CHECK_NEAR(mission.sightRange(hero), 20.0f);
}

FOSSYND_TEST(Mission, CyberneticEyesExtendEngagementRange)
{
    // An enemy 13 tiles off is beyond the default 12-tile sight, so it never
    // acquires the player and stays put; with an eyes upgrade (1.3x -> 15.6) it
    // engages and closes in. This is the behavioural proof the perception stat
    // feeds `seesTarget`, not just the getter.
    const float far = 13.0f;
    {
        TileGrid grid(20, 2);
        Mission mission(grid, 75u);
        mission.addAgent(makeAgent(1.0f, 1.0f, Team::Player, 1000.0f, Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
        const std::size_t foe = mission.addAgent(
            makeAgent(1.0f + far, 1.0f, Team::Enemy, 1000.0f, Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
        for (int i = 0; i < 40; ++i)
        {
            mission.update(0.05f);
        }
        FOSSYND_CHECK_EQ(mission.agents()[foe].aiState(), AiState::Idle);
    }
    {
        TileGrid grid(20, 2);
        Mission mission(grid, 75u);
        mission.addAgent(makeAgent(1.0f, 1.0f, Team::Player, 1000.0f, Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
        const std::size_t foe = mission.addAgent(
            makeAgent(1.0f + far, 1.0f, Team::Enemy, 1000.0f, Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
        FOSSYND_CHECK(mission.applyCybernetic(foe, "eyes"));
        bool engaged = false;
        for (int i = 0; i < 40 && !engaged; ++i)
        {
            mission.update(0.05f);
            engaged = mission.agents()[foe].aiState() == AiState::Engage;
        }
        FOSSYND_CHECK(engaged);
    }
}

FOSSYND_TEST(Mission, CyberneticAccuracyIsClampedToCertain)
{
    // An accuracy upgrade must never make a shot certain: a 0.95 base times 1.15
    // would be 1.09, so it is clamped to 1.0.
    TileGrid grid(20, 20);
    Mission mission(grid, 74u);
    const std::size_t hero =
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Sure", 10.0f, 10.0f, 1.0f, 0.95f}));
    FOSSYND_CHECK(mission.applyCybernetic(hero, "arms"));
    FOSSYND_CHECK_NEAR(mission.agents()[hero].weapon().accuracy, 1.0f);
}

FOSSYND_TEST(Mission, CyberneticNoOpForCivilianUnknownOrInvalid)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 73u);

    // A civilian cannot take a passive combat upgrade.
    const std::size_t bystander = mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian, 20.0f));
    const float civilianSpeed = mission.agents()[bystander].speed();
    FOSSYND_CHECK(!mission.applyCybernetic(bystander, "legs"));
    FOSSYND_CHECK_NEAR(mission.agents()[bystander].speed(), civilianSpeed);

    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const float heroSpeed = mission.agents()[hero].speed();
    FOSSYND_CHECK(!mission.applyCybernetic(hero, "jetpack")); // unknown id
    FOSSYND_CHECK_NEAR(mission.agents()[hero].speed(), heroSpeed);
    FOSSYND_CHECK(!mission.applyCybernetic(hero, ""));        // empty id
    FOSSYND_CHECK(!mission.applyCybernetic(999, "legs"));     // invalid index
    FOSSYND_CHECK_NEAR(mission.agents()[hero].speed(), heroSpeed);
}

FOSSYND_TEST(Mission, LoadoutGrantsCybernetic)
{
    // A bought cybernetic reaches the deployed agent through the same loadout
    // path as a weapon/gadget: a permanent stat change on the mapped slot.
    const MissionData data = loadoutFixture();
    TileGrid grid = buildGrid(data);
    Mission mission(grid, 5u);
    populateMission(data, mission);

    const float baseSpeed = mission.agents()[0].speed();
    const float baseHealth = mission.agents()[1].maxHealth();

    PlayerLoadout loadout;
    loadout.slots.resize(2);
    loadout.slots[0].cyberneticId = "legs";
    loadout.slots[1].cyberneticId = "chest";

    std::string error;
    const WeaponCatalogue weapons = loadoutWeapons();
    FOSSYND_CHECK(applyLoadout(loadout, mission, weapons, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_NEAR(mission.agents()[0].speed(), baseSpeed * 1.20f);
    FOSSYND_CHECK_NEAR(mission.agents()[1].maxHealth(), baseHealth * 1.25f);
}

FOSSYND_TEST(Mission, LoadoutRejectsUnknownCybernetic)
{
    const MissionData data = loadoutFixture();
    TileGrid grid = buildGrid(data);
    Mission mission(grid, 5u);
    populateMission(data, mission);

    const float baseSpeed = mission.agents()[0].speed();

    PlayerLoadout loadout;
    loadout.slots.resize(1);
    loadout.slots[0].cyberneticId = "wings";

    std::string error;
    const WeaponCatalogue weapons = loadoutWeapons();
    FOSSYND_CHECK(!applyLoadout(loadout, mission, weapons, error));
    FOSSYND_CHECK(!error.empty());
    // Nothing was applied: the stat is intact.
    FOSSYND_CHECK_NEAR(mission.agents()[0].speed(), baseSpeed);
}

// --- vehicles (Milestone A, issue #318) ---------------------------------------

FOSSYND_TEST(Mission, AddVehicleAndQueryDriver)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    const std::size_t car = mission.addVehicle(makeCar(5.0f, 5.0f));

    FOSSYND_CHECK_EQ(mission.vehicles().size(), 1u);
    FOSSYND_CHECK_EQ(mission.vehicleOf(hero), -1);
    FOSSYND_CHECK(!mission.isDriving(hero));
    FOSSYND_CHECK_EQ(mission.vehicleDriver(car), -1);
}

FOSSYND_TEST(Mission, HijackRequiresProximity)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    // The hero starts far from the car: the hijack is refused.
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    const std::size_t car = mission.addVehicle(makeCar(10.0f, 10.0f));

    FOSSYND_CHECK(!mission.orderHijack(car, hero));
    FOSSYND_CHECK_EQ(mission.vehicleDriver(car), -1);
    FOSSYND_CHECK(!mission.isDriving(hero));
}

FOSSYND_TEST(Mission, HijackTakesTheWheelAndFoldsPosition)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    // The hero stands next to the car (within the hijack radius).
    const std::size_t hero = mission.addAgent(makeAgent(4.0f, 4.0f, Team::Player));
    const std::size_t car = mission.addVehicle(makeCar(5.0f, 5.0f));

    FOSSYND_CHECK(mission.orderHijack(car, hero));
    FOSSYND_CHECK(mission.isDriving(hero));
    FOSSYND_CHECK_EQ(mission.vehicleOf(hero), static_cast<int>(car));
    FOSSYND_CHECK_EQ(mission.vehicleDriver(car), static_cast<int>(hero));
    // The driver rides in the car: its position is folded onto the vehicle's.
    FOSSYND_CHECK_NEAR(mission.agents()[hero].position().x, 5.0f);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].position().y, 5.0f);
}

FOSSYND_TEST(Mission, HijackIsSingleDriverAndEjectFreesTheAgent)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    const std::size_t hero = mission.addAgent(makeAgent(4.0f, 4.0f, Team::Player));
    const std::size_t mate = mission.addAgent(makeAgent(5.0f, 5.0f, Team::Player));
    const std::size_t car = mission.addVehicle(makeCar(5.0f, 5.0f));

    FOSSYND_CHECK(mission.orderHijack(car, hero));
    // A second agent cannot take a car that is already driven.
    FOSSYND_CHECK(!mission.orderHijack(car, mate));
    FOSSYND_CHECK_EQ(mission.vehicleDriver(car), static_cast<int>(hero));

    mission.ejectVehicle(car);
    FOSSYND_CHECK_EQ(mission.vehicleDriver(car), -1);
    FOSSYND_CHECK(!mission.isDriving(hero));
    // The ejected driver is placed on the vehicle's tile.
    FOSSYND_CHECK_NEAR(mission.agents()[hero].position().x, 5.0f);

    // Now the mate may take it.
    FOSSYND_CHECK(mission.orderHijack(car, mate));
    FOSSYND_CHECK_EQ(mission.vehicleDriver(car), static_cast<int>(mate));
}

FOSSYND_TEST(Mission, OrderHijackRejectsCivilianAndInvalid)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    const std::size_t bystander = mission.addAgent(makeAgent(5.0f, 5.0f, Team::Civilian));
    const std::size_t hero = mission.addAgent(makeAgent(1.0f, 1.0f, Team::Player));
    const std::size_t car = mission.addVehicle(makeCar(5.0f, 5.0f));

    // A civilian cannot drive, even standing on the car.
    FOSSYND_CHECK(!mission.orderHijack(car, bystander));
    // Invalid indices are rejected rather than crashing.
    FOSSYND_CHECK(!mission.orderHijack(99, hero));
    FOSSYND_CHECK(!mission.orderHijack(car, 99));
}

FOSSYND_TEST(Mission, OrderDriveRequiresADriver)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    const std::size_t car = mission.addVehicle(makeCar(3.0f, 3.0f));

    // No driver: the drive order is refused, so an empty car stays put.
    FOSSYND_CHECK(!mission.orderDrive(car, TilePos{15, 15}));
    FOSSYND_CHECK(!mission.vehicles()[car].hasPath());

    const std::size_t hero = mission.addAgent(makeAgent(3.0f, 3.0f, Team::Player));
    FOSSYND_CHECK(mission.orderHijack(car, hero));
    FOSSYND_CHECK(mission.orderDrive(car, TilePos{15, 15}));
    FOSSYND_CHECK(mission.vehicles()[car].hasPath());
}

FOSSYND_TEST(Mission, DrivingCarriesTheDriver)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    const std::size_t hero = mission.addAgent(makeAgent(3.0f, 3.0f, Team::Player));
    const std::size_t car = mission.addVehicle(makeCar(3.0f, 3.0f));
    // A distant enemy keeps the default `eliminate` objective from completing on
    // the first update (which would stop the mission and freeze the car).
    mission.addAgent(makeAgent(1.0f, 18.0f, Team::Enemy, 100000.0f));
    FOSSYND_CHECK(mission.orderHijack(car, hero));
    FOSSYND_CHECK(mission.orderDrive(car, TilePos{15, 3}));

    for (int i = 0; i < 60; ++i)
    {
        mission.update(0.05f);
    }
    // The car moved and the driver went with it: their positions are identical.
    FOSSYND_CHECK(mission.vehicles()[car].position().x > 5.0f);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].position().x,
                       mission.vehicles()[car].position().x);
    FOSSYND_CHECK_NEAR(mission.agents()[hero].position().y,
                       mission.vehicles()[car].position().y);
}

FOSSYND_TEST(Mission, MountedFireHitsEnemy)
{
    TileGrid grid(30, 5);
    Mission mission(grid, 11u);
    // High health so neither side ends the mission during the sample.
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 1000.0f));
    const std::size_t foe = mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100000.0f));
    const std::size_t car = mission.addVehicle(makeCar(2.0f, 2.0f));
    FOSSYND_CHECK(mission.orderHijack(car, hero));

    for (int i = 0; i < 200; ++i)
    {
        mission.update(0.05f);
    }
    // The driver did not fire a personal weapon, but the mounted uzi did: the
    // enemy took damage from the car.
    FOSSYND_CHECK(mission.agents()[foe].health() < 100000.0f);
}

FOSSYND_TEST(Mission, DriverDoesNotFirePersonalWeapon)
{
    // A driver fires only the vehicle's mounted weapon, never a personal one.
    // The same fight on foot (identical personal weapon) must deal clearly more
    // damage than the driver's weak mounted gun, which is only possible if the
    // driver's personal weapon is suppressed.
    const Weapon strong{"Test", 40.0f, 20.0f, 10.0f, 1.0f};

    TileGrid gridA(30, 5);
    Mission onFoot(gridA, 11u);
    onFoot.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 1000.0f, strong));
    const std::size_t foeA = onFoot.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100000.0f));

    TileGrid gridB(30, 5);
    Mission driven(gridB, 11u);
    const std::size_t heroB = driven.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 1000.0f, strong));
    const std::size_t foeB = driven.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100000.0f));
    const std::size_t car = driven.addVehicle(makeCar(2.0f, 2.0f));
    FOSSYND_CHECK(driven.orderHijack(car, heroB));

    for (int i = 0; i < 200; ++i)
    {
        onFoot.update(0.05f);
        driven.update(0.05f);
    }
    const float footDamage = 100000.0f - onFoot.agents()[foeA].health();
    const float drivenDamage = 100000.0f - driven.agents()[foeB].health();
    FOSSYND_CHECK(drivenDamage > 0.0f);      // the mounted uzi still fires
    FOSSYND_CHECK(footDamage > drivenDamage); // personal fire is suppressed in the car
}

FOSSYND_TEST(Mission, VehicleAttackOrderFocusesFire)
{
    TileGrid grid(30, 5);
    Mission mission(grid, 21u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 1000.0f));
    // Two enemies in mounted-weapon range: `near` is closer, `far` is the
    // explicitly ordered target.
    const std::size_t near = mission.addAgent(makeAgent(5.0f, 2.0f, Team::Enemy, 100000.0f));
    const std::size_t far = mission.addAgent(makeAgent(6.0f, 2.0f, Team::Enemy, 100000.0f));
    const std::size_t car = mission.addVehicle(makeCar(2.0f, 2.0f));
    FOSSYND_CHECK(mission.orderHijack(car, hero));
    mission.orderVehicleAttack(car, far);

    for (int i = 0; i < 200; ++i)
    {
        mission.update(0.05f);
    }
    // The ordered target took the damage, not the auto-acquired nearer one.
    FOSSYND_CHECK(mission.agents()[far].health() < 100000.0f);
    FOSSYND_CHECK_NEAR(mission.agents()[near].health(), 100000.0f);
}

FOSSYND_TEST(Mission, WreckEjectsDriverAndStopsTheVehicle)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    const std::size_t hero = mission.addAgent(makeAgent(3.0f, 3.0f, Team::Player));
    const std::size_t car = mission.addVehicle(makeCar(3.0f, 3.0f));
    FOSSYND_CHECK(mission.orderHijack(car, hero));
    FOSSYND_CHECK(mission.orderDrive(car, TilePos{15, 3}));
    mission.update(0.05f);
    FOSSYND_CHECK(mission.vehicles()[car].hasPath());

    // Destroy the car directly.
    mission.vehicles()[car].applyDamage(100000.0f);
    FOSSYND_CHECK(mission.vehicles()[car].isDestroyed());
    FOSSYND_CHECK(!mission.vehicles()[car].hasPath());
    FOSSYND_CHECK(!mission.isDriving(hero));
    FOSSYND_CHECK_EQ(mission.vehicleDriver(car), -1);
}

FOSSYND_TEST(Mission, ExplosionWrecksVehicleAndHurtsNeighbours)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 3u);
    // An enemy shooter with an explosive weapon, a stationary *player* target
    // (player agents never move on their own), a car beside the target, and a
    // second player agent far enough to be outside the blast but within the
    // wreck's own radius. The small blast radius (1.0) keeps the neighbour out
    // of the direct explosion, so the damage it takes can only come from the
    // wreck detonating.
    const std::size_t shooter =
        mission.addAgent(makeAgent(1.0f, 5.0f, Team::Enemy, 100000.0f,
                                   Weapon{"Boom", 50.0f, 30.0f, 10.0f, 1.0f, /*areaRadius=*/1.0f}));
    const std::size_t target = mission.addAgent(makeAgent(7.0f, 5.0f, Team::Player, 100000.0f));
    const std::size_t neighbour = mission.addAgent(makeAgent(5.0f, 5.0f, Team::Player, 100.0f));
    const std::size_t car = mission.addVehicle(makeCar(6.0f, 5.0f));
    // Force fire on the far target so the shot (and its blast) lands at (7,5)
    // rather than on the nearer neighbour the enemy would auto-acquire.
    mission.orderAttack(shooter, target);

    for (int i = 0; i < 60 && mission.vehicles()[car].isAlive(); ++i)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK(mission.vehicles()[car].isDestroyed());
    // The neighbour was outside the blast (2 tiles from the impact) but inside
    // the wreck radius (1 tile from the car): only the detonation could hurt it.
    FOSSYND_CHECK(mission.agents()[neighbour].health() < 100.0f);
    (void)shooter;
    (void)target;
}

// --- Shipped campaign mission set, slice 1 (issue #416) --------------------
//
// The first wave of authored campaign missions must load through the *real*
// loader and be genuinely winnable, not merely parse. Each case loads its file
// from the shipped tree, builds the real grid, populates the real mission, and
// drives the objective's own mechanic to the win — so a mission that is
// unreachable, walled off, or short of its own target count fails here instead
// of in a playtest. The set is deliberately one objective kind per mission
// (destroy / persuade / reach) and builds in difficulty.

namespace
{

std::string shippedMission(const std::string& id)
{
    return std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions/" + id + ".json";
}

int indexOfAgentNamed(const Mission& mission, const std::string& name)
{
    const std::vector<Agent>& agents = mission.agents();
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents[i].name() == name)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

FOSSYND_TEST(Mission, ShippedBlackoutDestroyIsWinnable)
{
    MissionData data;
    std::string error;
    FOSSYND_CHECK(MissionData::loadFromFile(shippedMission("blackout"), data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::Destroy);

    TileGrid grid = buildGrid(data);
    const TilePos goal{data.objective.tileX, data.objective.tileY};
    // The objective names a destructible wall, not a floor tile: without a
    // breachable target the mission could never be won.
    FOSSYND_CHECK(grid.isDestructible(goal.x, goal.y));
    FOSSYND_CHECK(!grid.isWalkable(goal.x, goal.y));

    Mission mission(grid, 11u);
    populateMission(data, mission);
    FOSSYND_CHECK(mission.objective().kind == ObjectiveSpec::Kind::Destroy);

    // The player's real action: order the squad onto the defender standing
    // behind the wall. The wall blocks the shot, so the agent chips the wall —
    // the destroy objective's actual mechanic — rather than hitting the guard.
    const int warden = indexOfAgentNamed(mission, "WARDEN");
    FOSSYND_CHECK(warden >= 0);
    mission.orderAttack(0, static_cast<std::size_t>(warden));

    FOSSYND_CHECK(!grid.isRubble(goal.x, goal.y));
    runUntilDone(mission, 20.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK(grid.isRubble(goal.x, goal.y));
}

FOSSYND_TEST(Mission, ShippedAssemblyPersuadeIsWinnable)
{
    MissionData data;
    std::string error;
    FOSSYND_CHECK(MissionData::loadFromFile(shippedMission("assembly"), data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::Persuade);
    FOSSYND_CHECK(data.objective.targetCount >= 1);

    // The mission must actually ship enough civilians to meet its own target.
    std::size_t civilians = 0;
    for (const AgentSpec& spec : data.agents)
    {
        if (spec.team == Team::Civilian)
        {
            ++civilians;
        }
    }
    FOSSYND_CHECK(civilians >= static_cast<std::size_t>(data.objective.targetCount));

    TileGrid grid = buildGrid(data);
    Mission mission(grid, 12u);
    populateMission(data, mission);

    // The player's real action: use the Compliance Beacon on the crowd. The
    // beacon is a per-agent gadget with a cooldown, so pace the uses and let the
    // mission decide the win.
    const int target = mission.objective().targetCount;
    for (int i = 0; i < 60 && mission.state() == MissionState::Running; ++i)
    {
        mission.persuade(0);
        mission.update(1.05f);
    }
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK(mission.followerCount(0) >= static_cast<std::size_t>(target));
}

FOSSYND_TEST(Mission, ShippedOverpassReachIsWinnable)
{
    MissionData data;
    std::string error;
    FOSSYND_CHECK(MissionData::loadFromFile(shippedMission("overpass"), data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::ReachTile);

    TileGrid grid = buildGrid(data);
    const TilePos goal{data.objective.tileX, data.objective.tileY};
    FOSSYND_CHECK(grid.isWalkable(goal.x, goal.y));

    Mission mission(grid, 13u);
    populateMission(data, mission);

    // The goal must be genuinely reachable from the squad, not walled off: a
    // reach objective on an unreachable tile would be silently unwinnable.
    const TilePos start = mission.agents()[0].tile();
    FOSSYND_CHECK(!findPath(grid, start, goal, /*allowDiagonal=*/true).empty());

    // The player's real action: send the squad to the exfil tile and fight
    // through. The mission is won when a living agent stands on the tile.
    for (std::size_t i = 0; i < mission.agents().size(); ++i)
    {
        if (mission.agents()[i].team() == Team::Player)
        {
            mission.orderMove(i, goal);
        }
    }
    runUntilDone(mission, 120.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
}

FOSSYND_TEST(Mission, ShippedPrecinctPoliceAndSaboteurAreReachableAndWinnable)
{
    // #462: the `police` and `saboteur` shared templates must be referenced by a
    // shipped mission, or they are inert catalogue entries. This mission also
    // proves the time-bomb carrier (the saboteur) is reachable in play.
    MissionData data;
    std::string error;
    FOSSYND_CHECK(MissionData::loadFromFile(shippedMission("precinct"), data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::EliminateAll);

    // The two templates really resolved from the catalogue, with their own
    // stats (not the mission's inline defaults).
    bool sawPolice = false;
    bool sawSaboteur = false;
    for (const AgentSpec& spec : data.agents)
    {
        if (spec.name == "PATROL-1")
        {
            FOSSYND_CHECK_EQ(spec.team, Team::Enemy);
            FOSSYND_CHECK_NEAR(spec.health, 75.0f);
            FOSSYND_CHECK_EQ(spec.weapon.name, std::string("Uzi"));
            sawPolice = true;
        }
        if (spec.name == "SABOTEUR")
        {
            FOSSYND_CHECK_EQ(spec.team, Team::Enemy);
            FOSSYND_CHECK_NEAR(spec.health, 80.0f);
            FOSSYND_CHECK_EQ(spec.weapon.name, std::string("Time Bomb"));
            sawSaboteur = true;
        }
    }
    FOSSYND_CHECK(sawPolice);
    FOSSYND_CHECK(sawSaboteur);

    // The mission's eliminate objective must be genuinely satisfiable: every
    // enemy must be reachable from the squad, not walled off. This is checked
    // directly, because the saboteur's time bomb has an area blast, so a boxed-in
    // saboteur would still die to its own splash and mask an unreachable enemy.
    TileGrid grid = buildGrid(data);
    const TilePos start{static_cast<int>(data.agents.front().x),
                        static_cast<int>(data.agents.front().y)};
    for (const AgentSpec& spec : data.agents)
    {
        if (spec.team != Team::Enemy)
        {
            continue;
        }
        const TilePos foe{static_cast<int>(spec.x), static_cast<int>(spec.y)};
        FOSSYND_CHECK(!findPath(grid, start, foe, /*allowDiagonal=*/true).empty());
    }

    Mission mission(grid, 16u);
    populateMission(data, mission);

    for (int guard = 0; guard < 40 && mission.state() == MissionState::Running; ++guard)
    {
        for (std::size_t i = 0; i < mission.agents().size(); ++i)
        {
            if (mission.agents()[i].team() != Team::Player)
            {
                continue;
            }
            for (std::size_t t = 0; t < mission.agents().size(); ++t)
            {
                if (mission.agents()[t].team() == Team::Enemy &&
                    mission.agents()[t].isAlive())
                {
                    mission.orderAttack(i, t);
                }
            }
        }
        runUntilDone(mission, 6.0f);
    }

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    for (const Agent& agent : mission.agents())
    {
        if (agent.team() == Team::Enemy)
        {
            FOSSYND_CHECK(!agent.isAlive());
        }
    }
}

// --- Shipped campaign mission set, slice 2 (issues #444/#473) --------------
//
// The two new objective kinds (assassinate, retrieve) must be reachable in play
// through a *shipped* mission, not merely evaluated by the core (#471) and
// parsed by the loader (#472) — the #99/#101 "landed but never wired in" trap.
// Each case loads the real file through the real loader, builds the real grid,
// and drives the objective's own mechanic to the win.

FOSSYND_TEST(Mission, ShippedDecapitationAssassinateIsWinnable)
{
    MissionData data;
    std::string error;
    FOSSYND_CHECK(MissionData::loadFromFile(shippedMission("decapitation"), data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::Assassinate);

    // The objective must name a real enemy, and specifically the VIP: a missing
    // or mis-indexed target would make the mission unwinnable or win on the
    // wrong kill.
    const int target = data.objective.agentIndex;
    FOSSYND_CHECK(target >= 0);
    FOSSYND_CHECK(static_cast<std::size_t>(target) < data.agents.size());
    FOSSYND_CHECK_EQ(data.agents[static_cast<std::size_t>(target)].team, Team::Enemy);
    FOSSYND_CHECK_EQ(data.agents[static_cast<std::size_t>(target)].name, std::string("OVERSEER"));

    // The garrison must outnumber the single target, so a win proves the named
    // kill rather than a wipe (the whole point of the kind).
    std::size_t enemies = 0;
    for (const AgentSpec& spec : data.agents)
    {
        if (spec.team == Team::Enemy)
        {
            ++enemies;
        }
    }
    FOSSYND_CHECK(enemies >= 2);

    TileGrid grid = buildGrid(data);
    // The VIP must be reachable from the squad, not walled off.
    const TilePos start{static_cast<int>(data.agents.front().x),
                        static_cast<int>(data.agents.front().y)};
    const TilePos vip{static_cast<int>(data.agents[static_cast<std::size_t>(target)].x),
                      static_cast<int>(data.agents[static_cast<std::size_t>(target)].y)};
    FOSSYND_CHECK(!findPath(grid, start, vip, /*allowDiagonal=*/true).empty());

    Mission mission(grid, 21u);
    populateMission(data, mission);
    FOSSYND_CHECK(mission.objective().kind == ObjectiveSpec::Kind::Assassinate);

    // The player's real action: order the squad onto the VIP. Re-issue the order
    // so the squad keeps closing as it fights through the garrison.
    for (int guard = 0; guard < 60 && mission.state() == MissionState::Running; ++guard)
    {
        for (std::size_t i = 0; i < mission.agents().size(); ++i)
        {
            if (mission.agents()[i].team() == Team::Player &&
                mission.agents()[i].isAlive())
            {
                mission.orderAttack(i, static_cast<std::size_t>(target));
            }
        }
        runUntilDone(mission, 4.0f);
    }

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK(!mission.agents()[static_cast<std::size_t>(target)].isAlive());
    // The win was the named kill, not a garrison wipe.
    FOSSYND_CHECK(mission.aliveCount(Team::Enemy) >= static_cast<std::size_t>(1));
}

FOSSYND_TEST(Mission, ShippedExtractionRetrieveIsWinnable)
{
    MissionData data;
    std::string error;
    FOSSYND_CHECK(MissionData::loadFromFile(shippedMission("extraction"), data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(data.objective.kind == ObjectiveSpec::Kind::Retrieve);

    TileGrid grid = buildGrid(data);
    const TilePos item{data.objective.itemX, data.objective.itemY};
    const TilePos exit{data.objective.tileX, data.objective.tileY};
    // Both legs must be walkable and reachable, or the mission is silently
    // unwinnable (the #212 trap).
    FOSSYND_CHECK(grid.isWalkable(item.x, item.y));
    FOSSYND_CHECK(grid.isWalkable(exit.x, exit.y));
    const TilePos start{static_cast<int>(data.agents.front().x),
                        static_cast<int>(data.agents.front().y)};
    FOSSYND_CHECK(!findPath(grid, start, item, /*allowDiagonal=*/true).empty());
    FOSSYND_CHECK(!findPath(grid, item, exit, /*allowDiagonal=*/true).empty());

    Mission mission(grid, 22u);
    populateMission(data, mission);

    const auto orderSquadTo = [&mission](TilePos goal) {
        for (std::size_t i = 0; i < mission.agents().size(); ++i)
        {
            if (mission.agents()[i].team() == Team::Player &&
                mission.agents()[i].isAlive())
            {
                mission.orderMove(i, goal);
            }
        }
    };

    // Leg 1: pick the item up. The mission must not end merely on pickup.
    orderSquadTo(item);
    runUntilDone(mission, 20.0f);
    FOSSYND_CHECK(mission.state() == MissionState::Running);

    // Leg 2: carry it to the exit. The mission is won when a living agent stands
    // on the exit tile *after* the pickup.
    orderSquadTo(exit);
    runUntilDone(mission, 40.0f);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
}

// A shipped mission's `vehicles[]` must reach the board (issue #417). The
// parser (`MissionData::parseVehicle`) filled `data.vehicles` from the start,
// but `populateMission` never registered them with the `Mission`, so a placed
// vehicle was silently inert — the "landed but never wired in" trap. This case
// loads the real shipped mission, confirms the spec parsed, then asserts the
// vehicle is on the `Mission` with its catalogue kind and its authored position.
FOSSYND_TEST(Mission, ShippedPrecinctVehicleReachesTheBoard)
{
    MissionData data;
    std::string error;
    FOSSYND_CHECK(MissionData::loadFromFile(shippedMission("precinct"), data, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(data.vehicles.size(), 1u);

    TileGrid grid = buildGrid(data);
    Mission mission(grid, 16u);
    populateMission(data, mission);

    FOSSYND_CHECK_EQ(mission.vehicles().size(), 1u);
    if (mission.vehicles().size() != 1u)
    {
        return;
    }
    const Vehicle& car = mission.vehicles().front();
    FOSSYND_CHECK(car.kind() == VehicleKind::Car);
    FOSSYND_CHECK_EQ(car.name(), std::string("Car"));
    FOSSYND_CHECK_NEAR(car.position().x, 6.5f);
    FOSSYND_CHECK_NEAR(car.position().y, 21.5f);
    FOSSYND_CHECK(car.isAlive());
    FOSSYND_CHECK(!car.isOccupied());
    // Placed on a walkable/drivable tile, not inside a building or wall.
    FOSSYND_CHECK(grid.isWalkable(car.tile().x, car.tile().y));
    // The vehicle is optional cover, not a required step: the eliminate
    // objective is still satisfiable with it untouched.
    FOSSYND_CHECK(car.hasPath() == false);
}

// --- time bomb (issue #449) ------------------------------------------------
//
// A fused charge: arming queues a detonation at a tile, it lands after the fuse,
// and it uses the *same* area-damage path as an explosive weapon — so a shield
// still covers it and walls in the blast breach. `fuseSeconds == 0` (the
// default) is the old immediate behaviour and must be untouched.
//
// The enemy in these fixtures carries a long-range, zero-accuracy weapon so it
// holds position and never wounds anyone: the only damage in the scene is the
// bomb's, which keeps the assertions exact.

namespace
{
std::size_t addTimeBombTarget(Mission& mission, float x, float y, float health = 100.0f)
{
    return mission.addAgent(makeAgent(x, y, Team::Enemy, health,
                                      Weapon{"None", 0.0f, 100.0f, 1.0f, 0.0f}));
}

// Ticks until no bomb is pending (or a safety guard trips).
void runUntilDetonated(Mission& mission)
{
    int guard = 0;
    while (mission.pendingDetonations() > 0 && guard++ < 400)
    {
        mission.update(0.05f);
    }
}
} // namespace

FOSSYND_TEST(Mission, TimeBombDetonatesAfterTheFuse)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 71u);
    const std::size_t hero =
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    const std::size_t foe = addTimeBombTarget(mission, 10.0f, 2.0f);

    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 0u);
    FOSSYND_CHECK(mission.orderTimeBomb(hero, TilePos{10, 2}));
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 1u);

    // Not yet: the fuse is still burning, so nothing has been damaged.
    mission.update(0.05f);
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 100.0f);
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 1u);

    // After the fuse, the blast lands on the enemy (60 damage) and is cleared.
    runUntilDetonated(mission);
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 0u);
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 40.0f);
}

FOSSYND_TEST(Mission, TimeBombBreachesWallsInItsBlast)
{
    TileGrid grid(30, 30);
    grid.setWalkable(11, 2, false);
    grid.setDestructible(11, 2, true, 10);
    Mission mission(grid, 72u);
    const std::size_t hero =
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    addTimeBombTarget(mission, 10.0f, 2.0f, 1000.0f);

    FOSSYND_CHECK(grid.isDestructible(11, 2));
    FOSSYND_CHECK(mission.orderTimeBomb(hero, TilePos{10, 2}));
    runUntilDetonated(mission);

    // The wall one tile from the impact is inside the blast radius and opens.
    FOSSYND_CHECK(!grid.isDestructible(11, 2));
    FOSSYND_CHECK(grid.isWalkable(11, 2));
}

FOSSYND_TEST(Mission, TimeBombSurvivesItsOwnersDeath)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 73u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    // A second player keeps the mission Running after the owner dies, so the
    // bomb actually gets a chance to tick (a lone player's death ends it).
    mission.addAgent(makeAgent(2.0f, 5.0f, Team::Player, 100.0f));
    const std::size_t foe = addTimeBombTarget(mission, 10.0f, 2.0f);

    FOSSYND_CHECK(mission.orderTimeBomb(hero, TilePos{10, 2}));

    // Kill the owner before the fuse runs out: the bomb is already independent.
    mission.agents()[hero].applyDamage(1000.0f);
    FOSSYND_CHECK(!mission.agents()[hero].isAlive());

    runUntilDetonated(mission);
    FOSSYND_CHECK_EQ(mission.state(), MissionState::Running);
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 40.0f);
}

FOSSYND_TEST(Mission, TimeBombBlastIsBlockedByShield)
{
    // The bomb reuses the area-damage path, so a raised shield absorbs it whole
    // and drops, exactly as it would for an explosive weapon hit.
    TileGrid grid(30, 30);
    Mission mission(grid, 79u);
    const std::size_t hero =
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    const std::size_t foe = addTimeBombTarget(mission, 10.0f, 2.0f);

    mission.setShields(foe, 1);
    FOSSYND_CHECK(mission.activateShield(foe));
    FOSSYND_CHECK(mission.orderTimeBomb(hero, TilePos{10, 2}));
    runUntilDetonated(mission);

    FOSSYND_CHECK(!mission.shieldUp(foe));
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 100.0f); // absorbed
}

FOSSYND_TEST(Mission, TimeBombCannotBeStackedByOneAgent)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 74u);
    const std::size_t hero =
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
    addTimeBombTarget(mission, 10.0f, 2.0f, 1000.0f);

    FOSSYND_CHECK(mission.orderTimeBomb(hero, TilePos{10, 2}));
    // A second plant while the first is still counting down is refused.
    FOSSYND_CHECK(!mission.orderTimeBomb(hero, TilePos{10, 2}));
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 1u);

    // Once it has fired, the agent may plant another.
    runUntilDetonated(mission);
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 0u);
    FOSSYND_CHECK(mission.orderTimeBomb(hero, TilePos{10, 2}));
}

FOSSYND_TEST(Mission, TimeBombRejectsCivilianDeadAndInvalid)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 75u);
    const std::size_t civ = mission.addAgent(makeAgent(5.0f, 5.0f, Team::Civilian, 50.0f));
    const std::size_t dead = mission.addAgent(makeAgent(8.0f, 8.0f, Team::Enemy, 10.0f));
    mission.agents()[dead].applyDamage(100.0f);

    FOSSYND_CHECK(!mission.orderTimeBomb(civ, TilePos{5, 5}));   // not a combatant
    FOSSYND_CHECK(!mission.orderTimeBomb(dead, TilePos{8, 8}));  // corpse
    FOSSYND_CHECK(!mission.orderTimeBomb(999, TilePos{1, 1}));   // out of range
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 0u);
}

FOSSYND_TEST(Mission, TimeBombRejectsADriver)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 76u);
    const std::size_t hero = mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f));
    const std::size_t car = mission.addVehicle(makeCar(3.0f, 2.0f));

    FOSSYND_CHECK(mission.orderHijack(car, hero));
    FOSSYND_CHECK(mission.isDriving(hero));
    FOSSYND_CHECK(!mission.orderTimeBomb(hero, TilePos{10, 2}));
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 0u);
}

FOSSYND_TEST(Mission, FusedWeaponArmsOnFireInsteadOfDamaging)
{
    // A weapon with a fuse does not wound on impact: the shot arms a bomb at the
    // target's tile, and the damage arrives when the fuse runs out. A slow
    // fireRate keeps the shooter from re-arming before the first bomb lands.
    TileGrid grid(30, 30);
    Mission mission(grid, 77u);
    const std::size_t hero =
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"Bomb", 60.0f, 20.0f, 0.5f, 1.0f, 2.0f, 0.5f}));
    const std::size_t foe = addTimeBombTarget(mission, 5.0f, 2.0f, 500.0f);
    mission.orderAttack(hero, foe);

    // The hit roll is stochastic, so fire until the fused weapon arms (a miss
    // leaves the target unhurt, so this cannot mask a damaging hit).
    int guard = 0;
    while (mission.pendingDetonations() == 0 && guard++ < 200)
    {
        mission.update(0.05f);
    }
    // Arming lands no damage now: the target is untouched and a bomb is pending.
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 1u);
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 500.0f);

    // The fuse runs out and the blast lands for the weapon's damage.
    runUntilDetonated(mission);
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 440.0f);
}

FOSSYND_TEST(Mission, ZeroFuseWeaponStillDamagesImmediately)
{
    // fuseSeconds == 0 (the default) must keep the old immediate-hit behaviour
    // and never queue a detonation.
    TileGrid grid(30, 30);
    Mission mission(grid, 78u);
    const std::size_t hero =
        mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                   Weapon{"Gun", 40.0f, 20.0f, 10.0f, 1.0f}));
    const std::size_t foe = addTimeBombTarget(mission, 5.0f, 2.0f);
    mission.orderAttack(hero, foe);

    // Fire until the (stochastic) shot lands.
    int guard = 0;
    while (mission.agents()[foe].health() >= 100.0f && guard++ < 200)
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_EQ(mission.pendingDetonations(), 0u);
    FOSSYND_CHECK_NEAR(mission.agents()[foe].health(), 60.0f);
}

FOSSYND_TEST(Mission, TimeBombDetonationIsDeterministic)
{
    auto run = []() {
        TileGrid grid(30, 30);
        Mission mission(grid, 80u);
        const std::size_t hero =
            mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                                       Weapon{"None", 0.0f, 1.0f, 1.0f, 0.0f}));
        const std::size_t foe = addTimeBombTarget(mission, 10.0f, 2.0f);
        mission.orderTimeBomb(hero, TilePos{10, 2});
        for (int i = 0; i < 80; ++i)
        {
            mission.update(0.05f);
        }
        return std::make_pair(mission.agents()[foe].health(), mission.pendingDetonations());
    };

    const auto a = run();
    const auto b = run();
    FOSSYND_CHECK_NEAR(a.first, b.first);
    FOSSYND_CHECK_EQ(a.second, b.second);
}

