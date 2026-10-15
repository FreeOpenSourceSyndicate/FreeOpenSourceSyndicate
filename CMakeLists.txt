# AI/LLM-generated file. Do not treat as human-written source documentation. This comment must be preserved in future generated files.
cmake_minimum_required(VERSION 3.15)
project(FOSSynd VERSION 0.6.0 LANGUAGES CXX)

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

# Warnings-as-errors profile: the build always uses -Wall -Wextra -Wpedantic, but
# those stay warnings, so a new one can still land on `main` (AGENTS.md 4 Rule 9
# and the Definition of Done say it must not). Off by default so normal and
# low-spec builds keep the warnings-only behaviour; a CI lane turns it on.
option(FOSSYND_WERROR "Treat compiler warnings as errors" OFF)
if(FOSSYND_WERROR)
    add_compile_options(-Werror)
    message(STATUS "Warnings are errors (-Werror)")
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

# ---------------------------------------------------------------------------
# Packaging (CPack): produces a portable .tar.gz on every platform, plus a
# Debian .deb where dpkg-deb is available, a drag-and-drop .dmg on macOS, and a
# portable .zip on Windows — all built from the install() rules above, so the
# package layout stays in one place. Additive: nothing changes unless the
# `package` target is invoked.
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "FOSSynd")
set(CPACK_PACKAGE_VENDOR "FOSSynd project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "FOSSynd — an open-source recreation of the Syndicate (1993) gameplay")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "FOSSynd maintainers")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate")
set(CPACK_PACKAGE_FILE_NAME "FOSSynd-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")

set(CPACK_GENERATOR "TGZ")
if(UNIX AND NOT APPLE)
    find_program(FOSSYND_DPKG_DEB dpkg-deb)
    if(FOSSYND_DPKG_DEB)
        list(APPEND CPACK_GENERATOR "DEB")
    endif()
endif()
if(APPLE)
    # DragNDrop is macOS-only; hdiutil (which it needs) ships with the OS.
    list(APPEND CPACK_GENERATOR "DragNDrop")
endif()
if(WIN32)
    # ZIP is self-contained: no NSIS/WiX tooling needed on the runner, and it is
    # portable (unpack anywhere and run). FOSSynd.exe needs SDL2.dll next to it,
    # which the install() rules ship (see src/CMakeLists.txt). Issue #133.
    list(APPEND CPACK_GENERATOR "ZIP")
endif()

# Debian metadata. libsdl2-2.0-0 is the SDL2 runtime the game links against.
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "FOSSynd maintainers")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)

message(STATUS "")
message(STATUS "=== FOSSynd ===")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build game target: ${FOSSYND_BUILD_GAME}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Low-spec profile: ${FOSSYND_LOW_SPEC}")
message(STATUS "Sanitizers: ${FOSSYND_SANITIZE}")
message(STATUS "")
