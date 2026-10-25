/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "game/HudStyle.h"

// The build sets this from the project() version (src/CMakeLists.txt); the
// fallback keeps a plain compile (no CMake definitions) working.
#ifndef FOSSYND_VERSION
#define FOSSYND_VERSION "0.0.0-dev"
#endif

// HUD rendering (issue #69): the objective banner, squad roster, and end-of-
// mission banner. Split out of Game.cpp so the engine layer stops growing one
// file. View-only: it reads core state and draws; it never mutates the sim.
namespace fossynd::game
{

std::string Game::objectiveBannerText(bool& urgent) const
{
    urgent = false;
    std::string objective;
    switch (mission_.objective().kind)
    {
    case core::ObjectiveSpec::Kind::ReachTile:
        objective = hudstyle::substitute(tr("hud.objective.reach", "OBJECTIVE: REACH TILE ({x},{y})"),
                               {{"x", std::to_string(mission_.objective().tileX)},
                                {"y", std::to_string(mission_.objective().tileY)}});
        break;
    case core::ObjectiveSpec::Kind::Survive:
    {
        const int remaining =
            static_cast<int>(mission_.objective().seconds - mission_.elapsedSeconds() + 0.999f);
        urgent = remaining <= hudstyle::kUrgentSurviveSeconds;
        objective =
            hudstyle::substitute(tr("hud.objective.survive", "OBJECTIVE: SURVIVE {seconds}s"),
                       {{"seconds", std::to_string(remaining > 0 ? remaining : 0)}});
        break;
    }
    case core::ObjectiveSpec::Kind::Escort:
    {
        // Name the escorted unit when it is known; fall back to a generic label
        // so a hand-built spec without a valid index still renders.
        const int index = mission_.objective().agentIndex;
        std::string name = "AGENT";
        if (index >= 0 && static_cast<std::size_t>(index) < mission_.agents().size())
        {
            name = mission_.agents()[static_cast<std::size_t>(index)].name();
        }
        objective = hudstyle::substitute(
            tr("hud.objective.escort", "OBJECTIVE: ESCORT {name} TO ({x},{y})"),
            {{"name", name},
             {"x", std::to_string(mission_.objective().tileX)},
             {"y", std::to_string(mission_.objective().tileY)}});
        break;
    }
    case core::ObjectiveSpec::Kind::Destroy:
        objective = hudstyle::substitute(tr("hud.objective.destroy", "OBJECTIVE: DESTROY ({x},{y})"),
                               {{"x", std::to_string(mission_.objective().tileX)},
                                {"y", std::to_string(mission_.objective().tileY)}});
        break;
    case core::ObjectiveSpec::Kind::Persuade:
    {
        // Show how many more followers are still needed, mirroring the
        // "enemies left" counter of the eliminate objective.
        std::size_t recruited = 0;
        for (std::size_t i = 0; i < mission_.agents().size(); ++i)
        {
            recruited += mission_.followerCount(i);
        }
        const int target = mission_.objective().targetCount < 1 ? 1
                                                                : mission_.objective().targetCount;
        const int remaining = target - static_cast<int>(recruited);
        objective = hudstyle::substitute(
            tr("hud.objective.persuade", "OBJECTIVE: RECRUIT {count} FOLLOWERS"),
            {{"count", std::to_string(remaining > 0 ? remaining : 0)}});
        break;
    }
    case core::ObjectiveSpec::Kind::EliminateAll:
    default:
    {
        const int left = static_cast<int>(mission_.aliveCount(core::Team::Enemy));
        urgent = left <= hudstyle::kUrgentEnemiesLeft;
        objective = hudstyle::substitute(tr("hud.objective.eliminate",
                                  "OBJECTIVE: ELIMINATE ALL {count} ENEMY AGENTS"),
                               {{"count", std::to_string(left)}});
        break;
    }
    }
    return objective;
}

void Game::renderHud()
{
    const int margin = 8;
    const int lineH = font_.lineHeight();

    // Semi-transparent top bar.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 170);
    SDL_Rect topBar{0, 0, screenWidth_, lineH + margin};
    SDL_RenderFillRect(renderer_, &topBar);

    bool urgent = false;
    const std::string objective = objectiveBannerText(urgent);
    std::size_t followers = 0;
    for (std::size_t i = 0; i < mission_.agents().size(); ++i)
    {
        followers += mission_.followerCount(i);
    }
    // Followers/collateral are status prefixes; the controls hint is static
    // chrome. Both stay dim so only the objective label carries the urgency
    // tint.
    std::string prefix;
    if (followers > 0)
    {
        prefix = hudstyle::substitute(tr("hud.followers", "FOLLOWERS: {count}"),
                            {{"count", std::to_string(followers)}}) +
                 "   ";
    }
    if (mission_.civilianKills() > 0)
    {
        prefix = hudstyle::substitute(tr("hud.collateral", "COLLATERAL: {count}"),
                            {{"count", std::to_string(mission_.civilianKills())}}) +
                 "   " + prefix;
    }
    const std::string controls = tr("hud.controls",
                                    "[SHIFT+CLICK] QUEUE   [CLICK ENEMY/RIGHT] ATTACK   [1-4] GROUP   [A]LL   "
                                    "[P] PERSUADE   [F] FREE   [H] MEDKIT   [S] SHIELD   [C] SCAN   [B] DRUGS   [/] BAR   ;/' ADJUST   [E] HIJACK   [R] RESTART");
    // The objective label is tinted by state; the static controls hint stays dim.
    // Together they used to overflow the default width, which right-aligned the
    // line and pushed the objective (its leftmost part) off-screen, so show the
    // hint only when it fits and never draw past the left margin.
    const SDL_Color objectiveColor = urgent ? hudstyle::kHudBad : hudstyle::kHudDim;
    const std::string label = prefix + objective;
    const bool showControls =
        font_.textWidth(label + "   " + controls) <= screenWidth_ - 2 * margin;
    const std::string tail = showControls ? "   " + controls : std::string();
    const int objectiveX = std::max(margin, screenWidth_ - margin - font_.textWidth(label + tail));

    // Top-left: the version, then the mission name (issue #187) so the current
    // mission is visible in-game, not just in the window title (#176). The name
    // is dropped rather than allowed to overlap the right-aligned objective on a
    // narrow window; it is a status prefix, so it stays dim like the others.
    const std::string title = tr("app.title", "FOSSYND") + " " +
                              hudstyle::substitute(tr("app.version", "v{version}"),
                                                   {{"version", FOSSYND_VERSION}});
    std::string titleLine = title;
    if (!missionName_.empty())
    {
        const std::string withName = title + " - " + missionName_;
        if (font_.textWidth(withName) <= objectiveX - margin - 8)
        {
            titleLine = withName;
        }
    }
    font_.draw(renderer_, margin, margin / 2, titleLine, hudstyle::kHudText);

    font_.draw(renderer_, objectiveX, margin / 2, label, objectiveColor);
    if (showControls)
    {
        font_.draw(renderer_, objectiveX + font_.textWidth(label), margin / 2, tail, hudstyle::kHudDim);
    }

    // Squad panel (bottom-left).
    const auto& agents = mission_.agents();
    int panelLines = 1;
    for (const core::Agent& agent : agents)
    {
        if (agent.team() == core::Team::Player)
        {
            ++panelLines;
        }
    }
    const int panelH = lineH * panelLines + margin;
    const int panelY = screenHeight_ - panelH;

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 170);
    SDL_Rect panel{0, panelY, 320, panelH};
    SDL_RenderFillRect(renderer_, &panel);

    int y = panelY + margin / 2;
    font_.draw(renderer_, margin, y, tr("hud.squad", "SQUAD"), hudstyle::kHudText);
    y += lineH;
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        const core::Agent& agent = agents[i];
        if (agent.team() != core::Team::Player)
        {
            continue;
        }
        const bool isSelected =
            std::find(selected_.begin(), selected_.end(), i) != selected_.end();
        const int hp = static_cast<int>(std::ceil(agent.healthFraction() * 100.0f));
        const std::string marker = isSelected ? "> " : "  ";
        // Show which control group(s) the agent belongs to (issue #33), so the
        // assignment is visible without recalling the group.
        std::string groupTag;
        for (int slot = 0; slot < kControlGroups; ++slot)
        {
            if (std::find(controlGroups_[slot].begin(), controlGroups_[slot].end(), i) !=
                controlGroups_[slot].end())
            {
                groupTag += " [" + std::to_string(slot + 1) + "]";
            }
        }
        std::string row =
            hudstyle::substitute(tr("hud.agentRow", "{marker}{name}  {weapon}  HP {hp}"),
                       {{"marker", marker},
                        {"name", agent.name()},
                        {"weapon", agent.weapon().name},
                        {"hp", std::to_string(hp)}}) +
            groupTag;
        const SDL_Color color = agent.isAlive() ? (isSelected ? hudstyle::kHudText : hudstyle::kHudDim) : hudstyle::kHudDim;
        font_.draw(renderer_, margin, y, row, color);
        y += lineH;
    }

    // Campaign management screen hint (Milestone B): shown only when a campaign
    // loaded, so `M` is discoverable from the tactical view. A plain literal, not
    // an i18n key (localisation beyond English is out of scope, ADR-0024). Placed
    // bottom-right, clear of the squad panel (bottom-left) and the minimap
    // (top-right); skipped on a narrow window where the panel fills the width.
    if (campaignValid())
    {
        const std::string hint = "M MAP";
        const int hintW = font_.textWidth(hint);
        if (screenWidth_ - margin - hintW > 320 + margin)
        {
            font_.draw(renderer_, screenWidth_ - margin - hintW, screenHeight_ - lineH - 2, hint,
                       hudstyle::kHudDim);
        }
    }

    // Centre banner when the mission ends.
    if (mission_.state() != core::MissionState::Running)
    {
        const bool won = mission_.state() == core::MissionState::Won;
        const std::string message = tr(won ? "state.won" : "state.lost", mission_.stateName());
        const std::string sub = won ? tr("hud.winSub", "PRESS R TO RUN IT AGAIN")
                                    : tr("hud.loseSub", "PRESS R TO RETRY");
        const int cx = screenWidth_ / 2;

        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 190);
        SDL_Rect banner{0, screenHeight_ / 2 - lineH * 2, screenWidth_, lineH * 4};
        SDL_RenderFillRect(renderer_, &banner);

        const SDL_Color color = won ? hudstyle::kHudGood : hudstyle::kHudBad;
        font_.draw(renderer_, cx - font_.textWidth(message) / 2, screenHeight_ / 2 - lineH * 3 / 2,
                   message, color);
        font_.draw(renderer_, cx - font_.textWidth(sub) / 2, screenHeight_ / 2 + lineH / 2, sub,
                   hudstyle::kHudText);
    }

    // Pause banner (issue #210): a distinct band so a frozen board is obvious.
    // Drawn only mid-mission; the end-of-mission banner above wins otherwise.
    if (paused_ && mission_.state() == core::MissionState::Running)
    {
        const std::string label = tr("state.paused", "PAUSED");
        const int cx = screenWidth_ / 2;
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 170);
        SDL_Rect band{0, screenHeight_ / 2 - lineH, screenWidth_, lineH * 2};
        SDL_RenderFillRect(renderer_, &band);
        font_.draw(renderer_, cx - font_.textWidth(label) / 2, screenHeight_ / 2 - lineH / 2,
                   label, hudstyle::kHudText);
    }
}

} // namespace fossynd::game
