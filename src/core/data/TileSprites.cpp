/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/data/TileSprites.h"

#include <iterator>

namespace fossynd::core
{

const char* const kTileFrameNames[5] = {"floor", "floorAlt", "wall", "destructible", "rubble"};

namespace
{

// The authored ground palette. `body` is the tile's base colour, `trim` its lit
// top face, `skin` its shaded lower face and `dark` its outline. Values echo the
// flat fills that preceded the art (Game.cpp kTileLight/kTileDark/...), so the
// board keeps the same read while gaining texture.
const SpritePalette kGroundPalette{{52, 60, 52}, {70, 80, 70}, {40, 46, 40}, {24, 27, 24}};

} // namespace

bool bakeTileSprites(const std::string& text, TileSprites& out, std::string& error)
{
    const std::vector<std::string> names(std::begin(kTileFrameNames), std::end(kTileFrameNames));
    // No anchor: tiles are drawn aligned to their cell, so an 'A' is optional and
    // the bottom-centre default is unused.
    return bakeSheet(text, kGroundPalette, names, out.sheet, error, /*requireAnchor=*/false);
}

} // namespace fossynd::core
