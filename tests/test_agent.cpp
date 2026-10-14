/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/sim/Agent.h"

using namespace fossynd::core;

FOSSYND_TEST(Agent, MoveToFindsPath)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSSYND_CHECK(agent.moveTo(grid, TilePos{4, 0}));
    FOSSYND_CHECK(agent.hasPath());
}

FOSSYND_TEST(Agent, MoveToUnreachableStops)
{
    TileGrid grid(5, 5);
    for (int y = 0; y < 5; ++y)
    {
        grid.setWalkable(2, y, false);
    }
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSSYND_CHECK(!agent.moveTo(grid, TilePos{4, 0}));
    FOSSYND_CHECK(!agent.hasPath());
}

FOSSYND_TEST(Agent, ReachesGoalAfterEnoughTime)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 2.0f); // 2 tiles per second
    agent.moveTo(grid, TilePos{4, 0});

    // 4 tiles at 2 tiles/sec = 2 seconds. Step 0.1s at a time.
    for (int i = 0; i < 40; ++i)
    {
        agent.update(0.1f);
    }

    FOSSYND_CHECK(!agent.hasPath());
    FOSSYND_CHECK_NEAR(agent.position().x, 4.0f);
    FOSSYND_CHECK_NEAR(agent.position().y, 0.0f);
}

FOSSYND_TEST(Agent, MovesIncrementallyAtFixedSpeed)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 1.0f); // 1 tile per second
    agent.moveTo(grid, TilePos{4, 0});

    agent.update(1.0f); // one second -> one tile
    FOSSYND_CHECK_NEAR(agent.position().x, 1.0f);
    FOSSYND_CHECK_NEAR(agent.position().y, 0.0f);
    FOSSYND_CHECK(agent.hasPath());
}

FOSSYND_TEST(Agent, MoveToUsesSmoothDiagonalRoute)
{
    // Regression for the #99/#100 overlap: PR #99's string-pulling was gated
    // behind allowDiagonal=true while moveTo passed the 4-way default, so agents
    // still staircased in game. moveTo must now produce a diagonal route: a
    // non-45-degree goal staircases under 4-way A*, but collapses to a straight
    // line once smoothed.
    TileGrid grid(8, 8);
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSSYND_CHECK(agent.moveTo(grid, TilePos{5, 3}));

    const auto& path = agent.path();
    FOSSYND_CHECK_EQ(static_cast<int>(path.size()), 2); // just start + goal
    FOSSYND_CHECK(path.front() == (TilePos{0, 0}));
    FOSSYND_CHECK(path.back() == (TilePos{5, 3}));
}

FOSSYND_TEST(Agent, ZeroOrNegativeDtIsNoOp)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.moveTo(grid, TilePos{4, 0});

    agent.update(0.0f);
    agent.update(-1.0f);
    FOSSYND_CHECK_NEAR(agent.position().x, 0.0f);
    FOSSYND_CHECK_NEAR(agent.position().y, 0.0f);
}

FOSSYND_TEST(Agent, HealRestoresHealthAndCapsAtMax)
{
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.setMaxHealth(100.0f);
    agent.applyDamage(60.0f);
    FOSSYND_CHECK_NEAR(agent.health(), 40.0f);

    // Healing more than the missing amount caps at max and reports the real gain.
    FOSSYND_CHECK_NEAR(agent.heal(80.0f), 60.0f);
    FOSSYND_CHECK_NEAR(agent.health(), 100.0f);
}

FOSSYND_TEST(Agent, HealPartialAndNoOpCases)
{
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.setMaxHealth(100.0f);
    agent.applyDamage(50.0f);

    FOSSYND_CHECK_NEAR(agent.heal(20.0f), 20.0f);
    FOSSYND_CHECK_NEAR(agent.health(), 70.0f);

    // Zero/negative heal is a no-op; a dead agent is not revived.
    FOSSYND_CHECK_NEAR(agent.heal(0.0f), 0.0f);
    FOSSYND_CHECK_NEAR(agent.heal(-5.0f), 0.0f);

    agent.applyDamage(1000.0f);
    FOSSYND_CHECK(!agent.isAlive());
    FOSSYND_CHECK_NEAR(agent.heal(50.0f), 0.0f);
    FOSSYND_CHECK(!agent.isAlive());
}
