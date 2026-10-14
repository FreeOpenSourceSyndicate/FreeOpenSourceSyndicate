# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
#
# Core unit tests. These link only against fossynd_core (no engine dependency),
# so they build and run anywhere, including CI.

set(FOSSYND_TEST_SOURCES
    main.cpp
    test_isometric.cpp
    test_tilegrid.cpp
    test_pathfinding.cpp
    test_agent.cpp
    test_timestep.cpp
    test_rng.cpp
    test_mission.cpp
    test_json.cpp
    test_missiondata.cpp
    test_config.cpp
    test_persuadertron.cpp
    test_sprites.cpp
    test_i18n.cpp
)

add_executable(run_tests ${FOSSYND_TEST_SOURCES})

target_link_libraries(run_tests PRIVATE fossynd_core)

target_include_directories(run_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Lets tests locate repository files (e.g. assets/data) regardless of the
# directory ctest runs them from.
target_compile_definitions(run_tests PRIVATE FOSSYND_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

set_target_properties(run_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_test(NAME core_unit_tests COMMAND run_tests)

# Under the sanitizer profile (FOSSYND_SANITIZE, ADR-0015) make the run strict:
# the first error aborts, leak detection is on, and UBSan prints a stack trace.
if(FOSSYND_SANITIZE)
    set_tests_properties(core_unit_tests PROPERTIES ENVIRONMENT
        "ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:check_initialization_order=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1")
endif()
