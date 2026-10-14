/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

namespace fossynd::core
{

// The win condition of a mission, data-driven (ADR-0003). Engine-agnostic: the
// view reads it, the simulation evaluates it (`Mission::evaluateOutcome`).
//
//   EliminateAll — win when no enemy agent is left (the default; unchanged).
//   ReachTile    — win as soon as any living player agent stands on (tileX,
//                  tileY). A classic "get to the exit" objective.
//   Survive      — win after `seconds` of mission time have elapsed.
//
// The lose condition is universal and unchanged: every player agent is dead.
// Civilians never affect win/lose.
struct ObjectiveSpec
{
    enum class Kind
    {
        EliminateAll,
        ReachTile,
        Survive,
    };

    Kind kind = Kind::EliminateAll;

    // ReachTile only: the target tile (integer tile coordinates).
    int tileX = 0;
    int tileY = 0;

    // Survive only: mission seconds that must elapse.
    float seconds = 60.0f;
};

// Human-readable name of an objective kind, for logs, errors, and the HUD.
inline const char* objectiveKindName(ObjectiveSpec::Kind kind)
{
    switch (kind)
    {
    case ObjectiveSpec::Kind::EliminateAll:
        return "eliminate";
    case ObjectiveSpec::Kind::ReachTile:
        return "reach";
    case ObjectiveSpec::Kind::Survive:
        return "survive";
    }
    return "?";
}

} // namespace fossynd::core
