/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

namespace fossynd::game
{

// The English string table (assets/lang/en.json) embedded into the binary at
// build time, so the game still runs as a single self-contained executable on a
// machine with no data files (AGENTS.md 1b). The view prefers an external file
// (for translation/modding), then falls back to this copy (ADR-0016).
extern const char* const kEnglishLangJson;

} // namespace fossynd::game
