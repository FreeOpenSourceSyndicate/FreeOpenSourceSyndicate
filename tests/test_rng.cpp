/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/util/Rng.h"

using namespace fossynd::core;

FOSSYND_TEST(Rng, SameSeedSameSequence)
{
    Rng a(12345u);
    Rng b(12345u);
    for (int i = 0; i < 100; ++i)
    {
        FOSSYND_CHECK_EQ(a.nextUint64(), b.nextUint64());
    }
}

FOSSYND_TEST(Rng, DifferentSeedsDiffer)
{
    Rng a(1u);
    Rng b(2u);
    bool differed = false;
    for (int i = 0; i < 10; ++i)
    {
        if (a.nextUint64() != b.nextUint64())
        {
            differed = true;
        }
    }
    FOSSYND_CHECK(differed);
}

FOSSYND_TEST(Rng, NextIntRespectsBound)
{
    Rng rng(99u);
    for (int i = 0; i < 1000; ++i)
    {
        const int value = rng.nextInt(7);
        FOSSYND_CHECK(value >= 0 && value < 7);
    }
    FOSSYND_CHECK_EQ(rng.nextInt(0), 0);
}

FOSSYND_TEST(Rng, NextFloatInUnitRange)
{
    Rng rng(7u);
    for (int i = 0; i < 1000; ++i)
    {
        const float value = rng.nextFloat();
        FOSSYND_CHECK(value >= 0.0f && value < 1.0f);
    }
}

// The 53-bit draw whose float-narrowing rounds up to 2^53 would return exactly
// 1.0f and break the documented [0, 1) range. Pinned so the clamp cannot regress
// (issue #383); a plain 1000-draw loop never reaches this state.
FOSSYND_TEST(Rng, NextFloatNeverReachesOne)
{
    Rng rng(1u);
    rng.setState(3437049264879337801ull);
    const float value = rng.nextFloat();
    FOSSYND_CHECK(value < 1.0f);
    FOSSYND_CHECK(value >= 0.0f);
}

FOSSYND_TEST(Rng, ChanceIsDeterministicPerSeed)
{
    Rng a(42u);
    Rng b(42u);
    int hits = 0;
    for (int i = 0; i < 1000; ++i)
    {
        const bool hit = a.chance(0.5f);
        FOSSYND_CHECK_EQ(hit, b.chance(0.5f));
        if (hit)
        {
            ++hits;
        }
    }
    FOSSYND_CHECK_EQ(a.state(), b.state());
    // A fair coin over 1000 flips should be comfortably inside this band.
    FOSSYND_CHECK(hits > 350 && hits < 650);
}
