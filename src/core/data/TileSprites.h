/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstdint>
#include <string>

#include "core/data/AgentSprites.h"

namespace fossynd::core
{

// The authored ground tiles FOSSynd draws for the static map (issue #189).
// Tiles are 16x8 native, exactly the isometric diamond they fill, so unlike
// agent sprites they are drawn aligned to their cell and need no anchor.
enum class TileKind : std::uint8_t
{
    FloorLight = 0,
    FloorDark = 1,
    Wall = 2,
    DestructibleWall = 3,
    Rubble = 4,
    Count = 5,
};

struct TileSprites
{
    BakedSheet sheet;

    static int index(TileKind kind) { return static_cast<int>(kind); }
    const BakedFrame& frame(TileKind kind) const { return sheet.frame(index(kind)); }
    bool valid() const { return sheet.frames.size() == static_cast<std::size_t>(TileKind::Count); }
};

// The authored tile frame names, in sheet order (also the order
// bakeTileSprites expects). Exposed so the view can report a readable error.
extern const char* const kTileFrameNames[5];

// Bakes the five authored ground tiles from `text` (the content of
// assets/tiles/ground.txt, or the copy embedded at build time). All five frames
// must exist and share one size. No anchor is required: a tile is drawn aligned
// to its diamond cell. Engine-agnostic and free of I/O, like bakeSheet.
bool bakeTileSprites(const std::string& text, TileSprites& out, std::string& error);

} // namespace fossynd::core
