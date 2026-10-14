/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fossynd::core
{

// Per-sheet colours used when baking a sprite sheet. All values are authored for
// FOSSynd; the four roles map onto the sprite characters B (body), T (trim),
// S (skin), D (dark).
struct SpritePalette
{
    std::uint8_t body[3] = {0, 0, 0};
    std::uint8_t trim[3] = {0, 0, 0};
    std::uint8_t skin[3] = {0, 0, 0};
    std::uint8_t dark[3] = {0, 0, 0};
};

// One baked frame: RGBA8888 pixels (row-major, top-left origin) plus the anchor
// (the pixel the frame is drawn at, so it sits on its tile). Anchors and sizes
// are in "cell" pixels; the view may scale them for the display.
struct BakedFrame
{
    int width = 0;
    int height = 0;
    int anchorX = 0;
    int anchorY = 0;
    std::vector<std::uint8_t> rgba;
};

// A set of baked frames in the order they were requested.
struct BakedSheet
{
    std::vector<BakedFrame> frames;

    const BakedFrame& frame(int index) const { return frames[static_cast<std::size_t>(index)]; }
};

// Parses an authored sprite sheet (see assets/sprites/agents.txt) and bakes the
// frames named in `frameNames`, in that order, for `palette`. Every requested
// frame must exist and all frames must be the same size. If `requireAnchor` is
// true each frame must contain exactly one anchor pixel ('A'); otherwise an
// anchor is optional and defaults to the bottom-centre of the frame (so props
// rest on their tile). Returns false and sets `error` on malformed input.
// Engine-agnostic and free of I/O: the caller reads the file (or the embedded
// copy) and passes the text.
bool bakeSheet(const std::string& text, const SpritePalette& palette,
               const std::vector<std::string>& frameNames, BakedSheet& out, std::string& error,
               bool requireAnchor = true);

// --- agent sheet -----------------------------------------------------------

// A baked agent sheet: four frames selected by facing and walk phase. The frame
// order matches the names passed to bakeAgentSprites below.
//
//   frameIndex(back=false, step=false)  front, standing
//   frameIndex(back=false, step=true)   front, mid-stride
//   frameIndex(back=true,  step=false)  back, standing
//   frameIndex(back=true,  step=true)   back, mid-stride
struct AgentSpriteSheet
{
    BakedSheet sheet;

    static int frameIndex(bool back, bool step) { return (back ? 2 : 0) + (step ? 1 : 0); }
    const BakedFrame& frame(bool back, bool step) const { return sheet.frame(frameIndex(back, step)); }
    bool valid() const { return sheet.frames.size() == 4; }
};

// Bakes the four authored agent frames: front, frontStep, back, backStep.
bool bakeAgentSprites(const std::string& text, const SpritePalette& palette,
                      AgentSpriteSheet& out, std::string& error);

// The authored agent frame names, in sheet order (also the order
// bakeAgentSprites expects). Exposed so the view can report a readable error.
extern const char* const kAgentFrameNames[4];

} // namespace fossynd::core
