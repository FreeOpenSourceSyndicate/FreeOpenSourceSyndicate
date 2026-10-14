/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

namespace fossynd::game
{

// The authored agent sprite sheet (assets/sprites/agents.txt) embedded into the
// binary at build time, so the game still runs as a single self-contained
// executable on a machine with no data files (AGENTS.md 1b). The view prefers an
// external file next to the binary (for modding), then falls back to this copy.
extern const char* const kAgentSpritesText;

} // namespace fossynd::game
