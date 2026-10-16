/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "core/data/Json.h"
#include "core/data/MissionSpecs.h"
#include "core/sim/Weapon.h"
#include "core/world/Vehicle.h"

namespace fossynd::core
{

// Shared weapon/agent catalogues, referenced from missions by id (ADR-0003).
// Header-only on purpose: the loader is small, and keeping it inline means adding
// the catalogues needs no new translation unit or build-file change, so the
// feature is self-contained and the binary stays dependency-light (AGENTS.md 1b).

// A weapon in the shared catalogue, referenced from missions by id.
struct WeaponEntry
{
    std::string id;
    Weapon weapon;
};

// An agent template in the shared catalogue, referenced from missions by id.
// The `x`/`y` in the spec are ignored for a template (a mission supplies the
// position); the remaining fields are the defaults an agent inherits.
struct AgentEntry
{
    std::string id;
    AgentSpec spec;
};

// A vehicle template in the shared catalogue, referenced from missions by id.
// Like `AgentEntry`, the spec's `x`/`y` are ignored for a template; a mission
// supplies the placement.
struct VehicleEntry
{
    std::string id;
    VehicleSpec spec;
};

namespace detail
{

// True if `error` reports that a file could not be opened (as opposed to a parse
// or schema error). Lets a missing optional catalogue be tolerated.
inline bool isMissingFileError(const std::string& error)
{
    return error.rfind("cannot open file:", 0) == 0;
}

// Ensures `id` is non-empty and not already present in `entries`.
template <typename Entry>
inline bool acceptId(const std::vector<Entry>& entries, const std::string& id, const char* what,
                     std::string& error)
{
    if (id.empty())
    {
        error = std::string("each ") + what + " catalogue entry needs a non-empty 'id'";
        return false;
    }
    for (const Entry& entry : entries)
    {
        if (entry.id == id)
        {
            error = std::string("duplicate ") + what + " id '" + id + "'";
            return false;
        }
    }
    return true;
}

template <typename Entry>
inline const Entry* findEntry(const std::vector<Entry>& entries, const std::string& id)
{
    for (const Entry& entry : entries)
    {
        if (entry.id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}

// Parses a team name ("player" | "enemy" | "civilian"). Returns false if unknown.
inline bool parseTeamName(const std::string& value, Team& out)
{
    if (value == "player")
    {
        out = Team::Player;
        return true;
    }
    if (value == "enemy")
    {
        out = Team::Enemy;
        return true;
    }
    if (value == "civilian")
    {
        out = Team::Civilian;
        return true;
    }
    return false;
}

// Parses a `weapon` object over `out` (needs an object with `fireRate` > 0).
// Omitted fields keep their current values. Returns false and sets `error`.
inline bool parseWeaponNode(const Json& node, Weapon& out, std::string& error)
{
    if (!node.isObject())
    {
        error = "'weapon' must be an object";
        return false;
    }
    if (node.has("name"))
    {
        out.name = node["name"].asString();
    }
    out.damage = node["damage"].asFloat(out.damage);
    out.range = node["range"].asFloat(out.range);
    out.fireRate = node["fireRate"].asFloat(out.fireRate);
    out.accuracy = node["accuracy"].asFloat(out.accuracy);
    // Optional: an explosive weapon sets a blast radius; omitted keeps the
    // inherited/default value (0 = single-target).
    out.areaRadius = node["areaRadius"].asFloat(out.areaRadius);
    // Field ranges: a bad catalogue number must fail the load, not misbehave at
    // play time (issue #190). Callers prefix the offending entry's id, so the
    // message names the file (validator), the entry, and the field.
    if (out.fireRate <= 0.0f)
    {
        error = "'fireRate' must be greater than 0";
        return false;
    }
    if (out.damage <= 0.0f)
    {
        error = "'damage' must be greater than 0";
        return false;
    }
    if (out.range <= 0.0f)
    {
        error = "'range' must be greater than 0";
        return false;
    }
    if (out.accuracy < 0.0f || out.accuracy > 1.0f)
    {
        error = "'accuracy' must be between 0 and 1";
        return false;
    }
    if (out.areaRadius < 0.0f)
    {
        error = "'areaRadius' must not be negative";
        return false;
    }
    return true;
}

// Applies `name`, `team`, `speed`, and `health` from `node` to `out` when
// present; omitted fields keep their current (default or inherited) values.
inline bool applyAgentCommon(const Json& node, AgentSpec& out, std::string& error)
{
    if (node.has("name"))
    {
        out.name = node["name"].asString();
    }
    if (node.has("team"))
    {
        if (!parseTeamName(node["team"].asString(), out.team))
        {
            error = "unknown team '" + node["team"].asString() +
                    "' (expected player, enemy, or civilian)";
            return false;
        }
    }
    out.speed = node["speed"].asFloat(out.speed);
    out.health = node["health"].asFloat(out.health);
    if (out.speed <= 0.0f || out.health <= 0.0f)
    {
        error = "agent 'speed' and 'health' must be positive";
        return false;
    }
    return true;
}

// Parses a vehicle kind name ("car" | "train"). Returns false if unknown.
inline bool parseVehicleKind(const std::string& value, VehicleKind& out)
{
    if (value == "car")
    {
        out = VehicleKind::Car;
        return true;
    }
    if (value == "train")
    {
        out = VehicleKind::Train;
        return true;
    }
    return false;
}

// Applies `name`, `kind`, `speed`, and `health` from `node` to `out` when
// present; omitted fields keep their current (default or inherited) values.
// The mounted weapon is applied separately (it is a `weapon` object or a
// `weaponId` reference), so this stays the vehicle's shared field set.
inline bool applyVehicleCommon(const Json& node, VehicleSpec& out, std::string& error)
{
    if (node.has("name"))
    {
        out.name = node["name"].asString();
    }
    if (node.has("kind"))
    {
        if (!parseVehicleKind(node["kind"].asString(), out.kind))
        {
            error = "unknown vehicle kind '" + node["kind"].asString() +
                    "' (expected car or train)";
            return false;
        }
    }
    out.speed = node["speed"].asFloat(out.speed);
    out.health = node["health"].asFloat(out.health);
    if (out.speed <= 0.0f || out.health <= 0.0f)
    {
        error = "vehicle 'speed' and 'health' must be positive";
        return false;
    }
    return true;
}

} // namespace detail

// The shared weapon catalogue (`assets/data/weapons.json`). Keyed by id, so two
// agents can add weapons without touching the same file or any C++ (ADR-0003).
class WeaponCatalogue
{
public:
    // Returns the weapon for `id`, or nullptr if there is no such entry.
    const Weapon* find(const std::string& id) const
    {
        const WeaponEntry* entry = detail::findEntry(entries_, id);
        return entry != nullptr ? &entry->weapon : nullptr;
    }

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }
    const std::vector<WeaponEntry>& entries() const { return entries_; }

    // Parses `{ "weapons": [ { "id": "rifle", "name": "Rifle", ... } ] }`.
    // Returns false and sets `error` on a schema error (a missing id, a duplicate
    // id, or an invalid weapon). A missing `weapons` key yields an empty catalogue.
    bool loadFromJson(const Json& root, std::string& error)
    {
        entries_.clear();
        if (!root.isObject())
        {
            error = "weapon catalogue root must be a JSON object";
            return false;
        }
        if (!root.has("weapons"))
        {
            return true;
        }
        const Json& array = root["weapons"];
        if (!array.isArray())
        {
            error = "'weapons' must be an array";
            return false;
        }
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            const Json& node = array[i];
            if (!node.isObject())
            {
                error = "each weapon must be an object";
                return false;
            }
            WeaponEntry entry;
            entry.id = node.has("id") ? node["id"].asString() : std::string();
            if (!detail::acceptId(entries_, entry.id, "weapon", error))
            {
                return false;
            }
            entry.weapon = makePistol();
            if (!detail::parseWeaponNode(node, entry.weapon, error))
            {
                error = "weapon '" + entry.id + "': " + error;
                return false;
            }
            entries_.push_back(std::move(entry));
        }
        return true;
    }

    // Reads and parses a catalogue file. A missing file is not an error: `found`
    // is false and the catalogue stays empty (so the game still starts with no
    // data files, AGENTS.md 1b). Returns false only on a parse or schema error.
    bool loadFromFile(const std::string& path, bool& found, std::string& error)
    {
        found = false;
        error.clear();
        Json root;
        if (!Json::parseFile(path, root, error))
        {
            if (detail::isMissingFileError(error))
            {
                error.clear();
                return true;
            }
            return false;
        }
        found = true;
        return loadFromJson(root, error);
    }

private:
    std::vector<WeaponEntry> entries_;
};

// The shared agent catalogue (`assets/data/agents.json`). Keyed by agent id. An
// agent template may itself reference a weapon by `weaponId`, so loading one
// needs the weapon catalogue.
class AgentCatalogue
{
public:
    const AgentEntry* find(const std::string& id) const
    {
        return detail::findEntry(entries_, id);
    }

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }
    const std::vector<AgentEntry>& entries() const { return entries_; }

    // Applies an agent's weapon from `node`: an inline `weapon` object, or a
    // `weaponId` reference into `weapons`. Both at once is an error; neither
    // leaves `weapon` unchanged. Used here and by the mission loader.
    static bool applyWeaponToAgent(const Json& node, const WeaponCatalogue& weapons, Weapon& weapon,
                                   std::string& error)
    {
        const bool hasInline = node.has("weapon");
        const bool hasRef = node.has("weaponId");
        if (hasInline && hasRef)
        {
            error = "an agent cannot set both 'weapon' and 'weaponId'";
            return false;
        }
        if (hasRef)
        {
            const std::string id = node["weaponId"].asString();
            const Weapon* found = weapons.find(id);
            if (found == nullptr)
            {
                error = "unknown weaponId '" + id + "' (no such weapon in the catalogue)";
                return false;
            }
            weapon = *found;
            return true;
        }
        if (hasInline)
        {
            return detail::parseWeaponNode(node["weapon"], weapon, error);
        }
        return true;
    }

    bool loadFromJson(const Json& root, const WeaponCatalogue& weapons, std::string& error)
    {
        entries_.clear();
        if (!root.isObject())
        {
            error = "agent catalogue root must be a JSON object";
            return false;
        }
        if (!root.has("agents"))
        {
            return true;
        }
        const Json& array = root["agents"];
        if (!array.isArray())
        {
            error = "'agents' must be an array";
            return false;
        }
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            const Json& node = array[i];
            if (!node.isObject())
            {
                error = "each agent template must be an object";
                return false;
            }
            AgentEntry entry;
            entry.id = node.has("id") ? node["id"].asString() : std::string();
            if (!detail::acceptId(entries_, entry.id, "agent", error))
            {
                return false;
            }
            entry.spec.id = entry.id;
            if (!detail::applyAgentCommon(node, entry.spec, error))
            {
                return false;
            }
            if (!applyWeaponToAgent(node, weapons, entry.spec.weapon, error))
            {
                error = "agent '" + entry.id + "': " + error;
                return false;
            }
            entries_.push_back(std::move(entry));
        }
        return true;
    }

    bool loadFromFile(const std::string& path, const WeaponCatalogue& weapons, bool& found,
                      std::string& error)
    {
        found = false;
        error.clear();
        Json root;
        if (!Json::parseFile(path, root, error))
        {
            if (detail::isMissingFileError(error))
            {
                error.clear();
                return true;
            }
            return false;
        }
        found = true;
        return loadFromJson(root, weapons, error);
    }

private:
    std::vector<AgentEntry> entries_;
};

// The shared vehicle catalogue (`assets/data/vehicles.json`). Keyed by vehicle
// id, so a designer can add a car/train kind (speed, health, mounted weapon)
// without touching C++ (golden rule 7, ADR-0003). A vehicle template may itself
// reference a weapon by `weaponId`, so loading one needs the weapon catalogue.
class VehicleCatalogue
{
public:
    const VehicleEntry* find(const std::string& id) const
    {
        return detail::findEntry(entries_, id);
    }

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }
    const std::vector<VehicleEntry>& entries() const { return entries_; }

    bool loadFromJson(const Json& root, const WeaponCatalogue& weapons, std::string& error)
    {
        entries_.clear();
        if (!root.isObject())
        {
            error = "vehicle catalogue root must be a JSON object";
            return false;
        }
        if (!root.has("vehicles"))
        {
            return true;
        }
        const Json& array = root["vehicles"];
        if (!array.isArray())
        {
            error = "'vehicles' must be an array";
            return false;
        }
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            const Json& node = array[i];
            if (!node.isObject())
            {
                error = "each vehicle template must be an object";
                return false;
            }
            VehicleEntry entry;
            entry.id = node.has("id") ? node["id"].asString() : std::string();
            if (!detail::acceptId(entries_, entry.id, "vehicle", error))
            {
                return false;
            }
            entry.spec.id = entry.id;
            if (!detail::applyVehicleCommon(node, entry.spec, error))
            {
                error = "vehicle '" + entry.id + "': " + error;
                return false;
            }
            // A mounted weapon is a `weapon` object or a `weaponId` reference;
            // reuse the agent rule so both catalogues behave identically.
            if (!AgentCatalogue::applyWeaponToAgent(node, weapons, entry.spec.weapon, error))
            {
                error = "vehicle '" + entry.id + "': " + error;
                return false;
            }
            entries_.push_back(std::move(entry));
        }
        return true;
    }

    bool loadFromFile(const std::string& path, const WeaponCatalogue& weapons, bool& found,
                      std::string& error)
    {
        found = false;
        error.clear();
        Json root;
        if (!Json::parseFile(path, root, error))
        {
            if (detail::isMissingFileError(error))
            {
                error.clear();
                return true;
            }
            return false;
        }
        found = true;
        return loadFromJson(root, weapons, error);
    }

private:
    std::vector<VehicleEntry> entries_;
};

// Default file names, resolved against a content directory by the engine layer.
inline const char* const kWeaponsFileName = "weapons.json";
inline const char* const kAgentsFileName = "agents.json";
inline const char* const kVehiclesFileName = "vehicles.json";

// Resolves a catalogue file name for a mission file. Missions live one level
// below the content root (`assets/data/missions/<name>.json`), so the catalogue
// is the mission directory's parent plus `fileName`
// (`assets/data/weapons.json`). Returns an empty string when `missionPath` has
// no directory component. Engine-free: pure string handling.
inline std::string cataloguePathForMission(const std::string& missionPath, const char* fileName)
{
    const std::size_t slash = missionPath.find_last_of("/\\");
    if (slash == std::string::npos || slash == 0)
    {
        return {};
    }
    const std::string dir = missionPath.substr(0, slash + 1);
    const std::size_t parent = dir.find_last_of("/\\", slash - 1);
    if (parent == std::string::npos)
    {
        return {};
    }
    return dir.substr(0, parent + 1) + fileName;
}

} // namespace fossynd::core
