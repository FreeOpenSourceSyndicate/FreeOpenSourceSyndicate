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

// The metagame world map's filename, under `dataDir/world/`. `campaign.json`
// also names it in its `world` field; this is the default the guard reads.
inline const char* const kWorldMapFileName = "world.json";

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
    return name == kWeaponsFileName || name == kAgentsFileName ||
           name == kVehiclesFileName;
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
                                const AgentCatalogue& agents, const VehicleCatalogue& vehicles,
                                ValidationReport& report)
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
    if (!MissionData::fromJson(root, data, weapons, agents, vehicles, error))
    {
        report.problems.push_back({path.string(), error});
    }
}

// Cross-checks the metagame world data (`dataDir/world/*.json`) against the
// shared weapon catalogue. `equipment.json` names weapons by id, and
// `research.json` `unlocks` names an equipment id; nothing else validated those
// references, so a weapon id the catalogue does not define (e.g. the pre-#369
// `longshot`/`railgun`) was bought and then made `applyLoadout` reject the
// *whole* squad loadout at deploy. Missing world files are skipped (a minimal
// content dir need not ship them); a present file is validated.
inline void validateWorldData(const std::filesystem::path& dir, const WeaponCatalogue& weapons,
                              ValidationReport& report)
{
    const std::filesystem::path worldDir = dir / "world";

    // Every equipment id, so a research `unlocks` can be resolved against it.
    std::vector<std::string> equipmentIds;

    std::string text;
    std::string error;
    const std::filesystem::path equipmentPath = worldDir / "equipment.json";
    if (detail::readFile(equipmentPath, text, error))
    {
        const Json root = Json::parse(text, error);
        if (!error.empty())
        {
            report.problems.push_back({equipmentPath.string(), error});
        }
        else
        {
            const Json& items = root["items"];
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                const Json& item = items[i];
                if (!item.isObject())
                {
                    continue; // shape errors are the economy loader's to report
                }
                const std::string id = item["id"].asString();
                if (id.empty())
                {
                    continue;
                }
                equipmentIds.push_back(id);
                const std::string kind = item["kind"].asString();
                if (kind == "weapon" && weapons.find(id) == nullptr)
                {
                    report.problems.push_back(
                        {equipmentPath.string(),
                         "equipment item '" + id + "' is a weapon but '" + id +
                             "' is not in the weapon catalogue (" + std::string(kWeaponsFileName) +
                             ")"});
                }
            }
        }
    }

    const std::filesystem::path researchPath = worldDir / "research.json";
    if (detail::readFile(researchPath, text, error))
    {
        const Json root = Json::parse(text, error);
        if (!error.empty())
        {
            report.problems.push_back({researchPath.string(), error});
        }
        else
        {
            const Json& nodes = root["nodes"];
            for (std::size_t i = 0; i < nodes.size(); ++i)
            {
                const Json& node = nodes[i];
                if (!node.isObject())
                {
                    continue;
                }
                const std::string unlocks = node["unlocks"].asString();
                if (unlocks.empty())
                {
                    continue;
                }
                // A research node unlocks an equipment id; it must name a real
                // catalogue item or the purchase it gates can never deploy.
                if (std::find(equipmentIds.begin(), equipmentIds.end(), unlocks) ==
                    equipmentIds.end())
                {
                    const std::string id = node["id"].asString();
                    report.problems.push_back(
                        {researchPath.string(),
                         "research node '" + id + "' unlocks '" + unlocks +
                             "', which is not an equipment id (" + std::string("equipment.json") +
                             ")"});
                }
            }
        }
    }
}

// Cross-checks the world map's region→mission links. `world.json` names a
// `missionId` per territory; nothing else validated that the id resolves to a
// shipped mission file, so a typo shipped a region the player could never attack
// (the silent-break class of the closed #369). A missing `world.json` is
// skipped (tolerant, like the other optional world files); a present file is
// checked, and a missing `missions/` directory means every non-empty id is
// dangling. Only the file's existence is required - the mission's own validity
// is `validateMissionFile`'s job.
inline void validateWorldMapMissions(const std::filesystem::path& worldDir,
                                     const std::filesystem::path& missionsDir,
                                     ValidationReport& report)
{
    const std::filesystem::path worldPath = worldDir / kWorldMapFileName;
    std::string text;
    std::string error;
    if (!detail::readFile(worldPath, text, error))
    {
        return; // absent (or unreadable) world file: nothing to cross-check
    }

    const Json root = Json::parse(text, error);
    if (!error.empty())
    {
        report.problems.push_back({worldPath.string(), error});
        return;
    }

    const Json& territories = root["territories"];
    for (std::size_t i = 0; i < territories.size(); ++i)
    {
        const Json& territory = territories[i];
        if (!territory.isObject())
        {
            continue; // shape errors are the map loader's to report
        }
        const std::string missionId = territory["missionId"].asString();
        if (missionId.empty())
        {
            continue; // a region with no mission is a valid state
        }
        std::error_code ec;
        if (std::filesystem::exists(missionsDir / (missionId + ".json"), ec))
        {
            continue;
        }
        const std::string name = territory["name"].asString();
        report.problems.push_back(
            {worldPath.string(),
             "territory '" + (name.empty() ? std::to_string(territory["id"].asInt()) : name) +
                 "' names missionId '" + missionId + "', but missions/" + missionId +
                 ".json does not exist"});
    }
}

// Loads `weapons.json`/`agents.json`/`vehicles.json` from `dataDir` (missing
// files are fine: the catalogues stay empty), cross-checks the metagame world
// data and the map's region→mission links, then validates every `*.json` under
// `dataDir/missions`. `dataDir` is the directory holding the catalogues and the
// `missions/` subdirectory (`assets/data`).
inline ValidationReport validateContentDir(const std::string& dataDir)
{
    ValidationReport report;
    const std::filesystem::path dir(dataDir);

    WeaponCatalogue weapons;
    AgentCatalogue agents;
    VehicleCatalogue vehicles;
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
    if (!vehicles.loadFromFile((dir / kVehiclesFileName).string(), weapons, found, error))
    {
        report.problems.push_back({(dir / kVehiclesFileName).string(), error});
    }
    if (weapons.empty() && agents.empty() && vehicles.empty())
    {
        report.problems.push_back(
            {dir.string(), "no catalogue files found (expected " + std::string(kWeaponsFileName) +
                               ", " + std::string(kAgentsFileName) + " and/or " +
                               std::string(kVehiclesFileName) + ")"});
    }
    report.cataloguesChecked = weapons.size() + agents.size() + vehicles.size();

    validateWorldData(dir, weapons, report);
    validateWorldMapMissions(dir / "world", dir / "missions", report);

    for (const std::filesystem::path& mission : detail::sortedJsonFiles(dir / "missions"))
    {
        validateMissionFile(mission, weapons, agents, vehicles, report);
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
