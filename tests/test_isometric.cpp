/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/math/Isometric.h"

using namespace fossynd::core;

FOSSYND_TEST(Isometric, OriginMapsToOrigin)
{
    const Vec2 w = tileToWorld(0.0f, 0.0f, 64.0f, 32.0f);
    FOSSYND_CHECK_NEAR(w.x, 0.0f);
    FOSSYND_CHECK_NEAR(w.y, 0.0f);
}

FOSSYND_TEST(Isometric, TileAxesProjectCorrectly)
{
    // Moving +1 on the x tile axis moves right and down by half a tile.
    const Vec2 wx = tileToWorld(1.0f, 0.0f, 64.0f, 32.0f);
    FOSSYND_CHECK_NEAR(wx.x, 32.0f);
    FOSSYND_CHECK_NEAR(wx.y, 16.0f);

    // Moving +1 on the y tile axis moves left and down by half a tile.
    const Vec2 wy = tileToWorld(0.0f, 1.0f, 64.0f, 32.0f);
    FOSSYND_CHECK_NEAR(wy.x, -32.0f);
    FOSSYND_CHECK_NEAR(wy.y, 16.0f);
}

FOSSYND_TEST(Isometric, WorldToTileInvertsTileToWorld)
{
    const float tileW = 64.0f;
    const float tileH = 32.0f;
    const float points[][2] = {{0.0f, 0.0f}, {3.0f, 4.0f}, {-2.5f, 1.25f}, {10.0f, -3.0f}};
    for (const auto& p : points)
    {
        const Vec2 world = tileToWorld(p[0], p[1], tileW, tileH);
        const Vec2 back = worldToTile(world.x, world.y, tileW, tileH);
        FOSSYND_CHECK_NEAR(back.x, p[0]);
        FOSSYND_CHECK_NEAR(back.y, p[1]);
    }
}
