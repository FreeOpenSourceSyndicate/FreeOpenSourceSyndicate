#pragma once

#include "cocos2d.h"

class GameScene : public cocos2d::Scene
{
public:
    static GameScene* createScene();

    virtual bool init() override;

    // a selector callback
    void menuCloseCallback(cocos2d::Ref* sender);

    // implement the "static create()" method manually
    CREATE_FUNC(GameScene);

private:
    cocos2d::Label* titleLabel = nullptr;
    cocos2d::Sprite* background = nullptr;
};
