#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COCOS2D_X_ROOT="${ROOT_DIR}/third_party/cocos2d-x"

mkdir -p "${ROOT_DIR}/third_party"

if [ -d "${COCOS2D_X_ROOT}/.git" ]; then
    echo "Cocos2d-x already present at ${COCOS2D_X_ROOT}"
    exit 0
fi

echo "Fetching Cocos2d-x into ${COCOS2D_X_ROOT}"
git clone --depth 1 --branch master https://github.com/cocos/cocos2d-x.git "${COCOS2D_X_ROOT}"

echo "Cocos2d-x bootstrap complete."
