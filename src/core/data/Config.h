/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>

namespace foss::core
{

// The small set of user-tunable options read from a config file. Kept in core
// so the parser is unit-tested headlessly and stays SDL-free (ADR-0002).
//
// Format (INI-like, one `key = value` per line):
//   # comment
//   mission      = assets/data/missions/demo.json
//   mission_dir  = /usr/share/fossynd/missions
struct GameConfig
{
    // Explicit path to the mission file to load.
    std::string mission;

    // A directory to search for `<mission_name>.json` when no explicit path is
    // given.
    std::string missionDir;
};

// Parses config text. Returns false and sets `error` on a malformed line.
// Unknown keys are ignored so the format can grow.
bool parseConfig(const std::string& text, GameConfig& out, std::string& error);

// Reads a config file. On success `found` reports whether the file existed (a
// missing config file is not an error). Returns false only on a parse error.
bool loadConfigFile(const std::string& path, GameConfig& out, bool& found, std::string& error);

} // namespace foss::core
