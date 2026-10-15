/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "core/data/Catalogues.h"
#include "core/data/Json.h"
#include "core/data/MissionData.h"

// Dependency-free content validation (ADR-0003): resolve every mission's
// `weaponId`/`agentId` references against the shared catalogues and reject
// geometry that does not fit the map, so bad content is caught in CI instead of
// at play time. Engine-agnostic and header-only (like Catalogues.h), so adding
// it needs no new translation unit.
namespace fossynd::core::content
{

struct ContentProblem
{
    std::string file;   // the mission or catalogue file the problem was found in
    std::string message;// a readable description (what, where, which id)
};

struct ValidationReport
{
    std::vector<ContentProblem> problems;
    std::size_t missionsChecked = 0;
    std::size_t cataloguesChecked = 0;

    bool ok() const { return problems.empty(); }
};

namespace detail
{

inline bool hasJsonExtension(const std::filesystem::path& path)
{
    return path.extension() == ".json";
}

// The catalogue files are validated as catalogues, not as missions, so they are
// skipped when scanning the missions directory.
inline bool isCatalogueFile(const std::filesystem::path& path)
{
    const std::string name = path.filename().string();
    return name == kWeaponsFileName || name == kAgentsFileName;
}

// `directory_iterator` returns entries in an unspecified order. Sorting by path
// makes validation deterministic and the reported order stable across machines.
inline std::vector<std::filesystem::path> sortedJsonFiles(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec))
    {
        return files;
    }
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (entry.is_regular_file() && hasJsonExtension(entry.path()))
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// Reads a file whole. Returns false and sets `error` when it cannot be read.
inline bool readFile(const std::filesystem::path& path, std::string& out, std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "cannot open file: " + path.string();
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

} // namespace detail

// Validates one mission against the catalogues. Collects every problem (rather
// than stopping at the first) so a single run reports all of them. The mission
// is parsed with `MissionData::fromJson`, which resolves `weaponId`/`agentId`
// through the catalogues and already rejects unknown ids and inline+reference
// conflicts; this adds the file name to each error and the catalogue-duplicate
// check.
inline void validateMissionFile(const std::filesystem::path& path, const WeaponCatalogue& weapons,
                                const AgentCatalogue& agents, ValidationReport& report)
{
    ++report.missionsChecked;

    std::string text;
    std::string error;
    if (!detail::readFile(path, text, error))
    {
        report.problems.push_back({path.string(), error});
        return;
    }

    const Json root = Json::parse(text, error);
    if (!error.empty())
    {
        report.problems.push_back({path.string(), error});
        return;
    }

    MissionData data;
    if (!MissionData::fromJson(root, data, weapons, agents, error))
    {
        report.problems.push_back({path.string(), error});
    }
}

// Loads `weapons.json`/`agents.json` from `dataDir` (missing files are fine:
// the catalogues stay empty), then validates every `*.json` under
// `dataDir/missions`. `dataDir` is the directory holding the catalogues and the
// `missions/` subdirectory (`assets/data`).
inline ValidationReport validateContentDir(const std::string& dataDir)
{
    ValidationReport report;
    const std::filesystem::path dir(dataDir);

    WeaponCatalogue weapons;
    AgentCatalogue agents;
    bool found = false;
    std::string error;

    if (!weapons.loadFromFile((dir / kWeaponsFileName).string(), found, error))
    {
        report.problems.push_back({(dir / kWeaponsFileName).string(), error});
    }
    if (!agents.loadFromFile((dir / kAgentsFileName).string(), weapons, found, error))
    {
        report.problems.push_back({(dir / kAgentsFileName).string(), error});
    }
    if (weapons.empty() && agents.empty())
    {
        report.problems.push_back(
            {dir.string(), "no catalogue files found (expected " + std::string(kWeaponsFileName) +
                               " and/or " + std::string(kAgentsFileName) + ")"});
    }
    report.cataloguesChecked = weapons.size() + agents.size();

    for (const std::filesystem::path& mission : detail::sortedJsonFiles(dir / "missions"))
    {
        validateMissionFile(mission, weapons, agents, report);
    }

    return report;
}

// Renders a report as one line per problem, e.g.
//   content: assets/data/missions/demo.json: unknown weaponId 'nope' (...)
inline std::string formatReport(const ValidationReport& report)
{
    std::string out;
    for (const ContentProblem& problem : report.problems)
    {
        out += "content: " + problem.file + ": " + problem.message + "\n";
    }
    return out;
}

} // namespace fossynd::core::content
