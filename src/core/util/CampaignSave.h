/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>
#include <vector>

#include "core/economy/Campaign.h"

namespace fossynd::core
{

// A versioned, plain-text snapshot of a Campaign's *progress* between sessions
// (Milestone C, "save/load"). Engine-free and SDL-free (ADR-0002, golden rule 1).
//
// A save carries only progress — what the player changed — never topology: the
// territory graph, the research tree, and the equipment catalogue all come from
// the base campaign file the game already loads with `--campaign`. So a save is
// an *overlay* on a freshly loaded base campaign, which keeps it small and keeps
// authored content in one place (the campaign JSON).
//
// The format is the project's schema style (JSON, ADR-0003) with a mandatory
// `format` tag and an integer `version`, so an older or foreign file is rejected
// with a readable error instead of loading half a game. The schema is recorded
// in ADR-0039 and in the tests; a `docs/DATA_FORMAT.md` section is a pending
// content-area request.
//
// Round-trip: `parse(serialize(capture(c))) == capture(c)` exactly, for every
// field below. `applyTo` restores the progress a freshly loaded base campaign
// can accept through the frozen metagame API.
struct CampaignSave
{
    // Written as the top-level `format` tag; a file that does not carry it is not
    // a save. Bump `kVersion` when the schema changes incompatibly. v2 added the
    // player's chosen syndicate identity (`syndicate` became meaningful and a
    // `syndicateLogo` was added): a v1 build would parse a v2 file and silently
    // drop the chosen logo, so the version bump makes it reject the file instead.
    static constexpr const char* kFormat = "fossynd-save";
    static constexpr int kVersion = 2;

    // One territory's mutable progress. Names, neighbours, and mission ids are
    // topology and live in the base campaign file, not the save.
    struct TerritoryProgress
    {
        TerritoryId id = -1;
        int owner = -1;   // syndicate index, or -1 = neutral
        int taxLevel = 0;

        bool operator==(const TerritoryProgress& other) const
        {
            return id == other.id && owner == other.owner && taxLevel == other.taxLevel;
        }
    };

    struct FundsProgress
    {
        int syndicate = 0;
        float amount = 0.0f;

        bool operator==(const FundsProgress& other) const
        {
            return syndicate == other.syndicate && amount == other.amount;
        }
    };

    struct ResearchProgress
    {
        int syndicate = 0;
        std::vector<std::string> completed;
        std::string inProgress;       // node id, empty if idle
        float remainingSeconds = 0.0f;

        bool operator==(const ResearchProgress& other) const
        {
            return syndicate == other.syndicate && completed == other.completed &&
                   inProgress == other.inProgress && remainingSeconds == other.remainingSeconds;
        }
    };

    struct LoadoutProgress
    {
        int syndicate = 0;
        int slot = 0;
        std::vector<std::string> items; // item ids in assignment order

        bool operator==(const LoadoutProgress& other) const
        {
            return syndicate == other.syndicate && slot == other.slot && items == other.items;
        }
    };

    std::string syndicate;                  // Campaign::syndicateName(); the player's display name
    std::string syndicateLogo;              // Campaign::syndicateLogo(); a stable logo id
    std::vector<TerritoryProgress> territories; // ascending id
    std::vector<FundsProgress> funds;           // ascending syndicate
    std::vector<ResearchProgress> research;     // ascending syndicate; entries with progress only
    std::vector<LoadoutProgress> loadouts;      // ascending (syndicate, slot); non-empty only

    bool operator==(const CampaignSave& other) const
    {
        return syndicate == other.syndicate && syndicateLogo == other.syndicateLogo &&
               territories == other.territories && funds == other.funds &&
               research == other.research && loadouts == other.loadouts;
    }
    bool operator!=(const CampaignSave& other) const { return !(*this == other); }

    // Reads the progress above out of `campaign` (a pure read of the frozen
    // metagame API). Deterministic: the same campaign always yields the same
    // snapshot.
    static CampaignSave capture(const Campaign& campaign);

    // Writes the versioned text. Deterministic: the same snapshot always yields
    // the same bytes, so a save file is diffable.
    bool serialize(std::string& out, std::string& error) const;

    // Parses text produced by serialize(). Rejects a non-save document, a missing
    // or wrong `format`, a missing or unsupported `version`, and any malformed
    // field, each with a readable `error`.
    static bool parse(const std::string& text, CampaignSave& out, std::string& error);

    bool saveToFile(const std::string& path, std::string& error) const;
    static bool loadFromFile(const std::string& path, CampaignSave& out, std::string& error);

    // Restores the saved progress onto `campaign`, which must be a freshly loaded
    // base campaign (the same `--campaign` file the save was taken from, so its
    // topology matches). The player's chosen name and logo are applied, ownership
    // and tax are set for every saved territory, the funds ledger is reset to
    // the saved amounts, and the squad loadouts are replaced. Returns false and
    // sets `error` if the save references a territory the base campaign does not
    // have, or carries an out-of-range field (validated before anything mutates,
    // so a rejected save leaves the campaign untouched).
    //
    // Research is *captured and serialised* but not reapplied: the frozen
    // `Research` API exposes no restore setter, so a save's completed list and
    // in-progress node cannot be written back without a metagame accessor
    // (Rule 4 request filed to `focus: metagame`). Until that lands, a loaded
    // campaign starts its research from the base file's state.
    bool applyTo(Campaign& campaign, std::string& error) const;
};

} // namespace fossynd::core
