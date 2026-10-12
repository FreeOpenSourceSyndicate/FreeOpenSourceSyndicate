/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SDL.h"

#include "core/math/Vec2.h"
#include "core/sim/Agent.h"
#include "core/world/TileGrid.h"

namespace foss::game
{

// The SDL2 application: window, main loop, input, and rendering.
//
// All game rules live in foss::core; this class only translates input into core
// commands and draws core state. It must stay lightweight enough for old,
// low-end hardware (see AGENTS.md 1b): the static map is rendered once to a
// texture and blitted each frame, and the renderer is software-capable.
class Game
{
public:
    Game() = default;
    ~Game();

    // Creates the window and renderer and builds the map. When `headless` is
    // true the dummy video driver is used so the game can run without a
    // display (used for smoke tests). Returns false on failure.
    bool init(int width, int height, bool headless);

    // Runs the main loop until the player quits. `maxFrames` > 0 runs that many
    // frames then returns (used for smoke tests).
    int run(int maxFrames = 0);

    // Renders one frame and saves it to `path` as a BMP. Returns false on
    // failure. Used for headless verification.
    bool saveScreenshot(const std::string& path);

private:
    void buildMap();
    void spawnAgents();
    void buildMapTexture();
    void handleEvents(bool& running);
    void update(float dt);
    void render();

    core::Vec2 tileToScreen(const core::Vec2& tile) const;
    core::TilePos screenToTile(int screenX, int screenY) const;

    // Writes a filled isometric diamond directly into a surface (used once).
    void fillDiamond(SDL_Surface* surface, int centerX, int centerY, float halfW, float halfH,
                     std::uint32_t color);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* mapTexture_ = nullptr;

    int screenWidth_ = 0;
    int screenHeight_ = 0;

    // Tile size in pixels (a tile diamond is tileWidth_ x tileHeight_).
    static constexpr float kTileWidth = 64.0f;
    static constexpr float kTileHeight = 32.0f;

    // Static map layout and the surface offset that maps world space to it.
    static constexpr int kMapWidth = 20;
    static constexpr int kMapHeight = 20;
    int mapSurfaceWidth_ = 0;
    int mapSurfaceHeight_ = 0;
    float mapOffsetX_ = 0.0f;
    float mapOffsetY_ = 0.0f;

    // Camera position (top-left of the map texture on screen), in pixels.
    float cameraX_ = 0.0f;
    float cameraY_ = 0.0f;

    core::TileGrid grid_{kMapWidth, kMapHeight};
    std::vector<core::Agent> agents_;
    int selected_ = 0;

    bool running_ = false;
    std::string status_ = "Free Open Source Syndicate - prototype";
};

} // namespace foss::game
