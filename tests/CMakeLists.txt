# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
#
# Core unit tests. These link only against foss_core (no engine dependency),
# so they build and run anywhere, including CI.

set(FOSS_TEST_SOURCES
    main.cpp
    test_isometric.cpp
    test_tilegrid.cpp
    test_pathfinding.cpp
    test_agent.cpp
    test_rng.cpp
    test_mission.cpp
    test_json.cpp
    test_missiondata.cpp
)

add_executable(run_tests ${FOSS_TEST_SOURCES})

target_link_libraries(run_tests PRIVATE foss_core)

target_include_directories(run_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Lets tests locate repository files (e.g. assets/data) regardless of the
# directory ctest runs them from.
target_compile_definitions(run_tests PRIVATE FOSS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

set_target_properties(run_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_test(NAME core_unit_tests COMMAND run_tests)
