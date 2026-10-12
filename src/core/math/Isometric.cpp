/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/math/Isometric.h"

namespace foss::core
{

Vec2 tileToWorld(float tileX, float tileY, float tileWidth, float tileHeight)
{
    const float halfWidth = tileWidth * 0.5f;
    const float halfHeight = tileHeight * 0.5f;
    return Vec2{(tileX - tileY) * halfWidth, (tileX + tileY) * halfHeight};
}

Vec2 worldToTile(float worldX, float worldY, float tileWidth, float tileHeight)
{
    const float halfWidth = tileWidth * 0.5f;
    const float halfHeight = tileHeight * 0.5f;

    // Invert:
    //   worldX / halfWidth  = tileX - tileY
    //   worldY / halfHeight = tileX + tileY
    const float a = worldX / halfWidth;
    const float b = worldY / halfHeight;
    return Vec2{(a + b) * 0.5f, (b - a) * 0.5f};
}

} // namespace foss::core
