# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
#
# SDL2 game target. Links the engine-agnostic core (fossynd_core) and SDL2. The
# renderer is software-capable so the game runs on old, low-end hardware.

set(FOSSYND_GAME_SOURCES
    main.cpp
    game/Game.cpp
    game/Font.cpp
    game/Minimap.cpp
    game/Sprite.cpp
)

# Embed the demo mission JSON into the binary so the game stays a single,
# self-contained executable (AGENTS.md 1b) while still being data-driven
# (ADR-0003). The generated source is produced at configure time.
set(FOSSYND_DEMO_MISSION_FILE "${CMAKE_SOURCE_DIR}/assets/data/missions/demo.json")
file(READ "${FOSSYND_DEMO_MISSION_FILE}" FOSSYND_DEMO_MISSION_JSON)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/game/EmbeddedMission.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/game/EmbeddedMission.cpp"
    @ONLY
)
list(APPEND FOSSYND_GAME_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/game/EmbeddedMission.cpp")

# Embed the authored agent sprite sheet the same way, so the sprite pipeline is
# data-driven but the binary stays self-contained.
set(FOSSYND_SPRITES_FILE "${CMAKE_SOURCE_DIR}/assets/sprites/agents.txt")
file(READ "${FOSSYND_SPRITES_FILE}" FOSSYND_AGENT_SPRITES_TEXT)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/game/EmbeddedSprites.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/game/EmbeddedSprites.cpp"
    @ONLY
)
list(APPEND FOSSYND_GAME_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/game/EmbeddedSprites.cpp")

# Embed the English string table so the game is localisable from data (ADR-0016)
# while still running with no data files present.
set(FOSSYND_LANG_FILE "${CMAKE_SOURCE_DIR}/assets/lang/en.json")
file(READ "${FOSSYND_LANG_FILE}" FOSSYND_ENGLISH_LANG_JSON)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/game/EmbeddedLang.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/game/EmbeddedLang.cpp"
    @ONLY
)
list(APPEND FOSSYND_GAME_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/game/EmbeddedLang.cpp")

add_executable(FOSSynd ${FOSSYND_GAME_SOURCES})

# The version shown in the window title, the HUD, and the title card. Single
# source of truth: the project() version in the top-level CMakeLists.txt.
target_compile_definitions(FOSSynd PRIVATE FOSSYND_VERSION="${PROJECT_VERSION}")

target_include_directories(FOSSynd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# The game layer sits on top of the engine-agnostic core (ADR-0002).
target_link_libraries(FOSSynd PRIVATE fossynd_core)

# SDL2: prefer the CMake config package, fall back to pkg-config.
find_package(SDL2 QUIET)
if(TARGET SDL2::SDL2)
    target_link_libraries(FOSSynd PRIVATE SDL2::SDL2)
elseif(SDL2_FOUND)
    target_include_directories(FOSSynd PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(FOSSynd PRIVATE ${SDL2_LIBRARIES})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    target_include_directories(FOSSynd PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(FOSSynd PRIVATE ${SDL2_LIBRARIES})
endif()

set_target_properties(FOSSynd PROPERTIES
    VERSION ${PROJECT_VERSION}
)

# Install rules: the binary, the mission data, and the example config. The game
# searches these locations at runtime (see core/data/ContentPaths.h). Install is
# optional; the binary is self-contained thanks to the embedded mission.
include(GNUInstallDirs)
install(TARGETS FOSSynd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${CMAKE_SOURCE_DIR}/assets/data/missions/demo.json"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fossynd/missions)
install(FILES "${CMAKE_SOURCE_DIR}/assets/data/fossynd.conf.example"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fossynd)
install(FILES "${CMAKE_SOURCE_DIR}/assets/lang/en.json"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fossynd/lang)

# Ship the license and readme with the package (docs/LEGAL.md).
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/fossynd)
install(FILES "${CMAKE_SOURCE_DIR}/README.md"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/fossynd)

# Headless smoke test: boots the game with the dummy video driver, runs a few
# frames, and exits. Verifies the SDL layer initialises and ticks.
if(BUILD_TESTS)
    add_test(NAME game_smoke
             COMMAND FOSSynd --headless --frames 30)
    # Attract mode: runs the squad AI through combat and win/lose headless. Guards
    # against crashes in the mission loop without needing a display.
    add_test(NAME game_demo
             COMMAND FOSSynd --headless --demo --frames 3000)
    # Low-spec runtime path (ADR-0008): small window + software renderer, headless.
    add_test(NAME game_low_spec
             COMMAND FOSSynd --headless --low-spec --frames 30)
    # Title card: the authored pixel-art screen renders and the mission still
    # starts when the card times out. Guards the v0.4.0 title-card path.
    add_test(NAME game_title
             COMMAND FOSSynd --headless --title --frames 120)
    # Sprites: the authored agent/prop sheet bakes into textures for every role
    # (the depth-sorted sprite pass, Phase 2). Exits non-zero on a bad sheet.
    add_test(NAME game_sprites
             COMMAND FOSSynd --headless --check-sprites)
    # Minimap: the whole-board overview panel bakes and places a blip for every
    # living agent (Phase 2). Exits non-zero if a blip is missing or off-panel.
    add_test(NAME game_minimap
             COMMAND FOSSynd --headless --check-minimap)
    # Selection / control groups (issue #33): exercises the selection helpers and
    # the control-group round-trip headlessly. Exits non-zero on a broken
    # invariant.
    add_test(NAME game_selection
             COMMAND FOSSynd --headless --check-selection)
    # UI strings (issue #53): the string table loads (external file or embedded
    # copy) and resolves every key the HUD/title card reads. Exits non-zero if a
    # key is missing, so a broken assets/lang/en.json cannot hide behind the
    # literal fallback.
    add_test(NAME game_lang
             COMMAND FOSSynd --headless --check-lang)
endif()
