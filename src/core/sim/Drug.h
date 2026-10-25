/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>

namespace fossynd::core
{

// The three regulated drug bars of the v1.0.0 model (ADR-0043). Drugs are not an
// inventory item: each agent carries three bars, each in [0, 1], that the player
// raises and lowers. `Mission` holds the per-agent state and the normalisation;
// this header is the shared vocabulary so consumers do not re-declare it.
enum class Drug
{
    Adrenalin,
    Intelligence,
    Physical,
};

// Number of drug bars. A constant so arrays and loops stay in step with the enum.
constexpr int kDrugCount = 3;

// The level every bar starts at, and the value the drifting neutral point starts
// at (the body's resting adaptation). 0.5 is "normal": an agent at 0.5 on every
// bar has no drug effect at all.
constexpr float kDrugNeutralStart = 0.5f;

// Maps a bar to its array slot. The enum is contiguous and starts at 0, so this
// is just a cast, but keeping it in one place means a future reorder stays safe.
inline std::size_t drugSlot(Drug drug)
{
    return static_cast<std::size_t>(drug);
}

// One agent's drug state: the current (player-regulated) level of each bar and
// the drifting neutral point it is measured against. `level` is set by the player
// and never drifts; `neutral` drifts toward `level` over time (normalisation), so
// a permanently-maxed bar stops paying off and dropping it is a real penalty. The
// value a consumer reads is `level - neutral` (see `Mission::drugEffect`).
//
// Engine-agnostic and data-only (it holds floats, never a resolved `Agent`), so
// `core/sim` stays free of the content loader — mirrors `CyberneticEffect`
// (ADR-0031).
struct DrugState
{
    float level[kDrugCount] = {kDrugNeutralStart, kDrugNeutralStart, kDrugNeutralStart};
    float neutral[kDrugCount] = {kDrugNeutralStart, kDrugNeutralStart, kDrugNeutralStart};
};

} // namespace fossynd::core
