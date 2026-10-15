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
    // is never fatal — AGENTS.md §1b).
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

    // Deploys the selected territory's mission with the full four-agent squad.
    // Returns false when nothing selectable is selected.
    bool deploySelected();

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

    core::Campaign campaign_;
    // Seeded so a campaign is reproducible: the only randomness is the revolt
    // roll (golden rule 2). Never reseeded at runtime.
    core::Rng rng_{0xC0FFEEu};
    bool valid_ = false;
    bool visible_ = false;
    core::TerritoryId selected_ = -1;
    std::vector<LayoutPoint> layout_;
};

} // namespace fossynd::game
