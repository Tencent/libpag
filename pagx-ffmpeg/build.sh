#!/usr/bin/env bash
# Copyright (C) 2026 Tencent. All rights reserved.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLATFORM=""
ARCH=""
JOBS=""
CLEAN=0
SKIP_SYNC=0

usage() {
  cat <<'USAGE'
Build a minimal FFmpeg executable with libx264 for pagx MP4 export.

Usage:
  ./build.sh [--platform mac|linux|win] [--arch x64|x86|arm64] [options]

Options:
  --platform NAME  Target platform. Defaults to the host platform.
  --arch NAME      Target architecture. Defaults to the host architecture.
  --jobs N         Number of parallel build jobs.
  --skip-sync      Use existing third_party sources without running depsync.
  --clean          Remove the selected target's build and output directories.
  -h, --help       Show this help.

Cross-compilation is supported between architectures on the same host OS. The
Windows x86, x64, and ARM64 targets require MINGW32, MINGW64/UCRT64, and
CLANGARM64 MSYS2 environments respectively.
USAGE
}

fail() {
  echo "error: $*" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --platform)
      [[ $# -ge 2 ]] || fail "--platform requires a value"
      PLATFORM="$2"
      shift 2
      ;;
    --arch)
      [[ $# -ge 2 ]] || fail "--arch requires a value"
      ARCH="$2"
      shift 2
      ;;
    --jobs)
      [[ $# -ge 2 ]] || fail "--jobs requires a value"
      JOBS="$2"
      shift 2
      ;;
    --skip-sync)
      SKIP_SYNC=1
      shift
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "unknown argument: $1"
      ;;
  esac
done

case "$(uname -s)" in
  Darwin) HOST_PLATFORM="mac" ;;
  Linux) HOST_PLATFORM="linux" ;;
  MINGW*|MSYS*|CYGWIN*) HOST_PLATFORM="win" ;;
  *) fail "unsupported host platform: $(uname -s)" ;;
esac

case "$(uname -m)" in
  x86_64|amd64) HOST_ARCH="x64" ;;
  i386|i486|i586|i686|x86) HOST_ARCH="x86" ;;
  arm64|aarch64) HOST_ARCH="arm64" ;;
  *) fail "unsupported host architecture: $(uname -m)" ;;
esac

PLATFORM="${PLATFORM:-$HOST_PLATFORM}"
ARCH="${ARCH:-$HOST_ARCH}"
[[ "$PLATFORM" == "mac" || "$PLATFORM" == "linux" || "$PLATFORM" == "win" ]] ||
  fail "unsupported platform: $PLATFORM"
[[ "$ARCH" == "x64" || "$ARCH" == "x86" || "$ARCH" == "arm64" ]] ||
  fail "unsupported architecture: $ARCH"
[[ "$PLATFORM" == "$HOST_PLATFORM" ]] ||
  fail "cross-OS builds are not supported: host is $HOST_PLATFORM, target is $PLATFORM"
[[ "$ARCH" != "x86" || "$PLATFORM" == "win" ]] ||
  fail "the x86 architecture is only supported for Windows targets"

CROSS_COMPILE=0
if [[ "$ARCH" != "$HOST_ARCH" ]]; then
  CROSS_COMPILE=1
fi
CROSS_PREFIX=""
ARCH_CFLAGS=""
ARCH_LDFLAGS=""
FFMPEG_TARGET_OS=""

case "$PLATFORM-$ARCH" in
  mac-x64)
    X264_HOST="x86_64-apple-darwin"
    FFMPEG_ARCH="x86_64"
    FFMPEG_TARGET_OS="darwin"
    ARCH_CFLAGS="-arch x86_64"
    ARCH_LDFLAGS="-arch x86_64"
    ;;
  mac-arm64)
    X264_HOST="aarch64-apple-darwin"
    FFMPEG_ARCH="aarch64"
    FFMPEG_TARGET_OS="darwin"
    ARCH_CFLAGS="-arch arm64"
    ARCH_LDFLAGS="-arch arm64"
    ;;
  linux-x64)
    X264_HOST="x86_64-linux-gnu"
    FFMPEG_ARCH="x86_64"
    FFMPEG_TARGET_OS="linux"
    ;;
  linux-arm64)
    X264_HOST="aarch64-linux-gnu"
    FFMPEG_ARCH="aarch64"
    FFMPEG_TARGET_OS="linux"
    ;;
  win-x86)
    X264_HOST="i686-w64-mingw32"
    FFMPEG_ARCH="x86"
    FFMPEG_TARGET_OS="mingw32"
    ;;
  win-x64)
    X264_HOST="x86_64-w64-mingw32"
    FFMPEG_ARCH="x86_64"
    FFMPEG_TARGET_OS="mingw32"
    ;;
  win-arm64)
    X264_HOST="aarch64-w64-mingw32"
    FFMPEG_ARCH="aarch64"
    FFMPEG_TARGET_OS="mingw32"
    ;;
esac

if [[ "$PLATFORM" == "linux" && "$CROSS_COMPILE" == "1" ]]; then
  CROSS_PREFIX="$X264_HOST-"
fi

if [[ "$PLATFORM" == "win" ]]; then
  case "${MSYSTEM:-}" in
    MINGW32) TOOLCHAIN_ARCH="x86" ;;
    MINGW64|UCRT64) TOOLCHAIN_ARCH="x64" ;;
    CLANGARM64) TOOLCHAIN_ARCH="arm64" ;;
    *) fail "run build.ps1 or start an MINGW32/MINGW64/UCRT64/CLANGARM64 MSYS2 shell" ;;
  esac
  [[ "$ARCH" == "$TOOLCHAIN_ARCH" ]] ||
    fail "MSYSTEM=$MSYSTEM does not match the requested $ARCH architecture"
fi

TARGET="$PLATFORM-$ARCH"
HOST_TARGET="$HOST_PLATFORM-$HOST_ARCH"
USER_CFLAGS="${CFLAGS:-}"
USER_LDFLAGS="${LDFLAGS:-}"
TARGET_CFLAGS="$USER_CFLAGS${ARCH_CFLAGS:+ $ARCH_CFLAGS}"
TARGET_LDFLAGS="$USER_LDFLAGS${ARCH_LDFLAGS:+ $ARCH_LDFLAGS}"
WORK_DIR="$ROOT_DIR/build/$TARGET"
OUTPUT_DIR="$ROOT_DIR/out/$TARGET"
PREFIX_DIR="$WORK_DIR/prefix"
FFMPEG_EXTRA_CFLAGS="-I$PREFIX_DIR/include${ARCH_CFLAGS:+ $ARCH_CFLAGS}"
FFMPEG_EXTRA_LDFLAGS="-L$PREFIX_DIR/lib${ARCH_LDFLAGS:+ $ARCH_LDFLAGS}"
X264_SOURCE_DIR="$ROOT_DIR/third_party/x264"
FFMPEG_SOURCE_DIR="$ROOT_DIR/third_party/FFmpeg"
X264_BUILD_DIR="$WORK_DIR/x264"
FFMPEG_BUILD_DIR="$WORK_DIR/ffmpeg"

if [[ "$CLEAN" == "1" ]]; then
  rm -rf "$WORK_DIR" "$OUTPUT_DIR"
  echo "Cleaned $TARGET"
  exit 0
fi

if [[ -z "$JOBS" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
  elif command -v getconf >/dev/null 2>&1; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
  else
    JOBS=2
  fi
fi
[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || fail "--jobs must be a positive integer"

if [[ -z "${CC:-}" ]]; then
  if [[ -n "$CROSS_PREFIX" ]]; then
    CC="${CROSS_PREFIX}gcc"
  elif [[ "$PLATFORM" == "win" && "$ARCH" == "arm64" ]]; then
    CC="clang"
  elif [[ "$PLATFORM" == "win" ]]; then
    CC="gcc"
  else
    CC="cc"
  fi
fi

REQUIRED_TOOLS=(git make pkg-config node "$CC")
if [[ -n "$CROSS_PREFIX" ]]; then
  AR="${AR:-${CROSS_PREFIX}ar}"
  RANLIB="${RANLIB:-${CROSS_PREFIX}ranlib}"
  STRIP="${STRIP:-${CROSS_PREFIX}strip}"
  export AR RANLIB STRIP
  REQUIRED_TOOLS+=("$AR" "$RANLIB" "$STRIP")
fi
if [[ "$SKIP_SYNC" == "0" ]]; then
  REQUIRED_TOOLS+=(depsync)
fi
if [[ "$ARCH" == "x64" || "$ARCH" == "x86" ]]; then
  REQUIRED_TOOLS+=(nasm)
fi
for TOOL in "${REQUIRED_TOOLS[@]}"; do
  command -v "$TOOL" >/dev/null 2>&1 || fail "required tool not found: $TOOL"
done

if [[ "$SKIP_SYNC" == "0" ]]; then
  depsync "$PLATFORM" --project "$ROOT_DIR" --non-recursive
fi
[[ -x "$X264_SOURCE_DIR/configure" ]] ||
  fail "x264 sources are missing; run without --skip-sync"
[[ -x "$FFMPEG_SOURCE_DIR/configure" ]] ||
  fail "FFmpeg sources are missing; run without --skip-sync"

rm -rf "$WORK_DIR" "$OUTPUT_DIR"
mkdir -p "$X264_BUILD_DIR" "$FFMPEG_BUILD_DIR" "$PREFIX_DIR"

X264_CONFIGURE=(
  "--prefix=$PREFIX_DIR"
  "--host=$X264_HOST"
  --enable-static
  --disable-cli
  --disable-opencl
  --enable-pic
)
if [[ -n "$CROSS_PREFIX" ]]; then
  X264_CONFIGURE+=("--cross-prefix=$CROSS_PREFIX")
fi

echo "==> Configuring x264 for $TARGET"
(
  cd "$X264_BUILD_DIR"
  CC="$CC" CFLAGS="$TARGET_CFLAGS" LDFLAGS="$TARGET_LDFLAGS" \
    PKGCONFIG="${PKGCONFIG:-pkg-config}" \
    "$X264_SOURCE_DIR/configure" "${X264_CONFIGURE[@]}"
)
echo "==> Building x264"
make -C "$X264_BUILD_DIR" -j"$JOBS"
make -C "$X264_BUILD_DIR" install

export PKG_CONFIG_LIBDIR="$PREFIX_DIR/lib/pkgconfig"
unset PKG_CONFIG_PATH

FFMPEG_CONFIGURE=(
  "--prefix=$PREFIX_DIR"
  "--cc=$CC"
  "--arch=$FFMPEG_ARCH"
  "--target-os=$FFMPEG_TARGET_OS"
  "--extra-cflags=$FFMPEG_EXTRA_CFLAGS"
  "--extra-ldflags=$FFMPEG_EXTRA_LDFLAGS"
  --pkg-config=pkg-config
  --pkg-config-flags=--static
  --disable-all
  --disable-autodetect
  --disable-debug
  --disable-doc
  --disable-network
  --disable-shared
  --enable-static
  --enable-gpl
  --enable-ffmpeg
  --enable-avcodec
  --enable-avfilter
  --enable-avformat
  --enable-avutil
  --enable-swscale
  --enable-libx264
  --enable-decoder=rawvideo
  --enable-demuxer=rawvideo
  --enable-encoder=libx264
  --enable-muxer=mp4
  --enable-filter=scale,format
  --enable-protocol=file,pipe
)
if [[ "$PLATFORM" == "win" ]]; then
  FFMPEG_CONFIGURE+=(--disable-pthreads --enable-w32threads)
fi
if [[ "$CROSS_COMPILE" == "1" ]]; then
  FFMPEG_CONFIGURE+=(--enable-cross-compile)
fi
if [[ -n "$CROSS_PREFIX" ]]; then
  FFMPEG_CONFIGURE+=("--cross-prefix=$CROSS_PREFIX")
fi

echo "==> Configuring FFmpeg for $TARGET"
(
  cd "$FFMPEG_BUILD_DIR"
  CFLAGS="$USER_CFLAGS" LDFLAGS="$USER_LDFLAGS" \
    "$FFMPEG_SOURCE_DIR/configure" "${FFMPEG_CONFIGURE[@]}"
)
echo "==> Building FFmpeg"
make -C "$FFMPEG_BUILD_DIR" -j"$JOBS"
make -C "$FFMPEG_BUILD_DIR" install

BINARY_NAME="ffmpeg"
LICENSE_DIR="$OUTPUT_DIR/share/licenses"
if [[ "$PLATFORM" == "win" ]]; then
  BINARY_NAME="ffmpeg.exe"
fi
BINARY_PATH="$OUTPUT_DIR/bin/$BINARY_NAME"
mkdir -p "$OUTPUT_DIR/bin" "$LICENSE_DIR"
cp "$PREFIX_DIR/bin/$BINARY_NAME" "$BINARY_PATH"
cp "$FFMPEG_SOURCE_DIR/LICENSE.md" "$LICENSE_DIR/FFmpeg-LICENSE.md"
cp "$X264_SOURCE_DIR/COPYING" "$LICENSE_DIR/x264-COPYING"

for MACRO in CONFIG_FFMPEG CONFIG_AVCODEC CONFIG_AVFILTER CONFIG_AVFORMAT CONFIG_AVUTIL \
  CONFIG_SWSCALE CONFIG_LIBX264; do
  grep -q "^#define $MACRO 1$" "$FFMPEG_BUILD_DIR/config.h" ||
    fail "FFmpeg configuration check failed: $MACRO"
done
for MACRO in CONFIG_RAWVIDEO_DECODER CONFIG_RAWVIDEO_DEMUXER CONFIG_LIBX264_ENCODER \
  CONFIG_MP4_MUXER CONFIG_SCALE_FILTER CONFIG_FORMAT_FILTER CONFIG_FILE_PROTOCOL CONFIG_PIPE_PROTOCOL; do
  grep -q "^#define $MACRO 1$" "$FFMPEG_BUILD_DIR/config_components.h" ||
    fail "FFmpeg component check failed: $MACRO"
done

VALIDATION_MODE="configuration-only"
if [[ "$CROSS_COMPILE" == "0" ]]; then
  VALIDATION_MODE="runtime-smoke"
  for COMPONENT in encoder=libx264 decoder=rawvideo demuxer=rawvideo muxer=mp4 filter=scale filter=format; do
    "$BINARY_PATH" -hide_banner -h "$COMPONENT" >/dev/null 2>&1 ||
      fail "FFmpeg capability check failed: $COMPONENT"
  done

  SMOKE_DIR="$WORK_DIR/smoke"
  SMOKE_FILE="$SMOKE_DIR/pagx-smoke.mp4"
  mkdir -p "$SMOKE_DIR"
  echo "==> Running RGBA to MP4 smoke test"
  dd if=/dev/zero bs=16384 count=3 2>/dev/null |
    "$BINARY_PATH" -hide_banner -loglevel error -y \
      -f rawvideo -pixel_format rgba -video_size 64x64 -framerate 30 -i pipe:0 \
      -an -c:v libx264 -preset ultrafast -crf 20 -pix_fmt yuv420p \
      -vf scale=in_color_matrix=bt709:out_color_matrix=bt709:out_range=tv,format=yuv420p \
      -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv \
      -movflags +faststart -f mp4 "$SMOKE_FILE"
  [[ -s "$SMOKE_FILE" ]] || fail "FFmpeg smoke test did not produce an MP4 file"
else
  echo "==> Skipping runtime checks for cross-compiled $TARGET; validate the binary on its target OS"
fi

if command -v sha256sum >/dev/null 2>&1; then
  BINARY_SHA256="$(sha256sum "$BINARY_PATH" | awk '{print $1}')"
else
  BINARY_SHA256="$(shasum -a 256 "$BINARY_PATH" | awk '{print $1}')"
fi
if [[ "$CROSS_COMPILE" == "0" ]]; then
  FFMPEG_VERSION="$("$BINARY_PATH" -hide_banner -version | awk 'NR == 1 {print $3}')"
else
  FFMPEG_VERSION="$(tr -d '\r\n' < "$FFMPEG_SOURCE_DIR/RELEASE")"
fi
FFMPEG_COMMIT="$(git -C "$FFMPEG_SOURCE_DIR" rev-parse HEAD)"
X264_COMMIT="$(git -C "$X264_SOURCE_DIR" rev-parse HEAD)"
FFMPEG_CONFIGURE_TEXT="$(printf '%s\n' "${FFMPEG_CONFIGURE[@]}")"
X264_CONFIGURE_TEXT="$(printf '%s\n' "${X264_CONFIGURE[@]}")"

FFMPEG_VERSION="$FFMPEG_VERSION" \
FFMPEG_COMMIT="$FFMPEG_COMMIT" \
X264_COMMIT="$X264_COMMIT" \
FFMPEG_CONFIGURE="$FFMPEG_CONFIGURE_TEXT" \
X264_CONFIGURE="$X264_CONFIGURE_TEXT" \
BINARY_NAME="$BINARY_NAME" \
BINARY_SHA256="$BINARY_SHA256" \
TARGET="$TARGET" \
HOST_TARGET="$HOST_TARGET" \
CROSS_COMPILE="$CROSS_COMPILE" \
VALIDATION_MODE="$VALIDATION_MODE" \
CC="$CC" \
TARGET_CFLAGS="$TARGET_CFLAGS" \
TARGET_LDFLAGS="$TARGET_LDFLAGS" \
node - "$OUTPUT_DIR/build-info.json" <<'NODE'
const fs = require("fs");
const output = process.argv[2];
const info = {
  schemaVersion: 2,
  target: process.env.TARGET,
  host: process.env.HOST_TARGET,
  crossCompile: process.env.CROSS_COMPILE === "1",
  validation: process.env.VALIDATION_MODE,
  toolchain: {
    cc: process.env.CC,
    cflags: process.env.TARGET_CFLAGS,
    ldflags: process.env.TARGET_LDFLAGS,
  },
  sources: {
    FFmpeg: {
      version: process.env.FFMPEG_VERSION,
      commit: process.env.FFMPEG_COMMIT,
    },
    x264: {
      commit: process.env.X264_COMMIT,
    },
  },
  configure: {
    FFmpeg: process.env.FFMPEG_CONFIGURE.split("\n"),
    x264: process.env.X264_CONFIGURE.split("\n"),
  },
  binary: {
    path: `bin/${process.env.BINARY_NAME}`,
    sha256: process.env.BINARY_SHA256,
  },
};
fs.writeFileSync(output, `${JSON.stringify(info, null, 2)}\n`);
NODE

echo "==> Build completed"
echo "    FFmpeg: $BINARY_PATH"
echo "    Metadata: $OUTPUT_DIR/build-info.json"
echo "    Validation: $VALIDATION_MODE"
