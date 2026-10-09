# Build Instructions

This document provides platform-specific instructions for building Free Open Source Syndicate on Linux, macOS, and Windows.

## Prerequisites (All Platforms)

- **CMake** 3.15 or higher
- **C++17** compatible compiler
- **Git**

## Linux / Ubuntu / Linux Mint

### Install Dependencies

```bash
# Ubuntu/Debian-based systems
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    pkg-config \
    libogg-dev \
    libvorbis-dev \
    libopenal-dev

# For Linux Mint (based on Ubuntu)
# Same commands as above
```

### Build Steps

```bash
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Run the game
./FreeOpenSourceSyndicate
```

### Troubleshooting (Linux)

**Missing OpenGL libraries:**
```bash
sudo apt-get install -y libglvnd-dev libglvnd0
```

**CMake can't find packages:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

---

## macOS

### Install Dependencies

```bash
# Using Homebrew
brew install cmake git

# Xcode Command Line Tools (required for compilation)
xcode-select --install
```

### Build Steps

```bash
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

# Create build directory
mkdir build && cd build

# Configure for Xcode
cmake .. -G Xcode

# Build via Xcode
xcodebuild -configuration Release

# Or build via Make
cmake ..
make -j$(sysctl -n hw.ncpu)

# Run the game
./Release/FreeOpenSourceSyndicate
```

### Troubleshooting (macOS)

**Xcode license agreement not accepted:**
```bash
sudo xcode-select --reset
sudo xcode-select --install
sudo xcodebuild -license accept
```

**CMake not found:**
```bash
brew install cmake
```

---

## Windows

### Install Dependencies

**Visual Studio 2019 or later (Recommended):**
- Download from https://visualstudio.microsoft.com/
- Install "Desktop development with C++" workload
- Include CMake support

**Or use MinGW:**
- Download from https://www.mingw-w64.org/
- Add to system PATH

**CMake:**
- Download from https://cmake.org/download/
- Run installer and add to PATH

**Git:**
- Download from https://git-scm.com/
- Install with default options

### Build Steps (Visual Studio)

```cmd
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

# Create build directory
mkdir build && cd build

# Generate Visual Studio project
cmake .. -G "Visual Studio 16 2019" -A x64

# Build (Release configuration)
cmake --build . --config Release

# Run the game
.\Release\FreeOpenSourceSyndicate.exe
```

### Build Steps (MinGW)

```cmd
# Clone the repository
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate

# Create build directory
mkdir build && cd build

# Generate MinGW Makefiles
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j%NUMBER_OF_PROCESSORS%

# Run the game
FreeOpenSourceSyndicate.exe
```

### Troubleshooting (Windows)

**Visual Studio not found:**
- Ensure VS is installed with C++ support
- Run from "Developer Command Prompt for VS"

**MinGW compiler errors:**
```cmd
gcc --version
g++ --version
```
Ensure both are in PATH.

**CMake configuration fails:**
```cmd
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_GENERATOR_PLATFORM=x64
```

---

## General Troubleshooting

### Clean build
```bash
cd build
rm -rf *          # Linux/macOS
del /q *          # Windows CMD
cd ..
```

### Enable verbose output
```bash
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
make VERBOSE=1    # Linux/macOS
```

### Check CMake version
```bash
cmake --version
```

Must be 3.15 or higher.

---

## Next Steps

Once built successfully, see [ARCHITECTURE.md](ARCHITECTURE.md) for development guidelines.
