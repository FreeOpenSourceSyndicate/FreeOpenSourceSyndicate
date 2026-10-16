/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <vector>

#include "SDL.h"

#include "core/sim/Mission.h"
#include "core/world/TileGrid.h"

namespace fossynd::game
{

// A whole-board overview drawn over a corner of the HUD (Phase 2).
//
// The static terrain is baked once into a small texture (one texel per tile,
// magnified by `pixelScale`); only the agent blips are drawn per frame, so the
// per-frame cost stays tiny on old hardware (AGENTS.md 1b). This is engine-layer
// presentation only: it reads core state and never writes it, and its blip state
// never feeds the simulation (AGENTS.md golden rule 4).
class Minimap
{
public:
    Minimap() = default;
    ~Minimap();

    Minimap(const Minimap&) = delete;
    Minimap& operator=(const Minimap&) = delete;

    // Bakes the terrain texture from `grid`. `pixelScale` magnifies one texel per
    // tile (3 for the default window, 2 for the low-spec profile). Returns false
    // on failure; `valid()` then stays false and the view simply skips the panel.
    bool init(SDL_Renderer* renderer, const core::TileGrid& grid, int pixelScale);

    // Recomputes the blip list from the current mission state. Call once per
    // frame before render(); one entry per living agent plus one per vehicle
    // (wrecks included), so it stays cheap.
    void updateBlips(const core::Mission& mission, const std::vector<std::size_t>& selected);

    // Draws the panel with its top-left corner at (x, y).
    void render(SDL_Renderer* renderer, int x, int y) const;

    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const { return texture_ != nullptr; }

    // Headless self-check: true when a blip exists for every living agent and
    // every vehicle, and all blips lie inside the panel. Call updateBlips()
    // first. Used by the --check-minimap guard and the game_minimap ctest.
    bool verifyBlips(const core::Mission& mission) const;

private:
    struct Blip
    {
        int x = 0;
        int y = 0;
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        bool selected = false;
    };

    SDL_Texture* texture_ = nullptr;
    int gridWidth_ = 0;
    int gridHeight_ = 0;
    int pixelScale_ = 1;
    int width_ = 0;
    int height_ = 0;
    std::vector<Blip> blips_;
};

} // namespace fossynd::game
