/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/economy/Campaign.h"

#include <algorithm>
#include <utility>

#include "core/data/ContentPaths.h"

namespace fossynd::core
{
namespace
{

// The directory part of `path`, including the trailing separator ("" if none).
std::string directoryOf(const std::string& path)
{
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
    {
        return {};
    }
    return path.substr(0, slash + 1);
}

// Resolves a sibling file named in the campaign relative to the campaign file.
std::string siblingPath(const std::string& campaignPath, const std::string& name)
{
    if (name.empty() || name.front() == '/')
    {
        return name;
    }
    return directoryOf(campaignPath) + name;
}

} // namespace

bool Campaign::fromJson(const Json& root, Campaign& out, std::string& error)
{
    error.clear();

    if (!root.isObject())
    {
        error = "campaign root must be a JSON object";
        return false;
    }

    if (!Economy::fromJson(root, out.economy_.config(), error))
    {
        return false;
    }

    out.defaultSyndicateName_ =
        root.has("syndicate") ? root["syndicate"].asString() : std::string();
    // A player-chosen name/logo is not in the base file; it starts empty and falls
    // back to the default. `logo` (singular) is the default logo id, `logos` is
    // the pickable set (the art is a render slice; these are stable string keys).
    out.defaultSyndicateLogo_ = root.has("logo") ? root["logo"].asString() : std::string();
    out.logos_.clear();
    const Json& logos = root["logos"];
    if (!logos.isNull())
    {
        if (!logos.isArray())
        {
            error = "'logos' must be an array";
            return false;
        }
        for (std::size_t i = 0; i < logos.size(); ++i)
        {
            const std::string id = logos[i].asString();
            if (id.empty())
            {
                error = "each logo id must be a non-empty string";
                return false;
            }
            out.logos_.push_back(id);
        }
    }
    out.startingFunds_ = root["startingFunds"].asFloat(0.0f);
    if (out.startingFunds_ < 0.0f)
    {
        error = "'startingFunds' must not be negative";
        return false;
    }

    out.worldFile_ = root.has("world") ? root["world"].asString() : std::string();
    out.researchFile_ = root.has("research") ? root["research"].asString() : std::string();
    out.equipmentFile_ = root.has("equipment") ? root["equipment"].asString() : std::string();
    out.namesFile_ = root.has("names") ? root["names"].asString() : std::string();
    out.winCondition_ = root.has("winCondition") ? root["winCondition"].asString() : std::string();
    out.finalMission_ = root.has("finalMission") ? root["finalMission"].asString() : std::string();
    out.finalMissionWon_ = false;

    // A fresh campaign has no recruited agents; a save/load overlay restores the
    // names (issue #509, the persistence half is a Rule 4 request to `world`).
    out.agentNames_.assign(static_cast<std::size_t>(Equipment::kSquadSlots), std::string());

    out.missions_.clear();
    const Json& missions = root["missions"];
    if (!missions.isNull())
    {
        if (!missions.isArray())
        {
            error = "'missions' must be an array";
            return false;
        }
        for (std::size_t i = 0; i < missions.size(); ++i)
        {
            const std::string id = missions[i].asString();
            if (id.empty())
            {
                error = "each mission id must be a non-empty string";
                return false;
            }
            out.missions_.push_back(id);
        }
    }

    // The terminal mission must be one of the campaign's own missions, and a
    // `"final-mission"` win condition needs one — otherwise the campaign could
    // never be won, which is a data mistake worth catching at load (issue #456).
    if (!out.finalMission_.empty() &&
        std::find(out.missions_.begin(), out.missions_.end(), out.finalMission_) == out.missions_.end())
    {
        error = "'finalMission' must be one of 'missions'";
        return false;
    }
    if (out.winCondition_ == "final-mission" && out.finalMission_.empty())
    {
        error = "'winCondition' \"final-mission\" requires a 'finalMission' id";
        return false;
    }

    return true;
}

bool Campaign::loadFromFile(const std::string& path, Campaign& out, std::string& error)
{
    error.clear();

    Json root;
    if (!Json::parseFile(path, root, error))
    {
        return false;
    }
    if (!fromJson(root, out, error))
    {
        return false;
    }

    // A sibling named in the file that is missing on disk is tolerated (the
    // system starts empty); a sibling that exists but is invalid is an error.
    const std::string worldPath = siblingPath(path, out.worldFile_);
    if (!worldPath.empty() && fileExists(worldPath))
    {
        Json node;
        if (!Json::parseFile(worldPath, node, error))
        {
            return false;
        }
        if (!WorldMap::fromJson(node, out.map_, error))
        {
            return false;
        }
    }

    const std::string researchPath = siblingPath(path, out.researchFile_);
    if (!researchPath.empty() && fileExists(researchPath))
    {
        Json node;
        if (!Json::parseFile(researchPath, node, error))
        {
            return false;
        }
        if (!ResearchTree::fromJson(node, out.researchTree_, error))
        {
            return false;
        }
    }

    const std::string equipmentPath = siblingPath(path, out.equipmentFile_);
    if (!equipmentPath.empty() && fileExists(equipmentPath))
    {
        Json node;
        if (!Json::parseFile(equipmentPath, node, error))
        {
            return false;
        }
        if (!EquipmentCatalogue::fromJson(node, out.catalogue_, error))
        {
            return false;
        }
    }

    const std::string namesPath = siblingPath(path, out.namesFile_);
    if (!namesPath.empty() && fileExists(namesPath))
    {
        Json node;
        if (!Json::parseFile(namesPath, node, error))
        {
            return false;
        }
        if (!NamePools::fromJson(node, out.namePools_, error))
        {
            return false;
        }
    }

    // Start the corporation: the player begins with the campaign's
    // `startingFunds`, so the loop "tax -> research -> buy" has money from the
    // first frame. Granted here rather than in fromJson so that parsing a config
    // never mutates a ledger, and once per load (a caller passes a fresh `out`).
    if (out.startingFunds_ > 0.0f)
    {
        out.economy_.addFunds(kPlayerSyndicate, out.startingFunds_);
    }

    return true;
}

void Campaign::setSyndicateName(const std::string& name)
{
    // An empty name clears the choice, so the data-authored default renders again
    // (a player cannot leave the map without an identity).
    chosenSyndicateName_ = name;
}

void Campaign::setSyndicateLogo(const std::string& id)
{
    // An empty id, or one that is not a known logo, clears the choice and falls
    // back to the default: an unknown id is never fatal, it just picks the default
    // art (the art is the render lane's; here an id is only a stable key).
    if (id.empty() || std::find(logos_.begin(), logos_.end(), id) == logos_.end())
    {
        chosenSyndicateLogo_.clear();
        return;
    }
    chosenSyndicateLogo_ = id;
}

// Recruits an agent for `slot` in `region`, assigning the first name in the
// region's pool not already used by the roster. A slot that already has a name
// keeps it (a name never changes once assigned), so this is idempotent per slot.
// Deterministic (no RNG): the same roster and region always yield the same name,
// so a re-run is stable and a save overlay can reproduce it. An absent or
// exhausted pool assigns no name.
std::string Campaign::recruitAgent(int slot, TerritoryId region)
{
    if (slot < 0 || slot >= Equipment::kSquadSlots)
    {
        return {};
    }
    if (agentNames_.size() != static_cast<std::size_t>(Equipment::kSquadSlots))
    {
        agentNames_.assign(static_cast<std::size_t>(Equipment::kSquadSlots), std::string());
    }

    // A name, once assigned, never changes: re-recruiting a named slot returns the
    // name it already holds (the recruit screen only fills empty slots anyway).
    std::string& assigned = agentNames_[static_cast<std::size_t>(slot)];
    if (!assigned.empty())
    {
        return assigned;
    }

    const std::vector<std::string>* pool = namePools_.pool(region);
    if (pool == nullptr)
    {
        return {};
    }

    for (const std::string& candidate : *pool)
    {
        const bool used = std::find(agentNames_.begin(), agentNames_.end(), candidate) != agentNames_.end();
        if (!used)
        {
            assigned = candidate;
            return candidate;
        }
    }

    // The pool is exhausted (every name already taken): leave the slot unnamed
    // rather than duplicate an identity.
    return {};
}

const std::string& Campaign::agentName(int slot) const
{
    static const std::string kEmpty;
    if (slot < 0 || slot >= static_cast<int>(agentNames_.size()))
    {
        return kEmpty;
    }
    return agentNames_[static_cast<std::size_t>(slot)];
}

void Campaign::setAgentNames(const std::vector<std::string>& names)
{
    agentNames_.assign(static_cast<std::size_t>(Equipment::kSquadSlots), std::string());
    for (std::size_t i = 0; i < names.size() && i < agentNames_.size(); ++i)
    {
        agentNames_[i] = names[i];
    }
}

bool Campaign::canDeployTo(TerritoryId territory) const
{
    // The campaign is over once it is won or lost: no further deployment, so a
    // finished game cannot keep changing (the end state is terminal, not just a
    // banner). The engine layer also freezes its own input, but the rule lives
    // here with the rest of the campaign rules.
    if (finished())
    {
        return false;
    }
    const Territory* node = map_.find(territory);
    if (node == nullptr || node->missionId.empty())
    {
        return false;
    }
    if (node->owner == kPlayerSyndicate)
    {
        return true;
    }
    // Otherwise the territory must be on the frontier: adjacent to one the
    // player holds. That is what makes the map winnable — a neutral territory
    // reachable from an owned one can be attacked and claimed.
    for (const TerritoryId neighbour : node->neighbours)
    {
        if (map_.owner(neighbour) == kPlayerSyndicate)
        {
            return true;
        }
    }
    return false;
}

bool Campaign::selectMission(TerritoryId territory, const std::vector<int>& agentSlots)
{
    if (!canDeployTo(territory))
    {
        return false;
    }
    if (agentSlots.size() > static_cast<std::size_t>(Equipment::kSquadSlots))
    {
        return false;
    }
    for (int slot : agentSlots)
    {
        if (slot < 0 || slot >= Equipment::kSquadSlots)
        {
            return false;
        }
    }

    selectedTerritory_ = territory;
    selectedMissionId_ = map_.find(territory)->missionId;
    selectedSlots_ = agentSlots;
    return true;
}

PlayerLoadout Campaign::selectedLoadout() const
{
    PlayerLoadout loadout;
    for (const int slot : selectedSlots_)
    {
        LoadoutSlot entry;
        for (const std::string& itemId : equipment_.loadout(kPlayerSyndicate, slot))
        {
            const EquipmentItem* item = catalogue_.find(itemId);
            if (item == nullptr)
            {
                continue; // assigned but not in the catalogue: nothing to deploy
            }
            if (item->kind == EquipmentKind::Weapon && entry.weaponId.empty())
            {
                entry.weaponId = item->id;
            }
            else if (item->kind == EquipmentKind::Gadget && entry.gadgetId.empty())
            {
                entry.gadgetId = item->id;
            }
            else if (item->kind == EquipmentKind::Cybernetic && entry.cyberneticId.empty())
            {
                // A cybernetic is a passive upgrade, but it still deploys: R1's
                // LoadoutSlot carries one `cyberneticId`, which applyLoadout applies
                // once at mission load (issue #341, ADR-0031).
                entry.cyberneticId = item->id;
            }
            // Anything already set (a second weapon/gadget/cybernetic in a slot) is
            // ignored, because R1's LoadoutSlot carries at most one of each.
        }
        loadout.slots.push_back(std::move(entry));
    }
    return loadout;
}

std::vector<DrugCeilings> Campaign::selectedCeilings() const
{
    std::vector<DrugCeilings> result;
    for (const int slot : selectedSlots_)
    {
        result.push_back(equipment_.ceilings(kPlayerSyndicate, slot, catalogue_));
    }
    return result;
}

void Campaign::recordResult(bool won, int syndicate)
{
    // The end state is terminal: once won or lost, a later result cannot change
    // it (the player cannot "un-lose" by winning a straggler mission).
    if (finished())
    {
        return;
    }
    if (won && selectedTerritory_ >= 0)
    {
        map_.setOwner(selectedTerritory_, syndicate);
    }
    // Winning the terminal mission is the final confrontation: the campaign ends
    // then and there, however much of the map the player holds (issue #456).
    if (won && !finalMission_.empty() && selectedMissionId_ == finalMission_)
    {
        finalMissionWon_ = true;
    }
    evaluateEndState();
}

void Campaign::tick(float dt, Rng& rng)
{
    // A finished campaign is frozen: no income, research, or revolts after the
    // end state, so the final board the victory/defeat screen shows never
    // changes under the player.
    if (finished())
    {
        return;
    }
    economy_.tick(dt, map_, rng);
    research_.tick(dt);
}

void Campaign::evaluateEndState()
{
    if (state_ == CampaignState::Lost)
    {
        return;
    }

    bool playerOwnsAny = false;
    bool anyNeutral = false;
    bool allPlayer = true;
    for (const Territory& territory : map_.territories())
    {
        if (territory.owner == kPlayerSyndicate)
        {
            playerOwnsAny = true;
        }
        else
        {
            allPlayer = false;
        }
        if (territory.owner < 0)
        {
            anyNeutral = true;
        }
    }

    if (winCondition_ == "own-all-territories" && allPlayer && !map_.territories().empty())
    {
        state_ = CampaignState::Won;
        return;
    }
    // A `final-mission` campaign is won by winning the terminal mission, not by
    // taking the whole map (issue #456). The flag is set in `recordResult`.
    if (winCondition_ == "final-mission" && finalMissionWon_)
    {
        state_ = CampaignState::Won;
        return;
    }
    if (playerOwnsAny && !anyNeutral)
    {
        state_ = CampaignState::Lost;
    }
}

} // namespace fossynd::core
