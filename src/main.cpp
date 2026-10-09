#include "AppDelegate.h"
#include "cocos2d.h"
#include "game/GameScene.h"

USING_NS_CC;

int main(int argc, char** argv)
{
    // This is the standard entry point for a Cocos2d-x desktop app.
    // The app will be bootstrapped from the engine's AppDelegate class.
    AppDelegate app;
    return Application::getInstance()->run();
}
