/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/data/Json.h"
#include "core/economy/WorldMap.h"

namespace fossynd::core
{

// The per-region agent-name pools (issue #509): when the player recruits an
// agent in a region, it takes a fixed name drawn from that region's pool, so a
// squad member's identity is tied to the map rather than being a placeholder.
// Data-driven (ADR-0003) and engine-free (ADR-0002).
//
// The values live in `assets/data/world/names.json`; the shape and validation
// belong to `focus: content` (#510). A region with no pool is not an error: the
// caller assigns no name (a safe fallback), so the feature degrades gracefully
// on a partial data set (AGENTS.md §1b).
class NamePools
{
public:
    // Parses a name-pool document: an object with a `"pools"` array, each entry
    // an object `{ "region": <int territory id>, "names": [<non-empty ASCII>] }`.
    // Returns false and sets `error` on a schema error (a missing/empty names
    // array, a negative region, or a duplicate region entry). Names are required
    // to be non-empty; ASCII enforcement is the content validator's (#510), so a
    // non-ASCII glyph is not fatal here (it simply renders blank, as elsewhere).
    static bool fromJson(const Json& root, NamePools& out, std::string& error);

    // Reads and parses a name-pool file. Returns false and sets `error` on
    // failure.
    static bool loadFromFile(const std::string& path, NamePools& out, std::string& error);

    // The pool for `region`, or nullptr when the region has no pool (the caller
    // then assigns no name). Never an empty vector: a pool is non-empty by parse.
    const std::vector<std::string>* pool(TerritoryId region) const;

    // Every region that has a pool, in the order they were parsed.
    const std::vector<TerritoryId>& regions() const { return regions_; }

private:
    struct Entry
    {
        TerritoryId region = -1;
        std::vector<std::string> names;
    };

    std::vector<Entry> entries_;
    std::vector<TerritoryId> regions_; // parallel to entries_, for iteration
};

} // namespace fossynd::core
