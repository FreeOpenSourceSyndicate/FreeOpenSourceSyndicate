/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/i18n/MissionText.h"

namespace fossynd::core::i18n
{

namespace
{

// True if `id` is usable as a key segment. A content id (mission or weapon)
// becomes a middle segment of a dotted key, so an empty id or one containing a
// dot, whitespace, or an upper-case letter is rejected: localisation is skipped
// rather than building an ambiguous key (e.g. an id of "a.b" would collide with
// a nested key). A hyphen is allowed, so a catalogue id such as "time-bomb" is
// key-safe.
bool isKeySafeId(const std::string& id)
{
    if (id.empty())
    {
        return false;
    }
    for (const char c : id)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok)
        {
            return false;
        }
    }
    return true;
}

} // namespace

std::string missionNameKey(const std::string& id)
{
    return isKeySafeId(id) ? "mission." + id + ".name" : std::string();
}

std::string missionDescKey(const std::string& id)
{
    return isKeySafeId(id) ? "mission." + id + ".desc" : std::string();
}

std::string weaponNameKey(const std::string& id)
{
    return isKeySafeId(id) ? "weapon." + id + ".name" : std::string();
}

std::string missionIdFromPath(const std::string& path)
{
    const std::size_t slash = path.find_last_of("/\\");
    std::string stem = slash == std::string::npos ? path : path.substr(slash + 1);

    // Strip a trailing extension, if any ("demo.json" -> "demo").
    const std::size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos)
    {
        stem = stem.substr(0, dot);
    }

    return isKeySafeId(stem) ? stem : std::string();
}

std::string localizeMissionText(const StringTable& table, const std::string& key,
                                const std::string& raw)
{
    if (key.empty() || !table.has(key))
    {
        return raw;
    }
    const std::string value = table.get(key);
    // An empty value is treated as "not translated" so a half-filled locale
    // cannot blank a mission's copy.
    return value.empty() ? raw : value;
}

} // namespace fossynd::core::i18n
