/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>

namespace foss::core
{

// A small, fully deterministic pseudo-random generator (xorshift64*).
//
// Determinism matters: the simulation must produce identical results for a
// given seed and input sequence, so tests can assert exact outcomes and replays
// are reproducible. Never use <random> or rand() in core code.
class Rng
{
public:
    explicit Rng(std::uint64_t seed = 0x9E3779B97F4A7C15ull)
        : state_(seed == 0 ? 0x9E3779B97F4A7C15ull : seed)
    {
    }

    std::uint64_t nextUint64()
    {
        std::uint64_t x = state_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state_ = x;
        return x * 0x2545F4914F6CDD1Dull;
    }

    // Uniform integer in [0, bound). Returns 0 when bound <= 0.
    int nextInt(int bound)
    {
        if (bound <= 0)
        {
            return 0;
        }
        return static_cast<int>(nextUint64() % static_cast<std::uint64_t>(bound));
    }

    // Uniform float in [0, 1).
    float nextFloat()
    {
        return static_cast<float>(nextUint64() >> 11) * (1.0f / 9007199254740992.0f);
    }

    // True with probability `chance` (clamped to [0, 1]).
    bool chance(float probability)
    {
        if (probability <= 0.0f)
        {
            return false;
        }
        if (probability >= 1.0f)
        {
            return true;
        }
        return nextFloat() < probability;
    }

    std::uint64_t state() const { return state_; }
    void setState(std::uint64_t state) { state_ = state == 0 ? 1u : state; }

private:
    std::uint64_t state_;
};

} // namespace foss::core
