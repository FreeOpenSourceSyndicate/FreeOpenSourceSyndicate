/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/NewGameScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "game/HudStyle.h"

namespace fossynd::game
{

std::string NewGameScreen::sanitiseName(const std::string& raw)
{
    std::string out;
    out.reserve(std::min(raw.size(), kMaxNameLength));
    for (char c : raw)
    {
        if (out.size() >= kMaxNameLength)
        {
            break;
        }
        const unsigned char uc = static_cast<unsigned char>(c);
        // Printable ASCII only (32..126): the 5x7 bitmap font draws nothing else,
        // so a control byte or a non-ASCII glyph would render blank (AGENTS.md).
        if (uc >= 32 && uc <= 126)
        {
            out.push_back(c);
        }
    }
    return out;
}

void NewGameScreen::begin(const std::string& name, const std::vector<std::string>& logos,
                          const std::string& logo)
{
    name_ = sanitiseName(name);
    logos_ = logos;
    logoIndex_ = 0;
    for (std::size_t i = 0; i < logos_.size(); ++i)
    {
        if (logos_[i] == logo)
        {
            logoIndex_ = static_cast<int>(i);
            break;
        }
    }
    cursorTime_ = 0.0f;
    pending_ = false;
    active_ = true;
}

void NewGameScreen::close()
{
    active_ = false;
    pending_ = false;
}

void NewGameScreen::appendText(const std::string& text)
{
    name_ += sanitiseName(text);
    // sanitiseName bounds its own input, but appending to an existing name must
    // respect the cap too.
    if (name_.size() > kMaxNameLength)
    {
        name_.resize(kMaxNameLength);
    }
}

void NewGameScreen::backspace()
{
    if (!name_.empty())
    {
        name_.pop_back();
    }
}

void NewGameScreen::cycleLogo(int delta)
{
    if (logos_.empty())
    {
        return;
    }
    const int count = static_cast<int>(logos_.size());
    // Wrap at both ends so Left from the first picks the last, and vice versa.
    logoIndex_ = ((logoIndex_ + delta) % count + count) % count;
}

const std::string& NewGameScreen::logo() const
{
    static const std::string kEmpty;
    if (logoIndex_ < 0 || logoIndex_ >= static_cast<int>(logos_.size()))
    {
        return kEmpty;
    }
    return logos_[static_cast<std::size_t>(logoIndex_)];
}

void NewGameScreen::handleKey(int sym)
{
    switch (sym)
    {
    case SDLK_BACKSPACE:
        backspace();
        break;
    case SDLK_LEFT:
    case SDLK_UP:
        cycleLogo(-1);
        break;
    case SDLK_RIGHT:
    case SDLK_DOWN:
        cycleLogo(1);
        break;
    default:
        break;
    }
}

void NewGameScreen::update(float dt)
{
    cursorTime_ += dt;
}

void NewGameScreen::render(SDL_Renderer* renderer, const BitmapFont& font, int w, int h) const
{
    const int lineH = font.lineHeight();

    // Dark, slightly translucent backdrop so the board stays faintly visible,
    // matching the title card's look.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 6, 7, 9, 232);
    SDL_Rect full{0, 0, w, h};
    SDL_RenderFillRect(renderer, &full);

    const int cx = w / 2;
    int y = static_cast<int>(static_cast<float>(h) * 0.22f);

    const std::string title = "NEW SYNDICATE";
    font.draw(renderer, cx - font.textWidth(title) / 2, y, title, hudstyle::kHudText);
    y += lineH * 2;

    // Name field: a bordered box with the typed name and a blinking caret.
    const std::string label = "NAME";
    font.draw(renderer, cx - font.textWidth(label) / 2, y, label, hudstyle::kHudDim);
    y += lineH + 2;

    const int fieldW = std::min(w - 40, 360);
    const int fieldH = lineH + 8;
    SDL_Rect field{cx - fieldW / 2, y, fieldW, fieldH};
    SDL_SetRenderDrawColor(renderer, 24, 26, 30, 255);
    SDL_RenderFillRect(renderer, &field);
    SDL_SetRenderDrawColor(renderer, 90, 96, 90, 255);
    SDL_RenderDrawRect(renderer, &field);

    const std::string shown = name_.empty() ? "(TYPE A NAME)" : name_;
    const SDL_Color nameColor = name_.empty() ? hudstyle::kHudDim : hudstyle::kHudText;
    font.draw(renderer, field.x + 6, y + 4, shown, nameColor);
    // Caret blinks on the same 0.6 s cadence as the title card (no RNG).
    if (std::fmod(cursorTime_, 1.0f) < 0.6f)
    {
        const int caretX = field.x + 6 + font.textWidth(name_);
        SDL_SetRenderDrawColor(renderer, hudstyle::kHudText.r, hudstyle::kHudText.g,
                               hudstyle::kHudText.b, 255);
        SDL_Rect caret{caretX, y + 3, 2, lineH};
        SDL_RenderFillRect(renderer, &caret);
    }
    y += fieldH + lineH;

    // Logo picker: the selectable ids as a row, the current one highlighted. The
    // id is only a stable key here; the logo *art* is Render's (a follow-up).
    const std::string logoLabel = "LOGO";
    font.draw(renderer, cx - font.textWidth(logoLabel) / 2, y, logoLabel, hudstyle::kHudDim);
    y += lineH + 2;
    if (logos_.empty())
    {
        const std::string none = "(NONE)";
        font.draw(renderer, cx - font.textWidth(none) / 2, y, none, hudstyle::kHudDim);
        y += lineH;
    }
    else
    {
        int totalW = 0;
        for (const std::string& id : logos_)
        {
            totalW += font.textWidth(id) + 12;
        }
        totalW -= 12;
        int lx = cx - totalW / 2;
        for (std::size_t i = 0; i < logos_.size(); ++i)
        {
            const bool selected = static_cast<int>(i) == logoIndex_;
            const SDL_Color color = selected ? hudstyle::kHudGood : hudstyle::kHudDim;
            if (selected)
            {
                const int iw = font.textWidth(logos_[i]) + 8;
                SDL_Rect hl{lx - 4, y - 2, iw, lineH + 4};
                SDL_SetRenderDrawColor(renderer, 30, 40, 30, 255);
                SDL_RenderFillRect(renderer, &hl);
                SDL_SetRenderDrawColor(renderer, hudstyle::kHudGood.r, hudstyle::kHudGood.g,
                                       hudstyle::kHudGood.b, 255);
                SDL_RenderDrawRect(renderer, &hl);
            }
            font.draw(renderer, lx, y, logos_[i], color);
            lx += font.textWidth(logos_[i]) + 12;
        }
        y += lineH;
    }
    y += lineH;

    const std::string controls = "TYPE  NAME    LEFT/RIGHT  LOGO    ENTER  BEGIN    ESC  DEFAULT";
    font.draw(renderer, cx - font.textWidth(controls) / 2, y, controls, hudstyle::kHudDim);
}

bool NewGameScreen::selfTest()
{
    // Sanitisation: printable ASCII only, bounded by the cap. A non-ASCII byte
    // and a control byte are dropped (they would render blank in the 5x7 font).
    if (sanitiseName("Riverside Combine") != "Riverside Combine")
    {
        std::fprintf(stderr, "new-game self-test: a plain ASCII name was altered\n");
        return false;
    }
    if (sanitiseName(std::string("caf\xc3\xa9")) != "caf")
    {
        std::fprintf(stderr, "new-game self-test: a non-ASCII byte survived sanitisation\n");
        return false;
    }
    if (sanitiseName(std::string("a\nb\tc")) != "abc")
    {
        std::fprintf(stderr, "new-game self-test: a control byte survived sanitisation\n");
        return false;
    }
    if (sanitiseName(std::string(kMaxNameLength + 5, 'x')).size() != kMaxNameLength)
    {
        std::fprintf(stderr, "new-game self-test: the name cap was not enforced\n");
        return false;
    }

    const std::vector<std::string> logos{"cog", "anvil", "serpent"};
    NewGameScreen screen;
    screen.begin("Riverside Combine", logos, "anvil");
    if (!screen.active() || screen.name() != "Riverside Combine" || screen.logo() != "anvil")
    {
        std::fprintf(stderr, "new-game self-test: begin() did not seed the current identity\n");
        return false;
    }

    // Editing: append then backspace.
    screen.appendText(" II");
    if (screen.name() != "Riverside Combine II")
    {
        std::fprintf(stderr, "new-game self-test: appendText did not append\n");
        return false;
    }
    screen.backspace();
    if (screen.name() != "Riverside Combine I")
    {
        std::fprintf(stderr, "new-game self-test: backspace did not delete\n");
        return false;
    }

    // Append is still bounded when it pushes past the cap.
    screen.begin(std::string(kMaxNameLength, 'x'), logos, "cog");
    screen.appendText("yy");
    if (screen.name().size() != kMaxNameLength)
    {
        std::fprintf(stderr, "new-game self-test: appendText exceeded the cap\n");
        return false;
    }

    // Logo cycling wraps at both ends.
    screen.begin("X", logos, "cog");
    if (screen.logo() != "cog")
    {
        std::fprintf(stderr, "new-game self-test: the known logo was not selected\n");
        return false;
    }
    screen.cycleLogo(-1); // wrap backwards from the first
    if (screen.logo() != "serpent")
    {
        std::fprintf(stderr, "new-game self-test: backwards wrap failed\n");
        return false;
    }
    screen.cycleLogo(1); // and forwards back to the first
    if (screen.logo() != "cog")
    {
        std::fprintf(stderr, "new-game self-test: forwards wrap failed\n");
        return false;
    }
    screen.cycleLogo(1);
    screen.cycleLogo(1);
    screen.cycleLogo(1); // a full turn of 3 logos wraps back to the first
    if (screen.logo() != "cog")
    {
        std::fprintf(stderr, "new-game self-test: forwards cycle did not wrap at the end\n");
        return false;
    }

    // An unknown current logo falls back to the first pickable id.
    screen.begin("X", logos, "not-a-logo");
    if (screen.logo() != "cog")
    {
        std::fprintf(stderr, "new-game self-test: an unknown logo did not fall back\n");
        return false;
    }

    // No logos at all: cycling is a safe no-op and the pick is empty.
    screen.begin("X", {}, "");
    screen.cycleLogo(3);
    if (!screen.logo().empty())
    {
        std::fprintf(stderr, "new-game self-test: cycling with no logos produced an id\n");
        return false;
    }

    // handleKey drives the same decisions (the keys the engine routes here).
    screen.begin("AB", logos, "cog");
    screen.handleKey(SDLK_BACKSPACE);
    screen.handleKey(SDLK_RIGHT);
    if (screen.name() != "A" || screen.logo() != "anvil")
    {
        std::fprintf(stderr, "new-game self-test: handleKey did not edit/cycle\n");
        return false;
    }

    // close() takes the screen down.
    screen.close();
    if (screen.active() || screen.pending())
    {
        std::fprintf(stderr, "new-game self-test: close() left the screen up\n");
        return false;
    }
    return true;
}

} // namespace fossynd::game
