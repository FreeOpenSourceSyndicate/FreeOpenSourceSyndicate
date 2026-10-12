/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/sim/Agent.h"

using namespace foss::core;

FOSS_TEST(Agent, MoveToFindsPath)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSS_CHECK(agent.moveTo(grid, TilePos{4, 0}));
    FOSS_CHECK(agent.hasPath());
}

FOSS_TEST(Agent, MoveToUnreachableStops)
{
    TileGrid grid(5, 5);
    for (int y = 0; y < 5; ++y)
    {
        grid.setWalkable(2, y, false);
    }
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSS_CHECK(!agent.moveTo(grid, TilePos{4, 0}));
    FOSS_CHECK(!agent.hasPath());
}

FOSS_TEST(Agent, ReachesGoalAfterEnoughTime)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 2.0f); // 2 tiles per second
    agent.moveTo(grid, TilePos{4, 0});

    // 4 tiles at 2 tiles/sec = 2 seconds. Step 0.1s at a time.
    for (int i = 0; i < 40; ++i)
    {
        agent.update(0.1f);
    }

    FOSS_CHECK(!agent.hasPath());
    FOSS_CHECK_NEAR(agent.position().x, 4.0f);
    FOSS_CHECK_NEAR(agent.position().y, 0.0f);
}

FOSS_TEST(Agent, MovesIncrementallyAtFixedSpeed)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 1.0f); // 1 tile per second
    agent.moveTo(grid, TilePos{4, 0});

    agent.update(1.0f); // one second -> one tile
    FOSS_CHECK_NEAR(agent.position().x, 1.0f);
    FOSS_CHECK_NEAR(agent.position().y, 0.0f);
    FOSS_CHECK(agent.hasPath());
}

FOSS_TEST(Agent, ZeroOrNegativeDtIsNoOp)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.moveTo(grid, TilePos{4, 0});

    agent.update(0.0f);
    agent.update(-1.0f);
    FOSS_CHECK_NEAR(agent.position().x, 0.0f);
    FOSS_CHECK_NEAR(agent.position().y, 0.0f);
}
