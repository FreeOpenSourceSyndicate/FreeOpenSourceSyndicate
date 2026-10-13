/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/data/Config.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace fossynd::core
{
namespace
{

std::string trim(const std::string& s)
{
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
    {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
    {
        --end;
    }
    return s.substr(begin, end - begin);
}

} // namespace

bool parseConfig(const std::string& text, GameConfig& out, std::string& error)
{
    std::istringstream stream(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(stream, line))
    {
        ++lineNumber;
        // Strip a trailing carriage return (Windows line endings).
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
        {
            continue;
        }
        const std::size_t eq = trimmed.find('=');
        if (eq == std::string::npos)
        {
            std::ostringstream oss;
            oss << "line " << lineNumber << ": expected 'key = value'";
            error = oss.str();
            return false;
        }
        const std::string key = trim(trimmed.substr(0, eq));
        const std::string value = trim(trimmed.substr(eq + 1));
        if (key == "mission")
        {
            out.mission = value;
        }
        else if (key == "mission_dir" || key == "missionDir")
        {
            out.missionDir = value;
        }
        // Unknown keys are ignored (forward compatible).
    }
    return true;
}

bool loadConfigFile(const std::string& path, GameConfig& out, bool& found, std::string& error)
{
    error.clear();
    found = false;
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return true; // Missing config file is not an error.
    }
    found = true;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseConfig(buffer.str(), out, error);
}

} // namespace fossynd::core
