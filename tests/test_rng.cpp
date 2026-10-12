/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/util/Rng.h"

using namespace foss::core;

FOSS_TEST(Rng, SameSeedSameSequence)
{
    Rng a(12345u);
    Rng b(12345u);
    for (int i = 0; i < 100; ++i)
    {
        FOSS_CHECK_EQ(a.nextUint64(), b.nextUint64());
    }
}

FOSS_TEST(Rng, DifferentSeedsDiffer)
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
    FOSS_CHECK(differed);
}

FOSS_TEST(Rng, NextIntRespectsBound)
{
    Rng rng(99u);
    for (int i = 0; i < 1000; ++i)
    {
        const int value = rng.nextInt(7);
        FOSS_CHECK(value >= 0 && value < 7);
    }
    FOSS_CHECK_EQ(rng.nextInt(0), 0);
}

FOSS_TEST(Rng, NextFloatInUnitRange)
{
    Rng rng(7u);
    for (int i = 0; i < 1000; ++i)
    {
        const float value = rng.nextFloat();
        FOSS_CHECK(value >= 0.0f && value < 1.0f);
    }
}

FOSS_TEST(Rng, ChanceIsDeterministicPerSeed)
{
    Rng a(42u);
    Rng b(42u);
    int hits = 0;
    for (int i = 0; i < 1000; ++i)
    {
        const bool hit = a.chance(0.5f);
        FOSS_CHECK_EQ(hit, b.chance(0.5f));
        if (hit)
        {
            ++hits;
        }
    }
    FOSS_CHECK_EQ(a.state(), b.state());
    // A fair coin over 1000 flips should be comfortably inside this band.
    FOSS_CHECK(hits > 350 && hits < 650);
}
