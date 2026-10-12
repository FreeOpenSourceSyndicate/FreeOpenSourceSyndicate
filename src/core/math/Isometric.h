/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include "core/math/Vec2.h"

namespace foss::core
{

// Isometric projection helpers.
//
// A tile diamond is `tileWidth` x `tileHeight` pixels. Tile-space axes run at
// 45 degrees; the transform maps a fractional tile position to a screen/world
// position and back.
//
//   world.x = (tile.x - tile.y) * tileWidth  / 2
//   world.y = (tile.x + tile.y) * tileHeight / 2

Vec2 tileToWorld(float tileX, float tileY, float tileWidth, float tileHeight);

Vec2 worldToTile(float worldX, float worldY, float tileWidth, float tileHeight);

} // namespace foss::core
