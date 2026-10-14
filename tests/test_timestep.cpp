/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"

#include "core/math/Timestep.h"
#include "core/sim/Agent.h"

using namespace fossynd::core;

// --- FixedTimestep accumulator ------------------------------------------------

FOSSYND_TEST(Timestep, AccumulatesWholeSteps)
{
    FixedTimestep clock(0.1f);
    // 0.25s at a 0.1s step => two whole steps, 0.05s left over.
    FOSSYND_CHECK_EQ(clock.advance(0.25f), 2);
    FOSSYND_CHECK_NEAR(clock.alpha(), 0.5f);
}

FOSSYND_TEST(Timestep, NoStepUntilAccumulated)
{
    FixedTimestep clock(0.1f);
    FOSSYND_CHECK_EQ(clock.advance(0.04f), 0);
    FOSSYND_CHECK_EQ(clock.advance(0.04f), 0);
    FOSSYND_CHECK_EQ(clock.advance(0.04f), 1); // 0.12s banked
}

FOSSYND_TEST(Timestep, AlphaStaysInUnitRange)
{
    FixedTimestep clock(0.1f);
    for (int i = 0; i < 100; ++i)
    {
        clock.advance(0.037f);
        FOSSYND_CHECK(clock.alpha() >= 0.0f);
        FOSSYND_CHECK(clock.alpha() < 1.0f);
    }
}

FOSSYND_TEST(Timestep, NonPositiveFrameIsNoOp)
{
    FixedTimestep clock(0.1f);
    FOSSYND_CHECK_EQ(clock.advance(0.0f), 0);
    FOSSYND_CHECK_EQ(clock.advance(-5.0f), 0);
    FOSSYND_CHECK_NEAR(clock.alpha(), 0.0f);
}

FOSSYND_TEST(Timestep, ClampsLongFrameToCap)
{
    FixedTimestep clock(0.1f, 0.25f);
    // A 10s stall is clamped to the 0.25s cap => two steps, not a burst of 100.
    FOSSYND_CHECK_EQ(clock.advance(10.0f), 2);
}

FOSSYND_TEST(Timestep, ResetDropsBankedTime)
{
    FixedTimestep clock(0.1f);
    clock.advance(0.05f);
    clock.reset();
    FOSSYND_CHECK_NEAR(clock.alpha(), 0.0f);
    FOSSYND_CHECK_EQ(clock.advance(0.05f), 0);
}

FOSSYND_TEST(Timestep, DefaultsToSixtyHertz)
{
    FixedTimestep clock;
    FOSSYND_CHECK_NEAR(clock.step(), 1.0f / 60.0f);
    // Exactly one default step's worth of time yields one step.
    FOSSYND_CHECK_EQ(clock.advance(1.0f / 60.0f), 1);
}

// --- Agent interpolation ------------------------------------------------------

FOSSYND_TEST(Timestep, InterpolationBlendsPreviousAndCurrent)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 1.0f); // 1 tile per second
    agent.moveTo(grid, TilePos{4, 0});
    agent.update(1.0f); // moves from x=0 to x=1

    const Vec2 atStart = agent.interpolatedPosition(0.0f);
    FOSSYND_CHECK_NEAR(atStart.x, 0.0f);
    FOSSYND_CHECK_NEAR(atStart.y, 0.0f);

    const Vec2 atEnd = agent.interpolatedPosition(1.0f);
    FOSSYND_CHECK_NEAR(atEnd.x, 1.0f);

    const Vec2 half = agent.interpolatedPosition(0.5f);
    FOSSYND_CHECK_NEAR(half.x, 0.5f);
    FOSSYND_CHECK_NEAR(half.y, 0.0f);
}

FOSSYND_TEST(Timestep, InterpolationClampsOutOfRangeAlpha)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 1.0f);
    agent.moveTo(grid, TilePos{4, 0});
    agent.update(1.0f);

    FOSSYND_CHECK_NEAR(agent.interpolatedPosition(-1.0f).x, agent.previousPosition().x);
    FOSSYND_CHECK_NEAR(agent.interpolatedPosition(2.0f).x, agent.position().x);
}

FOSSYND_TEST(Timestep, InterpolationDoesNotChangeSimulationPosition)
{
    TileGrid grid(5, 5);
    Agent agent(0.0f, 0.0f, 2.0f);
    agent.moveTo(grid, TilePos{4, 0});

    for (int i = 0; i < 10; ++i)
    {
        agent.update(0.1f);
        // Reading the interpolated view must not perturb the authoritative
        // position the simulation will use next step (golden rule 4).
        (void)agent.interpolatedPosition(0.5f);
    }

    FOSSYND_CHECK_NEAR(agent.position().x, 2.0f);
    FOSSYND_CHECK_NEAR(agent.position().y, 0.0f);
}

FOSSYND_TEST(Timestep, SetPositionSnapsInterpolation)
{
    Agent agent(1.0f, 1.0f, 1.0f);
    agent.setPosition(8.0f, 3.0f);
    // No stale previous position: a teleport interpolates to itself.
    const Vec2 p = agent.interpolatedPosition(0.0f);
    FOSSYND_CHECK_NEAR(p.x, 8.0f);
    FOSSYND_CHECK_NEAR(p.y, 3.0f);
}

FOSSYND_TEST(Timestep, StationaryAgentInterpolatesToItself)
{
    Agent agent(2.0f, 2.0f, 1.0f);
    agent.update(0.5f); // no path: nothing moves
    FOSSYND_CHECK_NEAR(agent.interpolatedPosition(0.0f).x, 2.0f);
    FOSSYND_CHECK_NEAR(agent.interpolatedPosition(1.0f).y, 2.0f);
}
