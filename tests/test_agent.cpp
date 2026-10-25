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

// An off-centre agent must never clip the corner of a blocked tile on its way to
// the first waypoint. A path is validated centre-to-centre, so `moveTo` now keeps
// the agent's own tile centre as the first waypoint: skipping it let an agent
// standing at a fractional position walk straight at the second waypoint and cut
// a corner the centre line never crossed, landing inside a wall (and, once its
// tile was blocked, `findPathToNearest` could not path it back out).
FOSSYND_TEST(Agent, MoveToNeverEntersABlockedTileFromAnOffCentrePosition)
{
    TileGrid grid(8, 7);
    const TilePos blocked[] = {{6, 0}, {5, 1}, {7, 2}, {6, 3}, {1, 5}, {3, 5}, {0, 6}};
    for (const TilePos& b : blocked)
    {
        grid.setWalkable(b.x, b.y, false);
    }

    // Standing off-centre inside walkable (5,2) — the position a mid-move agent
    // actually has, not the integer centre the tests above assume.
    Agent agent(0.0f, 0.0f, 3.0f);
    agent.setPosition(5.29069996f, 1.57879996f);
    FOSSYND_CHECK_EQ(agent.tile().x, 5);
    FOSSYND_CHECK_EQ(agent.tile().y, 2);

    FOSSYND_CHECK(agent.moveTo(grid, TilePos{0, 1}));
    for (int tick = 0; tick < 800; ++tick)
    {
        agent.update(1.0f / 60.0f);
        const TilePos at = agent.tile();
        if (grid.inBounds(at.x, at.y))
        {
            FOSSYND_CHECK(grid.isWalkable(at.x, at.y));
        }
    }
}

// Re-issuing the *same* route every tick must keep the agent's progress, not
// yank it back to its own tile centre. The mission AI re-paths every tick while
// retreating (`Mission::stepAwayFrom`), so a `moveTo` that reset the waypoint
// index on every call would oscillate the agent in place and it would never
// arrive. Without the same-route guard this agent never leaves (0,0); with it,
// it walks the bent route to the goal.
FOSSYND_TEST(Agent, MoveToReissuedSameRouteKeepsProgress)
{
    TileGrid grid(12, 12);
    for (int y = 0; y < 12; ++y)
    {
        grid.setWalkable(9, y, false); // a wall to bend the route around
    }

    Agent agent(0.0f, 0.0f, 2.0f);
    FOSSYND_CHECK(agent.moveTo(grid, TilePos{5, 5}));

    bool reached = false;
    for (int tick = 0; tick < 3000; ++tick)
    {
        agent.update(1.0f / 60.0f);
        if (agent.tile() == (TilePos{5, 5}))
        {
            reached = true;
            break;
        }
        FOSSYND_CHECK(agent.moveTo(grid, TilePos{5, 5})); // re-issue, unchanged
    }
    FOSSYND_CHECK(reached);
    FOSSYND_CHECK_EQ(agent.tile(), (TilePos{5, 5}));
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

FOSSYND_TEST(Agent, SetMaxHealthPreservingFractionKeepsAWoundedAgentWounded)
{
    // The physical drug bar recomputes max health from a base every tick. If the
    // setter filled to the new maximum (as setMaxHealth does), a live bar would
    // grant free healing each tick; this must preserve the fraction instead.
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.setMaxHealth(100.0f);
    agent.applyDamage(50.0f); // 50/100 = 0.5
    FOSSYND_CHECK_NEAR(agent.healthFraction(), 0.5f);

    agent.setMaxHealthPreservingFraction(140.0f);
    FOSSYND_CHECK_NEAR(agent.maxHealth(), 140.0f);
    FOSSYND_CHECK_NEAR(agent.health(), 70.0f); // still 0.5
    FOSSYND_CHECK_NEAR(agent.healthFraction(), 0.5f);

    // Lowering the maximum keeps the fraction too (no free wound beyond it).
    agent.setMaxHealthPreservingFraction(60.0f);
    FOSSYND_CHECK_NEAR(agent.maxHealth(), 60.0f);
    FOSSYND_CHECK_NEAR(agent.health(), 30.0f);
    FOSSYND_CHECK_NEAR(agent.healthFraction(), 0.5f);
}

FOSSYND_TEST(Agent, SetMaxHealthPreservingFractionClampsAndKeepsDeadDead)
{
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.setMaxHealth(100.0f);
    // A maximum below 1 is clamped up, exactly like setMaxHealth.
    agent.setMaxHealthPreservingFraction(0.0f);
    FOSSYND_CHECK_NEAR(agent.maxHealth(), 1.0f);

    // A full-health agent stays full after the recompute.
    Agent full(0.0f, 0.0f, 1.0f);
    full.setMaxHealth(100.0f);
    full.setMaxHealthPreservingFraction(150.0f);
    FOSSYND_CHECK_NEAR(full.health(), 150.0f);

    // A dead agent (health 0) must not be revived by the recompute.
    Agent dead(0.0f, 0.0f, 1.0f);
    dead.setMaxHealth(100.0f);
    dead.applyDamage(1000.0f);
    FOSSYND_CHECK(!dead.isAlive());
    dead.setMaxHealthPreservingFraction(200.0f);
    FOSSYND_CHECK(!dead.isAlive());
    FOSSYND_CHECK_NEAR(dead.health(), 0.0f);
}
