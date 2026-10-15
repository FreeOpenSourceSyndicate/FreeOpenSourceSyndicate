/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/CampaignScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <queue>

#include "core/data/ContentPaths.h"
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

bool CampaignScreen::deploySelected()
{
    // The squad is the four fixed slots, all of them; deployment loadout mapping
    // into the mission is Tactical/Content's follow-up (docs/METAGAME.md §7, R1),
    // so this records the selection the same way the core contract does.
    std::vector<int> slots;
    for (int slot = 0; slot < core::Equipment::kSquadSlots; ++slot)
    {
        slots.push_back(slot);
    }
    return campaign_.selectMission(selected_, slots);
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

    font.draw(renderer, margin, y, "[M] TACTICAL   [-/+] TAX   [D] DEPLOY", hudstyle::kHudDim);

    if (campaign_.state() != core::CampaignState::Running)
    {
        const bool won = campaign_.state() == core::CampaignState::Won;
        const std::string message = won ? "CAMPAIGN WON" : "CAMPAIGN LOST";
        font.draw(renderer, w / 2 - font.textWidth(message) / 2, h / 2, message,
                  won ? hudstyle::kHudGood : hudstyle::kHudBad);
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

    return true;
}

} // namespace fossynd::game
