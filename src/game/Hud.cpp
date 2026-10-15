/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "game/Text.h"

#ifndef FOSSYND_VERSION
#define FOSSYND_VERSION "0.0.0-dev"
#endif

namespace fossynd::game
{
namespace
{

// HUD text colours.
constexpr SDL_Color kHudText{225, 235, 225, 255};
constexpr SDL_Color kHudGood{120, 230, 140, 255};
constexpr SDL_Color kHudBad{235, 110, 90, 255};
constexpr SDL_Color kHudDim{150, 160, 150, 255};

// The objective label is tinted "urgent" near the end of the objective - a
// survive timer within this many seconds, or this few enemies left to kill.
// This highlights the endgame (the outcome is about to be decided).
constexpr int kUrgentSurviveSeconds = 5;
constexpr int kUrgentEnemiesLeft = 1;

} // namespace

void Game::renderHud()
{
    const int margin = 8;
    const int lineH = font_.lineHeight();

    // Semi-transparent top bar.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 170);
    SDL_Rect topBar{0, 0, screenWidth_, lineH + margin};
    SDL_RenderFillRect(renderer_, &topBar);

    font_.draw(renderer_, margin, margin / 2,
               tr("app.title", "FOSSYND") + " " +
                   substitute(tr("app.version", "v{version}"), {{"version", FOSSYND_VERSION}}),
               kHudText);
    std::string objective;
    bool urgent = false;
    switch (mission_.objective().kind)
    {
    case core::ObjectiveSpec::Kind::ReachTile:
        objective = substitute(tr("hud.objective.reach", "OBJECTIVE: REACH TILE ({x},{y})"),
                               {{"x", std::to_string(mission_.objective().tileX)},
                                {"y", std::to_string(mission_.objective().tileY)}});
        break;
    case core::ObjectiveSpec::Kind::Survive:
    {
        const int remaining =
            static_cast<int>(mission_.objective().seconds - mission_.elapsedSeconds() + 0.999f);
        urgent = remaining <= kUrgentSurviveSeconds;
        objective =
            substitute(tr("hud.objective.survive", "OBJECTIVE: SURVIVE {seconds}s"),
                       {{"seconds", std::to_string(remaining > 0 ? remaining : 0)}});
        break;
    }
    case core::ObjectiveSpec::Kind::EliminateAll:
    default:
    {
        const int left = static_cast<int>(mission_.aliveCount(core::Team::Enemy));
        urgent = left <= kUrgentEnemiesLeft;
        objective = substitute(tr("hud.objective.eliminate",
                                  "OBJECTIVE: ELIMINATE ALL {count} ENEMY AGENTS"),
                               {{"count", std::to_string(left)}});
        break;
    }
    }
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
        prefix = substitute(tr("hud.followers", "FOLLOWERS: {count}"),
                            {{"count", std::to_string(followers)}}) +
                 "   ";
    }
    if (mission_.civilianKills() > 0)
    {
        prefix = substitute(tr("hud.collateral", "COLLATERAL: {count}"),
                            {{"count", std::to_string(mission_.civilianKills())}}) +
                 "   " + prefix;
    }
    const std::string controls = tr("hud.controls",
                                    "[L] MOVE   [CLICK ENEMY/RIGHT] ATTACK   [1-4] GROUP   [A]LL   "
                                    "[P] PERSUADE   [F] FREE   [R] RESTART");
    // The objective label is tinted by state; the static controls hint stays dim.
    // Together they used to overflow the default width, which right-aligned the
    // line and pushed the objective (its leftmost part) off-screen, so show the
    // hint only when it fits and never draw past the left margin.
    const SDL_Color objectiveColor = urgent ? kHudBad : kHudDim;
    const std::string label = prefix + objective;
    const bool showControls =
        font_.textWidth(label + "   " + controls) <= screenWidth_ - 2 * margin;
    const std::string tail = showControls ? "   " + controls : std::string();
    int x = std::max(margin, screenWidth_ - margin - font_.textWidth(label + tail));
    font_.draw(renderer_, x, margin / 2, label, objectiveColor);
    if (showControls)
    {
        font_.draw(renderer_, x + font_.textWidth(label), margin / 2, tail, kHudDim);
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
    font_.draw(renderer_, margin, y, tr("hud.squad", "SQUAD"), kHudText);
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
            substitute(tr("hud.agentRow", "{marker}{name}  {weapon}  HP {hp}"),
                       {{"marker", marker},
                        {"name", agent.name()},
                        {"weapon", agent.weapon().name},
                        {"hp", std::to_string(hp)}}) +
            groupTag;
        const SDL_Color color = agent.isAlive() ? (isSelected ? kHudText : kHudDim) : kHudDim;
        font_.draw(renderer_, margin, y, row, color);
        y += lineH;
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

        const SDL_Color color = won ? kHudGood : kHudBad;
        font_.draw(renderer_, cx - font_.textWidth(message) / 2, screenHeight_ / 2 - lineH * 3 / 2,
                   message, color);
        font_.draw(renderer_, cx - font_.textWidth(sub) / 2, screenHeight_ / 2 + lineH / 2, sub,
                   kHudText);
    }
}

} // namespace fossynd::game
