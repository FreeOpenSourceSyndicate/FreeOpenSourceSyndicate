# AI/LLM-generated file. Do not treat as human-written source documentation. This comment must be preserved in future generated files.
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Full build by default: core + tests + the SDL2 game target.
# Pass FOSS_BUILD_GAME=OFF to skip the game (no SDL2 needed).
cmake .. \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DFOSS_BUILD_GAME="${FOSS_BUILD_GAME:-ON}"
cmake --build . --config "${CMAKE_BUILD_TYPE:-Release}" -j"$(nproc)"

echo "Build complete. Binary: ${BUILD_DIR}/src/FreeOpenSourceSyndicate"
