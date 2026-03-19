#!/bin/bash

# Package script for PAGX Chrome Extension
#
# Usage:
#   bash package.sh --key KEY.pem       # Generate .crx with specified private key
#   bash package.sh crx --key KEY.pem   # Same as above (crx is the default format)
#   bash package.sh zip                 # Generate .zip for Chrome Web Store submission
#
# Examples:
#   bash package.sh --key ~/keys/pagx-viewer.pem
#   bash package.sh crx --key /path/to/pagx-viewer.pem
#   bash package.sh zip
#
# Run build.sh first to generate all build artifacts.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
VERSION=$(grep '"version"' "${SCRIPT_DIR}/manifest.json" | head -1 | sed 's/.*: *"\([^"]*\)".*/\1/')
FORMAT=""
PEM_ARG=""

# Parse all arguments
while [ $# -gt 0 ]; do
  case "$1" in
    crx|zip)
      FORMAT="$1"
      shift
      ;;
    --key)
      if [ -z "$2" ] || [ ! -f "$2" ]; then
        echo "Error: --key requires a valid .pem file path."
        exit 1
      fi
      PEM_ARG="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
      shift 2
      ;;
    -h|--help|help)
      FORMAT="help"
      shift
      ;;
    *)
      echo "Error: Unknown option '$1'"
      echo ""
      FORMAT="help"
      break
      ;;
  esac
done

# Default format
FORMAT="${FORMAT:-crx}"

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

# Create a clean temporary directory with only distributable files
create_dist_dir() {
  local DIST_DIR
  DIST_DIR=$(mktemp -d)
  cd "${SCRIPT_DIR}"

  cp manifest.json background.js "${DIST_DIR}/"
  mkdir -p "${DIST_DIR}/icons"
  cp icons/*.png "${DIST_DIR}/icons/"
  mkdir -p "${DIST_DIR}/viewer"
  cp viewer/viewer.html viewer/viewer.css viewer/viewer.js "${DIST_DIR}/viewer/"
  mkdir -p "${DIST_DIR}/wasm"
  cp wasm/pagx-playground.js wasm/pagx-playground.wasm "${DIST_DIR}/wasm/"
  if [ -f wasm/pagx-playground.worker.js ]; then
    cp wasm/pagx-playground.worker.js "${DIST_DIR}/wasm/"
  fi
  mkdir -p "${DIST_DIR}/fonts"
  cp fonts/NotoSansSC-Regular.otf fonts/NotoColorEmoji.ttf "${DIST_DIR}/fonts/"

  echo "${DIST_DIR}"
}

package_zip() {
  local ZIP_NAME="pagx-viewer-v${VERSION}.zip"
  echo "Packaging PAGX Viewer v${VERSION} (ZIP for Chrome Web Store)..."

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

  local ZIP_SIZE
  ZIP_SIZE=$(du -h "${OUTPUT_DIR}/${ZIP_NAME}" | cut -f1)
  echo ""
  echo "Package created: ${OUTPUT_DIR}/${ZIP_NAME} (${ZIP_SIZE})"
  echo ""
  echo "To submit to Chrome Web Store:"
  echo "  1. Go to https://chrome.google.com/webstore/devconsole"
  echo "  2. Click 'New item' -> Upload ${ZIP_NAME}"
  echo "  3. Fill in store listing details and privacy policy"
  echo "  4. Submit for review"
}

package_crx() {
  local PEM_FILE="${PEM_ARG:-${OUTPUT_DIR}/pagx-viewer.pem}"
  local CRX_NAME="pagx-viewer-v${VERSION}.crx"

  # Require a valid PEM file
  if [ ! -f "${PEM_FILE}" ]; then
    echo "Error: Private key file not found: ${PEM_FILE}"
    echo ""
    if [ -z "${PEM_ARG}" ]; then
      echo "No --key option specified and no default key found at:"
      echo "  ${PEM_FILE}"
    else
      echo "The specified key file does not exist:"
      echo "  ${PEM_FILE}"
    fi
    echo ""
    echo "To generate a new key, create a CRX via Chrome manually first:"
    echo "  1. Open chrome://extensions/ and enable Developer mode"
    echo "  2. Click 'Pack extension' with the extension directory"
    echo "  3. A .pem file will be generated alongside the .crx"
    echo "  4. Then run: bash package.sh crx --key /path/to/your.pem"
    exit 1
  fi

  echo "Packaging PAGX Viewer v${VERSION} (CRX for local distribution)..."
  echo "Using private key: ${PEM_FILE}"

  # Create clean dist directory
  local DIST_DIR
  DIST_DIR=$(create_dist_dir)

  # Find Chrome/Chromium binary
  local CHROME_BIN=""
  if [ -x "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" ]; then
    CHROME_BIN="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
  elif [ -x "/Applications/Chromium.app/Contents/MacOS/Chromium" ]; then
    CHROME_BIN="/Applications/Chromium.app/Contents/MacOS/Chromium"
  elif command -v google-chrome &>/dev/null; then
    CHROME_BIN="google-chrome"
  elif command -v chromium-browser &>/dev/null; then
    CHROME_BIN="chromium-browser"
  elif command -v chromium &>/dev/null; then
    CHROME_BIN="chromium"
  fi

  if [ -z "${CHROME_BIN}" ]; then
    echo "Error: Chrome/Chromium not found. Cannot generate .crx file."
    echo "Please install Chrome or use 'bash package.sh zip' instead."
    rm -rf "${DIST_DIR}"
    exit 1
  fi

  # Pack extension using Chrome
  rm -f "${OUTPUT_DIR}/${CRX_NAME}"
  "${CHROME_BIN}" --pack-extension="${DIST_DIR}" --pack-extension-key="${PEM_FILE}" --no-message-box 2>/dev/null || true

  # Chrome generates .crx and .pem next to the dist directory
  local GENERATED_CRX="${DIST_DIR}.crx"
  local GENERATED_PEM="${DIST_DIR}.pem"

  if [ ! -f "${GENERATED_CRX}" ]; then
    echo "Error: CRX generation failed."
    rm -rf "${DIST_DIR}"
    exit 1
  fi

  # Move CRX to target location, clean up generated PEM (we already have one)
  mv "${GENERATED_CRX}" "${OUTPUT_DIR}/${CRX_NAME}"
  rm -f "${GENERATED_PEM}"

  # Cleanup
  rm -rf "${DIST_DIR}"

  local CRX_SIZE
  CRX_SIZE=$(du -h "${OUTPUT_DIR}/${CRX_NAME}" | cut -f1)
  echo ""
  echo "Package created: ${OUTPUT_DIR}/${CRX_NAME} (${CRX_SIZE})"
  echo ""
  echo "To install locally:"
  echo "  1. Open chrome://extensions/"
  echo "  2. Enable 'Developer mode' (top right)"
  echo "  3. Drag ${CRX_NAME} into the page"
  echo "  4. Enable 'Allow access to file URLs' in extension details"
}

case "${FORMAT}" in
  zip)
    package_zip
    ;;
  crx)
    package_crx
    ;;
  help|*)
    echo "Usage: bash package.sh [FORMAT] [OPTIONS]"
    echo ""
    echo "Formats:"
    echo "  crx  - Generate .crx for local distribution (default)"
    echo "  zip  - Generate .zip for Chrome Web Store submission"
    echo ""
    echo "Options:"
    echo "  --key PATH  - Path to .pem private key file (required for crx)"
    echo "  --help      - Show this help message"
    echo ""
    echo "Examples:"
    echo "  bash package.sh --key ~/keys/pagx-viewer.pem"
    echo "  bash package.sh crx --key /path/to/pagx-viewer.pem"
    echo "  bash package.sh zip"
    exit 1
    ;;
esac
