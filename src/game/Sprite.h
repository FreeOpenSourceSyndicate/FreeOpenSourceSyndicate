/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>
#include <vector>

#include "SDL.h"

#include "core/data/AgentSprites.h"

namespace fossynd::game
{

// One uploaded sprite sheet: several frames laid out left to right in a single
// texture, plus each frame's anchor. The core bakes the pixels (engine-agnostic,
// unit-tested); this class only uploads them and reports frame geometry. Owned
// by the view; never copied.
class SpriteSheet
{
public:
    SpriteSheet() = default;
    ~SpriteSheet();

    SpriteSheet(const SpriteSheet&) = delete;
    SpriteSheet& operator=(const SpriteSheet&) = delete;

    // Uploads `sheet`. Returns false (leaving the sheet invalid) on any failure,
    // so the caller can fall back to flat markers.
    bool build(SDL_Renderer* renderer, const core::BakedSheet& sheet);

    void destroy();

    bool valid() const { return texture_ != nullptr; }
    SDL_Texture* texture() const { return texture_; }

    // Source rectangle of one frame in the sheet texture.
    SDL_Rect frameRect(int index) const;
    int anchorX(int index) const { return anchorX_[static_cast<std::size_t>(index)]; }
    int anchorY(int index) const { return anchorY_[static_cast<std::size_t>(index)]; }

    int frameWidth() const { return frameWidth_; }
    int frameHeight() const { return frameHeight_; }

private:
    SDL_Texture* texture_ = nullptr;
    int frameWidth_ = 0;
    int frameHeight_ = 0;
    std::vector<int> anchorX_;
    std::vector<int> anchorY_;
};

// The agent sprite sheets, one per role. All are baked from the same authored
// text with different palettes. On failure `valid()` stays false and the view
// falls back to flat markers.
class AgentSprites
{
public:
    enum Role
    {
        Player = 0,
        Enemy = 1,
        Civilian = 2,
        Follower = 3,
        RoleCount = 4,
    };

    // Statically-known board props (drawn on blocked tiles).
    enum Prop
    {
        Crate = 0,
        Barrel = 1,
        PropCount = 2,
    };

    // Hijackable vehicle sprites (issue #363), one per `core::VehicleKind`.
    enum Vehicle
    {
        VehicleCar = 0,
        VehicleTrain = 1,
        VehicleCount = 2,
    };

    bool build(SDL_Renderer* renderer, const std::string& text, std::string& error);
    void destroy();

    bool valid() const { return sheets_[Player].valid(); }
    const SpriteSheet& sheet(Role role) const { return sheets_[role]; }
    const SpriteSheet& prop(Prop prop) const { return props_[prop]; }
    const SpriteSheet& vehicle(Vehicle vehicle) const { return vehicles_[vehicle]; }

    // Frame index inside a role sheet (see AgentSpriteSheet::frameIndex).
    static int frameIndex(bool back, bool step) { return (back ? 2 : 0) + (step ? 1 : 0); }

private:
    SpriteSheet sheets_[RoleCount];
    SpriteSheet props_[PropCount];
    SpriteSheet vehicles_[VehicleCount];
};

// The authored per-role palettes (original colours echoing the previous flat
// markers: green players, red enemies, pale civilians, cyan followers).
core::SpritePalette paletteForRole(int roleIndex);

} // namespace fossynd::game
