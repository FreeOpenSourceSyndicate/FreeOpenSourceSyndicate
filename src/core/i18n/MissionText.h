/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>

#include "core/i18n/StringTable.h"

// Mission-text localisation (issue #142, ADR-0020).
//
// Mission-authored text (the `name` / `description` in
// `assets/data/missions/*.json`) is content, not UI chrome, so it is keyed by a
// stable, language-independent mission id and resolved through the same
// `StringTable` the HUD uses. This header is pure and engine-free: it derives
// keys and resolves strings, and never touches the filesystem or the mission
// loader, so the caller decides where the id and the raw text come from.
namespace fossynd::core::i18n
{

// The string-table key for a mission's localised display name / description,
// derived from its id. Pure.
//
//   missionNameKey("demo") == "mission.demo.name"
//   missionDescKey("demo") == "mission.demo.desc"
std::string missionNameKey(const std::string& id);
std::string missionDescKey(const std::string& id);

// The mission id used for localisation, taken from a mission content path: the
// file stem without its directory or extension
// ("assets/data/missions/demo.json" -> "demo"). Returns "" for an empty path, so
// a caller can treat that as "no localisation". Pure.
std::string missionIdFromPath(const std::string& path);

// Resolves one authored mission text field through `table`. Returns the table's
// value for `key` when it is present and non-empty, otherwise `raw` unchanged —
// the mission's own text. So an untranslated mission shows its authored copy,
// never a blank field and never a raw key. Pure; holds no state.
std::string localizeMissionText(const StringTable& table, const std::string& key,
                                const std::string& raw);

} // namespace fossynd::core::i18n
