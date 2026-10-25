/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/util/CampaignSave.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace fossynd::core
{
namespace
{

// Serialises a float so it round-trips exactly through strtod: 9 significant
// decimal digits is FLT_DECIMAL_DIG, the count that always reproduces a 32-bit
// float. `%g` keeps the output compact and may use exponent notation, which the
// JSON reader accepts.
std::string formatFloat(float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
    return buffer;
}

// Appends `text` to `out` as a JSON string literal, escaping the characters the
// reader treats specially. Our ids and names are ASCII, but a hand-edited save
// could carry anything, so escaping is not optional.
void writeString(std::string& out, const std::string& text)
{
    out.push_back('"');
    for (const char c : text)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    out.push_back('"');
}

// The highest syndicate index a save needs to describe: the owner of any
// territory, floored at 0 so the player (syndicate 0) is always represented.
// Ownership is the only way a syndicate enters the funds ledger or holds a
// loadout, so this range is complete for every reachable state.
int syndicateCeiling(const Campaign& campaign)
{
    int ceiling = 0;
    for (const Territory& territory : campaign.map().territories())
    {
        ceiling = std::max(ceiling, territory.owner);
    }
    return ceiling;
}

// Reads an array of non-empty strings. On failure sets `error` and returns an
// empty vector (the caller checks `error`).
std::vector<std::string> readStringArray(const Json& node, const std::string& where,
                                         std::string& error)
{
    std::vector<std::string> values;
    if (!node.isArray())
    {
        error = where + " must be an array";
        return values;
    }
    for (std::size_t i = 0; i < node.size(); ++i)
    {
        if (!node[i].isString())
        {
            error = where + ": each entry must be a string";
            return values;
        }
        const std::string value = node[i].asString();
        if (value.empty())
        {
            error = where + ": entries must be non-empty";
            return values;
        }
        values.push_back(value);
    }
    return values;
}

// Reads an array of strings that may be empty (the roster's unnamed slots). On
// failure sets `error` and returns an empty vector (the caller checks `error`).
std::vector<std::string> readStringArrayAllowingEmpty(const Json& node, const std::string& where,
                                                      std::string& error)
{
    std::vector<std::string> values;
    if (!node.isArray())
    {
        error = where + " must be an array";
        return values;
    }
    for (std::size_t i = 0; i < node.size(); ++i)
    {
        if (!node[i].isString())
        {
            error = where + ": each entry must be a string";
            return values;
        }
        values.push_back(node[i].asString());
    }
    return values;
}

// Reads a required non-negative integer field.
bool readInt(const Json& node, const char* key, const std::string& where, int& out,
             std::string& error)
{
    if (!node.has(key) || !node[key].isNumber())
    {
        error = where + ": missing numeric '" + key + "'";
        return false;
    }
    out = node[key].asInt();
    if (out < 0)
    {
        error = where + ": '" + key + "' must be >= 0";
        return false;
    }
    return true;
}

} // namespace

CampaignSave CampaignSave::capture(const Campaign& campaign)
{
    CampaignSave save;
    save.syndicate = campaign.syndicateName();
    save.syndicateLogo = campaign.syndicateLogo();

    for (const Territory& territory : campaign.map().territories())
    {
        save.territories.push_back(
            TerritoryProgress{territory.id, territory.owner, territory.taxLevel});
    }
    std::sort(save.territories.begin(), save.territories.end(),
              [](const TerritoryProgress& a, const TerritoryProgress& b) { return a.id < b.id; });

    const int ceiling = syndicateCeiling(campaign);
    for (int syndicate = 0; syndicate <= ceiling; ++syndicate)
    {
        save.funds.push_back(FundsProgress{syndicate, campaign.economy().funds(syndicate)});

        const ResearchState& state = campaign.research().state(syndicate);
        if (!state.completed.empty() || !state.inProgress.empty() || state.remainingSeconds != 0.0f)
        {
            ResearchProgress progress;
            progress.syndicate = syndicate;
            progress.completed = state.completed;
            progress.inProgress = state.inProgress;
            progress.remainingSeconds = state.remainingSeconds;
            save.research.push_back(std::move(progress));
        }

        for (int slot = 0; slot < Equipment::kSquadSlots; ++slot)
        {
            std::vector<std::string> items = campaign.equipment().loadout(syndicate, slot);
            if (!items.empty())
            {
                save.loadouts.push_back(LoadoutProgress{syndicate, slot, std::move(items)});
            }
        }
    }

    // The recruited roster (per-slot agent names, #509/#532). Copied as-is and
    // padded/truncated to the squad size so a round-trip is exact regardless of
    // whether the campaign has recruited anyone yet.
    save.roster = campaign.agentNames();
    save.roster.resize(static_cast<std::size_t>(Equipment::kSquadSlots));

    return save;
}

bool CampaignSave::serialize(std::string& out, std::string& error) const
{
    error.clear();
    out.clear();

    std::string text;
    text += "{\n";
    text += "  \"format\": ";
    writeString(text, kFormat);
    text += ",\n";
    text += "  \"version\": " + std::to_string(kVersion) + ",\n";

    text += "  \"syndicate\": ";
    writeString(text, syndicate);
    text += ",\n";

    text += "  \"syndicateLogo\": ";
    writeString(text, syndicateLogo);
    text += ",\n";

    text += "  \"territories\": [";
    for (std::size_t i = 0; i < territories.size(); ++i)
    {
        const TerritoryProgress& territory = territories[i];
        if (i != 0)
        {
            text += ",";
        }
        text += "\n    {\"id\": " + std::to_string(territory.id) +
                ", \"owner\": " + std::to_string(territory.owner) +
                ", \"taxLevel\": " + std::to_string(territory.taxLevel) + "}";
    }
    if (!territories.empty())
    {
        text += "\n  ";
    }
    text += "],\n";

    text += "  \"funds\": [";
    for (std::size_t i = 0; i < funds.size(); ++i)
    {
        const FundsProgress& entry = funds[i];
        if (i != 0)
        {
            text += ",";
        }
        text += "\n    {\"syndicate\": " + std::to_string(entry.syndicate) +
                ", \"amount\": " + formatFloat(entry.amount) + "}";
    }
    if (!funds.empty())
    {
        text += "\n  ";
    }
    text += "],\n";

    text += "  \"research\": [";
    for (std::size_t i = 0; i < research.size(); ++i)
    {
        const ResearchProgress& entry = research[i];
        if (i != 0)
        {
            text += ",";
        }
        text += "\n    {\"syndicate\": " + std::to_string(entry.syndicate) + ", \"completed\": [";
        for (std::size_t c = 0; c < entry.completed.size(); ++c)
        {
            if (c != 0)
            {
                text += ", ";
            }
            writeString(text, entry.completed[c]);
        }
        text += "], \"inProgress\": ";
        writeString(text, entry.inProgress);
        text += ", \"remainingSeconds\": " + formatFloat(entry.remainingSeconds) + "}";
    }
    if (!research.empty())
    {
        text += "\n  ";
    }
    text += "],\n";

    text += "  \"loadouts\": [";
    for (std::size_t i = 0; i < loadouts.size(); ++i)
    {
        const LoadoutProgress& entry = loadouts[i];
        if (i != 0)
        {
            text += ",";
        }
        text += "\n    {\"syndicate\": " + std::to_string(entry.syndicate) +
                ", \"slot\": " + std::to_string(entry.slot) + ", \"items\": [";
        for (std::size_t j = 0; j < entry.items.size(); ++j)
        {
            if (j != 0)
            {
                text += ", ";
            }
            writeString(text, entry.items[j]);
        }
        text += "]}";
    }
    if (!loadouts.empty())
    {
        text += "\n  ";
    }
    text += "],\n";

    text += "  \"roster\": [";
    for (std::size_t i = 0; i < roster.size(); ++i)
    {
        if (i != 0)
        {
            text += ", ";
        }
        writeString(text, roster[i]);
    }
    text += "]\n";

    text += "}\n";

    out = std::move(text);
    return true;
}

bool CampaignSave::parse(const std::string& text, CampaignSave& out, std::string& error)
{
    error.clear();
    out = CampaignSave{};

    Json root;
    root = Json::parse(text, error);
    if (!error.empty())
    {
        return false;
    }
    if (!root.isObject())
    {
        error = "save root must be a JSON object";
        return false;
    }

    // The format tag and version are checked first: a foreign or older file must
    // be rejected before any field is trusted.
    if (!root.has("format") || !root["format"].isString() || root["format"].asString() != kFormat)
    {
        error = std::string("not a ") + kFormat + " save (missing or wrong 'format')";
        return false;
    }
    if (!root.has("version") || !root["version"].isNumber())
    {
        error = "save is missing a numeric 'version'";
        return false;
    }
    const int version = root["version"].asInt();
    if (version != kVersion)
    {
        error = "unsupported save version " + std::to_string(version) + " (this build reads " +
                std::to_string(kVersion) + ")";
        return false;
    }

    if (root.has("syndicate") && !root["syndicate"].isString())
    {
        error = "'syndicate' must be a string";
        return false;
    }
    out.syndicate = root.has("syndicate") ? root["syndicate"].asString() : std::string();
    if (root.has("syndicateLogo") && !root["syndicateLogo"].isString())
    {
        error = "'syndicateLogo' must be a string";
        return false;
    }
    out.syndicateLogo =
        root.has("syndicateLogo") ? root["syndicateLogo"].asString() : std::string();

    // Territories.
    const Json& territories = root["territories"];
    if (!territories.isArray())
    {
        error = "'territories' must be an array";
        return false;
    }
    for (std::size_t i = 0; i < territories.size(); ++i)
    {
        const Json& node = territories[i];
        const std::string where = "territory entry " + std::to_string(i);
        if (!node.isObject())
        {
            error = where + " must be an object";
            return false;
        }
        TerritoryProgress entry;
        if (!readInt(node, "id", where, entry.id, error))
        {
            return false;
        }
        if (!node.has("owner") || !node["owner"].isNumber())
        {
            error = where + ": missing numeric 'owner'";
            return false;
        }
        entry.owner = node["owner"].asInt();
        if (entry.owner < -1)
        {
            error = where + ": 'owner' must be >= -1 (-1 = neutral)";
            return false;
        }
        if (!readInt(node, "taxLevel", where, entry.taxLevel, error))
        {
            return false;
        }
        out.territories.push_back(entry);
    }

    // Funds.
    const Json& funds = root["funds"];
    if (!funds.isArray())
    {
        error = "'funds' must be an array";
        return false;
    }
    for (std::size_t i = 0; i < funds.size(); ++i)
    {
        const Json& node = funds[i];
        const std::string where = "funds entry " + std::to_string(i);
        if (!node.isObject())
        {
            error = where + " must be an object";
            return false;
        }
        FundsProgress entry;
        if (!readInt(node, "syndicate", where, entry.syndicate, error))
        {
            return false;
        }
        if (!node.has("amount") || !node["amount"].isNumber())
        {
            error = where + ": missing numeric 'amount'";
            return false;
        }
        entry.amount = node["amount"].asFloat();
        if (entry.amount < 0.0f)
        {
            error = where + ": 'amount' must not be negative";
            return false;
        }
        out.funds.push_back(entry);
    }

    // Research.
    const Json& research = root["research"];
    if (!research.isArray())
    {
        error = "'research' must be an array";
        return false;
    }
    for (std::size_t i = 0; i < research.size(); ++i)
    {
        const Json& node = research[i];
        const std::string where = "research entry " + std::to_string(i);
        if (!node.isObject())
        {
            error = where + " must be an object";
            return false;
        }
        ResearchProgress entry;
        if (!readInt(node, "syndicate", where, entry.syndicate, error))
        {
            return false;
        }
        entry.completed = readStringArray(node["completed"], where + " 'completed'", error);
        if (!error.empty())
        {
            return false;
        }
        if (node.has("inProgress") && !node["inProgress"].isString())
        {
            error = where + ": 'inProgress' must be a string";
            return false;
        }
        entry.inProgress = node.has("inProgress") ? node["inProgress"].asString() : std::string();
        if (node.has("remainingSeconds"))
        {
            if (!node["remainingSeconds"].isNumber())
            {
                error = where + ": 'remainingSeconds' must be a number";
                return false;
            }
            entry.remainingSeconds = node["remainingSeconds"].asFloat();
            if (entry.remainingSeconds < 0.0f)
            {
                error = where + ": 'remainingSeconds' must not be negative";
                return false;
            }
        }
        out.research.push_back(std::move(entry));
    }

    // Loadouts.
    const Json& loadouts = root["loadouts"];
    if (!loadouts.isArray())
    {
        error = "'loadouts' must be an array";
        return false;
    }
    for (std::size_t i = 0; i < loadouts.size(); ++i)
    {
        const Json& node = loadouts[i];
        const std::string where = "loadout entry " + std::to_string(i);
        if (!node.isObject())
        {
            error = where + " must be an object";
            return false;
        }
        LoadoutProgress entry;
        if (!readInt(node, "syndicate", where, entry.syndicate, error))
        {
            return false;
        }
        if (!readInt(node, "slot", where, entry.slot, error))
        {
            return false;
        }
        if (entry.slot >= Equipment::kSquadSlots)
        {
            error = where + ": 'slot' must be < " + std::to_string(Equipment::kSquadSlots);
            return false;
        }
        entry.items = readStringArray(node["items"], where + " 'items'", error);
        if (!error.empty())
        {
            return false;
        }
        out.loadouts.push_back(std::move(entry));
    }

    // Roster: the per-slot agent names (#509/#532). Optional so a hand-written or
    // older-tooling file without it still loads (the slots are simply unnamed);
    // a v3 file always writes it.
    if (root.has("roster"))
    {
        out.roster = readStringArrayAllowingEmpty(root["roster"], "'roster'", error);
        if (!error.empty())
        {
            return false;
        }
    }

    return true;
}

bool CampaignSave::saveToFile(const std::string& path, std::string& error) const
{
    std::string text;
    if (!serialize(text, error))
    {
        return false;
    }
    std::ofstream file(path);
    if (!file)
    {
        error = "cannot open '" + path + "' for writing";
        return false;
    }
    file << text;
    if (!file)
    {
        error = "failed to write '" + path + "'";
        return false;
    }
    return true;
}

bool CampaignSave::loadFromFile(const std::string& path, CampaignSave& out, std::string& error)
{
    std::ifstream file(path);
    if (!file)
    {
        error = "cannot open '" + path + "' for reading";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str(), out, error);
}

bool CampaignSave::applyTo(Campaign& campaign, std::string& error) const
{
    error.clear();

    // Validate before mutating anything: a save that names a territory the base
    // campaign does not have is a mismatch between the save and the loaded
    // `--campaign` file, which must not silently drop progress — and must not
    // half-apply the identity either.
    for (const TerritoryProgress& entry : territories)
    {
        if (campaign.map().find(entry.id) == nullptr)
        {
            error = "save references territory " + std::to_string(entry.id) +
                    ", which the loaded campaign does not have";
            return false;
        }
    }

    // The player's identity, so a loaded campaign shows the name and logo the
    // player chose. An empty name/logo clears the choice and falls back to the
    // base campaign's default (never fatal).
    campaign.setSyndicateName(syndicate);
    campaign.setSyndicateLogo(syndicateLogo);

    // Ownership and tax.
    for (const TerritoryProgress& entry : territories)
    {
        campaign.map().setOwner(entry.id, entry.owner);
        campaign.map().setTaxLevel(entry.id, entry.taxLevel);
    }

    // Funds: the frozen Economy has addFunds/spend but no setter, so reset the
    // ledger to zero by spending what is there, then add the saved amount. Only
    // the syndicates the save names are touched; a syndicate the save omits keeps
    // the base file's funds (which for a fresh load is `startingFunds` for the
    // player and 0 elsewhere).
    for (const FundsProgress& entry : funds)
    {
        const float current = campaign.economy().funds(entry.syndicate);
        if (current > 0.0f && !campaign.economy().spend(entry.syndicate, current))
        {
            error = "could not reset funds for syndicate " + std::to_string(entry.syndicate);
            return false;
        }
        campaign.economy().addFunds(entry.syndicate, entry.amount);
    }

    // Loadouts: clear each saved slot, then reassign in the saved order.
    for (const LoadoutProgress& entry : loadouts)
    {
        campaign.equipment().clear(entry.syndicate, entry.slot);
        for (const std::string& item : entry.items)
        {
            campaign.equipment().assign(entry.syndicate, entry.slot, item);
        }
    }

    // Research: reinstate each saved syndicate's progress verbatim (the fields
    // were validated when the save was parsed).
    for (const ResearchProgress& entry : research)
    {
        ResearchState state;
        state.completed = entry.completed;
        state.inProgress = entry.inProgress;
        state.remainingSeconds = entry.remainingSeconds;
        campaign.research().restore(entry.syndicate, state);
    }

    // Roster: the recruited agent names (#509/#532). `setAgentNames` sizes and
    // pads, so a short saved roster cannot leave the roster the wrong length.
    campaign.setAgentNames(roster);

    return true;
}

} // namespace fossynd::core
