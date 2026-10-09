#include "game/GameScene.h"

USING_NS_CC;

GameScene* GameScene::createScene()
{
    auto scene = GameScene::create();
    return scene;
}

bool GameScene::init()
{
    if (!Scene::init())
    {
        return false;
    }

    auto visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    auto background = LayerColor::create(Color4B(18, 22, 31, 255));
    this->addChild(background, -10);

    titleLabel = Label::createWithTTF("Free Open Source Syndicate", "fonts/Marker Felt.ttf", 36);
    if (titleLabel)
    {
        titleLabel->setPosition(Vec2(origin.x + visibleSize.width / 2,
                                     origin.y + visibleSize.height * 0.8f));
        titleLabel->setTextColor(Color4B::WHITE);
        this->addChild(titleLabel, 10);
    }

    auto subtitleLabel = Label::createWithTTF("Syndicate-inspired isometric tactical prototype", "fonts/Marker Felt.ttf", 18);
    if (subtitleLabel)
    {
        subtitleLabel->setPosition(Vec2(origin.x + visibleSize.width / 2,
                                       origin.y + visibleSize.height * 0.7f));
        subtitleLabel->setTextColor(Color4B(180, 180, 180, 255));
        this->addChild(subtitleLabel, 10);
    }

    auto hintLabel = Label::createWithTTF("Prototype startup scene for Linux/Windows/macOS build", "fonts/Marker Felt.ttf", 14);
    if (hintLabel)
    {
        hintLabel->setPosition(Vec2(origin.x + visibleSize.width / 2,
                                    origin.y + visibleSize.height * 0.6f));
        hintLabel->setTextColor(Color4B(140, 140, 140, 255));
        this->addChild(hintLabel, 10);
    }

    auto closeItem = MenuItemImage::create(
        "CloseNormal.png",
        "CloseSelected.png",
        CC_CALLBACK_1(GameScene::menuCloseCallback, this));

    if (closeItem)
    {
        closeItem->setPosition(Vec2(origin.x + visibleSize.width - 20,
                                    origin.y + 20));

        auto menu = Menu::create(closeItem, nullptr);
        menu->setPosition(Vec2::ZERO);
        this->addChild(menu, 10);
    }

    return true;
}

void GameScene::menuCloseCallback(Ref* sender)
{
    Director::getInstance()->end();
}
