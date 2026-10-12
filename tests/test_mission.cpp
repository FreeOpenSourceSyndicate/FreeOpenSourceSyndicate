/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/sim/Mission.h"

using namespace foss::core;

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

FOSS_TEST(Mission, DamageKillsAgent)
{
    Agent agent(0.0f, 0.0f, 2.0f);
    agent.setMaxHealth(20.0f);
    FOSS_CHECK(agent.isAlive());
    FOSS_CHECK_NEAR(agent.applyDamage(15.0f), 15.0f);
    FOSS_CHECK(agent.isAlive());
    FOSS_CHECK_NEAR(agent.healthFraction(), 0.25f);
    agent.applyDamage(100.0f);
    FOSS_CHECK(!agent.isAlive());
    FOSS_CHECK_NEAR(agent.healthFraction(), 0.0f);
}

FOSS_TEST(Mission, DamageOnDeadAgentIsZero)
{
    Agent agent(0.0f, 0.0f, 2.0f);
    agent.setMaxHealth(10.0f);
    agent.applyDamage(10.0f);
    FOSS_CHECK(!agent.isAlive());
    FOSS_CHECK_NEAR(agent.applyDamage(5.0f), 0.0f);
}

FOSS_TEST(Mission, PlayerWinsWhenAllEnemiesDie)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 1u);
    Agent hero = makeAgent(2.0f, 2.0f, Team::Player, 100.0f, Weapon{"Test", 1000.0f, 5.0f, 10.0f, 1.0f});
    Agent foe = makeAgent(3.0f, 2.0f, Team::Enemy, 30.0f);
    const std::size_t heroIndex = mission.addAgent(std::move(hero));
    mission.addAgent(std::move(foe));

    mission.orderMove(heroIndex, TilePos{4, 2});
    runUntilDone(mission, 5.0f);

    FOSS_CHECK_EQ(mission.state(), MissionState::Won);
    FOSS_CHECK_EQ(mission.playerKills(), 1);
}

FOSS_TEST(Mission, PlayerLosesWhenSquadDies)
{
    TileGrid grid(20, 20);
    Mission mission(grid, 2u);
    Agent hero = makeAgent(2.0f, 2.0f, Team::Player, 20.0f);
    Agent foe = makeAgent(3.0f, 2.0f, Team::Enemy, 100.0f, Weapon{"Test", 1000.0f, 5.0f, 10.0f, 1.0f});
    mission.addAgent(std::move(hero));
    mission.addAgent(std::move(foe));

    runUntilDone(mission, 5.0f);

    FOSS_CHECK_EQ(mission.state(), MissionState::Lost);
    FOSS_CHECK_EQ(mission.enemyKills(), 1);
}

FOSS_TEST(Mission, EnemyClosesToWeaponRange)
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

    FOSS_CHECK(endDistance < startDistance);
    FOSS_CHECK(mission.state() == MissionState::Running);
}

FOSS_TEST(Mission, SameSeedProducesIdenticalOutcome)
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
    FOSS_CHECK_EQ(a.first, b.first);
    FOSS_CHECK_EQ(a.second, b.second);
}

FOSS_TEST(Mission, NoOrdersStalemateStaysRunning)
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
    FOSS_CHECK(mission.state() == MissionState::Running);
}
