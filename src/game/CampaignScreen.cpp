/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/CampaignScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <queue>

#include "core/data/ContentPaths.h"
#include "core/util/CampaignSave.h"
#include "game/HudStyle.h"

namespace fossynd::game
{
namespace
{

// The default campaign file, searched when no explicit path is given.
constexpr const char* kDefaultCampaignFile = "assets/data/world/campaign.json";

// Candidate campaign locations, in priority order, mirroring the mission/lang
// search (core/data/ContentPaths.h, core/i18n/StringTable.h): a build tree first,
// then alongside the executable and the common install prefixes.
std::vector<std::string> defaultCampaignPaths(const std::string& exeDir)
{
    std::vector<std::string> paths;
    paths.push_back(kDefaultCampaignFile);
    if (!exeDir.empty())
    {
        paths.push_back(exeDir + "/" + kDefaultCampaignFile);
        paths.push_back(exeDir + "/../share/fossynd/world/campaign.json");
        paths.push_back(exeDir + "/world/campaign.json");
    }
    paths.push_back("/usr/local/share/fossynd/world/campaign.json");
    paths.push_back("/usr/share/fossynd/world/campaign.json");
    return paths;
}

// Candidate files for a mission id, in priority order. Mirrors the default
// mission search (core/data/ContentPaths.h): a build tree first, then alongside
// the executable and the common install prefixes. The mission directory is
// `assets/data/missions/` under the content root.
std::vector<std::string> missionFileCandidates(const std::string& missionId,
                                               const std::string& exeDir)
{
    const std::string file = missionId + ".json";
    std::vector<std::string> paths;
    paths.push_back("assets/data/missions/" + file);
    if (!exeDir.empty())
    {
        paths.push_back(exeDir + "/assets/data/missions/" + file);
        paths.push_back(exeDir + "/../share/fossynd/missions/" + file);
        paths.push_back(exeDir + "/data/missions/" + file);
    }
    paths.push_back("/usr/local/share/fossynd/missions/" + file);
    paths.push_back("/usr/share/fossynd/missions/" + file);
    return paths;
}

// The rectangle the map graph is laid out in: the screen minus a left panel and
// a top title bar. Shared by the draw path and the hit-test so a click lands on
// the node it looks like it lands on.
void mapRegion(int w, int h, int& x0, int& y0, int& x1, int& y1)
{
    const int panelW = std::min(static_cast<int>(static_cast<float>(w) * 0.42f), 360);
    const int margin = 12;
    const int topBar = 40;
    x0 = panelW + margin;
    y0 = topBar;
    x1 = w - margin;
    y1 = h - margin;
}

// A filled isometric diamond, the same node shape the tactical markers use, so
// the map reads as the same art (AGENTS.md §1).
void drawDiamond(SDL_Renderer* renderer, int cx, int cy, int halfW, int halfH, std::uint8_t r,
                 std::uint8_t g, std::uint8_t b)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    for (int py = cy - halfH; py <= cy + halfH; ++py)
    {
        const float dy = static_cast<float>(py - cy) / static_cast<float>(halfH);
        const float span = 1.0f - std::fabs(dy);
        if (span <= 0.0f)
        {
            continue;
        }
        SDL_Rect line;
        line.x = static_cast<int>(static_cast<float>(cx) - static_cast<float>(halfW) * span);
        line.w = static_cast<int>(2.0f * static_cast<float>(halfW) * span) + 1;
        line.y = py;
        line.h = 1;
        SDL_RenderFillRect(renderer, &line);
    }
}

// Owner colour: the player is the marker green, a rival is the enemy red, and a
// neutral territory is grey.
void ownerColor(int owner, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b)
{
    if (owner == core::Campaign::kPlayerSyndicate)
    {
        r = hudstyle::kPlayerBody[0];
        g = hudstyle::kPlayerBody[1];
        b = hudstyle::kPlayerBody[2];
    }
    else if (owner < 0)
    {
        r = 120;
        g = 125;
        b = 120;
    }
    else
    {
        r = hudstyle::kEnemyBody[0];
        g = hudstyle::kEnemyBody[1];
        b = hudstyle::kEnemyBody[2];
    }
}

} // namespace

bool CampaignScreen::init(const std::string& path, const std::string& exeDir)
{
    exeDir_ = exeDir;
    std::string resolved = path;
    if (resolved.empty())
    {
        for (const std::string& candidate : defaultCampaignPaths(exeDir))
        {
            if (core::fileExists(candidate))
            {
                resolved = candidate;
                break;
            }
        }
    }
    if (resolved.empty())
    {
        // No campaign data anywhere: stay invalid so the caller skips the screen
        // and the tactical game still runs (AGENTS.md §1b).
        return false;
    }

    std::string error;
    if (!core::Campaign::loadFromFile(resolved, campaign_, error))
    {
        std::fprintf(stderr, "campaign data error (%s): %s\n", resolved.c_str(), error.c_str());
        return false;
    }

    computeLayout();
    valid_ = true;
    campaignFile_ = resolved;
    // Start on the first player-owned territory so the map opens with something
    // selected.
    selected_ = -1;
    for (const core::Territory& territory : campaign_.map().territories())
    {
        if (territory.owner == core::Campaign::kPlayerSyndicate)
        {
            selected_ = territory.id;
            break;
        }
    }
    return true;
}

void CampaignScreen::computeLayout()
{
    const std::size_t n = campaign_.map().size();
    layout_.assign(n, LayoutPoint{});
    if (n == 0)
    {
        return;
    }

    // Breadth-first rings from territory 0 give a deterministic layout in which
    // neighbours sit on adjacent rings. Disconnected territories are appended to
    // the outermost ring, so every node still gets a position.
    std::vector<int> ring(n, -1);
    std::vector<std::vector<core::TerritoryId>> rings;
    std::queue<core::TerritoryId> frontier;
    ring[0] = 0;
    frontier.push(0);
    while (!frontier.empty())
    {
        const core::TerritoryId id = frontier.front();
        frontier.pop();
        const int depth = ring[id];
        if (static_cast<std::size_t>(depth) >= rings.size())
        {
            rings.resize(static_cast<std::size_t>(depth) + 1);
        }
        rings[static_cast<std::size_t>(depth)].push_back(id);
        for (const core::TerritoryId next : campaign_.map().neighbours(id))
        {
            if (next >= 0 && static_cast<std::size_t>(next) < n && ring[next] < 0)
            {
                ring[next] = depth + 1;
                frontier.push(next);
            }
        }
    }
    // Anything the walk did not reach (no path from 0) joins the last ring.
    if (rings.empty())
    {
        rings.resize(1);
    }
    for (std::size_t i = 0; i < n; ++i)
    {
        if (ring[i] < 0)
        {
            rings.back().push_back(static_cast<core::TerritoryId>(i));
        }
    }

    const float maxRing = static_cast<float>(rings.size() > 1 ? rings.size() - 1 : 1);
    for (std::size_t r = 0; r < rings.size(); ++r)
    {
        const std::vector<core::TerritoryId>& nodes = rings[r];
        const std::size_t count = nodes.size();
        const float radius = 0.46f * (static_cast<float>(r) / maxRing);
        for (std::size_t i = 0; i < count; ++i)
        {
            LayoutPoint point;
            if (r == 0 && count == 1)
            {
                point.nx = 0.5f;
                point.ny = 0.5f;
            }
            else
            {
                // Offset alternate rings so nodes do not stack radially.
                const float phase = (r % 2 == 0) ? 0.0f : 0.5f;
                const float angle =
                    2.0f * 3.14159265358979f * (static_cast<float>(i) + phase) /
                    static_cast<float>(count);
                point.nx = 0.5f + radius * std::cos(angle);
                point.ny = 0.5f + radius * std::sin(angle);
            }
            point.nx = std::min(0.97f, std::max(0.03f, point.nx));
            point.ny = std::min(0.97f, std::max(0.03f, point.ny));
            layout_[static_cast<std::size_t>(nodes[i])] = point;
        }
    }
}

void CampaignScreen::screenPos(std::size_t id, int w, int h, int& outX, int& outY) const
{
    if (id >= layout_.size())
    {
        outX = 0;
        outY = 0;
        return;
    }
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    mapRegion(w, h, x0, y0, x1, y1);
    outX = x0 + static_cast<int>(layout_[id].nx * static_cast<float>(x1 - x0));
    outY = y0 + static_cast<int>(layout_[id].ny * static_cast<float>(y1 - y0));
}

int CampaignScreen::nodeAtScreen(int x, int y, int w, int h, int radius) const
{
    int best = -1;
    int bestDistance = radius * radius;
    for (std::size_t id = 0; id < layout_.size(); ++id)
    {
        int nx = 0;
        int ny = 0;
        screenPos(id, w, h, nx, ny);
        const int dx = x - nx;
        const int dy = y - ny;
        const int distance = dx * dx + dy * dy;
        if (distance <= bestDistance)
        {
            bestDistance = distance;
            best = static_cast<int>(id);
        }
    }
    return best;
}

void CampaignScreen::update(float dt)
{
    if (!valid_ || dt <= 0.0f)
    {
        return;
    }
    campaign_.tick(dt, rng_);
}

bool CampaignScreen::handleClick(int x, int y, int w, int h)
{
    const int node = nodeAtScreen(x, y, w, h, 22);
    if (node < 0)
    {
        return false;
    }
    selected_ = static_cast<core::TerritoryId>(node);
    return true;
}

void CampaignScreen::adjustTax(int delta)
{
    // The campaign is frozen once it is won or lost: no taxation changes the
    // final board (matching Campaign::tick, which stops advancing).
    if (campaign_.finished())
    {
        return;
    }
    const core::Territory* territory = campaign_.map().find(selected_);
    if (territory == nullptr || territory->owner != core::Campaign::kPlayerSyndicate)
    {
        return;
    }
    const int maxLevel = campaign_.economy().config().maxTaxLevel;
    int level = territory->taxLevel + delta;
    level = std::min(maxLevel, std::max(0, level));
    campaign_.map().setTaxLevel(selected_, level);
}

bool CampaignScreen::recruitSelected()
{
    // A finished campaign is terminal: no new recruits change the final board.
    if (campaign_.finished())
    {
        return false;
    }
    const core::Territory* territory = campaign_.map().find(selected_);
    if (territory == nullptr)
    {
        return false;
    }
    // Fill the lowest-numbered unnamed slot, so recruits build the squad in order.
    for (int slot = 0; slot < core::Equipment::kSquadSlots; ++slot)
    {
        if (!campaign_.agentName(slot).empty())
        {
            continue;
        }
        return !campaign_.recruitAgent(slot, territory->id).empty();
    }
    return false; // the squad is already fully named
}

bool CampaignScreen::deploySelected()
{
    // The squad is the four fixed slots, all of them; the loadout resolved from
    // the bought equipment maps onto the deployed mission's player agents via
    // R1's applyLoadout (issue #312). A finished campaign cannot deploy: the
    // end state is terminal (Campaign::canDeployTo also refuses, but checking
    // here keeps the pending-deployment state clean).
    if (campaign_.finished())
    {
        return false;
    }
    std::vector<int> slots;
    for (int slot = 0; slot < core::Equipment::kSquadSlots; ++slot)
    {
        slots.push_back(slot);
    }
    if (!campaign_.selectMission(selected_, slots))
    {
        return false;
    }

    // Record what to play: the mission id and the bought loadout. The engine
    // layer starts it (Game::startDeployedMission) and calls recordDeployResult
    // when the mission ends. Kept here (not in Game) so the campaign rules stay
    // in this translation unit (docs/FOCUS_AREAS.md §4, Rule 2).
    deployedMissionId_ = campaign_.selectedMissionId();
    deployedMissionPath_ = resolveMissionFile(deployedMissionId_);
    deployedLoadout_ = campaign_.selectedLoadout();
    if (deployedMissionPath_.empty())
    {
        // No file for the mission: do not start a mission that is not the one
        // the territory names. The selection stands; the deploy is a no-op.
        std::fprintf(stderr, "campaign: mission '%s' has no file to deploy\n",
                     deployedMissionId_.c_str());
        deployedMissionId_.clear();
        deployedLoadout_ = core::PlayerLoadout{};
        return false;
    }
    hasDeployed_ = true;
    return true;
}

core::CampaignState CampaignScreen::recordDeployResult(bool won)
{
    hasDeployed_ = false;
    deployedMissionId_.clear();
    deployedMissionPath_.clear();
    deployedLoadout_ = core::PlayerLoadout{};
    campaign_.recordResult(won, core::Campaign::kPlayerSyndicate);
    return campaign_.state();
}

void CampaignScreen::toggleShop()
{
    if (campaign_.finished())
    {
        return; // the end state is terminal: no further spending
    }
    shopOpen_ = !shopOpen_;
    if (shopOpen_)
    {
        shopRow_ = 0;
        shopScroll_ = 0;
    }
}

bool CampaignScreen::handleKey(int sym)
{
    // The shop owns the screen while it is open: navigation and the buy/begin
    // action. Every other key falls through to the global bindings, so Esc still
    // quits the game from the map (the shop consumes Esc to close instead).
    if (shopOpen_)
    {
        // The shop is modal: it consumes every key so a management binding (M, D,
        // -/+) cannot fire behind it.
        if (sym == SDLK_ESCAPE || sym == SDLK_b)
        {
            shopOpen_ = false;
            return true;
        }
        const int rows = static_cast<int>(campaign_.researchTree().size() +
                                          campaign_.catalogue().size());
        if (rows <= 0)
        {
            return true; // an empty catalogue: swallow nav, nothing to move over
        }
        switch (sym)
        {
        case SDLK_UP:
            shopRow_ = (shopRow_ - 1 + rows) % rows;
            return true;
        case SDLK_DOWN:
            shopRow_ = (shopRow_ + 1) % rows;
            return true;
        case SDLK_LEFT:
            shopSlot_ = (shopSlot_ - 1 + core::Equipment::kSquadSlots) % core::Equipment::kSquadSlots;
            return true;
        case SDLK_RIGHT:
            shopSlot_ = (shopSlot_ + 1) % core::Equipment::kSquadSlots;
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        {
            const int player = core::Campaign::kPlayerSyndicate;
            const std::size_t researchCount = campaign_.researchTree().size();
            if (static_cast<std::size_t>(shopRow_) < researchCount)
            {
                const std::string& id = campaign_.researchTree().nodes()[static_cast<std::size_t>(shopRow_)].id;
                campaign_.research().begin(player, id, campaign_.researchTree(),
                                           campaign_.economy());
            }
            else
            {
                const std::size_t index = static_cast<std::size_t>(shopRow_) - researchCount;
                const std::string& id = campaign_.catalogue().items()[index].id;
                // Skip the purchase when the exact item is already in the target
                // slot: equip() would replace it with itself, so charging again
                // would take funds for nothing.
                const std::vector<std::string> slot =
                    campaign_.equipment().loadout(player, shopSlot_);
                if (std::find(slot.begin(), slot.end(), id) == slot.end() &&
                    campaign_.equipment().buy(player, id, campaign_.catalogue(),
                                              campaign_.research(), campaign_.economy()))
                {
                    campaign_.equipment().equip(player, shopSlot_, id, campaign_.catalogue());
                }
            }
            return true;
        }
        default:
            return true; // modal: swallow everything else
        }
    }

    if (sym == SDLK_b)
    {
        toggleShop();
        return true;
    }
    if (sym == SDLK_r)
    {
        // Recruit a named agent from the selected region (issue #509). Consumed
        // here (before the global `R` restart binding) so it acts on the map, not
        // on the frozen tactical board.
        recruitSelected();
        return true;
    }
    return false;
}

std::string CampaignScreen::resolveMissionFile(const std::string& missionId) const
{
    if (missionId.empty())
    {
        return {};
    }
    for (const std::string& candidate : missionFileCandidates(missionId, exeDir_))
    {
        if (core::fileExists(candidate))
        {
            return candidate;
        }
    }
    return {};
}

void CampaignScreen::renderShop(SDL_Renderer* renderer, const BitmapFont& font, int w, int h) const
{
    const int lineH = font.lineHeight();
    const int margin = 12;
    const int player = core::Campaign::kPlayerSyndicate;

    // Full-screen dim so the list reads as a modal over the frozen map.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
    SDL_Rect full{0, 0, w, h};
    SDL_RenderFillRect(renderer, &full);

    const auto& nodes = campaign_.researchTree().nodes();
    const auto& items = campaign_.catalogue().items();
    const int rowCount = static_cast<int>(nodes.size() + items.size());

    // Keep the selected row on screen. Rows are one line each; reserve the top two
    // lines (title + header) and the bottom two (selection detail + hint).
    const int visibleRows = std::max(1, (h - lineH * 5) / lineH);
    if (shopRow_ < shopScroll_)
    {
        shopScroll_ = shopRow_;
    }
    if (shopRow_ >= shopScroll_ + visibleRows)
    {
        shopScroll_ = shopRow_ - visibleRows + 1;
    }
    shopScroll_ = std::max(0, std::min(shopScroll_, std::max(0, rowCount - visibleRows)));

    int y = margin;
    font.draw(renderer, margin, y, "R&D - UP/DOWN MOVE, ENTER ACT, ESC CLOSE", hudstyle::kHudText);
    y += lineH * 2;

    const core::ResearchState& research = campaign_.research().state(player);
    for (int row = shopScroll_; row < rowCount && y < h - lineH * 2; ++row)
    {
        const bool selected = row == shopRow_;
        const std::string marker = selected ? "> " : "  ";
        std::string text;
        SDL_Color color = hudstyle::kHudDim;

        if (row < static_cast<int>(nodes.size()))
        {
            const core::ResearchNode& node = nodes[static_cast<std::size_t>(row)];
            const bool done = research.isCompleted(node.id);
            const bool inProgress = research.inProgress == node.id;
            text = marker + "R&D " + node.name + "  " + std::to_string(static_cast<int>(node.cost));
            if (done)
            {
                text += "  DONE";
                color = hudstyle::kHudGood;
            }
            else if (inProgress)
            {
                text += "  " + std::to_string(static_cast<int>(research.remainingSeconds + 0.999f)) + "s";
                color = hudstyle::kHudText;
            }
            else if (!campaign_.researchTree().isAvailable(node.id, research))
            {
                text += "  LOCKED";
            }
            else if (campaign_.economy().canAfford(player, node.cost))
            {
                color = hudstyle::kHudText;
            }
        }
        else
        {
            const core::EquipmentItem& item =
                items[static_cast<std::size_t>(row) - nodes.size()];
            const bool unlocked =
                item.requiresResearch.empty() || research.isCompleted(item.requiresResearch);
            text = marker + "BUY " + item.name + "  " + std::to_string(static_cast<int>(item.cost));
            if (!unlocked)
            {
                text += "  NEEDS " + item.requiresResearch;
            }
            else if (campaign_.economy().canAfford(player, item.cost))
            {
                color = hudstyle::kHudText;
            }
        }

        font.draw(renderer, margin, y, text, selected ? hudstyle::kHudText : color);
        y += lineH;
    }

    // Selection detail: the action Enter would take, and the target slot.
    y = h - lineH * 2;
    std::string action = "ENTER: ";
    if (shopRow_ < static_cast<int>(nodes.size()))
    {
        action += "FUND RESEARCH";
    }
    else
    {
        // Say what Enter would do: name the same-kind item this purchase replaces
        // (a slot deploys one weapon and one gadget), or that it is already there
        // so Enter is a no-op.
        const core::EquipmentItem& item =
            items[static_cast<std::size_t>(shopRow_) - nodes.size()];
        const std::vector<std::string> slot =
            campaign_.equipment().loadout(player, shopSlot_);
        if (std::find(slot.begin(), slot.end(), item.id) != slot.end())
        {
            action += "ALREADY EQUIPPED IN SLOT " + std::to_string(shopSlot_ + 1);
        }
        else
        {
            action += "BUY + EQUIP TO SLOT " + std::to_string(shopSlot_ + 1);
            for (const std::string& equippedId : slot)
            {
                const core::EquipmentItem* equipped = campaign_.catalogue().find(equippedId);
                if (equipped == nullptr || equipped->kind != item.kind)
                {
                    continue;
                }
                // A drug ceiling replaces only a ceiling for the same bar (a slot
                // carries one per bar, ADR-0043), so only name a same-bar item.
                if (item.kind == core::EquipmentKind::Drug && equipped->bar != item.bar)
                {
                    continue;
                }
                action += " (REPLACE " + equipped->name + ")";
                break;
            }
            action += " (LEFT/RIGHT)";
        }
    }
    font.draw(renderer, margin, y, action, hudstyle::kHudDim);
    y += lineH;
    font.draw(renderer, margin, y,
              "FUNDS: " + std::to_string(static_cast<int>(campaign_.economy().funds(player))),
              hudstyle::kHudGood);
}

void CampaignScreen::render(SDL_Renderer* renderer, const BitmapFont& font, int w, int h) const
{
    if (!valid_)
    {
        return;
    }

    const int lineH = font.lineHeight();
    const int margin = 12;

    // Near-opaque backdrop so the management screen is visually distinct from
    // the tactical board while a hint of the map shows through.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 10, 12, 240);
    SDL_Rect full{0, 0, w, h};
    SDL_RenderFillRect(renderer, &full);

    // Top title bar.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect topBar{0, 0, w, 2 + lineH + margin / 2};
    SDL_RenderFillRect(renderer, &topBar);
    font.draw(renderer, margin, 2, "WORLD MAP", hudstyle::kHudText);

    // The graph: draw every edge once (id < neighbour), then the nodes on top.
    const auto& territories = campaign_.map().territories();
    for (const core::Territory& territory : territories)
    {
        int ax = 0;
        int ay = 0;
        screenPos(static_cast<std::size_t>(territory.id), w, h, ax, ay);
        for (const core::TerritoryId neighbour : territory.neighbours)
        {
            if (neighbour <= territory.id)
            {
                continue;
            }
            int bx = 0;
            int by = 0;
            screenPos(static_cast<std::size_t>(neighbour), w, h, bx, by);
            SDL_SetRenderDrawColor(renderer, 70, 78, 72, 255);
            SDL_RenderDrawLine(renderer, ax, ay, bx, by);
        }
    }

    for (const core::Territory& territory : territories)
    {
        int cx = 0;
        int cy = 0;
        screenPos(static_cast<std::size_t>(territory.id), w, h, cx, cy);
        const bool isSelected = territory.id == selected_;

        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        ownerColor(territory.owner, r, g, b);
        if (isSelected)
        {
            drawDiamond(renderer, cx, cy, 18, 9, hudstyle::kPlayerBodySelected[0],
                        hudstyle::kPlayerBodySelected[1], hudstyle::kPlayerBodySelected[2]);
        }
        drawDiamond(renderer, cx, cy, 14, 7, r, g, b);

        // The name sits just above the node so nodes stay legible when close.
        const int labelX = cx - font.textWidth(territory.name) / 2;
        font.draw(renderer, labelX, cy - 9 - lineH, territory.name,
                  isSelected ? hudstyle::kHudText : hudstyle::kHudDim);
    }

    // Management panel (left).
    const int panelW = std::min(static_cast<int>(static_cast<float>(w) * 0.42f), 360);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 210);
    SDL_Rect panel{0, 2 + lineH + margin / 2, panelW, h};
    SDL_RenderFillRect(renderer, &panel);

    const int player = core::Campaign::kPlayerSyndicate;
    int y = 2 + lineH + margin;
    const std::string syndicate = campaign_.syndicateName().empty() ? "SYNDICATE"
                                                                    : campaign_.syndicateName();
    font.draw(renderer, margin, y, syndicate, hudstyle::kHudText);
    y += lineH;

    const int funds = static_cast<int>(campaign_.economy().funds(player));
    font.draw(renderer, margin, y, "FUNDS: " + std::to_string(funds), hudstyle::kHudGood);
    y += lineH;

    const std::size_t owned = campaign_.map().territoriesOwnedBy(player).size();
    font.draw(renderer, margin, y,
              "TERRITORIES: " + std::to_string(owned) + "/" +
                  std::to_string(campaign_.map().size()),
              hudstyle::kHudText);
    y += lineH * 2;

    const core::Territory* selected = campaign_.map().find(selected_);
    if (selected != nullptr)
    {
        font.draw(renderer, margin, y, "SELECTED: " + selected->name, hudstyle::kHudText);
        y += lineH;
        const char* owner = selected->owner == player ? "YOU"
                            : selected->owner < 0    ? "NEUTRAL"
                                                     : "RIVAL";
        font.draw(renderer, margin, y, std::string("OWNER: ") + owner, hudstyle::kHudDim);
        y += lineH;
        font.draw(renderer, margin, y,
                  "MISSION: " + (selected->missionId.empty() ? std::string("-")
                                                             : selected->missionId),
                  hudstyle::kHudDim);
        y += lineH;
        font.draw(renderer, margin, y, "TAX: " + std::to_string(selected->taxLevel),
                  hudstyle::kHudDim);
        y += lineH;
        // Whether the selected region can be deployed to right now (owned, or on
        // the frontier) — the map is attacked inward from the border, so a neutral
        // behind other neutrals is not yet a target (ADR-0027).
        const bool deployable = campaign_.canDeployTo(selected->id);
        font.draw(renderer, margin, y,
                  deployable ? "STATUS: ATTACK READY" : "STATUS: NOT REACHABLE",
                  deployable ? hudstyle::kHudGood : hudstyle::kHudDim);
        y += lineH * 2;
    }

    // Research status: in-progress node and remaining time, else completed count.
    const core::ResearchState& research = campaign_.research().state(player);
    std::string researchLine = "RESEARCH: IDLE";
    if (!research.inProgress.empty())
    {
        researchLine = "RESEARCH: " + research.inProgress + " (" +
                       std::to_string(static_cast<int>(research.remainingSeconds + 0.999f)) + "s)";
    }
    else if (!research.completed.empty())
    {
        researchLine = "RESEARCH DONE: " + std::to_string(research.completed.size());
    }
    font.draw(renderer, margin, y, researchLine, hudstyle::kHudText);
    y += lineH * 2;

    // The recruited squad (issue #509): one line per slot, showing the fixed name
    // a recruit assigned (or "-" while unnamed). Names are stable and never change
    // once assigned, so this is a read of campaign state, not view state.
    font.draw(renderer, margin, y, "SQUAD", hudstyle::kHudText);
    y += lineH;
    for (int slot = 0; slot < core::Equipment::kSquadSlots; ++slot)
    {
        const std::string& name = campaign_.agentName(slot);
        font.draw(renderer, margin, y,
                  std::to_string(slot + 1) + ": " + (name.empty() ? "-" : name),
                  name.empty() ? hudstyle::kHudDim : hudstyle::kHudText);
        y += lineH;
    }
    y += lineH;

    font.draw(renderer, margin, y, "[M] TACTICAL   [-/+] TAX   [D] DEPLOY   [B] R&D   [R] RECRUIT",
              hudstyle::kHudDim);

    // R&D shop overlay (Milestone B): the research tree and the equipment
    // catalogue in one list, so a player can fund research and buy gear — the
    // middle of the loop, previously reachable only from the self-test. Every
    // rule is in core; this only reads it and forwards the action.
    if (shopOpen_ && !campaign_.finished())
    {
        renderShop(renderer, font, w, h);
    }

    // End-state panel: once the campaign is won or lost the map is frozen
    // (Campaign::tick/recordResult/canDeployTo all refuse), so this is the final
    // board. A centred panel says what happened, how much of the world the player
    // held, and that there is nothing left to do.
    if (campaign_.finished())
    {
        const bool won = campaign_.state() == core::CampaignState::Won;
        // Winning the terminal mission (the original's final confrontation) is a
        // distinct end state: the campaign is over because the climax was won, not
        // because the map was swept. Say so, and in a warmer colour than an
        // ordinary territorial victory (issue #456). The colour is local to this
        // screen; `HudStyle.h` is Render's (Rule 4).
        const bool wonFinal = won && campaign_.wonFinalConfrontation();
        constexpr SDL_Color kFinalVictory{245, 205, 90, 255};

        // Dim the whole board so the panel reads as a modal overlay.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
        SDL_RenderFillRect(renderer, &full);

        const std::string headline =
            wonFinal ? "FINAL CONFRONTATION WON" : (won ? "CAMPAIGN WON" : "CAMPAIGN LOST");
        const std::string detail =
            wonFinal ? "THE SYNDICATE STANDS SUPREME"
                     : (won ? "ALL TERRITORIES SECURED" : "THE SYNDICATE HAS FALLEN");
        const std::string score =
            "TERRITORIES: " + std::to_string(owned) + "/" +
            std::to_string(campaign_.map().size());
        const std::string hint = "PRESS ESC TO QUIT";

        // Four lines drawn two line-heights apart, plus a margin top and bottom.
        const int widest = std::max(std::max(font.textWidth(headline), font.textWidth(detail)),
                                    std::max(font.textWidth(score), font.textWidth(hint)));
        const int panelW = std::min(w - 2 * margin, std::max(240, widest + 4 * margin));
        const int panelH = lineH * 7 + margin * 2;
        SDL_Rect panel{(w - panelW) / 2, (h - panelH) / 2, panelW, panelH};
        SDL_SetRenderDrawColor(renderer, 12, 14, 16, 235);
        SDL_RenderFillRect(renderer, &panel);
        const SDL_Color accent = wonFinal ? kFinalVictory : (won ? hudstyle::kHudGood : hudstyle::kHudBad);
        SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 255);
        SDL_RenderDrawRect(renderer, &panel);

        const int cx = panel.x + panelW / 2;
        int ty = panel.y + margin;
        font.draw(renderer, cx - font.textWidth(headline) / 2, ty, headline, accent);
        ty += lineH * 2;
        font.draw(renderer, cx - font.textWidth(detail) / 2, ty, detail, hudstyle::kHudText);
        ty += lineH * 2;
        font.draw(renderer, cx - font.textWidth(score) / 2, ty, score, hudstyle::kHudDim);
        ty += lineH * 2;
        font.draw(renderer, cx - font.textWidth(hint) / 2, ty, hint, hudstyle::kHudDim);
    }
}

bool CampaignScreen::selfTest()
{
    if (!valid_)
    {
        std::fprintf(stderr, "campaign self-test: no campaign loaded\n");
        return false;
    }

    // Work on a copy so the live campaign the player sees is untouched.
    core::Campaign campaign = campaign_;
    const core::Campaign pristine = campaign_; // for the shop step below
    core::Rng rng(0xC0FFEEu); // local stream: the self-test must not disturb rng_
    const int player = core::Campaign::kPlayerSyndicate;

    // 1. Starting funds seeded by load.
    if (!(campaign.economy().funds(player) > 0.0f))
    {
        std::fprintf(stderr, "campaign self-test: starting funds were not granted\n");
        return false;
    }

    // Find an owned territory (to tax) and an owned one with a mission (to
    // deploy); the shipped map has both.
    core::TerritoryId taxable = -1;
    core::TerritoryId deployable = -1;
    for (const core::Territory& territory : campaign.map().territories())
    {
        if (territory.owner != player)
        {
            continue;
        }
        if (taxable < 0)
        {
            taxable = territory.id;
        }
        if (deployable < 0 && !territory.missionId.empty())
        {
            deployable = territory.id;
        }
    }
    if (taxable < 0 || deployable < 0)
    {
        std::fprintf(stderr, "campaign self-test: shipped map has no player-owned territory\n");
        return false;
    }

    // 2. Taxing an owned territory earns income on the next tick. Tax 1 is below
    // the revolt threshold, so this is a deterministic income check.
    campaign.map().setTaxLevel(taxable, 1);
    const float beforeIncome = campaign.economy().funds(player);
    campaign.tick(campaign.economy().config().tickSeconds, rng);
    if (!(campaign.economy().funds(player) > beforeIncome))
    {
        std::fprintf(stderr, "campaign self-test: taxed territory produced no income\n");
        return false;
    }

    // 3. Fund and complete a research node through the campaign clock.
    campaign.economy().addFunds(player, 100000.0f);
    if (!campaign.research().begin(player, "minigun", campaign.researchTree(),
                                   campaign.economy()))
    {
        std::fprintf(stderr, "campaign self-test: could not begin research\n");
        return false;
    }
    for (int i = 0; i < 30 && !campaign.research().completed(player, "minigun"); ++i)
    {
        campaign.tick(10.0f, rng);
    }
    if (!campaign.research().completed(player, "minigun"))
    {
        std::fprintf(stderr, "campaign self-test: research did not complete\n");
        return false;
    }

    // 4. Buy the researched item and assign it to a squad slot.
    if (!campaign.equipment().buy(player, "minigun", campaign.catalogue(), campaign.research(),
                                  campaign.economy()))
    {
        std::fprintf(stderr, "campaign self-test: could not buy the researched item\n");
        return false;
    }
    campaign.equipment().assign(player, 0, "minigun");
    if (campaign.equipment().loadout(player, 0).empty())
    {
        std::fprintf(stderr, "campaign self-test: loadout assignment was lost\n");
        return false;
    }

    // 5. Select a mission and deploy the full squad.
    std::vector<int> slots;
    for (int slot = 0; slot < core::Equipment::kSquadSlots; ++slot)
    {
        slots.push_back(slot);
    }
    if (!campaign.selectMission(deployable, slots) ||
        campaign.selectedMissionId().empty())
    {
        std::fprintf(stderr, "campaign self-test: mission selection failed\n");
        return false;
    }

    // 6. Win by *playing* the loop: attack a frontier region through a real
    // mission selection and claim it, repeating until the map is won. This is the
    // path a player takes, so the guard fails if the shipped data makes the
    // campaign unwinnable (e.g. a neutral region with no mission, or no
    // player-held neighbour to attack it from).
    const std::size_t total = campaign.map().size();
    bool attackedFrontier = false;
    for (std::size_t guard = 0; guard < total * 4; ++guard)
    {
        core::TerritoryId target = -1;
        for (const core::Territory& territory : campaign.map().territories())
        {
            if (territory.owner != player && campaign.canDeployTo(territory.id))
            {
                target = territory.id;
                break;
            }
        }
        if (target < 0)
        {
            break; // nothing left to attack
        }
        if (!campaign.selectMission(target, slots) || campaign.selectedMissionId().empty())
        {
            std::fprintf(stderr, "campaign self-test: frontier deployment failed\n");
            return false;
        }
        campaign.recordResult(true, player);
        if (campaign.map().owner(target) != player)
        {
            std::fprintf(stderr, "campaign self-test: winning did not claim the territory\n");
            return false;
        }
        attackedFrontier = true;
    }
    if (!attackedFrontier)
    {
        std::fprintf(stderr, "campaign self-test: no attackable territory on the frontier\n");
        return false;
    }
    if (campaign.map().territoriesOwnedBy(player).size() != total ||
        campaign.state() != core::CampaignState::Won)
    {
        std::fprintf(stderr, "campaign self-test: the campaign could not be won by attacking\n");
        return false;
    }

    // 7. The end state is terminal. The won campaign is frozen: it refuses to
    // deploy and its clock stops. Exercise the *screen* guards (deploySelected /
    // adjustTax), which read the same finished() flag, by temporarily forcing the
    // live campaign to the won state, then restore it before returning.
    const core::Campaign savedCampaign = campaign_;
    const core::TerritoryId savedSelected = selected_;
    campaign_ = campaign; // the won copy
    selected_ = 0;
    campaign_.map().setTaxLevel(0, 0);
    const bool refusedDeploy = !deploySelected();
    adjustTax(1);
    const bool refusedTax = campaign_.map().taxLevel(0) == 0;
    campaign_ = savedCampaign;
    selected_ = savedSelected;
    if (!refusedDeploy || !refusedTax)
    {
        std::fprintf(stderr,
                     "campaign self-test: the end state did not freeze the screen "
                     "(deploy=%d tax=%d)\n",
                     refusedDeploy ? 1 : 0, refusedTax ? 1 : 0);
        return false;
    }

    // 8. The R&D shop drives research and purchase through the *player* keys, not
    // just the direct core calls above. This is the middle of the loop, so it must
    // be reachable from the screen (previously only the self-test called it). Do
    // it on the pristine copy: `B` opens, Enter funds the first research row, the
    // campaign clock completes it, then Enter on the minigun item buys and assigns
    // it to the selected slot. A finished campaign cannot open the shop.
    const core::Campaign savedCampaign2 = campaign_;
    const bool savedShopOpen = shopOpen_;
    const int savedShopRow = shopRow_;
    const int savedShopSlot = shopSlot_;
    campaign_ = pristine;
    campaign_.economy().addFunds(player, 100000.0f);
    shopOpen_ = false;
    shopRow_ = 0;
    shopSlot_ = 0;

    bool shopOk = handleKey(SDLK_b) && shopOpen();
    // Row 0 is the first research node (the tree lists before the catalogue).
    shopOk = shopOk && handleKey(SDLK_RETURN) &&
             campaign_.research().state(player).inProgress ==
                 campaign_.researchTree().nodes().front().id;
    for (int i = 0; i < 30 && !campaign_.research().completed(player, "minigun"); ++i)
    {
        campaign_.tick(10.0f, rng);
    }
    // Move down to the minigun catalogue row (research nodes come first) and buy.
    const std::size_t minigunRow =
        campaign_.researchTree().size() +
        static_cast<std::size_t>(std::distance(
            campaign_.catalogue().items().begin(),
            std::find_if(campaign_.catalogue().items().begin(),
                         campaign_.catalogue().items().end(),
                         [](const core::EquipmentItem& item) { return item.id == "minigun"; })));
    while (static_cast<std::size_t>(shopRow_) < minigunRow)
    {
        handleKey(SDLK_DOWN);
    }
    shopOk = shopOk && handleKey(SDLK_RETURN) &&
             !campaign_.equipment().loadout(player, shopSlot_).empty();
    // The upgrade path (issue #346): a second weapon bought into a slot must
    // replace the first, so the squad deploys the better gun instead of the funds
    // being spent and the purchase silently dropped at deploy time. Seed slot 1
    // with the free pistol, then buy the minigun into it through the shop key.
    campaign_.equipment().equip(player, 1, "pistol", campaign_.catalogue());
    shopSlot_ = 1;
    shopOk = shopOk && handleKey(SDLK_RETURN);
    int upgradedWeapons = 0;
    bool upgradedToMinigun = false;
    for (const std::string& equippedId : campaign_.equipment().loadout(player, 1))
    {
        const core::EquipmentItem* equipped = campaign_.catalogue().find(equippedId);
        if (equipped != nullptr && equipped->kind == core::EquipmentKind::Weapon)
        {
            ++upgradedWeapons;
            upgradedToMinigun = equippedId == "minigun";
        }
    }
    shopOk = shopOk && upgradedWeapons == 1 && upgradedToMinigun;
    // A finished campaign cannot open the shop.
    campaign_ = campaign; // the won copy from step 7
    shopOpen_ = false;
    const bool shopRefusedWhenFinished = handleKey(SDLK_b) && !shopOpen();

    campaign_ = savedCampaign2;
    shopOpen_ = savedShopOpen;
    shopRow_ = savedShopRow;
    shopSlot_ = savedShopSlot;
    if (!shopOk || !shopRefusedWhenFinished)
    {
        std::fprintf(stderr,
                     "campaign self-test: the R&D shop did not fund/buy through the "
                     "screen keys (ok=%d frozen=%d)\n",
                     shopOk ? 1 : 0, shopRefusedWhenFinished ? 1 : 0);
        return false;
    }

    // 9. Recruitment assigns a fixed, region-based name (issue #509) through the
    // screen key, not just the core call. On a pristine copy, select an owned
    // region, press `R`, and require every squad slot to fill with a distinct
    // name from that region's pool; a second `R` must not rename anyone.
    const core::Campaign savedCampaign3 = campaign_;
    const core::TerritoryId savedSelected3 = selected_;
    campaign_ = pristine;
    selected_ = taxable;
    const std::vector<std::string>* pool = campaign_.namePools().pool(taxable);
    bool recruitOk = pool != nullptr && !pool->empty();
    std::vector<std::string> recruited;
    for (int slot = 0; recruitOk && slot < core::Equipment::kSquadSlots; ++slot)
    {
        recruitOk = handleKey(SDLK_r) && !campaign_.agentName(slot).empty();
        recruited.push_back(campaign_.agentName(slot));
    }
    // Distinct names across the squad, and each drawn from the region's pool.
    for (std::size_t i = 0; recruitOk && i < recruited.size(); ++i)
    {
        for (std::size_t j = i + 1; recruitOk && j < recruited.size(); ++j)
        {
            recruitOk = recruited[i] != recruited[j];
        }
        if (recruitOk && pool != nullptr)
        {
            recruitOk = std::find(pool->begin(), pool->end(), recruited[i]) != pool->end();
        }
    }
    // A fully-named squad refuses a further recruit, and `R` never renames: the
    // screen key is consumed but recruitSelected() reports no assignment.
    const std::string kept = campaign_.agentName(0);
    handleKey(SDLK_r);
    recruitOk = recruitOk && !recruitSelected() && campaign_.agentName(0) == kept;
    campaign_ = savedCampaign3;
    selected_ = savedSelected3;
    if (!recruitOk)
    {
        std::fprintf(stderr, "campaign self-test: recruitment did not assign region names\n");
        return false;
    }

    // 10. Layout legibility (issue #448): the world map now has twelve regions,
    // so a graph whose BFS rings crowd two nodes on top of each other would draw
    // an unreadable map (and an ambiguous click target — nodeAtScreen's tolerance
    // is 22px). Require a minimum separation in the normalised layout the screen
    // actually uses, so a future world.json cannot quietly collapse the layout.
    // 0.10 in normalised space is ~58px across the ~576px map region at the
    // default 960-wide window, comfortably above the 44px click diameter.
    if (layout_.size() != campaign_.map().size())
    {
        std::fprintf(stderr, "campaign self-test: layout does not cover every region\n");
        return false;
    }
    float closest = 1.0f;
    for (std::size_t i = 0; i < layout_.size(); ++i)
    {
        for (std::size_t j = i + 1; j < layout_.size(); ++j)
        {
            const float dx = layout_[i].nx - layout_[j].nx;
            const float dy = layout_[i].ny - layout_[j].ny;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < closest)
            {
                closest = d;
            }
        }
    }
    if (closest < 0.10f)
    {
        std::fprintf(stderr,
                     "campaign self-test: world-map layout is too crowded "
                     "(closest nodes %.3f < 0.10)\n",
                     static_cast<double>(closest));
        return false;
    }

    return true;
}

void CampaignScreen::applySyndicateIdentity(const std::string& name, const std::string& logo)
{
    if (!valid_)
    {
        return;
    }
    // The rules live in the core (#445): an empty name clears to the authored
    // default, and an unknown logo id falls back to the default art. This is only
    // the engine-side hand-off.
    campaign_.setSyndicateName(name);
    campaign_.setSyndicateLogo(logo);
}

bool CampaignScreen::saveProgress(const std::string& path, std::string& error) const
{
    error.clear();
    if (!valid_)
    {
        error = "no campaign loaded";
        return false;
    }
    // A pure read of the frozen metagame API (golden rule 4): capture the live
    // progress and write it. Nothing in the screen changes.
    const core::CampaignSave save = core::CampaignSave::capture(campaign_);
    return save.saveToFile(path, error);
}

bool CampaignScreen::loadProgress(const std::string& path, std::string& error)
{
    error.clear();
    if (!valid_)
    {
        error = "no campaign loaded";
        return false;
    }
    // The two-step load (ADR-0039): the base campaign is already loaded by init();
    // the save is an overlay that restores the progress on top of it. A missing
    // or corrupt file fails here, before any field is trusted.
    core::CampaignSave save;
    if (!core::CampaignSave::loadFromFile(path, save, error))
    {
        return false;
    }
    if (!save.applyTo(campaign_, error))
    {
        return false;
    }
    // Ownership may have changed hands, so re-select the first territory the
    // player now holds (or -1) — the panel and deploy both read `selected_`.
    selected_ = -1;
    for (const core::Territory& territory : campaign_.map().territories())
    {
        if (territory.owner == core::Campaign::kPlayerSyndicate)
        {
            selected_ = territory.id;
            break;
        }
    }
    return true;
}

bool CampaignScreen::saveLoadSelfTest()
{
    if (!valid_ || campaignFile_.empty())
    {
        std::fprintf(stderr, "save/load self-test: no campaign loaded\n");
        return false;
    }

    const int player = core::Campaign::kPlayerSyndicate;

    // A scratch screen loaded fresh from the base file, so the live campaign the
    // player sees is never touched.
    CampaignScreen scratch;
    if (!scratch.init(campaignFile_, exeDir_))
    {
        std::fprintf(stderr, "save/load self-test: could not reload the base campaign\n");
        return false;
    }

    // Progress it in every field the save carries, so the round-trip has
    // something to lose. Ownership varies (player / rival / neutral) so the save
    // is not merely the base state.
    int index = 0;
    for (const core::Territory& territory : scratch.campaign_.map().territories())
    {
        const int owner = (index % 3 == 0) ? player : (index % 3 == 1 ? 1 : -1);
        scratch.campaign_.map().setOwner(territory.id, owner);
        scratch.campaign_.map().setTaxLevel(territory.id, index % 4);
        ++index;
    }
    scratch.campaign_.economy().addFunds(player, 1234.5f);
    scratch.campaign_.equipment().assign(player, 0, "medkit");
    scratch.campaign_.equipment().assign(player, 1, "targeting-implant");

    const core::CampaignSave expected = core::CampaignSave::capture(scratch.campaign_);

    // Save it to a uniquely named temp file, so a concurrent run cannot collide.
    // Plain C++ only: <filesystem> is deliberately avoided in the game layer
    // (AGENTS.md §1b, the low-spec size budget) — $TMPDIR with /tmp as the
    // fallback, as the rest of the tree does.
    static int counter = 0;
    const char* tmpDir = std::getenv("TMPDIR");
    const std::string base =
        std::string(tmpDir != nullptr && tmpDir[0] != '\0' ? tmpDir : "/tmp") +
        "/fossynd_save_load_" + std::to_string(++counter);
    const std::string file = base + ".json";
    const std::string corrupt = base + "_bad.json";

    std::string error;
    if (!scratch.saveProgress(file, error))
    {
        std::fprintf(stderr, "save/load self-test: save failed: %s\n", error.c_str());
        std::remove(file.c_str());
        return false;
    }

    // Load it into a *fresh* base campaign, as a real `--load` does.
    CampaignScreen reloaded;
    if (!reloaded.init(campaignFile_, exeDir_))
    {
        std::fprintf(stderr, "save/load self-test: could not reload a fresh base\n");
        std::remove(file.c_str());
        return false;
    }
    const core::CampaignSave freshBase = core::CampaignSave::capture(reloaded.campaign_);
    if (freshBase == expected)
    {
        std::fprintf(stderr,
                     "save/load self-test: the progressed save equals the base state\n");
        std::remove(file.c_str());
        return false;
    }

    bool ok = true;
    if (!reloaded.loadProgress(file, error))
    {
        std::fprintf(stderr, "save/load self-test: load failed: %s\n", error.c_str());
        ok = false;
    }
    else if (core::CampaignSave::capture(reloaded.campaign_) != expected)
    {
        std::fprintf(stderr,
                     "save/load self-test: the loaded progress does not match the save\n");
        ok = false;
    }

    // A corrupt file must be rejected, not silently loaded with fields missing.
    // Written with stdio, not <fstream>: the game layer avoids the ostream
    // machinery to protect the low-spec size budget (AGENTS.md §1b).
    if (std::FILE* bad = std::fopen(corrupt.c_str(), "wb"))
    {
        const char text[] = "{ this is not a save";
        const std::size_t written = std::fwrite(text, 1, sizeof(text) - 1, bad);
        (void)written;
        std::fclose(bad);
    }
    std::string corruptError;
    if (reloaded.loadProgress(corrupt, corruptError))
    {
        std::fprintf(stderr, "save/load self-test: a corrupt save was accepted\n");
        ok = false;
    }
    // A missing file must be rejected too (the #259 fail-fast pattern).
    std::string missingError;
    if (reloaded.loadProgress("/no/such/fossynd-save.json", missingError))
    {
        std::fprintf(stderr, "save/load self-test: a missing save was accepted\n");
        ok = false;
    }

    std::remove(file.c_str());
    std::remove(corrupt.c_str());
    return ok;
}

} // namespace fossynd::game
