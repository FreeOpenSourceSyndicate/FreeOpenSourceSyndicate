# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
#
# SDL2 game target. Links the engine-agnostic core (fossynd_core) and SDL2. The
# renderer is software-capable so the game runs on old, low-end hardware.

set(FOSSYND_GAME_SOURCES
    main.cpp
    game/Game.cpp
    game/Font.cpp
    game/Hud.cpp
    game/TitleCard.cpp
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
    # Windows: link SDL2main *before* SDL2. SDL provides WinMain() and calls
    # main() from it, so SDL2main is required; and it references SDL symbols that
    # the SDL2 import library resolves, so MinGW's order-sensitive static link
    # needs SDL2main first. Linux/macOS use the real main() and need nothing.
    if(WIN32 AND TARGET SDL2::SDL2main)
        target_link_libraries(FOSSynd PRIVATE SDL2::SDL2main)
    endif()
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
#
# The shared catalogues (weapons.json/agents.json) ship next to `missions/`
# because `--check-content` (and any external mission) resolves `weaponId`/
# `agentId` against them: a package without the catalogues cannot validate the
# content it ships (issue #91).
include(GNUInstallDirs)
install(TARGETS FOSSynd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# Windows: FOSSynd.exe links SDL2.dll, so ship the runtime DLL next to the
# binary to keep the ZIP self-contained (unpack and run). Linux/macOS resolve
# SDL2 from the system, so nothing extra is installed there. Issue #133.
if(WIN32 AND TARGET SDL2::SDL2)
    install(FILES "$<TARGET_FILE:SDL2::SDL2>" DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
# Install every mission, not just `demo.json`: a mission added to the tree must
# reach the package or an installed game silently loses it (issue #126).
# `FILES_MATCHING` keeps stray non-mission files out.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/data/missions/"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fossynd/missions
        FILES_MATCHING PATTERN "*.json")
install(FILES
            "${CMAKE_SOURCE_DIR}/assets/data/weapons.json"
            "${CMAKE_SOURCE_DIR}/assets/data/agents.json"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fossynd)
install(FILES "${CMAKE_SOURCE_DIR}/assets/data/fossynd.conf.example"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fossynd)
# Install the whole language directory, not just English: a translator's file
# added to the tree must reach the package or an installed game can only ever
# find `en.json` (issue #103). `FILES_MATCHING` keeps stray non-locale files out.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/lang/"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fossynd/lang
        FILES_MATCHING PATTERN "*.json")

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
    # Shift-click waypoint queue (issue #75): exercises orderSelectedMove against
    # the real Mission queue headlessly. Exits non-zero on a broken invariant.
    add_test(NAME game_waypoints
             COMMAND FOSSynd --headless --check-waypoints)
    # Box-select (issue #94): the drag rectangle must select exactly the enclosed
    # living player agents (and add on shift-drag). Exits non-zero on a broken
    # invariant.
    add_test(NAME game_box_select
             COMMAND FOSSynd --headless --check-box-select)
    # Queued-route overlay (issue #89): the screen-space route the renderer draws
    # for a selected agent must start at the agent and visit every queued
    # waypoint in order. Exits non-zero on a broken invariant.
    add_test(NAME game_waypoint_path
             COMMAND FOSSynd --headless --check-waypoint-path)
    # Integer zoom (issue #122): every level keeps the screen<->tile round-trip
    # exact and the level stays clamped, so clicks and orders stay correct when
    # zoomed. Exits non-zero on a broken invariant.
    add_test(NAME game_zoom
             COMMAND FOSSynd --headless --check-zoom)
    # `--zoom` selects the initial level and must clamp an out-of-range value
    # rather than fail or render nothing; both runs must exit 0.
    add_test(NAME game_zoom_flag
             COMMAND FOSSynd --headless --zoom 2 --frames 1)
    add_test(NAME game_zoom_clamped
             COMMAND FOSSynd --headless --zoom 99 --frames 1)
    # Content validation (issue #81): resolve every mission's weaponId/agentId
    # against the catalogues in the shipped assets/data and exit non-zero on a
    # dangling reference. Absolute path so it works from any build directory.
    add_test(NAME game_content
             COMMAND FOSSynd --headless --check-content ${CMAKE_SOURCE_DIR}/assets/data)
    # UI strings (issue #53): the string table loads (external file or embedded
    # copy) and resolves every key the HUD/title card reads. Exits non-zero if a
    # key is missing, so a broken assets/lang/en.json cannot hide behind the
    # literal fallback.
    add_test(NAME game_lang
             COMMAND FOSSynd --headless --check-lang)
    # Locale switch (issue #123): `--lang de` must overlay the German table over
    # the English fallback, and `--lang xx` (unknown) must warn and still start
    # in English rather than fail. `--frames 1` runs one headless frame so the
    # init path (which loads the language) is exercised, not just the flag.
    # WORKING_DIRECTORY is the repo root so the `assets/lang/*.json` files are
    # found (the documented "run from the repo root" fallback); otherwise a
    # build-dir cwd would only ever see the embedded English copy.
    add_test(NAME game_lang_de
             COMMAND FOSSynd --headless --lang de --check-lang de --frames 1)
    set_tests_properties(game_lang_de PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME game_lang_unknown_falls_back
             COMMAND FOSSynd --headless --lang xx --check-lang en --frames 1)
    set_tests_properties(game_lang_unknown_falls_back PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # CLI self-description (issue #147): `--version` prints the build version and
    # exits 0 without initialising SDL or loading content; the PASS regex fails
    # the test if the output is empty or not version-shaped. `--help` likewise
    # prints a usage block listing the flags.
    add_test(NAME game_version
             COMMAND FOSSynd --version)
    set_tests_properties(game_version PROPERTIES PASS_REGULAR_EXPRESSION "FOSSynd [0-9]")
    add_test(NAME game_help
             COMMAND FOSSynd --help)
    set_tests_properties(game_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: FOSSynd")
    # Window size (issue #152): a valid --window runs headless and exits 0; the
    # parse/validation is also covered by the invalid-input cases below.
    add_test(NAME game_window
             COMMAND FOSSynd --headless --window 640x480 --frames 1)
    add_test(NAME game_window_invalid
             COMMAND FOSSynd --headless --window 0x0 --frames 1)
    set_tests_properties(game_window_invalid PROPERTIES WILL_FAIL TRUE)
    add_test(NAME game_window_malformed
             COMMAND FOSSynd --headless --window 640 --frames 1)
    set_tests_properties(game_window_malformed PROPERTIES WILL_FAIL TRUE)
endif()
