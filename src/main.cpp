/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "game/Game.h"

int main(int argc, char** argv)
{
    int maxFrames = 0;
    bool headless = false;
    const char* screenshot = nullptr;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--headless") == 0)
        {
            headless = true;
        }
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
        {
            maxFrames = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
        {
            screenshot = argv[++i];
        }
    }

    foss::game::Game game;
    if (!game.init(960, 600, headless))
    {
        std::fprintf(stderr, "Failed to start the game.\n");
        return 1;
    }

    if (screenshot != nullptr)
    {
        const bool ok = game.saveScreenshot(screenshot);
        std::printf(ok ? "wrote %s\n" : "failed to write %s\n", screenshot);
        return ok ? 0 : 1;
    }

    return game.run(maxFrames);
}
