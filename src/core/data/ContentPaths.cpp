/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/data/ContentPaths.h"

#include <fstream>
#include <sstream>

namespace foss::core
{

const char* const kDefaultMissionFile = "demo.json";

bool fileExists(const std::string& path)
{
    if (path.empty())
    {
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    return static_cast<bool>(file);
}

std::vector<std::string> defaultMissionPaths(const std::string& exeDir)
{
    std::vector<std::string> paths;
    const std::string missionDir = "assets/data/missions/";
    const std::string file = kDefaultMissionFile;

    // Fresh build / run from the repo root.
    paths.push_back(missionDir + file);
    // Alongside the executable (e.g. <prefix>/bin/../share/...).
    if (!exeDir.empty())
    {
        paths.push_back(exeDir + "/assets/data/missions/" + file);
        paths.push_back(exeDir + "/../share/fossynd/missions/" + file);
        paths.push_back(exeDir + "/data/missions/" + file);
    }
    // Common install prefixes.
    paths.push_back("/usr/local/share/fossynd/missions/" + file);
    paths.push_back("/usr/share/fossynd/missions/" + file);
    return paths;
}

std::vector<std::string> defaultConfigPaths(const std::string& exeDir)
{
    std::vector<std::string> paths;
    paths.push_back("fossynd.conf"); // current directory (handy for a build tree)
    if (!exeDir.empty())
    {
        paths.push_back(exeDir + "/fossynd.conf");
    }
    paths.push_back("/etc/fossynd.conf");
    paths.push_back("/usr/local/etc/fossynd.conf");
    return paths;
}

namespace
{

std::string join(const std::string& dir, const std::string& name)
{
    if (dir.empty())
    {
        return name;
    }
    const char last = dir.back();
    if (last == '/' || last == '\\')
    {
        return dir + name;
    }
    return dir + "/" + name;
}

} // namespace

bool resolveMissionPath(const GameConfig& config, const std::string& cliMission,
                        const std::string& exeDir, std::string& out, MissionSource& source,
                        std::string& error)
{
    out.clear();
    source = MissionSource::None;
    error.clear();

    // 1. Command line (highest priority).
    if (!cliMission.empty())
    {
        if (!fileExists(cliMission))
        {
            error = "--mission file not found: " + cliMission;
            return false;
        }
        out = cliMission;
        source = MissionSource::CliFlag;
        return true;
    }

    // 2. Explicit path in the config file.
    if (!config.mission.empty())
    {
        if (!fileExists(config.mission))
        {
            error = "mission file from config not found: " + config.mission;
            return false;
        }
        out = config.mission;
        source = MissionSource::ConfigFile;
        return true;
    }

    // 3. A mission directory in the config file.
    if (!config.missionDir.empty())
    {
        const std::string candidate = join(config.missionDir, kDefaultMissionFile);
        if (!fileExists(candidate))
        {
            error = "mission_dir from config has no " + std::string(kDefaultMissionFile) + ": " +
                    config.missionDir;
            return false;
        }
        out = candidate;
        source = MissionSource::ConfigDir;
        return true;
    }

    // 4. Built-in fallbacks: first that exists wins.
    for (const std::string& candidate : defaultMissionPaths(exeDir))
    {
        if (fileExists(candidate))
        {
            out = candidate;
            source = MissionSource::Fallback;
            return true;
        }
    }

    // Nothing found: the caller uses its embedded copy.
    return true;
}

} // namespace foss::core
