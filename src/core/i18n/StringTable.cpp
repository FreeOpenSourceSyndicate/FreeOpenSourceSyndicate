/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/i18n/StringTable.h"

namespace fossynd::core::i18n
{

bool StringTable::loadFromJson(const Json& root, std::string& error)
{
    error.clear();
    if (!root.isObject())
    {
        error = "string table: expected a JSON object of \"key\": \"value\" pairs";
        return false;
    }

    // Build into a temporary so a malformed table leaves the current one intact.
    std::unordered_map<std::string, std::string> loaded;
    for (const auto& entry : root.asObject())
    {
        if (!entry.second.isString())
        {
            error = "string table: value for key '" + entry.first + "' is not a string";
            return false;
        }
        loaded[entry.first] = entry.second.asString();
    }

    entries_ = std::move(loaded);
    return true;
}

bool StringTable::loadFromFile(const std::string& path, std::string& error)
{
    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }
    return loadFromJson(root, error);
}

std::string StringTable::get(const std::string& key, const char* fallback) const
{
    const auto it = entries_.find(key);
    if (it != entries_.end())
    {
        return it->second;
    }
    return fallback != nullptr ? std::string(fallback) : key;
}

bool StringTable::has(const std::string& key) const
{
    return entries_.find(key) != entries_.end();
}

std::vector<std::string> defaultLangPaths(const std::string& exeDir, const std::string& lang)
{
    std::vector<std::string> paths;
    const std::string file = lang + ".json";

    // Fresh build / run from the repo root.
    paths.push_back("assets/lang/" + file);
    // Alongside the executable and common install prefixes.
    if (!exeDir.empty())
    {
        paths.push_back(exeDir + "/assets/lang/" + file);
        paths.push_back(exeDir + "/../share/fossynd/lang/" + file);
        paths.push_back(exeDir + "/lang/" + file);
    }
    paths.push_back("/usr/local/share/fossynd/lang/" + file);
    paths.push_back("/usr/share/fossynd/lang/" + file);
    return paths;
}

} // namespace fossynd::core::i18n
