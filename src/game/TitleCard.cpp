/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Game.h"

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

// The on-screen codename for the current release, shown on the title card.
// Bumped each cut in one place (like FOSSYND_VERSION); the English value is
// mirrored in assets/lang/en.json `app.tagline`, which can override it for
// translation (issue #48).
constexpr const char* kReleaseCodename = "RECRUITMENT";

// Agent body colours, shared with the in-game marker art (see Game.cpp).
constexpr std::uint8_t kPlayerBody[3] = {40, 170, 60};
constexpr std::uint8_t kPlayerBodySelected[3] = {90, 230, 130};

// Title-card text colours.
constexpr SDL_Color kHudText{225, 235, 225, 255};
constexpr SDL_Color kHudGood{120, 230, 140, 255};
constexpr SDL_Color kHudDim{150, 160, 150, 255};

} // namespace

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
    drawTitleDiamond(cx - spacing * 1.5f, badgeY, 0.42f, kPlayerBody[0], kPlayerBody[1],
                     kPlayerBody[2]);
    drawTitleDiamond(cx - spacing * 0.5f, badgeY - lineH, 0.42f, kPlayerBodySelected[0],
                     kPlayerBodySelected[1], kPlayerBodySelected[2]);
    drawTitleDiamond(cx + spacing * 0.5f, badgeY, 0.42f, kPlayerBody[0], kPlayerBody[1],
                     kPlayerBody[2]);
    drawTitleDiamond(cx + spacing * 1.5f, badgeY - lineH, 0.42f, kPlayerBodySelected[0],
                     kPlayerBodySelected[1], kPlayerBodySelected[2]);

    // Title and version, centred.
    const std::string title = tr("app.title", "FOSSYND");
    const std::string version =
        substitute(tr("app.version", "v{version}"), {{"version", FOSSYND_VERSION}});
    const std::string subtitle = tr("app.tagline", kReleaseCodename);

    int y = static_cast<int>(static_cast<float>(h) * 0.40f);
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(title) / 2, y, title, kHudText);
    y += lineH + 2;
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(subtitle) / 2, y, subtitle,
               kHudGood);
    y += lineH + 2;
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(version) / 2, y, version, kHudDim);

    // A blinking prompt (driven by the elapsed time, no RNG).
    const bool blinkOn = std::fmod(titleCardTime_, 1.0f) < 0.6f;
    if (blinkOn)
    {
        const std::string prompt = tr("title.prompt", "PRESS ANY KEY");
        y += lineH * 2;
        font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(prompt) / 2, y, prompt,
                   kHudText);
    }

    // Controls, centred lower down.
    const std::string controls[] = {
        tr("title.controls.move", "CLICK / L  MOVE THE SQUAD"),
        tr("title.controls.attack", "RIGHT-CLICK  FOCUS FIRE"),
        tr("title.controls.select", "1-4 RECALL  CTRL+1-4 SET  A  ALL"),
        tr("title.controls.misc", "ARROWS  SCROLL   R  RESTART   ESC  QUIT"),
    };
    int cy = static_cast<int>(static_cast<float>(h) * 0.68f);
    for (const std::string& line : controls)
    {
        font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(line) / 2, cy, line, kHudDim);
        cy += lineH;
    }

    // Small footer tag, so a screenshot self-identifies the build.
    const std::string footer = substitute(
        tr("title.footer", "FOSSYND v{version}  -  FREE OPEN SOURCE SYNDICATE"),
        {{"version", FOSSYND_VERSION}});
    font_.draw(renderer_, static_cast<int>(cx) - font_.textWidth(footer) / 2, h - lineH - 6, footer,
               kHudDim);
}

} // namespace fossynd::game
