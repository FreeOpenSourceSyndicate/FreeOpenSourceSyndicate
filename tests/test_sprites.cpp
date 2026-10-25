/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"

#include "core/data/AgentSprites.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace fossynd::core;

namespace
{

const SpritePalette kPalette{{10, 20, 30}, {40, 50, 60}, {70, 80, 90}, {100, 110, 120}};

const char* const kMinimalSheet =
    "# a tiny two-frame sheet\n"
    "frame front\n"
    ".B.\n"
    "DTD\n"
    ".A.\n"
    "end\n"
    "frame back\n"
    "BBB\n"
    "TTT\n"
    ".A.\n"
    "end\n";

} // namespace

FOSSYND_TEST(Sprites, BakesFramesInRequestedOrder)
{
    BakedSheet sheet;
    std::string error;
    const std::vector<std::string> names{"front", "back"};
    FOSSYND_CHECK(bakeSheet(kMinimalSheet, kPalette, names, sheet, error, true));

    FOSSYND_CHECK_EQ(static_cast<int>(sheet.frames.size()), 2);
    FOSSYND_CHECK_EQ(sheet.frame(0).width, 3);
    FOSSYND_CHECK_EQ(sheet.frame(0).height, 3);
    FOSSYND_CHECK_EQ(sheet.frame(0).anchorX, 1);
    FOSSYND_CHECK_EQ(sheet.frame(0).anchorY, 2);
}

FOSSYND_TEST(Sprites, AppliesPaletteAndAlpha)
{
    BakedSheet sheet;
    std::string error;
    const std::vector<std::string> names{"front"};
    FOSSYND_CHECK(bakeSheet(kMinimalSheet, kPalette, names, sheet, error, true));

    const BakedFrame& frame = sheet.frame(0);
    // Row 0 is ".B." so pixel (1,0) is the body colour, fully opaque.
    const std::size_t body = (0u * 3u + 1u) * 4u;
    FOSSYND_CHECK_EQ(static_cast<int>(frame.rgba[body + 0]), 10);
    FOSSYND_CHECK_EQ(static_cast<int>(frame.rgba[body + 1]), 20);
    FOSSYND_CHECK_EQ(static_cast<int>(frame.rgba[body + 2]), 30);
    FOSSYND_CHECK_EQ(static_cast<int>(frame.rgba[body + 3]), 255);

    // Row 1 starts with the dark colour, and pixel (0,0) is transparent.
    const std::size_t dark = (1u * 3u + 0u) * 4u;
    FOSSYND_CHECK_EQ(static_cast<int>(frame.rgba[dark + 0]), 100);
    FOSSYND_CHECK_EQ(static_cast<int>(frame.rgba[dark + 3]), 255);
    const std::size_t transparent = 0u;
    FOSSYND_CHECK_EQ(static_cast<int>(frame.rgba[transparent + 3]), 0);
}

FOSSYND_TEST(Sprites, RejectsMissingFrame)
{
    BakedSheet sheet;
    std::string error;
    const std::vector<std::string> names{"front", "nope"};
    FOSSYND_CHECK(!bakeSheet(kMinimalSheet, kPalette, names, sheet, error, true));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Sprites, RejectsRaggedFrame)
{
    const char* const ragged =
        "frame front\n"
        ".B.\n"
        "DT\n"
        ".A.\n"
        "end\n";
    BakedSheet sheet;
    std::string error;
    const std::vector<std::string> names{"front"};
    FOSSYND_CHECK(!bakeSheet(ragged, kPalette, names, sheet, error, true));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Sprites, RequiresExactlyOneAnchor)
{
    const char* const noAnchor =
        "frame front\n"
        ".B.\n"
        "DTD\n"
        "...\n"
        "end\n";
    BakedSheet sheet;
    std::string error;
    const std::vector<std::string> names{"front"};
    FOSSYND_CHECK(!bakeSheet(noAnchor, kPalette, names, sheet, error, true));

    // Without the requirement, the anchor defaults to bottom-centre.
    FOSSYND_CHECK(bakeSheet(noAnchor, kPalette, names, sheet, error, false));
    FOSSYND_CHECK_EQ(sheet.frame(0).anchorX, 1);
    FOSSYND_CHECK_EQ(sheet.frame(0).anchorY, 2);
}

FOSSYND_TEST(Sprites, RejectsUnknownPixelCharacter)
{
    const char* const bad =
        "frame front\n"
        ".X.\n"
        "DTD\n"
        ".A.\n"
        "end\n";
    BakedSheet sheet;
    std::string error;
    const std::vector<std::string> names{"front"};
    FOSSYND_CHECK(!bakeSheet(bad, kPalette, names, sheet, error, true));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Sprites, AgentSheetHasFourFrames)
{
    const char* const sheet =
        "frame front\n.A.\nend\n"
        "frame frontStep\n.A.\nend\n"
        "frame back\n.A.\nend\n"
        "frame backStep\n.A.\nend\n";
    AgentSpriteSheet agent;
    std::string error;
    FOSSYND_CHECK(bakeAgentSprites(sheet, kPalette, agent, error));
    FOSSYND_CHECK(agent.valid());
    FOSSYND_CHECK_EQ(AgentSpriteSheet::frameIndex(false, false), 0);
    FOSSYND_CHECK_EQ(AgentSpriteSheet::frameIndex(false, true), 1);
    FOSSYND_CHECK_EQ(AgentSpriteSheet::frameIndex(true, false), 2);
    FOSSYND_CHECK_EQ(AgentSpriteSheet::frameIndex(true, true), 3);
}

// The shipped sheet must author the four agent frames at the documented native
// size (docs/ART_STYLE.md). The view's size check (game::kAgentFrameWidth/Height)
// is the runtime guard; this pins the *shipped file* so a stale or mis-sized
// sheet is caught by `ctest` even before the SDL target is built. The value
// lives in src/game/Sprite.h (render-owned); keep the two in step.
FOSSYND_TEST(Sprites, ShippedAgentFramesAreTheNativeSize)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/sprites/agents.txt";
    std::ifstream in(path);
    FOSSYND_CHECK(in.good());
    std::ostringstream buffer;
    buffer << in.rdbuf();

    AgentSpriteSheet agent;
    std::string error;
    FOSSYND_CHECK(bakeAgentSprites(buffer.str(), kPalette, agent, error));
    FOSSYND_CHECK(agent.valid());
    for (int i = 0; i < 4; ++i)
    {
        FOSSYND_CHECK_EQ(agent.sheet.frames[static_cast<std::size_t>(i)].width, 20);
        FOSSYND_CHECK_EQ(agent.sheet.frames[static_cast<std::size_t>(i)].height, 28);
    }
}
