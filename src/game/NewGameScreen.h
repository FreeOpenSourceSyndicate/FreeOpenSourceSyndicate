/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "SDL.h"

#include "game/Font.h"

namespace fossynd::game
{

// The new-game screen (Milestone C, issue #447): the player types the
// syndicate's name and cycles a logo id before the campaign begins. This is the
// UI half of the player-customisable identity; the core half (the campaign
// carrying + persisting the choice) is `core::Campaign`'s
// `setSyndicateName`/`setSyndicateLogo` (#445).
//
// It owns only view state and the *decision* (what name/logo the player picked),
// and hands that to the campaign when the player confirms. It reimplements no
// rule: name validation is bounded ASCII sanitisation, and the logo id is only a
// stable string key (the art is Render's). Kept in its own translation unit
// rather than growing the shared `Game.cpp` (docs/FOCUS_AREAS.md §4, Rule 2).
class NewGameScreen
{
public:
    // The longest name the field accepts, in ASCII characters. Bounded so the
    // name fits the management panel and cannot grow without limit.
    static constexpr std::size_t kMaxNameLength = 24;

    // Seeds the fields from the campaign's current identity and shows the screen:
    // `name` is the starting text (the data-authored default, or a restored
    // save), `logos` the pickable logo ids in order, and `logo` the current one
    // (selected in `logos` when known, else the first).
    void begin(const std::string& name, const std::vector<std::string>& logos,
               const std::string& logo);

    // The screen is up (it freezes the board and consumes input) / waiting for
    // the title card to finish before it opens.
    bool active() const { return active_; }
    bool pending() const { return pending_; }
    void setPending(bool pending) { pending_ = pending; }
    void close();

    // --- the decision (pure, testable without a window) ---

    // Appends the printable ASCII of `text` (SDL_TEXTINPUT payload), bounded by
    // kMaxNameLength. A non-ASCII byte is dropped: the 5x7 bitmap font draws
    // only ASCII 32..126, so a non-ASCII glyph would render blank (AGENTS.md).
    void appendText(const std::string& text);

    // Removes the last character. A no-op on an empty name.
    void backspace();

    // Moves the logo selection by `delta`, wrapping at both ends. A no-op when
    // there are no logos to pick.
    void cycleLogo(int delta);

    // The picked name (sanitised, possibly empty — an empty name clears the
    // choice, so the campaign's data-authored default renders again) and logo id
    // (empty when there are no logos).
    const std::string& name() const { return name_; }
    const std::string& logo() const;

    // Handles a navigation key while the screen is up: Backspace deletes, and
    // Left/Right (and Up/Down) cycle the logo. The confirm key (Enter) is the
    // caller's, because only the engine layer can apply the choice to the
    // campaign. A no-op for any other key.
    void handleKey(int sym);

    // Advances the caret blink timer by `dt` seconds.
    void update(float dt);

    // Draws the screen over the frame: backdrop, name field with a caret, the
    // logo picker, and the controls. View-only; reads no core state.
    void render(SDL_Renderer* renderer, const BitmapFont& font, int w, int h) const;

    // Keeps only the printable ASCII of `raw`, bounded by kMaxNameLength. The one
    // validation the field applies; exposed so the guard and the field share it.
    static std::string sanitiseName(const std::string& raw);

    // Headless guard for the decision logic (issue #447): sanitisation keeps
    // printable ASCII only and honours the cap, append/backspace edit the field,
    // logo cycling wraps at both ends, and begin() seeds from a current identity.
    // No window or SDL renderer is needed. Used by the `--check-newgame` guard and
    // the `game_newgame` ctest.
    static bool selfTest();

private:
    bool active_ = false;
    bool pending_ = false;
    std::string name_;
    std::vector<std::string> logos_;
    int logoIndex_ = 0;
    float cursorTime_ = 0.0f;
};

} // namespace fossynd::game
