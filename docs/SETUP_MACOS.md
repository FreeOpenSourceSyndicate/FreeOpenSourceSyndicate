# Setup Instructions for macOS

This guide will help you set up the complete development environment for Free Open Source Syndicate on macOS.

## Prerequisites

You'll need:
- macOS 10.14 (Mojave) or later
- About 2-3 GB of disk space (for Cocos2d-x dependency and build artifacts)
- Internet connection for downloading dependencies
- Administrator access to install software

## Step 1: Install Homebrew (Package Manager)

Homebrew simplifies dependency management on macOS.

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Verify installation:

```bash
brew --version
```

## Step 2: Install Xcode Command Line Tools

The Xcode Command Line Tools include GCC/Clang compiler and git:

```bash
xcode-select --install
```

If you already have Xcode installed, you can skip this step.

Verify installation:

```bash
gcc --version
clang --version
git --version
```

## Step 3: Install Build Dependencies via Homebrew

```bash
brew install cmake
```

Verify:

```bash
cmake --version
```

Graphics libraries are typically built-in to macOS. If you encounter OpenGL issues, install:

```bash
brew install glfw3 glew
```

## Step 4: Clone the Repository

```bash
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate
```

## Step 5: Bootstrap the Cocos2d-x Engine

```bash
chmod +x scripts/bootstrap.sh
./scripts/bootstrap.sh
```

This will download and extract the Cocos2d-x engine into `third_party/cocos2d-x/`. This step may take a few minutes.

## Step 6: Build the Project

### Using the build script (Recommended)

```bash
chmod +x scripts/build.sh
./scripts/build.sh
```

### Or manually:

```bash
mkdir build
cd build
cmake .. -G Xcode
xcodebuild -configuration Release
```

Or with Make:

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release -j$(sysctl -n hw.ncpu)
```

### Run the game

```bash
./build/Release/FreeOpenSourceSyndicate
```

---

## Development Setup Options

### Option A: Xcode IDE (Recommended)

#### Open project in Xcode

```bash
chmod +x scripts/bootstrap.sh
./scripts/bootstrap.sh

mkdir build
cd build
cmake .. -G Xcode
open FreeOpenSourceSyndicate.xcodeproj
```

#### Build from Xcode

1. Product > Scheme > Select "FreeOpenSourceSyndicate"
2. Product > Build (⌘B)
3. Product > Run (⌘R)

#### Debug

1. Set breakpoints by clicking on the line number
2. Product > Run (⌘R) - will stop at breakpoints
3. Use the Debug Navigator and Variables view to inspect state

### Option B: VS Code

#### Install VS Code

```bash
brew install --cask visual-studio-code
```

#### Install extensions

1. Open VS Code
2. Go to Extensions (Cmd+Shift+X)
3. Search for and install:
   - **C/C++** (by Microsoft)
   - **CMake** (by twxs)
   - **CMake Tools** (by Microsoft)
   - **Git Graph** (by mhutchie)

#### Open and configure project

```bash
code FreeOpenSourceSyndicate/
```

1. In VS Code, press Cmd+Shift+P
2. Type "CMake: Select a Kit"
3. Select your Clang compiler
4. Type "CMake: Configure" and press Enter

#### Build

1. Press Cmd+Shift+B to build
2. Or click Build in the CMake Tools panel

### Option C: Text Editor + Terminal

#### Install a lightweight text editor

```bash
# Option 1: Nano (simplest, built-in)
nano src/game/GameScene.cpp

# Option 2: Vim (built-in)
vim src/game/GameScene.cpp

# Option 3: Sublime Text
brew install --cask sublime-text
```

#### Workflow

```bash
# Edit files
nano src/game/GameScene.cpp

# Build
cd build
cmake --build . --config Release -j$(sysctl -n hw.ncpu)

# Run
./Release/FreeOpenSourceSyndicate
```

#### Create a build helper script

Create `scripts/dev.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"/.. 
cd build
cmake --build . --config Release -j$(sysctl -n hw.ncpu)
echo "Build complete. Run with: ./Release/FreeOpenSourceSyndicate"
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
brew install cmake
cmake --version
```

### Xcode license agreement not accepted

```bash
sudo xcode-select --reset
sudo xcode-select --install
sudo xcodebuild -license accept
```

### Build fails with missing Xcode tools

```bash
xcode-select --install
```

### Cocos2d-x fetch fails

Check your internet connection and try again:

```bash
rm -rf third_party/cocos2d-x
./scripts/bootstrap.sh
```

### Permission denied when running scripts

```bash
chmod +x scripts/*.sh
```

### OpenGL issues

```bash
brew install glfw3 glew
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
