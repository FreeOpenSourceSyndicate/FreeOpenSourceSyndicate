# AI/LLM-generated file. Do not treat as human-written source documentation. This comment must be preserved in future generated files.
cmake_minimum_required(VERSION 3.15)
project(FOSSynd VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# The SDL2 game target is opt-in so the engine-agnostic core and its tests can
# build anywhere without graphics dependencies (this is what CI runs).
option(FOSSYND_BUILD_GAME "Build the SDL2 game target (requires libsdl2-dev)" OFF)
option(BUILD_TESTS "Build core unit tests" ON)

# Low-spec profile: optimise for very old, low-end hardware (AGENTS.md 1b,
# ADR-0008). Favours a small binary over raw speed; the runtime also defaults to
# a small window and the software renderer when this is set.
option(FOSSYND_LOW_SPEC "Optimise the build for very old, low-end hardware" OFF)
if(FOSSYND_LOW_SPEC)
    add_compile_options(-Os -fno-unwind-tables -fno-asynchronous-unwind-tables)
    add_link_options(-s)
    add_compile_definitions(FOSSYND_LOW_SPEC_BUILD=1)
    message(STATUS "Low-spec profile enabled (-Os, stripped, low-res defaults)")
endif()

# Sanitizer profile: build the engine-agnostic core and its tests with
# AddressSanitizer + UndefinedBehaviorSanitizer so memory and logic bugs are
# caught before merge. Off by default, so normal and low-spec builds are
# unchanged and pay no runtime cost (ADR-0015). Intended for a core-only build;
# the sanitizer CI job uses -DFOSSYND_BUILD_GAME=OFF so no graphics deps are
# needed. `-fno-sanitize-recover` makes the first UBSan report a hard failure.
option(FOSSYND_SANITIZE "Build core and tests with ASan + UBSan (developer/CI only)" OFF)
if(FOSSYND_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "Sanitizer profile enabled (ASan + UBSan, no-recover)")
endif()

if(WIN32)
    message(STATUS "Building for Windows")
elseif(APPLE)
    message(STATUS "Building for macOS")
else()
    message(STATUS "Building for Linux")
endif()

# Engine-agnostic core: always built, always tested.
add_subdirectory(src/core)

if(BUILD_TESTS)
    enable_testing()
endif()

if(FOSSYND_BUILD_GAME)
    add_subdirectory(src)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "=== FOSSynd ===")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build game target: ${FOSSYND_BUILD_GAME}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Low-spec profile: ${FOSSYND_LOW_SPEC}")
message(STATUS "Sanitizers: ${FOSSYND_SANITIZE}")
message(STATUS "")
