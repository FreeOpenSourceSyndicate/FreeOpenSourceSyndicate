/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>
#include <vector>

#include "core/data/Config.h"

namespace foss::core
{

// The default mission file name, used when a directory (not a full path) is
// given and when scanning the built-in fallback locations.
extern const char* const kDefaultMissionFile;

// True if `path` names a readable file.
bool fileExists(const std::string& path);

// Where the mission file came from, so callers can report it.
enum class MissionSource
{
    None,      // Nothing found; the caller should use its embedded fallback.
    CliFlag,   // Explicit path from the command line.
    ConfigFile,// Explicit path from the config file.
    ConfigDir, // A mission_dir in the config file.
    Fallback,  // A built-in search location that happened to exist.
};

// Resolves which mission file to load, in priority order (highest first):
//   1. `cliMission`        — an explicit --mission path (must exist).
//   2. `config.mission`    — an explicit mission path from the config file.
//   3. `config.missionDir` — <dir>/<kDefaultMissionFile>.
//   4. Built-in fallback locations, checked for existence in order; the first
//      that exists wins. These cover a fresh build tree and common install
//      prefixes (see defaultMissionPaths).
// If none applies, `source` is MissionSource::None and `out` is empty; the
// caller then uses its embedded copy (ADR-0003). An explicit path (1-3) that
// does not exist is an error: the user asked for it specifically.
bool resolveMissionPath(const GameConfig& config, const std::string& cliMission,
                        const std::string& exeDir, std::string& out, MissionSource& source,
                        std::string& error);

// The built-in fallback locations, in priority order. `exeDir` may be empty.
// Relative entries are resolved against the process working directory.
std::vector<std::string> defaultMissionPaths(const std::string& exeDir);

// The default config-file locations, in priority order, checked for existence.
// `exeDir` may be empty.
std::vector<std::string> defaultConfigPaths(const std::string& exeDir);

} // namespace foss::core
