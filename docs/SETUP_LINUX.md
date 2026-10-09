# Setup Instructions for Linux / Ubuntu / Linux Mint

This guide will help you set up the complete development environment for Free Open Source Syndicate from scratch.

## Prerequisites

You'll need:
- A terminal or command line interface
- About 2-3 GB of disk space (for Cocos2d-x dependency and build artifacts)
- Internet connection for downloading dependencies

## Step 1: Install Build Tools and Dependencies

### Update package manager

```bash
sudo apt-get update
sudo apt-get upgrade -y
```

### Install required development tools

```bash
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config
```

### Install graphics and media libraries

```bash
sudo apt-get install -y \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libogg-dev \
    libvorbis-dev \
    libopenal-dev \
    libglvnd-dev
```

### Verify installations

```bash
gcc --version
g++ --version
cmake --version
git --version
```

All commands should output version information without errors.

## Step 2: Clone the Repository

```bash
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate
```

## Step 3: Bootstrap the Cocos2d-x Engine

```bash
chmod +x scripts/bootstrap.sh
./scripts/bootstrap.sh
```

This will download and extract the Cocos2d-x engine into `third_party/cocos2d-x/`. This step may take a few minutes.

## Step 4: Build the Project

### Using the build script (Recommended)

```bash
chmod +x scripts/build.sh
./scripts/build.sh
```

### Or manually:

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release -j$(nproc)
```

### Run the game

```bash
./FreeOpenSourceSyndicate
```

---

## Development Setup Options

### Option A: VS Code (Recommended IDE)

#### Install VS Code

```bash
sudo apt-get install -y code
```

#### Install essential extensions

1. Open VS Code
2. Go to Extensions (Ctrl+Shift+X)
3. Search for and install:
   - **C/C++** (by Microsoft)
   - **CMake** (by twxs)
   - **CMake Tools** (by Microsoft)
   - **Git Graph** (by mhutchie)

#### Open the project

```bash
code FreeOpenSourceSyndicate/
```

#### Configure CMake

1. In VS Code, press Ctrl+Shift+P
2. Type "CMake: Select a Kit"
3. Select your GCC compiler (e.g., "GCC 11.4.0")
4. Type "CMake: Configure" and press Enter

#### Build from VS Code

1. Press Ctrl+Shift+B to build
2. Or click the Build button in the CMake Tools panel (left sidebar)
3. The executable will be in the `build/` directory

#### Debug

1. Open `.vscode/launch.json` (or create it with CMake Tools)
2. Set breakpoints by clicking on the left margin of code lines
3. Press F5 to start debugging

### Option B: Text Editor + Terminal

#### Install a lightweight text editor

```bash
# Option 1: Nano (simplest)
sudo apt-get install -y nano

# Option 2: Vim (more powerful)
sudo apt-get install -y vim

# Option 3: Gedit (GUI)
sudo apt-get install -y gedit
```

#### Workflow

```bash
# Edit files with nano
nano src/game/GameScene.cpp

# Build from terminal
cd build
cmake --build . --config Release -j$(nproc)

# Run
./FreeOpenSourceSyndicate
```

#### Create a build helper script

Create `scripts/dev.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"/.. 
cd build
cmake --build . --config Release -j$(nproc)
echo "Build complete. Run with: ./FreeOpenSourceSyndicate"
```

Then use it:

```bash
chmod +x scripts/dev.sh
./scripts/dev.sh
```

---

## Troubleshooting

### CMake not found

```bash
sudo apt-get install -y cmake
cmake --version
```

### Missing OpenGL

```bash
sudo apt-get install -y libglvnd-dev mesa-common-dev
```

### Build fails with missing headers

```bash
# Clean and rebuild
cd build
rm -rf *
cmake ..
cmake --build . -j$(nproc)
```

### Permission denied when running scripts

```bash
chmod +x scripts/*.sh
```

### Cocos2d-x fetch fails

Check your internet connection and try again:

```bash
rm -rf third_party/cocos2d-x
./scripts/bootstrap.sh
```

---

## Next Steps

1. Read [ARCHITECTURE.md](docs/ARCHITECTURE.md) for project design overview
2. Read [CONTRIBUTING.md](docs/CONTRIBUTING.md) for coding guidelines
3. Start with a small feature in `src/entities/` or `src/systems/`
4. Build and test locally before submitting a pull request

## Getting Help

- Check existing issues on GitHub
- Open a new issue if you encounter problems
- See [CONTRIBUTING.md](docs/CONTRIBUTING.md) for discussion guidelines
