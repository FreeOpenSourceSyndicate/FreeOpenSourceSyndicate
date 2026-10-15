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
    test_lineofsight.cpp
    test_agent.cpp
    test_timestep.cpp
    test_rng.cpp
    test_mission.cpp
    test_json.cpp
    test_missiondata.cpp
    test_config.cpp
    test_compliance_beacon.cpp
    test_sprites.cpp
    test_tilesprites.cpp
    test_worldmap.cpp
    test_economy.cpp
    test_research.cpp
    test_equipment.cpp
    test_i18n.cpp
    test_content_validation.cpp
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

# The coordination ledger tool (scripts/coord) is Python, so it gets its own
# ctest entry rather than joining the C++ harness. It is a hard gate: a
# coordination regression must not merge (docs/COORDINATION.md, ADR-0020).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME coord_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_coord.py)

    # String-table key coverage (issue #191): every key-shaped literal in src/
    # must resolve in assets/lang/en.json, so a localised run cannot silently
    # show an English fallback. Python because it scans source text.
    add_test(NAME i18n_key_coverage
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_i18n_keys.py)
else()
    message(STATUS "Python3 not found: skipping coord_tests")
endif()

# Under the sanitizer profile (FOSSYND_SANITIZE, ADR-0015) make the run strict:
# the first error aborts, leak detection is on, and UBSan prints a stack trace.
if(FOSSYND_SANITIZE)
    set_tests_properties(core_unit_tests PROPERTIES ENVIRONMENT
        "ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:check_initialization_order=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1")
endif()
