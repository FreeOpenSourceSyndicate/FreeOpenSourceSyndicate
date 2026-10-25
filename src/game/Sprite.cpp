/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Sprite.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace fossynd::game
{

SpriteSheet::~SpriteSheet()
{
    destroy();
}

void SpriteSheet::destroy()
{
    if (texture_ != nullptr)
    {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    frameWidth_ = 0;
    frameHeight_ = 0;
    frameCount_ = 0;
    anchorX_.clear();
    anchorY_.clear();
}

bool SpriteSheet::build(SDL_Renderer* renderer, const core::BakedSheet& sheet)
{
    destroy();
    if (renderer == nullptr || sheet.frames.empty())
    {
        return false;
    }

    frameWidth_ = sheet.frames[0].width;
    frameHeight_ = sheet.frames[0].height;
    const int frameCount = static_cast<int>(sheet.frames.size());
    frameCount_ = frameCount;

    // Lay the frames out left to right in one texture so a draw is a single
    // blit (cheap on the software renderer, AGENTS.md 1b).
    const int sheetWidth = frameWidth_ * frameCount;
    const int sheetHeight = frameHeight_;

    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(sheetWidth) *
                                      static_cast<std::size_t>(sheetHeight), 0u);
    anchorX_.assign(sheet.frames.size(), 0);
    anchorY_.assign(sheet.frames.size(), 0);

    for (int frame = 0; frame < frameCount; ++frame)
    {
        const core::BakedFrame& src = sheet.frames[static_cast<std::size_t>(frame)];
        if (src.width != frameWidth_ || src.height != frameHeight_)
        {
            std::fprintf(stderr, "sprite frames are not a uniform size\n");
            destroy();
            return false;
        }
        anchorX_[static_cast<std::size_t>(frame)] = src.anchorX;
        anchorY_[static_cast<std::size_t>(frame)] = src.anchorY;
        for (int y = 0; y < frameHeight_; ++y)
        {
            for (int x = 0; x < frameWidth_; ++x)
            {
                const std::size_t si =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth_) +
                     static_cast<std::size_t>(x)) *
                    4u;
                const std::uint32_t r = src.rgba[si + 0];
                const std::uint32_t g = src.rgba[si + 1];
                const std::uint32_t b = src.rgba[si + 2];
                const std::uint32_t a = src.rgba[si + 3];
                const int dx = frame * frameWidth_ + x;
                pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(sheetWidth) +
                       static_cast<std::size_t>(dx)] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }

    texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                 sheetWidth, sheetHeight);
    if (texture_ == nullptr)
    {
        std::fprintf(stderr, "sprite texture failed: %s\n", SDL_GetError());
        destroy();
        return false;
    }
    SDL_UpdateTexture(texture_, nullptr, pixels.data(),
                      sheetWidth * static_cast<int>(sizeof(std::uint32_t)));
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    return true;
}

SDL_Rect SpriteSheet::frameRect(int index) const
{
    SDL_Rect rect{index * frameWidth_, 0, frameWidth_, frameHeight_};
    return rect;
}

core::SpritePalette paletteForRole(int roleIndex)
{
    // Original FOSSynd colours, chosen to echo the flat markers that preceded
    // sprites: green players, red enemies, pale civilians, cyan followers.
    core::SpritePalette palette;
    switch (roleIndex)
    {
    case AgentSprites::Player:
        palette = core::SpritePalette{{60, 170, 95}, {200, 240, 210}, {205, 170, 130}, {22, 30, 24}};
        break;
    case AgentSprites::Enemy:
        palette = core::SpritePalette{{170, 50, 42}, {245, 200, 120}, {205, 165, 125}, {28, 22, 22}};
        break;
    case AgentSprites::Civilian:
        palette = core::SpritePalette{{150, 145, 130}, {225, 225, 215}, {210, 175, 135}, {40, 38, 34}};
        break;
    case AgentSprites::Follower:
    default:
        palette = core::SpritePalette{{50, 150, 175}, {210, 245, 250}, {205, 175, 135}, {20, 30, 34}};
        break;
    }
    return palette;
}

bool AgentSprites::build(SDL_Renderer* renderer, const std::string& text, std::string& error)
{
    destroy();

    const std::vector<std::string> agentFrames(std::begin(core::kAgentFrameNames),
                                               std::end(core::kAgentFrameNames));
    for (int role = 0; role < RoleCount; ++role)
    {
        core::BakedSheet baked;
        if (!core::bakeSheet(text, paletteForRole(role), agentFrames, baked, error, true))
        {
            destroy();
            return false;
        }
        // The sheet must author the agent frames at the documented native size
        // (docs/ART_STYLE.md). A mis-sized sheet is a content error, not a silent
        // rescale: fail here so `--check-sprites` catches it.
        if (baked.frames[0].width != kAgentFrameWidth ||
            baked.frames[0].height != kAgentFrameHeight)
        {
            error = "agent frames must be " + std::to_string(kAgentFrameWidth) + "x" +
                    std::to_string(kAgentFrameHeight) + " (got " +
                    std::to_string(baked.frames[0].width) + "x" +
                    std::to_string(baked.frames[0].height) + "); see docs/ART_STYLE.md";
            destroy();
            return false;
        }
        if (!sheets_[role].build(renderer, baked))
        {
            error = "could not upload agent sprite texture";
            destroy();
            return false;
        }
    }

    // Props share one grey palette; they are scenery, not team-coloured.
    const core::SpritePalette propPalette{{140, 110, 70}, {182, 150, 100}, {0, 0, 0}, {60, 42, 26}};
    const char* const propNames[] = {"crate", "barrel"};
    for (int i = 0; i < PropCount; ++i)
    {
        core::BakedSheet baked;
        std::vector<std::string> names{propNames[i]};
        if (!core::bakeSheet(text, propPalette, names, baked, error, /*requireAnchor=*/false))
        {
            destroy();
            return false;
        }
        if (!props_[i].build(renderer, baked))
        {
            error = "could not upload prop sprite texture";
            destroy();
            return false;
        }
    }

    // Vehicles are scenery too (issue #363): a neutral steel palette, not a team
    // colour, so a hijacked car does not recolour when its driver changes side.
    const core::SpritePalette vehiclePalette{{120, 128, 138}, {190, 198, 205}, {0, 0, 0}, {48, 54, 62}};
    // Each vehicle sheet carries two frames: 0 = intact, 1 = wreck (issue #455,
    // art slice A3). The wreck is a real authored frame, not the black veil the
    // draw pass used to paint over the intact sprite. A missing wreck frame fails
    // the bake, so the sheet is all-or-nothing (the flat-marker fallback covers a
    // bad sheet).
    const char* const vehicleFrames[VehicleCount][2] = {
        {"car", "carWreck"},
        {"train", "trainWreck"},
    };
    for (int i = 0; i < VehicleCount; ++i)
    {
        core::BakedSheet baked;
        const std::vector<std::string> names{vehicleFrames[i][0], vehicleFrames[i][1]};
        if (!core::bakeSheet(text, vehiclePalette, names, baked, error, /*requireAnchor=*/false))
        {
            destroy();
            return false;
        }
        if (!vehicles_[i].build(renderer, baked))
        {
            error = "could not upload vehicle sprite texture";
            destroy();
            return false;
        }
    }

    return true;
}

void AgentSprites::destroy()
{
    for (SpriteSheet& sheet : sheets_)
    {
        sheet.destroy();
    }
    for (SpriteSheet& prop : props_)
    {
        prop.destroy();
    }
    for (SpriteSheet& vehicle : vehicles_)
    {
        vehicle.destroy();
    }
}

} // namespace fossynd::game
