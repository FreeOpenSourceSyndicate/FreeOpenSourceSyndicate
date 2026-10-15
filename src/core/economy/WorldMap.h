/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/data/Json.h"

namespace fossynd::core
{

// A stable identifier for a territory within one world map. Dense: a valid map
// has ids 0..size-1 (see WorldMap::validate).
using TerritoryId = int;

// One region of the world map. Ownership and taxation are world state, so they
// live here rather than in the systems that read them, which keeps a future
// save/load in one place (docs/METAGAME.md).
struct Territory
{
    TerritoryId id = -1;                  // >= 0, unique, dense
    std::string name;                     // display name, data-authored
    std::vector<TerritoryId> neighbours;  // undirected; both sides must list it
    int owner = -1;                       // syndicate index, or -1 = neutral
    std::string missionId;                // mission deployed from here; may be empty
    int taxLevel = 0;                     // 0..EconomyConfig::maxTaxLevel
};

// The metagame world map: a graph of territories and who owns them (Milestone B,
// docs/METAGAME.md). Engine-free and data-driven (ADR-0002, ADR-0003); the map
// screen in src/game/** only reads it.
class WorldMap
{
public:
    // Parses a world document (`assets/data/world/world.json`). Returns false and
    // sets `error` on a schema error (a bad id, a duplicate id, an unknown
    // neighbour, a self-loop, or an adjacency that is not symmetric).
    static bool fromJson(const Json& root, WorldMap& out, std::string& error);

    // Reads and parses a world file. Returns false and sets `error` on failure.
    static bool loadFromFile(const std::string& path, WorldMap& out, std::string& error);

    std::size_t size() const { return territories_.size(); }
    const std::vector<Territory>& territories() const { return territories_; }

    // Returns the territory with `id`, or nullptr if there is no such territory.
    const Territory* find(TerritoryId id) const;

    // Neighbours of `id` (empty if unknown), and whether two territories are
    // adjacent.
    const std::vector<TerritoryId>& neighbours(TerritoryId id) const;
    bool areNeighbours(TerritoryId a, TerritoryId b) const;

    // Ownership. `owner` is a syndicate index, or -1 for neutral. setOwner is a
    // no-op for an unknown id.
    int owner(TerritoryId id) const;
    void setOwner(TerritoryId id, int syndicate);

    // Every territory owned by `syndicate`, in id order.
    std::vector<TerritoryId> territoriesOwnedBy(int syndicate) const;

    // Full structural check: dense ids (0..size-1), symmetric adjacency, no
    // self-loops, non-empty unique names, and every non-empty missionId unique.
    // Returns false and sets `error` on the first problem found.
    bool validate(std::string& error) const;

private:
    std::vector<Territory> territories_;
};

} // namespace fossynd::core
