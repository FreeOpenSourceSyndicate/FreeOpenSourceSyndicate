#include "world/IsometricMap.h"

USING_NS_CC;

IsometricMap* IsometricMap::create(const Size& tileSize, int mapWidth, int mapHeight)
{
    auto* instance = new (std::nothrow) IsometricMap();
    if (instance && instance->initWithParams(tileSize, mapWidth, mapHeight))
    {
        instance->autorelease();
        return instance;
    }
    CC_SAFE_DELETE(instance);
    return nullptr;
}

bool IsometricMap::initWithParams(const Size& tileSize, int mapWidth, int mapHeight)
{
    if (!Node::init())
    {
        return false;
    }

    this->tileSize = tileSize;
    this->mapWidth = mapWidth;
    this->mapHeight = mapHeight;

    tiles.resize(static_cast<size_t>(mapHeight), std::vector<int>(static_cast<size_t>(mapWidth), 0));
    buildDebugGrid();
    return true;
}

void IsometricMap::buildDebugGrid()
{
    if (gridNode != nullptr)
    {
        removeChild(gridNode, true);
    }

    gridNode = DrawNode::create();
    addChild(gridNode);

    const float halfWidth = tileSize.width * 0.5f;
    const float halfHeight = tileSize.height * 0.5f;

    for (int y = 0; y < mapHeight; ++y)
    {
        for (int x = 0; x < mapWidth; ++x)
        {
            const float screenX = (x - y) * halfWidth;
            const float screenY = (x + y) * halfHeight;

            std::vector<Vec2> diamond = {
                Vec2(screenX, screenY),
                Vec2(screenX + halfWidth, screenY + halfHeight),
                Vec2(screenX, screenY + tileSize.height),
                Vec2(screenX - halfWidth, screenY + halfHeight)
            };

            const Color4F color = (tiles[y][x] == 0)
                ? Color4F(0.15f, 0.18f, 0.22f, 1.0f)
                : Color4F(0.22f, 0.32f, 0.25f, 1.0f);

            gridNode->drawPolygon(diamond.data(), static_cast<int>(diamond.size()), color, 1.0f, Color4F(0.4f, 0.55f, 0.7f, 1.0f));
        }
    }
}

void IsometricMap::setTileAt(int x, int y, int tileType)
{
    if (x < 0 || y < 0 || x >= mapWidth || y >= mapHeight)
    {
        return;
    }

    tiles[y][x] = tileType;
    buildDebugGrid();
}

int IsometricMap::getTileAt(int x, int y) const
{
    if (x < 0 || y < 0 || x >= mapWidth || y >= mapHeight)
    {
        return -1;
    }

    return tiles[y][x];
}
