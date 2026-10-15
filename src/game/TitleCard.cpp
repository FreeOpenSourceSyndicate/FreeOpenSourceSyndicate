/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Game.h"

#include <cmath>
#include <string>

#include "game/HudStyle.h"

// The build sets this from the project() version (src/CMakeLists.txt); the
// fallback keeps a plain compile (no CMake definitions) working.
#ifndef FOSSYND_VERSION
#define FOSSYND_VERSION "0.0.0-dev"
#endif

// The authored pixel-art title/attract screen (issue #69): split out of
// Game.cpp. View-only; the badge reuses the in-game marker shape so it reads as
// the same art.
namespace fossynd::game
{

void Game::drawTitleDiamond(float cx, float cy, float radius, std::uint8_t r, std::uint8_t g,
                            std::uint8_t b)
{
    // A flat isometric diamond drawn straight into the renderer. `radius` is in
    // the same 0..1 world units as fillDiamondWorld, so the title card reuses the
    // in-game marker shape and reads as the same art.
    const float halfW = radius * (kTileWidth * 0.5f);
    const float halfH = radius * (kTileHeight * 0.5f);
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    const int minY = static_cast<int>(cy - halfH);
    const int maxY = static_cast<int>(cy + halfH);
    for (int py = minY; py <= maxY; ++py)
    {
        const float dy = (static_cast<float>(py) - cy) / halfH;
        const float span = 1.0f - std::fabs(dy);
        if (span <= 0.0f)
        {
            continue;
        }
        SDL_Rect line;
        line.x = static_cast<int>(cx - halfW * span);
        line.w = static_cast<int>(2.0f * halfW * span) + 1;
        line.y = py;
        line.h = 1;
        SDL_RenderFillRect(renderer_, &line);
    }
}

void Game::renderTitleCard()
{
    const int w = screenWidth_;
    const int h = screenHeight_;
    const int lineH = font_.lineHeight();

    // Dark, slightly translucent backdrop so the map stays faintly visible.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 6, 7, 9, 232);
    SDL_Rect full{0, 0, w, h};
    SDL_RenderFillRect(renderer_, &full);

    // A row of squad diamonds (the in-game player marker) as the badge.
    const float cx = static_cast<float>(w) * 0.5f;
    const float badgeY = static_cast<float>(h) * 0.24f;
    const float spacing = static_cast<float>(w) * 0.05f;
    drawTitleDiamond(cx - spacing * 1.5f, badgeY, 0.42f, hudstyle::kPlayerBody[0], hudstyle::kPlayerBody[1],
                     hudstyle::kPlayerBody[2]);
    drawTitleDiamond(cx - spacing * 0.5f, badgeY - lineH, 0.42f, hudstyle::kPlayerBodySelected[0],
                     hudstyle::kPlayerBodySelected[1], hudstyle::kPlayerBodySelected[2]);
    drawTitleDiamond(cx + spacing * 0.5f, badgeY, 0.42f, hudstyle::kPlayerBody[0], hudstyle::kPlayerBody[1],
                     hudstyle::kPlayerBody[2]);
    drawTitleDiamond(cx + spacing * 1.5f, badgeY - lineH, 0.42f, hudstyle::kPlayerBodySelected[0],
                     hudstyle::kPlayerBodySelected[1], hudstyle::kPlayerBodySelected[2]);

    // Title and version, centred.
    const std::string title = tr("app.title", "FOSSYND");
    const std::string version =
        hudstyle::substitute(tr("app.version", "v{version}"), {{"version", FOSSYND_VERSION}});
    const std::string subtitle = tr("app.tagline", hudstyle::kReleaseCodename);

    int y = static_cast<int>(static_cast<float>(h) * 0.40f);
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(title) / 2, y, title, hudstyle::kHudText);
    y += lineH + 2;
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(subtitle) / 2, y, subtitle,
               hudstyle::kHudGood);
    y += lineH + 2;
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(version) / 2, y, version, hudstyle::kHudDim);

    // A blinking prompt (driven by the elapsed time, no RNG).
    const bool blinkOn = std::fmod(titleCardTime_, 1.0f) < 0.6f;
    if (blinkOn)
    {
        const std::string prompt = tr("title.prompt", "PRESS ANY KEY");
        y += lineH * 2;
        font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(prompt) / 2, y, prompt,
                   hudstyle::kHudText);
    }

    // Controls, centred lower down.
    const std::string controls[] = {
        tr("title.controls.move", "CLICK  MOVE THE SQUAD"),
        tr("title.controls.attack", "RIGHT-CLICK  FOCUS FIRE"),
        tr("title.controls.select", "1-4 RECALL  CTRL+1-4 SET  A  ALL"),
        tr("title.controls.misc", "ARROWS  SCROLL   SPACE  PAUSE   R  RESTART   ESC  QUIT"),
    };
    int cy = static_cast<int>(static_cast<float>(h) * 0.68f);
    for (const std::string& line : controls)
    {
        font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(line) / 2, cy, line, hudstyle::kHudDim);
        cy += lineH;
    }
    // The campaign management screen has its own controls (Milestone B); advertise
    // the way in only when a campaign actually loaded. A plain literal, not an
    // i18n key (localisation beyond English is out of scope, ADR-0024).
    if (campaignValid())
    {
        const std::string mapLine = "M  WORLD MAP   -/+  TAX   D  DEPLOY";
        font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(mapLine) / 2, cy, mapLine,
                   hudstyle::kHudDim);
        cy += lineH;
    }

    // Small footer tag, so a screenshot self-identifies the build.
    const std::string footer = hudstyle::substitute(
        tr("title.footer", "FOSSYND v{version}  -  FREE OPEN SOURCE SYNDICATE"),
        {{"version", FOSSYND_VERSION}});
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(footer) / 2, h - lineH - 6, footer,
               hudstyle::kHudDim);
}

} // namespace fossynd::game
