#!/bin/bash

# Build script for PAGX Chrome Extension
# This script builds the WASM module and assembles the extension

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "Building PAGX Chrome Extension..."

# Step 1: Build WASM module using playground build system
echo "Step 1: Building WASM module..."
cd "${PROJECT_ROOT}/playground"
npm run build:release

# Step 2: Copy WASM artifacts to extension
echo "Step 2: Copying WASM artifacts..."
mkdir -p "${SCRIPT_DIR}/wasm"
cp "${PROJECT_ROOT}/playground/wasm-mt/pagx-playground.js" "${SCRIPT_DIR}/wasm/"
cp "${PROJECT_ROOT}/playground/wasm-mt/pagx-playground.wasm" "${SCRIPT_DIR}/wasm/"
if [ -f "${PROJECT_ROOT}/playground/wasm-mt/pagx-playground.worker.js" ]; then
  cp "${PROJECT_ROOT}/playground/wasm-mt/pagx-playground.worker.js" "${SCRIPT_DIR}/wasm/"
fi

# Step 3: Copy fonts
echo "Step 3: Copying fonts..."
mkdir -p "${SCRIPT_DIR}/fonts"
if [ -f "${PROJECT_ROOT}/resources/font/NotoSansSC-Regular.otf" ]; then
  cp "${PROJECT_ROOT}/resources/font/NotoSansSC-Regular.otf" "${SCRIPT_DIR}/fonts/"
fi
if [ -f "${PROJECT_ROOT}/resources/font/NotoColorEmoji.ttf" ]; then
  cp "${PROJECT_ROOT}/resources/font/NotoColorEmoji.ttf" "${SCRIPT_DIR}/fonts/"
fi

# Step 4: Install dependencies and build viewer.js
echo "Step 4: Building viewer.js..."
cd "${SCRIPT_DIR}"
npm install
npm run build

echo "Build complete! Extension ready at: ${SCRIPT_DIR}"
echo ""
echo "To load the extension in Chrome:"
echo "1. Open chrome://extensions"
echo "2. Enable 'Developer mode' (top right)"
echo "3. Click 'Load unpacked' and select: ${SCRIPT_DIR}"
echo ""
echo "Note: You may need to manually enable 'Allow access to file URLs' permission"
