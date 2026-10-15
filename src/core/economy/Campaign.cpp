/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/economy/Campaign.h"

#include <algorithm>

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

    out.syndicateName_ = root.has("syndicate") ? root["syndicate"].asString() : std::string();
    out.startingFunds_ = root["startingFunds"].asFloat(0.0f);
    if (out.startingFunds_ < 0.0f)
    {
        error = "'startingFunds' must not be negative";
        return false;
    }

    out.worldFile_ = root.has("world") ? root["world"].asString() : std::string();
    out.researchFile_ = root.has("research") ? root["research"].asString() : std::string();
    out.equipmentFile_ = root.has("equipment") ? root["equipment"].asString() : std::string();
    out.winCondition_ = root.has("winCondition") ? root["winCondition"].asString() : std::string();

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

bool Campaign::selectMission(TerritoryId territory, const std::vector<int>& agentSlots)
{
    const Territory* node = map_.find(territory);
    if (node == nullptr || node->owner != kPlayerSyndicate || node->missionId.empty())
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
    selectedMissionId_ = node->missionId;
    selectedSlots_ = agentSlots;
    return true;
}

void Campaign::recordResult(bool won, int syndicate)
{
    if (won && selectedTerritory_ >= 0)
    {
        map_.setOwner(selectedTerritory_, syndicate);
    }
    evaluateEndState();
}

void Campaign::tick(float dt, Rng& rng)
{
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
    if (playerOwnsAny && !anyNeutral)
    {
        state_ = CampaignState::Lost;
    }
}

} // namespace fossynd::core
