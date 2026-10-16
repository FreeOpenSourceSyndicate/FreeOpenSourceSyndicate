/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "SDL.h"

#include "core/economy/Campaign.h"
#include "core/util/Rng.h"
#include "game/Font.h"

namespace fossynd::game
{

// The campaign management screen (Milestone B, docs/METAGAME.md): the world map,
// the funds ledger, research, equipment, and mission deployment, driven from one
// engine-layer view. It owns a `core::Campaign` and reads/writes it; all the
// rules stay in `core/economy/**` (engine-free, ADR-0002), so this class is
// presentation and input only — it never reimplements a rule.
//
// Kept in its own translation unit rather than growing the shared `Game.cpp`
// (docs/FOCUS_AREAS.md §4, Rule 2). It is view-only in the golden-rule-4 sense:
// it advances the campaign clock (economy + research), which is metagame state,
// but it never touches the tactical simulation.
class CampaignScreen
{
public:
    CampaignScreen() = default;

    // Loads the campaign from `path`, or from the first existing default location
    // when `path` is empty. Returns false when no campaign could be loaded (the
    // screen then stays invalid and the caller skips it, so a missing data file
    // is never fatal — AGENTS.md §1b). `exeDir` is used to search for campaign
    // and mission files alongside the executable.
    bool init(const std::string& path, const std::string& exeDir);

    bool valid() const { return valid_; }

    // Visibility is a view-only toggle (the `M` key). Hidden by default so the
    // tactical mission is what the player sees first.
    void toggle() { visible_ = !visible_; }
    bool visible() const { return visible_; }
    void setVisible(bool visible) { visible_ = visible; }

    // Advances the campaign clock by `dt` in-game seconds (economy + research).
    void update(float dt);

    // Handles a click at screen `(x, y)` on a `w` x `h` surface. Returns true when
    // the click selected a territory (so the caller can stop propagating it).
    bool handleClick(int x, int y, int w, int h);

    // Changes the selected territory's tax level by `delta` (clamped to
    // [0, maxTaxLevel]). A no-op unless a player-owned territory is selected.
    void adjustTax(int delta);

    // The territory currently selected on the map, or -1. Read-only, for the
    // engine layer (e.g. the deploy self-test).
    core::TerritoryId selectedTerritory() const { return selected_; }

    // Deploys the selected territory's mission with the full four-agent squad.
    // Returns false when nothing selectable is selected.
    bool deploySelected();

    // True once a mission has been deployed from the map and is waiting to be
    // played (issue #312). The engine layer reads the pending mission and its
    // loadout, starts it, and clears the flag by calling `recordDeployResult`.
    bool hasDeployed() const { return hasDeployed_; }

    // The mission id and the loadout of the pending deployment. Both are empty
    // when nothing is pending. `deployedLoadout` is what R1's `applyLoadout`
    // takes, so the bought squad reaches the deployed agents.
    const std::string& deployedMissionId() const { return deployedMissionId_; }
    const core::PlayerLoadout& deployedLoadout() const { return deployedLoadout_; }
    // The resolved mission file for the pending deployment, or empty when the
    // mission id has no file on disk (the deploy then fails rather than starting
    // the wrong mission).
    const std::string& deployedMissionPath() const { return deployedMissionPath_; }

    // Records the played mission's result: a win claims the territory for the
    // player and the campaign advances; either way the pending deployment is
    // cleared, so the map is ready for the next deploy. Returns the campaign
    // state after the result, so the caller can show the end state.
    core::CampaignState recordDeployResult(bool won);

    // --- R&D shop (Milestone B: research and purchase in play) ---

    // The shop overlay. It lists the research tree and the equipment catalogue so
    // a player can fund research and buy gear without a debug flag — the middle of
    // the loop (tax -> research -> buy -> deploy), which was previously reachable
    // only from the self-test. View-only state; every rule (cost, prerequisites,
    // research gate, funds) stays in `core/economy/**`.
    bool shopOpen() const { return shopOpen_; }
    void toggleShop();

    // Handles a management-screen key. Returns true when the screen consumed it.
    // `B` opens/closes the shop; while it is open the shop owns Up/Down (move the
    // row), Left/Right (target squad slot), Enter (act), and Esc (close). Returns
    // false for every other key (and once the campaign is finished) so the global
    // bindings still run.
    bool handleKey(int sym);

    // Draws the screen: backdrop, world map, and the management panel.
    void render(SDL_Renderer* renderer, const BitmapFont& font, int w, int h) const;

    // Headless self-check of the real management loop, on a *copy* of the loaded
    // campaign so the live state is untouched: starting funds are seeded, taxing
    // an owned territory earns income, a research node can be funded and
    // completed, an item can be bought and assigned, a mission can be deployed,
    // and claiming every territory wins the campaign. Returns false on the first
    // broken invariant. Used by the `--check-campaign` guard and the
    // `game_campaign` ctest.
    bool selfTest();

private:
    // A view-only node position in normalised [0,1] map space. Never saved and
    // never part of the simulation: it is recomputed from the territory graph so
    // the map has a stable, deterministic layout (no RNG).
    struct LayoutPoint
    {
        float nx = 0.5f;
        float ny = 0.5f;
    };

    // Places every territory by breadth-first ring from territory 0, so the
    // layout is deterministic and neighbours sit near each other.
    void computeLayout();

    // Maps a territory id to a screen position inside the map region.
    void screenPos(std::size_t id, int w, int h, int& outX, int& outY) const;

    // The territory whose node is under `(x, y)`, or -1. `radius` is the click
    // tolerance in pixels.
    int nodeAtScreen(int x, int y, int w, int h, int radius) const;

    // Draws the R&D shop overlay: the research nodes and the equipment items as
    // one scrollable list, with the selected row's cost/state and the target squad
    // slot. Reads `core::Campaign` only (golden rule 4).
    void renderShop(SDL_Renderer* renderer, const BitmapFont& font, int w, int h) const;

    // Resolves a mission id (as named by a territory) to its JSON file. Searches
    // the mission directory under the repo root, then alongside the executable,
    // then the common install prefixes — the same order as the default mission
    // search (core/data/ContentPaths.h). Returns an empty string when no file
    // exists, so a deploy with no mission file fails instead of loading the
    // wrong mission.
    std::string resolveMissionFile(const std::string& missionId) const;

    core::Campaign campaign_;
    // Seeded so a campaign is reproducible: the only randomness is the revolt
    // roll (golden rule 2). Never reseeded at runtime.
    core::Rng rng_{0xC0FFEEu};
    bool valid_ = false;
    bool visible_ = false;
    core::TerritoryId selected_ = -1;
    std::vector<LayoutPoint> layout_;

    // The executable directory, kept so a deployed mission id resolves to a
    // file alongside the executable when run from outside the repo (issue #312).
    std::string exeDir_;

    // A deployment waiting to be played (issue #312): the mission the selected
    // territory names, its resolved file, and the loadout resolved from the
    // bought squad. Cleared by recordDeployResult() once the mission has been
    // played.
    bool hasDeployed_ = false;
    std::string deployedMissionId_;
    std::string deployedMissionPath_;
    core::PlayerLoadout deployedLoadout_;

    // R&D shop view state. Rows are the research nodes followed by the equipment
    // items; `shopRow_` indexes that list, `shopSlot_` is the squad slot a bought
    // item is assigned to. Never saved and never part of the simulation.
    // `shopScroll_` is mutable: render() is const but clamps the scroll window to
    // keep the selected row visible, and it is a pure view cache.
    bool shopOpen_ = false;
    int shopRow_ = 0;
    int shopSlot_ = 0;
    mutable int shopScroll_ = 0;
};

} // namespace fossynd::game
