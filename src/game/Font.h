/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>

#include "SDL.h"

namespace foss::game
{

// A tiny, dependency-free bitmap font (5x7 glyphs, ASCII 32..126).
//
// SDL_ttf is deliberately avoided to keep the dependency surface small and the
// game runnable on old hardware; glyphs are baked once into a single atlas
// texture and blitted with the renderer.
class BitmapFont
{
public:
    BitmapFont() = default;
    ~BitmapFont();

    BitmapFont(const BitmapFont&) = delete;
    BitmapFont& operator=(const BitmapFont&) = delete;

    // Builds the glyph atlas. `scale` is the integer pixel magnification.
    bool init(SDL_Renderer* renderer, int scale);

    void draw(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color) const;

    int textWidth(const std::string& text) const;
    int lineHeight() const;

private:
    SDL_Texture* atlas_ = nullptr;
    int scale_ = 1;
};

} // namespace foss::game
