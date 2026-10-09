#include "game/GameManager.h"

GameManager::GameManager()
{
    initialize();
}

GameManager::~GameManager() = default;

void GameManager::initialize()
{
    running = true;
}

void GameManager::update(float /*deltaTime*/)
{
    // Placeholder for future mission, AI, and gameplay update systems.
}

bool GameManager::isRunning() const
{
    return running;
}
