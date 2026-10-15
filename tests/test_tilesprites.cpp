/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"

#include <fstream>
#include <sstream>
#include <string>

#include "core/data/TileSprites.h"

using namespace fossynd::core;

namespace
{

const char* const kMinimalSheet =
    "frame floor\n"
    "BT\n"
    ".D\n"
    "end\n"
    "frame floorAlt\n"
    "TT\n"
    ".D\n"
    "end\n"
    "frame wall\n"
    "DD\n"
    "DD\n"
    "end\n"
    "frame destructible\n"
    "DT\n"
    "DD\n"
    "end\n";

std::string readAuthoredSheet()
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/tiles/ground.txt";
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

FOSSYND_TEST(TileSprites, BakesFourFramesInKindOrder)
{
    TileSprites tiles;
    std::string error;
    FOSSYND_CHECK(bakeTileSprites(kMinimalSheet, tiles, error));
    FOSSYND_CHECK(tiles.valid());
    FOSSYND_CHECK_EQ(static_cast<int>(tiles.sheet.frames.size()), 4);

    // The kind -> frame mapping is the order the art is authored in.
    FOSSYND_CHECK_EQ(TileSprites::index(TileKind::FloorLight), 0);
    FOSSYND_CHECK_EQ(TileSprites::index(TileKind::FloorDark), 1);
    FOSSYND_CHECK_EQ(TileSprites::index(TileKind::Wall), 2);
    FOSSYND_CHECK_EQ(TileSprites::index(TileKind::DestructibleWall), 3);
}

FOSSYND_TEST(TileSprites, TilesNeedNoAnchor)
{
    // Unlike agent sprites, tiles are drawn aligned to their cell, so an 'A' is
    // optional and must not be required.
    TileSprites tiles;
    std::string error;
    FOSSYND_CHECK(bakeTileSprites(kMinimalSheet, tiles, error));
}

FOSSYND_TEST(TileSprites, RejectsMissingFrame)
{
    const char* const incomplete =
        "frame floor\nBT\n.D\nend\n"
        "frame floorAlt\nTT\n.D\nend\n"
        "frame wall\nDD\nDD\nend\n";
    TileSprites tiles;
    std::string error;
    FOSSYND_CHECK(!bakeTileSprites(incomplete, tiles, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(TileSprites, RejectsRaggedFrames)
{
    const char* const ragged =
        "frame floor\nBT\n.D\nend\n"
        "frame floorAlt\nTT\n.\nend\n"
        "frame wall\nDD\nDD\nend\n"
        "frame destructible\nDT\nDD\nend\n";
    TileSprites tiles;
    std::string error;
    FOSSYND_CHECK(!bakeTileSprites(ragged, tiles, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(TileSprites, AuthoredSheetBakesAtTheExpectedSize)
{
    const std::string text = readAuthoredSheet();
    FOSSYND_CHECK(!text.empty());

    TileSprites tiles;
    std::string error;
    FOSSYND_CHECK(bakeTileSprites(text, tiles, error));
    FOSSYND_CHECK(tiles.valid());

    // Every tile fills the isometric diamond it is drawn on.
    for (int i = 0; i < static_cast<int>(TileKind::Count); ++i)
    {
        FOSSYND_CHECK_EQ(tiles.sheet.frame(i).width, 16);
        FOSSYND_CHECK_EQ(tiles.sheet.frame(i).height, 8);
    }
}

FOSSYND_TEST(TileSprites, AuthoredFloorIsTransparentAtTheCorners)
{
    const std::string text = readAuthoredSheet();
    TileSprites tiles;
    std::string error;
    FOSSYND_CHECK(bakeTileSprites(text, tiles, error));

    const BakedFrame& floor = tiles.frame(TileKind::FloorLight);
    // The top-left corner is outside the diamond, so it stays transparent; the
    // centre is inside it, so it is opaque.
    FOSSYND_CHECK_EQ(static_cast<int>(floor.rgba[3]), 0);
    const std::size_t centre =
        (static_cast<std::size_t>(4) * 16u + 8u) * 4u;
    FOSSYND_CHECK_EQ(static_cast<int>(floor.rgba[centre + 3]), 255);
}
