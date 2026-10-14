/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

#include "SDL.h"

// Shared presentation constants for the engine layer's UI (HUD + title card).
// View-only: nothing here may influence the simulation (AGENTS.md golden rule
// 4). Split out of Game.cpp (issue #69) so the HUD and title-card translation
// units share one palette instead of duplicating it.
namespace fossynd::game::hudstyle
{

// The on-screen codename for the current release, shown on the title card.
// Bumped each cut in one place (like FOSSYND_VERSION); the English value is
// mirrored in assets/lang/en.json `app.tagline`, which can override it for
// translation (issue #48).
constexpr const char* kReleaseCodename = "RECRUITMENT";

// Agent body colours (the in-game marker and the title-card badge).
constexpr std::uint8_t kPlayerBody[3] = {40, 170, 60};
constexpr std::uint8_t kPlayerBodySelected[3] = {90, 230, 130};
constexpr std::uint8_t kEnemyBody[3] = {200, 55, 45};
constexpr std::uint8_t kEnemyBodyFlash[3] = {255, 210, 90};
constexpr std::uint8_t kCivilianBody[3] = {205, 200, 180};
constexpr std::uint8_t kFollowerBody[3] = {90, 205, 230};

// Queued-route overlay (issue #89): the polyline and its numbered waypoint dots.
constexpr std::uint8_t kWaypointLine[3] = {255, 236, 120};
constexpr std::uint8_t kWaypointDot[3] = {60, 110, 220};

// HUD text colours.
constexpr SDL_Color kHudText{225, 235, 225, 255};
constexpr SDL_Color kHudGood{120, 230, 140, 255};
constexpr SDL_Color kHudBad{235, 110, 90, 255};
constexpr SDL_Color kHudDim{150, 160, 150, 255};

// The objective label is tinted "urgent" near the end of the objective — a
// survive timer within this many seconds, or this few enemies left to kill.
// This highlights the endgame (the outcome is about to be decided).
constexpr int kUrgentSurviveSeconds = 5;
constexpr int kUrgentEnemiesLeft = 1;

// Replaces every `{name}` token in `text` with the matching argument, in one
// left-to-right pass so a substituted value is never rescanned. Keeps the
// localised format strings plain data (ADR-0016).
inline std::string substitute(std::string text,
                              std::initializer_list<std::pair<const char*, std::string>> values)
{
    std::size_t pos = 0;
    while (pos < text.size())
    {
        const std::size_t open = text.find('{', pos);
        if (open == std::string::npos)
        {
            break;
        }
        const std::size_t close = text.find('}', open + 1);
        if (close == std::string::npos)
        {
            break;
        }
        const std::string key = text.substr(open + 1, close - open - 1);
        bool replaced = false;
        for (const auto& value : values)
        {
            if (key == value.first)
            {
                text.replace(open, close - open + 1, value.second);
                pos = open + value.second.size();
                replaced = true;
                break;
            }
        }
        if (!replaced)
        {
            pos = close + 1;
        }
    }
    return text;
}

} // namespace fossynd::game::hudstyle
