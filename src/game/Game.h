/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SDL.h"

#include "core/math/Vec2.h"
#include "core/sim/Mission.h"
#include "core/world/TileGrid.h"
#include "game/Font.h"
#include "game/Sprite.h"

namespace fossynd::game
{

// The SDL2 application: window, main loop, input, and rendering.
//
// All game rules live in fossynd::core; this class only translates input into core
// commands and draws core state. It must stay lightweight enough for old,
// low-end hardware (see AGENTS.md 1b): the static map is rendered once to a
// texture and blitted each frame, the font is a single atlas, and the renderer
// is software-capable.
class Game
{
public:
    Game() = default;
    ~Game();

    // Creates the window and renderer and builds the mission. When `headless`
    // is true the dummy video driver is used so the game can run without a
    // display (used for smoke tests). When `lowSpec` is true the software
    // renderer is forced (old-hardware profile, ADR-0008). Returns false on
    // failure.
    bool init(int width, int height, bool headless, bool lowSpec = false);

    // Runs the main loop until the player quits. `maxFrames` > 0 runs that many
    // frames then returns (used for smoke tests).
    int run(int maxFrames = 0);

    // When enabled, the squad automatically advances on the nearest enemy.
    // Used for headless verification and as an attract mode for demos.
    void setAutoPlay(bool enabled) { autoPlay_ = enabled; }

    // Sets the mission file to load (already resolved by main from the config
    // file, a CLI flag, or the fallback search). An empty path means "use the
    // copy embedded in the binary". Call before init().
    void setMissionPath(const std::string& path) { missionPath_ = path; }

    // Shows the authored pixel-art title card before play. Off by default so
    // automated runs (smoke tests, screenshots, attract mode) go straight to the
    // map; the interactive player sees it and presses a key to start.
    void setShowTitleCard(bool enabled) { showTitleCard_ = enabled; }

    // Renders one frame and saves it to `path` as a BMP. Returns false on
    // failure. Used for headless verification.
    bool saveScreenshot(const std::string& path);

    // True once the authored sprite sheets have baked successfully. When false
    // the renderer falls back to flat markers. Used by the sprite ctest.
    bool spritesValid() const { return sprites_.valid(); }

private:
    void buildMission();
    void buildMapTexture();
    void buildSprites();
    void restart();
    void handleEvents(bool& running);
    void update(float dt);
    void updateAutoPlay(float dt);
    void followSquad(float dt);
    void updateAgentViews(float dt);
    void render();
    void renderAgents();
    void renderHud();
    void renderTitleCard();
    void drawTitleDiamond(float cx, float cy, float radius, std::uint8_t r, std::uint8_t g,
                          std::uint8_t b);

    // Draws one sprite frame anchored on `screenAnchor`, scaled by kSpriteScale.
    // A non-zero `flash` alpha blends a brief white veil over the sprite.
    void drawSpriteFrame(const SpriteSheet& sheet, int frameIndex, int screenAnchorX,
                         int screenAnchorY, std::uint8_t flash = 0);

    // Selection helpers, all clamped to living player agents.
    int agentAtScreen(int x, int y) const;
    int enemyAtScreen(int x, int y) const;
    void selectOnly(std::size_t index);
    void toggleSelect(std::size_t index);
    void selectAllPlayers();
    void orderAttackOn(int targetIndex);
    // Each selected agent uses its Persuadertron on the nearest civilian in range.
    void persuadeNearestCivilian();
    // Releases the followers of every selected agent.
    void freeSelectedFollowers();

    core::Vec2 tileToScreen(const core::Vec2& tile) const;
    core::TilePos screenToTile(int screenX, int screenY) const;
    void centerOn(const core::Vec2& tile);
    void clampCamera();

    // Writes a filled isometric diamond directly into a surface (used once).
    void fillDiamond(SDL_Surface* surface, int centerX, int centerY, float halfW, float halfH,
                     std::uint32_t color);

    // World-space filled diamond for the renderer (agents, selection, health).
    void fillDiamondWorld(const core::Vec2& worldCenter, float radius, std::uint8_t r, std::uint8_t g,
                          std::uint8_t b);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* mapTexture_ = nullptr;

    int screenWidth_ = 0;
    int screenHeight_ = 0;

    // Tile size in pixels (a tile diamond is tileWidth_ x tileHeight_).
    static constexpr float kTileWidth = 64.0f;
    static constexpr float kTileHeight = 32.0f;

    // Integer magnification of the authored 14x18 sprite cells. Kept small so
    // agents read as people on a 64px tile and stay cheap to blit (AGENTS.md 1b).
    static constexpr int kSpriteScale = 3;

    // Roughly how fast the walk cycle alternates, in steps per second. A view
    // detail only: it never feeds back into the simulation.
    static constexpr float kWalkCadence = 2.4f;

    static constexpr int kMapWidth = 24;
    static constexpr int kMapHeight = 24;

    int mapSurfaceWidth_ = 0;
    int mapSurfaceHeight_ = 0;
    float mapOffsetX_ = 0.0f;
    float mapOffsetY_ = 0.0f;

    // Camera position, in pixels: the map texture is drawn at (-cameraX_, -cameraY_).
    float cameraX_ = 0.0f;
    float cameraY_ = 0.0f;

    core::TileGrid grid_{kMapWidth, kMapHeight};
    core::Mission mission_{grid_};

    // Resolved mission file, or empty to use the embedded copy (ADR-0003).
    std::string missionPath_;

    BitmapFont font_;

    // Authored pixel-art sprites (Phase 2). When `sprites_.valid()` is false the
    // renderer falls back to the original flat diamonds, so a bad sheet never
    // stops the game from running.
    AgentSprites sprites_;

    // Per-agent view-only animation state. Kept out of core: it is presentation,
    // so it must never influence the simulation (AGENTS.md golden rule 4).
    struct AgentView
    {
        bool initialized = false;
        core::Vec2 lastPosition{0.0f, 0.0f};
        float facingX = 0.0f;
        float facingY = 1.0f;
        float walkPhase = 0.0f;
        float walkTimer = 0.0f;
    };
    std::vector<AgentView> views_;

    // Indices into mission_.agents() selected for orders. For the demo the whole
    // squad moves together; selection is tracked for the HUD.
    std::vector<std::size_t> selected_;
    std::uint64_t seed_ = 0xC0FFEEu;

    bool autoPlay_ = false;
    float autoPlayTimer_ = 0.0f;

    // Authored title card (pixel art), shown before play.
    bool showTitleCard_ = false;
    bool titleCardActive_ = false;
    float titleCardTime_ = 0.0f;
    bool titleCardStartRequested_ = false;

    bool running_ = false;
};

} // namespace fossynd::game
