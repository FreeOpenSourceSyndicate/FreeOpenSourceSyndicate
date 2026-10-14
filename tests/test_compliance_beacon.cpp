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

void runUntilDone(Mission& mission, float maxSeconds, float dt = 0.05f)
{
    for (float t = 0.0f; t < maxSeconds && mission.state() == MissionState::Running; t += dt)
    {
        mission.update(dt);
    }
}

} // namespace

FOSSYND_TEST(ComplianceBeacon, RecruitsNearestCivilianInRange)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 21u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));    // 0
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian));  // 1, nearest
    mission.addAgent(makeAgent(12.0f, 2.0f, Team::Civilian)); // 2, out of range

    FOSSYND_CHECK_EQ(mission.persuade(0), 1);
    FOSSYND_CHECK(mission.agents()[1].isFollower());
    FOSSYND_CHECK_EQ(mission.agents()[1].followerLeader(), 0);
    FOSSYND_CHECK(!mission.agents()[2].isFollower());
    FOSSYND_CHECK_EQ(mission.followerCount(0), static_cast<std::size_t>(1));
}

FOSSYND_TEST(ComplianceBeacon, IgnoresTargetsOutOfRange)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 22u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    mission.addAgent(makeAgent(20.0f, 2.0f, Team::Civilian)); // device range is 4 tiles

    FOSSYND_CHECK_EQ(mission.persuade(0), -1);
    FOSSYND_CHECK_EQ(mission.followerCount(0), static_cast<std::size_t>(0));
}

FOSSYND_TEST(ComplianceBeacon, CooldownBlocksRepeatedUse)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 23u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));   // 0
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian)); // 1
    mission.addAgent(makeAgent(3.0f, 3.0f, Team::Civilian)); // 2, also in range
    // A far, harmless enemy keeps the mission running so timers tick.
    mission.addAgent(makeAgent(28.0f, 28.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    FOSSYND_CHECK_EQ(mission.persuade(0), 1);
    FOSSYND_CHECK(mission.agents()[0].persuadeCooldown() > 0.0f);
    // A second use in the same instant is refused while the device is cooling.
    FOSSYND_CHECK_EQ(mission.persuade(0), -1);

    // After the cooldown elapses, the next civilian can be recruited.
    for (int i = 0; i < 30; ++i) // 1.5s > the 1.0s cooldown
    {
        mission.update(0.05f);
    }
    FOSSYND_CHECK_EQ(mission.persuade(0), 2);
    FOSSYND_CHECK_EQ(mission.followerCount(0), static_cast<std::size_t>(2));
}

FOSSYND_TEST(ComplianceBeacon, FollowerTrailsTheLeader)
{
    TileGrid grid(40, 40);
    Mission mission(grid, 24u);
    const std::size_t leader = mission.addAgent(makeAgent(5.0f, 2.0f, Team::Player));
    const std::size_t follower = mission.addAgent(makeAgent(9.0f, 2.0f, Team::Civilian));
    // A far, harmless enemy keeps the mission running so movement can be observed.
    mission.addAgent(makeAgent(38.0f, 38.0f, Team::Enemy, 100000.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    FOSSYND_CHECK_EQ(mission.persuade(leader), static_cast<int>(follower));
    const Vec2 start = mission.agents()[follower].position();
    const float startGap = length(mission.agents()[leader].position() - start);

    mission.orderMove(leader, TilePos{20, 2});
    for (int i = 0; i < 240; ++i) // 12s: the leader needs 7.5s to cross 15 tiles
    {
        mission.update(0.05f);
    }

    const Vec2 end = mission.agents()[follower].position();
    FOSSYND_CHECK(length(end - start) > 5.0f);              // it actually moved
    FOSSYND_CHECK(mission.agents()[follower].isFollower()); // still recruited
    FOSSYND_CHECK(mission.agents()[follower].team() == Team::Civilian);
    const float endGap = length(mission.agents()[leader].position() - end);
    FOSSYND_CHECK(endGap < startGap); // it closed the distance
    FOSSYND_CHECK(endGap < 3.0f);     // and kept up
}

FOSSYND_TEST(ComplianceBeacon, FreeReleasesFollowers)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 25u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian));

    FOSSYND_CHECK_EQ(mission.persuade(0), 1);
    FOSSYND_CHECK_EQ(mission.followerCount(0), static_cast<std::size_t>(1));

    FOSSYND_CHECK_EQ(mission.freeFollowersOf(0), static_cast<std::size_t>(1));
    FOSSYND_CHECK(!mission.agents()[1].isFollower());
    FOSSYND_CHECK_EQ(mission.followerCount(0), static_cast<std::size_t>(0));
}

FOSSYND_TEST(ComplianceBeacon, FollowersAreNotAutoTargeted)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 26u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player, 100.0f,
                               Weapon{"Test", 1000.0f, 10.0f, 10.0f, 1.0f}));
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian));
    mission.addAgent(makeAgent(8.0f, 2.0f, Team::Enemy, 100.0f,
                               Weapon{"None", 0.0f, 0.5f, 1.0f, 0.0f}));

    FOSSYND_CHECK_EQ(mission.persuade(0), 1);
    runUntilDone(mission, 10.0f);

    FOSSYND_CHECK_EQ(mission.state(), MissionState::Won);
    FOSSYND_CHECK(mission.agents()[1].isAlive()); // the follower was never targeted
    FOSSYND_CHECK_EQ(mission.civilianKills(), 0);
}

FOSSYND_TEST(ComplianceBeacon, NonCombatantCannotPersuade)
{
    TileGrid grid(30, 30);
    Mission mission(grid, 27u);
    mission.addAgent(makeAgent(2.0f, 2.0f, Team::Player));   // 0
    mission.addAgent(makeAgent(3.0f, 2.0f, Team::Civilian)); // 1
    mission.addAgent(makeAgent(3.5f, 2.0f, Team::Civilian)); // 2

    // A civilian has no ComplianceBeacon.
    FOSSYND_CHECK_EQ(mission.persuade(1), -1);
    FOSSYND_CHECK_EQ(mission.followerCount(1), static_cast<std::size_t>(0));
}
