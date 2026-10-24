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
//   Assassinate  — win when the single agent named by `agentIndex` is dead,
//                  regardless of the other defenders. Unlike EliminateAll the
//                  garrison may survive; the target's death alone is the win.
//   Retrieve     — win when the squad has picked up the item on (itemX, itemY)
//                  and a living player agent then stands on the exit tile
//                  (tileX, tileY). A latched squad-level "carried flag": once
//                  picked up the item stays retrieved, so any member may carry
//                  it out. Unlike Escort it names no unit, so no member's death
//                  is itself a loss (the universal squad-dead rule still is).
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
        Assassinate,
        Retrieve,
    };

    Kind kind = Kind::EliminateAll;

    // ReachTile / Escort: the target tile. Destroy: the tile to breach.
    // Retrieve: the exit tile the item must be brought to. Integer tile
    // coordinates.
    int tileX = 0;
    int tileY = 0;

    // Survive only: mission seconds that must elapse.
    float seconds = 60.0f;

    // Escort / Assassinate only: the agent the objective names — the unit to
    // deliver alive (escort) or the unit to kill (assassinate). Required
    // (>= 0); the loader rejects an objective that does not name a valid agent.
    int agentIndex = -1;

    // Persuade only: how many followers the player must have recruited.
    int targetCount = 1;

    // Retrieve only: the tile the item rests on. Integer tile coordinates. The
    // exit tile is (tileX, tileY).
    int itemX = 0;
    int itemY = 0;
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
    case ObjectiveSpec::Kind::Assassinate:
        return "assassinate";
    case ObjectiveSpec::Kind::Retrieve:
        return "retrieve";
    }
    return "?";
}

} // namespace fossynd::core
