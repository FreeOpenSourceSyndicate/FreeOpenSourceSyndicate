/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/sim/Agent.h"
#include "core/world/Pathfinding.h"

using namespace fossynd::core;

FOSSYND_TEST(Agent, MoveToFindsPath)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSSYND_CHECK(agent.moveTo(grid, TilePos{4, 0}));
    FOSSYND_CHECK(agent.hasPath());
}

FOSSYND_TEST(Agent, MoveToUnreachableWalksAsCloseAsPossible)
{
    // A full wall at x=2 blocks the goal at (4,0). moveTo no longer drops the
    // order: the agent walks to the reachable tile nearest the goal (#149).
    TileGrid grid(5, 5);
    for (int y = 0; y < 5; ++y)
    {
        grid.setWalkable(2, y, false);
    }
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSSYND_CHECK(agent.moveTo(grid, TilePos{4, 0}));
    FOSSYND_CHECK(agent.hasPath());

    // Guard the size before indexing so a regression fails cleanly rather than
    // dereferencing an empty path.
    FOSSYND_CHECK(!agent.path().empty());
    if (agent.path().empty())
    {
        return;
    }
    const std::vector<TilePos> path = agent.path();
    // The route ends on the start side of the wall, adjacent to it.
    FOSSYND_CHECK(path.back() == (TilePos{1, 0}));
    // Every step is walkable: the route never enters the wall.
    for (const TilePos& tile : path)
    {
        FOSSYND_CHECK(grid.isWalkable(tile.x, tile.y));
    }
}

FOSSYND_TEST(Agent, MoveToReachablePathIsUnchanged)
{
    // When the goal is reachable, moveTo must be exactly findPath: the fallback
    // must not perturb a good route.
    TileGrid grid(8, 8);
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSSYND_CHECK(agent.moveTo(grid, TilePos{5, 3}));

    const std::vector<TilePos> expected = findPath(grid, TilePos{0, 0}, TilePos{5, 3}, true);
    FOSSYND_CHECK(!expected.empty());
    FOSSYND_CHECK(agent.path() == expected);
}

FOSSYND_TEST(Agent, MoveToEnclosedStartDropsOrder)
{
    // Nowhere to go: the start is sealed in, so there is no nearest reachable
    // tile and the order is dropped (the one case that still returns false).
    TileGrid grid(5, 5);
    grid.setWalkable(1, 0, false);
    grid.setWalkable(0, 1, false);
    grid.setWalkable(1, 1, false);
    Agent agent(0.0f, 0.0f, 2.0f);
    FOSSYND_CHECK(!agent.moveTo(grid, TilePos{4, 4}));
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

FOSSYND_TEST(Agent, SightRangeDefaultsToTheLegacyConstant)
{
    // A default agent perceives as far as the AI's old hard-coded `kSightRange`,
    // so adding the field changes no existing behaviour (FOCUS_AREAS Rule 3).
    const Agent agent(0.0f, 0.0f, 1.0f);
    FOSSYND_CHECK_NEAR(agent.sightRange(), kDefaultSightRange);
    FOSSYND_CHECK_NEAR(agent.sightRange(), 12.0f);
}

FOSSYND_TEST(Agent, SightRangeRoundTrips)
{
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.setSightRange(20.0f);
    FOSSYND_CHECK_NEAR(agent.sightRange(), 20.0f);
    // A cybernetic "eyes" upgrade can lower as well as raise it.
    agent.setSightRange(4.5f);
    FOSSYND_CHECK_NEAR(agent.sightRange(), 4.5f);
}

FOSSYND_TEST(Agent, SightRangeClampsNegativeToZero)
{
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.setSightRange(-7.0f);
    FOSSYND_CHECK_NEAR(agent.sightRange(), 0.0f);
    // Zero is a legal "blind" value, and a later positive set is unaffected.
    agent.setSightRange(0.0f);
    FOSSYND_CHECK_NEAR(agent.sightRange(), 0.0f);
    agent.setSightRange(9.0f);
    FOSSYND_CHECK_NEAR(agent.sightRange(), 9.0f);
}

FOSSYND_TEST(Agent, SightRangeIsPerAgent)
{
    // Two agents can perceive differently — the point of moving this off the
    // mission-wide constant.
    Agent sharp(0.0f, 0.0f, 1.0f);
    Agent dull(0.0f, 1.0f, 1.0f);
    sharp.setSightRange(18.0f);
    dull.setSightRange(3.0f);
    FOSSYND_CHECK(sharp.sightRange() > dull.sightRange());
    FOSSYND_CHECK_NEAR(dull.sightRange(), 3.0f);
}
