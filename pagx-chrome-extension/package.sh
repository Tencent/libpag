#!/bin/bash

# Package script for PAGX Chrome Extension
# Creates a ZIP file ready for Chrome Web Store submission.
# Run build.sh first to generate all build artifacts.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/.."
VERSION=$(grep '"version"' "${SCRIPT_DIR}/manifest.json" | head -1 | sed 's/.*: *"\([^"]*\)".*/\1/')
ZIP_NAME="pagx-viewer-v${VERSION}.zip"

# Verify build artifacts exist
MISSING=()
[ ! -f "${SCRIPT_DIR}/viewer/viewer.js" ] && MISSING+=("viewer/viewer.js")
[ ! -f "${SCRIPT_DIR}/wasm/pagx-playground.js" ] && MISSING+=("wasm/pagx-playground.js")
[ ! -f "${SCRIPT_DIR}/wasm/pagx-playground.wasm" ] && MISSING+=("wasm/pagx-playground.wasm")
[ ! -f "${SCRIPT_DIR}/fonts/NotoSansSC-Regular.otf" ] && MISSING+=("fonts/NotoSansSC-Regular.otf")
[ ! -f "${SCRIPT_DIR}/fonts/NotoColorEmoji.ttf" ] && MISSING+=("fonts/NotoColorEmoji.ttf")

if [ ${#MISSING[@]} -gt 0 ]; then
  echo "Error: Missing build artifacts. Run build.sh first."
  for f in "${MISSING[@]}"; do
    echo "  - ${f}"
  done
  exit 1
fi

echo "Packaging PAGX Viewer v${VERSION}..."

cd "${SCRIPT_DIR}"
rm -f "${OUTPUT_DIR}/${ZIP_NAME}"

zip -r "${OUTPUT_DIR}/${ZIP_NAME}" \
  manifest.json \
  background.js \
  icons/ \
  viewer/viewer.html \
  viewer/viewer.css \
  viewer/viewer.js \
  wasm/ \
  fonts/ \
  -x "*.DS_Store" "fonts/.DS_Store"

ZIP_SIZE=$(du -h "${OUTPUT_DIR}/${ZIP_NAME}" | cut -f1)
echo ""
echo "Package created: ${OUTPUT_DIR}/${ZIP_NAME} (${ZIP_SIZE})"
echo ""
echo "To submit to Chrome Web Store:"
echo "  1. Go to https://chrome.google.com/webstore/devconsole"
echo "  2. Click 'New item' -> Upload ${ZIP_NAME}"
echo "  3. Fill in store listing details and privacy policy"
echo "  4. Submit for review"
