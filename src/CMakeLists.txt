# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
#
# SDL2 game target. Links the engine-agnostic core (foss_core) and SDL2. The
# renderer is software-capable so the game runs on old, low-end hardware.

set(FOSS_GAME_SOURCES
    main.cpp
    game/Game.cpp
    game/Font.cpp
)

# Embed the demo mission JSON into the binary so the game stays a single,
# self-contained executable (AGENTS.md 1b) while still being data-driven
# (ADR-0003). The generated source is produced at configure time.
set(FOSS_DEMO_MISSION_FILE "${CMAKE_SOURCE_DIR}/assets/data/missions/demo.json")
file(READ "${FOSS_DEMO_MISSION_FILE}" FOSS_DEMO_MISSION_JSON)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/game/EmbeddedMission.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/game/EmbeddedMission.cpp"
    @ONLY
)
list(APPEND FOSS_GAME_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/game/EmbeddedMission.cpp")

add_executable(FreeOpenSourceSyndicate ${FOSS_GAME_SOURCES})

target_include_directories(FreeOpenSourceSyndicate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# The game layer sits on top of the engine-agnostic core (ADR-0002).
target_link_libraries(FreeOpenSourceSyndicate PRIVATE foss_core)

# SDL2: prefer the CMake config package, fall back to pkg-config.
find_package(SDL2 QUIET)
if(TARGET SDL2::SDL2)
    target_link_libraries(FreeOpenSourceSyndicate PRIVATE SDL2::SDL2)
elseif(SDL2_FOUND)
    target_include_directories(FreeOpenSourceSyndicate PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(FreeOpenSourceSyndicate PRIVATE ${SDL2_LIBRARIES})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    target_include_directories(FreeOpenSourceSyndicate PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(FreeOpenSourceSyndicate PRIVATE ${SDL2_LIBRARIES})
endif()

set_target_properties(FreeOpenSourceSyndicate PROPERTIES
    VERSION 0.1.0
)

# Headless smoke test: boots the game with the dummy video driver, runs a few
# frames, and exits. Verifies the SDL layer initialises and ticks.
if(BUILD_TESTS)
    add_test(NAME game_smoke
             COMMAND FreeOpenSourceSyndicate --headless --frames 30)
    # Attract mode: runs the squad AI through combat and win/lose headless. Guards
    # against crashes in the mission loop without needing a display.
    add_test(NAME game_demo
             COMMAND FreeOpenSourceSyndicate --headless --demo --frames 3000)
    # Low-spec runtime path (ADR-0008): small window + software renderer, headless.
    add_test(NAME game_low_spec
             COMMAND FreeOpenSourceSyndicate --headless --low-spec --frames 30)
endif()
