#pragma once

#include "cocos2d.h"

class Agent : public cocos2d::Node
{
public:
    static Agent* create(const cocos2d::Color4F& color);

    bool initWithColor(const cocos2d::Color4F& color);
    void setTilePosition(float tileX, float tileY);

private:
    cocos2d::DrawNode* bodyNode = nullptr;
    float tileX = 0.0f;
    float tileY = 0.0f;
};
