#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIR="${ROOT_DIR}/third_party/cocos2d-x"
COCOS2D_X_REPO="https://github.com/cocos/cocos2d-x.git"
COCOS2D_X_VERSION="${COCOS2D_X_VERSION:-master}"

mkdir -p "${ROOT_DIR}/third_party"

if [ -d "${TARGET_DIR}/.git" ]; then
    echo "Cocos2d-x already present at ${TARGET_DIR}"
    echo "Updating to ${COCOS2D_X_VERSION}..."
    git -C "${TARGET_DIR}" fetch --all --tags --prune
    git -C "${TARGET_DIR}" checkout "${COCOS2D_X_VERSION}" || git -C "${TARGET_DIR}" checkout -B "${COCOS2D_X_VERSION}" "origin/${COCOS2D_X_VERSION}"
    exit 0
fi

echo "Cloning Cocos2d-x into ${TARGET_DIR}"
git clone --depth 1 --branch "${COCOS2D_X_VERSION}" "${COCOS2D_X_REPO}" "${TARGET_DIR}"

echo "Cocos2d-x setup complete."
echo "Root: ${TARGET_DIR}"
