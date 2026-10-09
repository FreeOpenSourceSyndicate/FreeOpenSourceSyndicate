#pragma once

#include "cocos2d.h"

class GameManager
{
public:
    GameManager();
    ~GameManager();

    void initialize();
    void update(float deltaTime);
    bool isRunning() const;

private:
    bool running = false;
};
