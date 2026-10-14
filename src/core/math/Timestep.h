/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

namespace fossynd::core
{

// Fixed-timestep accumulator (AGENTS.md golden rule 4: simulation != rendering).
//
// Gameplay must tick on a fixed step so outcomes never depend on frame rate;
// the renderer draws between steps using an interpolation factor. This helper
// owns only the *timing* arithmetic — it runs no simulation itself — so the
// view can drive it without touching game rules.
//
// Usage each frame:
//
//   const int steps = clock.advance(frameSeconds);
//   for (int i = 0; i < steps; ++i) { mission.update(clock.step()); }
//   render(clock.alpha());
//
// Header-only and dependency-free: it must build in a plain core-only
// configuration with no engine headers (ADR-0002).
class FixedTimestep
{
public:
    // `stepSeconds` is the fixed simulation step; `maxFrameSeconds` bounds a
    // single frame so a long stall cannot trigger an unbounded catch-up burst
    // (the spiral-of-death guard). The remainder beyond the cap is dropped.
    explicit FixedTimestep(float stepSeconds = 1.0f / 60.0f, float maxFrameSeconds = 0.25f)
        : step_(stepSeconds > 0.0f ? stepSeconds : 1.0f / 60.0f)
        , maxFrameSeconds_(maxFrameSeconds > 0.0f ? maxFrameSeconds : 0.25f)
    {
    }

    // Adds one frame's elapsed time and returns how many whole fixed steps the
    // caller should now run. Non-positive input is ignored (no steps, no time
    // banked). A frame longer than the cap is clamped before accumulating.
    int advance(float frameSeconds)
    {
        if (frameSeconds <= 0.0f)
        {
            return 0;
        }
        accumulator_ += (frameSeconds < maxFrameSeconds_) ? frameSeconds : maxFrameSeconds_;

        int steps = 0;
        while (accumulator_ >= step_)
        {
            accumulator_ -= step_;
            ++steps;
        }
        return steps;
    }

    // Length of one fixed simulation step, in seconds.
    float step() const { return step_; }

    // Fraction into the next step, in [0, 1). Pass to Agent::interpolatedPosition.
    float alpha() const { return accumulator_ / step_; }

    // Drops any banked time (e.g. after a pause or a restart).
    void reset() { accumulator_ = 0.0f; }

private:
    float step_;
    float maxFrameSeconds_;
    float accumulator_ = 0.0f;
};

} // namespace fossynd::core
