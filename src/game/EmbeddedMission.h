/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

namespace foss::game
{

// The demo mission (assets/data/missions/demo.json) embedded into the binary at
// build time, so the game still runs as a single self-contained executable on a
// machine that has no data files. `Game::buildMission` prefers an external file
// when one is present (for modding), then falls back to this embedded copy.
extern const char* const kDemoMissionJson;

} // namespace foss::game
