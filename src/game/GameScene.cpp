#pragma once

#include "cocos2d.h"
#include "entities/Agent.h"
#include "world/IsometricMap.h"

class GameScene : public cocos2d::Scene
{
public:
    static GameScene* createScene();

    virtual bool init() override;
    void menuCloseCallback(cocos2d::Ref* sender);

    CREATE_FUNC(GameScene);

private:
    cocos2d::Label* titleLabel = nullptr;
    cocos2d::Node* sceneRoot = nullptr;
    IsometricMap* map = nullptr;
    Agent* activeAgent = nullptr;
    Agent* allyAgent = nullptr;
};
