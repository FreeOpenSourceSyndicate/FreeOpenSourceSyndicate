<!-- AI/LLM-generated file. Do not treat as human-written source documentation. This comment must be preserved in future generated files. -->
# Build Instructions

This document provides platform-specific instructions for building Free Open Source Syndicate on Linux, macOS, and Windows.

## Two build modes

The project builds in two modes:

| Mode | Command flag | Needs the engine? | Use for |
|------|--------------|-------------------|---------|
| **Core** (default) | `-DFOSS_BUILD_GAME=OFF` | No | All game logic, unit tests, CI |
| **Full game** | `-DFOSS_BUILD_GAME=ON` | Yes (Cocos2d-x) | Running the actual game |

**Prefer the core build while developing.** Game logic lives in `src/core/` and
must build without the engine (see `AGENTS.md`, ADR-0002).

### Core build (fast)

```bash
cmake -S . -B build -DFOSS_BUILD_GAME=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Prerequisites (All Platforms)

- **CMake** 3.15 or higher
- **C++17** compatible compiler
- **Git**

## Fast Start

This project includes helper scripts to fetch and build the Cocos2d-x engine dependency.

### Linux / Ubuntu / Linux Mint

```bash
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

# Fetch Cocos2d-x dependency
chmod +x scripts/fetch_cocos2dx.sh scripts/build.sh
./scripts/fetch_cocos2dx.sh

# Configure and build
./scripts/build.sh
```

### macOS

```bash
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

chmod +x scripts/fetch_cocos2dx.sh scripts/build.sh
./scripts/fetch_cocos2dx.sh
./scripts/build.sh
```

### Windows

```powershell
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

# In PowerShell or Git Bash
chmod +x scripts/fetch_cocos2dx.sh scripts/build.sh
./scripts/fetch_cocos2dx.sh
./scripts/build.sh
```

## Manual Build Steps

If you prefer a direct CMake workflow:

### Linux / Ubuntu / Linux Mint

```bash
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

# Fetch dependency
./scripts/fetch_cocos2dx.sh

# Create build directory
mkdir build && cd build

# Configure and build
cmake .. -DCOCOS2D_X_ROOT="$(pwd)/../third_party/cocos2d-x"
cmake --build . --config Release
```

### Windows (Visual Studio)

```cmd
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

# Fetch dependency
bash scripts/fetch_cocos2dx.sh

# Create build directory
mkdir build && cd build

# Configure for Visual Studio
cmake .. -G "Visual Studio 17 2022" -A x64 -DCOCOS2D_X_ROOT="%CD%\..\third_party\cocos2d-x"
cmake --build . --config Release
```

### macOS

```bash
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

./scripts/fetch_cocos2dx.sh
mkdir build && cd build
cmake .. -DCOCOS2D_X_ROOT="$(pwd)/../third_party/cocos2d-x"
cmake --build . --config Release
```

## Notes

- The helper scripts are intentionally minimal at this stage and create a structured integration path for the Cocos2d-x engine.
- The full gameplay scene and engine integration will be added in the next phase of development.
- The project remains compatible with Linux, Windows, and macOS build targets.
