/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "game/Game.h"

int main(int argc, char** argv)
{
    int maxFrames = 0;
    bool headless = false;
    bool demo = false;
    bool lowSpec = false;
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
        else if (std::strcmp(argv[i], "--demo") == 0)
        {
            demo = true;
        }
        else if (std::strcmp(argv[i], "--low-spec") == 0)
        {
            lowSpec = true;
        }
    }

#if defined(FOSS_LOW_SPEC_BUILD)
    // A low-spec build always runs in low-spec mode unless the caller opts out.
    lowSpec = true;
#endif

    // Default window size; the low-spec profile shrinks it (ADR-0008).
    int width = lowSpec ? 320 : 960;
    int height = lowSpec ? 200 : 600;

    foss::game::Game game;
    if (!game.init(width, height, headless, lowSpec))
    {
        std::fprintf(stderr, "Failed to start the game.\n");
        return 1;
    }
    game.setAutoPlay(demo);

    if (screenshot != nullptr)
    {
        // Optionally advance the simulation first (e.g. to capture mid-combat).
        if (maxFrames > 0)
        {
            game.run(maxFrames);
        }
        const bool ok = game.saveScreenshot(screenshot);
        std::printf(ok ? "wrote %s\n" : "failed to write %s\n", screenshot);
        return ok ? 0 : 1;
    }

    return game.run(maxFrames);
}
