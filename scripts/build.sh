#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
COCOS2D_X_ROOT="${COCOS2D_X_ROOT:-${ROOT_DIR}/third_party/cocos2d-x}"

if [ ! -d "${COCOS2D_X_ROOT}" ]; then
    echo "Cocos2d-x not found at ${COCOS2D_X_ROOT}"
    echo "Run ./scripts/fetch_cocos2dx.sh first."
    exit 1
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake .. \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCOCOS2D_X_ROOT="${COCOS2D_X_ROOT}"

cmake --build . --config "${CMAKE_BUILD_TYPE:-Release}"

echo "Build complete."
