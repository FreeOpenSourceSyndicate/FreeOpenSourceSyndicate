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
//   Escort       — win when the designated agent (`agentIndex`) reaches
//                  (tileX, tileY) alive. Unlike ReachTile it names one unit, so
//                  the objective is lost if that unit dies on the way.
//   Destroy      — win when the target tile (tileX, tileY) has been breached —
//                  it is destructible rubble (issue #217). The mission is not
//                  won merely by killing the defenders.
//   Persuade     — win when at least `targetCount` civilians are followers of
//                  the player's agents (`Mission::followerCount`), tying the win
//                  to the ComplianceBeacon.
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
        Escort,
        Destroy,
        Persuade,
    };

    Kind kind = Kind::EliminateAll;

    // ReachTile / Escort: the target tile. Destroy: the tile to breach.
    // Integer tile coordinates.
    int tileX = 0;
    int tileY = 0;

    // Survive only: mission seconds that must elapse.
    float seconds = 60.0f;

    // Escort only: the agent that must reach the tile alive. Required (>= 0);
    // the loader rejects an escort objective that does not name a valid agent.
    int agentIndex = -1;

    // Persuade only: how many followers the player must have recruited.
    int targetCount = 1;
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
    case ObjectiveSpec::Kind::Escort:
        return "escort";
    case ObjectiveSpec::Kind::Destroy:
        return "destroy";
    case ObjectiveSpec::Kind::Persuade:
        return "persuade";
    }
    return "?";
}

} // namespace fossynd::core
