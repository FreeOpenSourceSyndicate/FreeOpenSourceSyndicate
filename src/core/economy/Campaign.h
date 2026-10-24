/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>
#include <vector>

#include "core/data/Json.h"
#include "core/economy/Economy.h"
#include "core/economy/Equipment.h"
#include "core/economy/Research.h"
#include "core/economy/WorldMap.h"
#include "core/sim/PlayerLoadout.h"
#include "core/util/Rng.h"

namespace fossynd::core
{

enum class CampaignState
{
    Running,
    Won,
    Lost,
};

// The top-level metagame object: it owns the world map, economy, research, and
// equipment, and is what the game layer drives (Milestone B, docs/METAGAME.md).
// Engine-free (ADR-0002); the systems are composed, not coupled.
class Campaign
{
public:
    // The player is syndicate 0; the map's other indices are rivals.
    static constexpr int kPlayerSyndicate = 0;

    // Parses a campaign object: the economy fields (read by Economy::fromJson),
    // the syndicate name, starting funds, the sibling file names, the mission
    // list, and the win condition. Does not load the sibling files; loadFromFile
    // does. Returns false and sets `error` on a schema error.
    static bool fromJson(const Json& root, Campaign& out, std::string& error);

    // Parses `campaign.json` and then loads its sibling world/research/equipment
    // files, resolved relative to the campaign file. A sibling named in the file
    // that is missing on disk is not an error (the system starts empty, matching
    // the mission/catalogue tolerance, AGENTS.md §1b); a sibling that exists but
    // is invalid is an error.
    static bool loadFromFile(const std::string& path, Campaign& out, std::string& error);

    WorldMap& map() { return map_; }
    Economy& economy() { return economy_; }
    Research& research() { return research_; }
    Equipment& equipment() { return equipment_; }
    const WorldMap& map() const { return map_; }
    const Economy& economy() const { return economy_; }
    const Research& research() const { return research_; }
    const Equipment& equipment() const { return equipment_; }

    // The loaded research tree and equipment catalogue. The systems above need
    // them to start research and buy items, so the campaign exposes them.
    const ResearchTree& researchTree() const { return researchTree_; }
    const EquipmentCatalogue& catalogue() const { return catalogue_; }

    // The player syndicate's display name and logo id. A fresh campaign uses the
    // data-authored defaults from `campaign.json`; a player choice overrides them.
    // The name falls back to the default when cleared (empty), and the logo falls
    // back when it is empty or not one of the data-authored logo ids, so an
    // unknown value never leaves the map without an identity — and is never fatal
    // (the art itself is the render lane's; a logo id is just a stable string key
    // it maps to art). Both are plain data, so the core stays SDL-free (rule 1).
    const std::string& syndicateName() const
    {
        return chosenSyndicateName_.empty() ? defaultSyndicateName_ : chosenSyndicateName_;
    }
    const std::string& syndicateLogo() const
    {
        return chosenSyndicateLogo_.empty() ? defaultSyndicateLogo_ : chosenSyndicateLogo_;
    }
    void setSyndicateName(const std::string& name); // empty clears to the default
    void setSyndicateLogo(const std::string& id);   // empty/unknown clears to the default

    // The data-authored defaults and the known logo ids, so a UI can offer a
    // "reset" and build a logo picker from the campaign's own catalogue.
    const std::string& defaultSyndicateName() const { return defaultSyndicateName_; }
    const std::string& defaultSyndicateLogo() const { return defaultSyndicateLogo_; }
    const std::vector<std::string>& syndicateLogos() const { return logos_; }

    float startingFunds() const { return startingFunds_; }
    const std::vector<std::string>& missions() const { return missions_; }
    const std::string& winCondition() const { return winCondition_; }

    // The id of the campaign's terminal mission (the original's "final
    // confrontation"), or empty when the campaign has none. Winning *this*
    // mission ends the campaign with the victory state regardless of how much of
    // the map the player holds (issue #456); a `"final-mission"` win condition
    // requires it, and `own-all-territories` still wins the map outright.
    const std::string& finalMission() const { return finalMission_; }

    // True once the campaign has been won *by* winning the final confrontation,
    // so the victory screen can show a distinct end state (issue #456). False
    // under an `own-all-territories` win condition even if the terminal mission
    // was among those won, because the campaign did not end *because* of it.
    bool wonFinalConfrontation() const
    {
        return state_ == CampaignState::Won && finalMissionWon_ &&
               winCondition_ == "final-mission";
    }

    // --- selection & deployment ---

    // True if a mission can be deployed to `territory`: it must have a mission,
    // and it must be player-owned or on the frontier — adjacent to a territory
    // the player holds. A win at a frontier territory claims it, so ownership
    // grows inward from the border (the Syndicate attack model, ADR-0027).
    // Always false once the campaign is finished (the end state is terminal).
    bool canDeployTo(TerritoryId territory) const;

    // Selects the mission deployed from `territory` and the agent slots to send
    // (at most Equipment::kSquadSlots). `territory` must satisfy canDeployTo.
    // Returns false otherwise, or if a slot is out of range.
    bool selectMission(TerritoryId territory, const std::vector<int>& agentSlots);

    TerritoryId selectedTerritory() const { return selectedTerritory_; } // -1 if none
    const std::string& selectedMissionId() const { return selectedMissionId_; } // empty if none
    const std::vector<int>& selectedSlots() const { return selectedSlots_; }

    // The bought loadout for the selected mission, resolved from the squad's
    // equipment slots (Milestone B deployment). Slot i of the result is the
    // equipment assigned to squad slot `selectedSlots()[i]`: a weapon id from
    // the shared weapon catalogue, a gadget id from the tactical gadget
    // vocabulary, and/or a cybernetic id from the tactical cybernetic
    // vocabulary (a passive upgrade applied once at mission load). An empty
    // result means "keep the mission's authored squad" (R1, docs/METAGAME.md
    // §7). The engine layer hands this to `applyLoadout`, so a bought
    // weapon/gadget/cybernetic reaches the deployed agent instead of the
    // mission's default (issues #312, #341).
    PlayerLoadout selectedLoadout() const;

    // --- progression ---

    // Records the result of the selected mission. A win claims the territory for
    // `syndicate`; a loss leaves ownership unchanged. Then updates the end state.
    // A no-op once the campaign is finished (the end state is terminal).
    void recordResult(bool won, int syndicate);

    CampaignState state() const { return state_; }

    // True once the campaign has reached an end state (won or lost). A finished
    // campaign is terminal: no further deployment, results, or clock ticks change
    // it, so the victory/defeat screen shows a frozen final board.
    bool finished() const { return state_ != CampaignState::Running; }

    // Advances the economy and research by `dt` in-game seconds (the campaign
    // clock, not the tactical timestep).
    void tick(float dt, Rng& rng);

private:
    void evaluateEndState();

    WorldMap map_;
    Economy economy_;
    Research research_;
    Equipment equipment_;
    ResearchTree researchTree_;
    EquipmentCatalogue catalogue_;

    std::string defaultSyndicateName_;
    std::string defaultSyndicateLogo_;
    std::string chosenSyndicateName_; // empty = use the default
    std::string chosenSyndicateLogo_; // empty = use the default
    std::vector<std::string> logos_;  // known logo ids from the data file
    float startingFunds_ = 0.0f;
    std::vector<std::string> missions_;
    std::string winCondition_;
    std::string finalMission_;           // id of the terminal mission, empty if none
    bool finalMissionWon_ = false;       // the terminal mission was won (issue #456)

    std::string worldFile_;
    std::string researchFile_;
    std::string equipmentFile_;

    TerritoryId selectedTerritory_ = -1;
    std::string selectedMissionId_;
    std::vector<int> selectedSlots_;
    CampaignState state_ = CampaignState::Running;
};

} // namespace fossynd::core
