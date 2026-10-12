<!--
AI/LLM-generated file. Do not treat as human-written source documentation.
This comment must be preserved in future generated files.
-->
# Setup Instructions for Windows

This guide will help you set up the complete development environment for Free Open Source Syndicate on Windows.

## Prerequisites

You'll need:
- Windows 10 or later
- About 2-3 GB of disk space (for Cocos2d-x dependency and build artifacts)
- Internet connection for downloading dependencies
- Administrator access to install software

## Step 1: Install Visual Studio

Visual Studio Community Edition is free and recommended for C++ development.

### Download and Install

1. Go to https://visualstudio.microsoft.com/downloads/
2. Download **Visual Studio Community**
3. Run the installer
4. When prompted, select:
   - **Desktop development with C++**
   - Check **CMake tools for Windows**
   - Check **Git for Windows**
5. Click Install and wait for completion (this may take 30 minutes)

### Verify Installation

Open "Developer Command Prompt for Visual Studio" and run:

```cmd
clang --version
cl.exe
```

Both should output version information.

## Step 2: Install CMake

### Download and Install

1. Go to https://cmake.org/download/
2. Download the Windows x64 Installer
3. Run the installer
4. **Important:** Check "Add CMake to the system PATH for all users"
5. Complete the installation

### Verify Installation

Open a new Command Prompt and run:

```cmd
cmake --version
```

Should output CMake version (3.15 or higher).

## Step 3: Install Git

If not already installed with Visual Studio:

1. Go to https://git-scm.com/download/win
2. Download the Windows installer
3. Run and follow the default installation steps

### Verify Installation

```cmd
git --version
```

## Step 4: Clone the Repository

Open a Command Prompt or PowerShell:

```cmd
git clone https://github.com/FreeOpenSourceSyndicate/FreeOpenSourceSyndicate.git
cd FreeOpenSourceSyndicate
```

## Step 5: Bootstrap the Cocos2d-x Engine

In Command Prompt or Git Bash:

```bash
# Using Git Bash (recommended) or bash shell
chmod +x scripts/bootstrap.sh
./scripts/bootstrap.sh
```

Or if using only Command Prompt, manually download:

```cmd
git clone --depth 1 --branch master https://github.com/cocos/cocos2d-x.git third_party\cocos2d-x
```

## Step 6: Build the Project

### Using Visual Studio (Recommended)

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Or open the generated `.sln` file:

```cmd
start FreeOpenSourceSyndicate.sln
```

Then in Visual Studio:
1. Right-click "FreeOpenSourceSyndicate" project
2. Select "Set as Startup Project"
3. Build > Build Solution (Ctrl+Shift+B)
4. Debug > Start Without Debugging (Ctrl+F5)

### Using Command Line

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
```

### Run the game

```cmd
.\Release\FreeOpenSourceSyndicate.exe
```

---

## Development Setup Options

### Option A: Visual Studio IDE (Recommended)

#### Open and configure project

1. Open Visual Studio
2. File > Open > Folder
3. Navigate to the FreeOpenSourceSyndicate folder
4. Select Folder
5. Wait for CMake configuration to complete

#### Build and Run

1. Select "FreeOpenSourceSyndicate" from the Startup Item dropdown (top of window)
2. Build > Build Solution (Ctrl+Shift+B)
3. Debug > Start Without Debugging (Ctrl+F5)

#### Debug

1. Click on a line number to set a breakpoint
2. Debug > Start Debugging (F5)
3. Use Debug toolbar and Watch window to inspect variables

### Option B: VS Code

#### Install VS Code

1. Go to https://code.visualstudio.com/
2. Download and install the Windows version

#### Install extensions

1. Open VS Code
2. Go to Extensions (Ctrl+Shift+X)
3. Search for and install:
   - **C/C++** (by Microsoft)
   - **CMake** (by twxs)
   - **CMake Tools** (by Microsoft)
   - **Git Graph** (by mhutchie)

#### Open and configure project

```cmd
code FreeOpenSourceSyndicate
```

1. In VS Code, press Ctrl+Shift+P
2. Type "CMake: Select a Kit"
3. Choose "Visual Studio Community 2022 Release - x64"
4. Type "CMake: Configure" and press Enter

#### Build and Run

1. Press Ctrl+Shift+B to build
2. Or click Build in CMake Tools panel
3. Debug > Run and Debug (F5)

### Option C: Text Editor + Command Line

#### Install a lightweight text editor

```cmd
# Notepad++ (GUI editor, recommended for Windows)
# Download from https://notepad-plus-plus.org/
# Or use Windows built-in Notepad
```

#### Workflow

```cmd
# Edit with Notepad++
start notepad++ src\game\GameScene.cpp

# Build with Command Prompt
cd build
cmake --build . --config Release

# Run
.\Release\FreeOpenSourceSyndicate.exe
```

#### Create a build helper script

Create `scripts\dev.cmd`:

```batch
@echo off
cd /d "%~dp0\.."
cd build
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
echo Build complete. Run with: .\Release\FreeOpenSourceSyndicate.exe
```

Then use it:

```cmd
scripts\dev.cmd
```

---

## Troubleshooting

### CMake not found

1. Reinstall CMake with "Add to PATH" option checked
2. Restart Command Prompt after installation
3. Verify: `cmake --version`

### Visual Studio not found

```cmd
# Run from Developer Command Prompt for Visual Studio
# Start > Visual Studio > Developer Command Prompt
```

### Build fails with missing dependencies

1. Make sure Visual Studio "Desktop development with C++" is installed
2. Run Visual Studio Installer
3. Click Modify
4. Ensure "C++ build tools" is selected
5. Apply and wait for completion

### Git not recognized

Restart Command Prompt after installing Git. If still not found:

```cmd
# Add Git to PATH manually
set PATH=%PATH%;C:\Program Files\Git\cmd
```

### Cocos2d-x fetch fails

Check your internet connection. Try manually:

```cmd
rmdir /s /q third_party\cocos2d-x
git clone --depth 1 --branch master https://github.com/cocos/cocos2d-x.git third_party\cocos2d-x
```

### Permission denied errors

If you get permission errors, try running Command Prompt as Administrator:

1. Right-click on Command Prompt
2. Select "Run as administrator"

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
