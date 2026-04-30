# Technology Stack

**Analysis Date:** 2026-04-28

## Languages

**Primary:**
- C++17 - Core rendering engine, codec, data model (all of `src/base/`, `src/codec/`, `src/rendering/`, `src/pagx/`, `src/renderer/`)
- Objective-C / Objective-C++ - Apple platform integration in `src/platform/cocoa/`, `src/platform/ios/`, `src/platform/mac/` (`.m` / `.mm` files)

**Secondary:**
- Java / Kotlin - Android SDK layer in `android/libpag/src/main/` (bridged via JNI from `src/platform/android/J*.cpp`)
- TypeScript / JavaScript - Web/WASM SDK in `web/src/`, build tooling in `web/script/`
- ArkTS (ETS) - OpenHarmony SDK in `ohos/libpag/src/main/` (bridged via NAPI from `src/platform/ohos/`)
- CMake - Build configuration (`CMakeLists.txt`, `third_party/vendor_tools/vendor.cmake`)
- Shell - Build/tooling scripts (`sync_deps.sh`, `install_tools.sh`, `codeformat.sh`, `accept_baseline.sh`)

## Runtime

**Language standard:**
- `CMAKE_CXX_STANDARD 17` (required) — see `CMakeLists.txt:21-22`
- `CMAKE_CXX_VISIBILITY_PRESET hidden` (symbols hidden by default)

**Deployment targets** (`CMakeLists.txt:141-153`):
- macOS arm64: `11.0` minimum
- macOS x86_64: `10.15` minimum
- iOS: `9.0` minimum
- Android: `minSdkVersion 21`, `targetSdkVersion 33`, NDK `28.0.13004108` (`android/build.gradle`)
- Web: Emscripten (version `4.0.11` explicitly blocked — `CMakeLists.txt:643-648`)

**Build tooling required:**
- `node`, `cmake`, `ninja`, `yasm`, `git-lfs` — installed via Homebrew on macOS
- `emscripten` — required when building web target
- `depsync` — npm package for syncing `DEPS` dependencies

## Frameworks

**Core build system:**
- CMake 3.13+ with Ninja generator (primary)
- Gradle 8.8.1 + Android Gradle Plugin `8.8.1` (Android wrapper build, `android/build.gradle`)
- Xcode (iOS/macOS framework builds via `mac/gen_mac`, `ios/gen_ios`, `ios/gen_simulator`)
- Hvigor (OpenHarmony build system, `ohos/hvigorfile.ts`)
- Rollup + TypeScript + Babel (web/WASM bundling, `web/package.json`)

**Testing:**
- Google Test `1.16.0` — pulled from `${TGFX_DIR}/third_party/googletest/`. Test targets: `PAGFullTest`, `PAGUnitTest`, `UpdateBaseline` (`CMakeLists.txt:799-855`).
- Cypress 9.5 — web end-to-end tests (`web/cypress/`)

**Render backends** (selected at configure time, `CMakeLists.txt:27-31`):
- OpenGL / OpenGL ES (default, `PAG_USE_OPENGL=ON`)
- Metal (Apple, when `PAG_USE_OPENGL=OFF`)
- ANGLE (`PAG_USE_ANGLE=ON`, Windows only, headers from `${TGFX_DIR}/vendor/angle/`)
- SwiftShader (`PAG_USE_SWIFTSHADER=ON`, CPU rendering, libs from `${TGFX_DIR}/vendor/swiftshader/`)
- WebGL 2 (web target via Emscripten `-sMAX_WEBGL_VERSION=2`)
- Qt OpenGL (`PAG_USE_QT=ON`, requires Qt6 Core/Widgets/OpenGL/Quick)

## Key Dependencies

**Rendering engine (required):**
- `tgfx` 2.1.1 (Tencent 2D graphics engine, `https://github.com/libpag/tgfx.git`) — pinned commit in `DEPS`, synced to `third_party/tgfx/`. Can be linked three ways (`CMakeLists.txt:525-613`):
  1. Prebuilt external: via `-DTGFX_LIB=... -DTGFX_INCLUDE=...`
  2. Custom source dir: via `-DTGFX_DIR=../tgfx` (used for local TGFX debugging)
  3. Built-in: uses `third_party/tgfx/out/cache` if present, else `add_subdirectory`

**Binary codec:**
- `lz4` 1.10.0 (`https://github.com/lz4/lz4.git`) — embedded build compiles `third_party/lz4/lib/lz4.c` (`CMakeLists.txt:512-516`). On Apple platforms with `PAG_USE_SYSTEM_LZ4=ON`, links system `compression` framework instead. Used for sequence frame compression in `src/rendering/caches/`.
- `libavc` (H.264 software decoder, `https://github.com/libpag/libavc.git`) — fallback enabled via `PAG_USE_LIBAVC` on non-Android/non-iOS/non-OHOS platforms. Headers: `third_party/libavc/common`, `third_party/libavc/decoder`.
- `ffavc` (prebuilt binary in `vendor/ffavc/` and `android/libpag/libs/${ANDROID_ABI}/libffavc.so`) — alternate H.264 fallback on Android (`PAG_USE_FFAVC=ON`).

**Text & internationalization** (only when `PAG_BUILD_PAGX=ON` or `PAG_BUILD_TESTS=ON`):
- `harfbuzz` 10.1.0 (`https://github.com/harfbuzz/harfbuzz.git`) — text shaping. Used by `src/renderer/` text layout engine.
- `expat` 2.6.3 (`https://github.com/libexpat/libexpat.git`) — XML parsing for PAGX format. Built static.
- `SheenBidi` 3.0.0 (`https://github.com/Tehreer/SheenBidi.git`) — Unicode bidirectional text (BiDi) algorithm.
- `libxml2` 2.15.2 (`https://github.com/GNOME/libxml2.git`) — XPath + advanced XML (used by CLI/tests only). Configured with all optional features OFF. Platforms: mac, win, linux.

**Optional:**
- `rttr` (`https://github.com/rttrorg/rttr.git`) — runtime type reflection, enabled only when `PAG_USE_RTTR=ON`. Platforms: mac, win, linux.
- `vendor_tools` (`https://github.com/libpag/vendor_tools.git`) — CMake helpers for third-party targets (`third_party/vendor_tools/vendor.cmake`).

**Image codecs** (provided by TGFX, controlled via `PAG_USE_PNG_DECODE/ENCODE`, `PAG_USE_JPEG_DECODE/ENCODE`, `PAG_USE_WEBP_DECODE/ENCODE`):
- `libpng` 1.6.47 — PNG encode/decode (TGFX `third_party/libpng/`)
- `libjpeg-turbo` 2.1.1 — JPEG encode/decode (TGFX `third_party/libjpeg-turbo/`)
- `libwebp` 1.x — WebP encode/decode (TGFX `third_party/libwebp/`)
- `freetype` 2.13.3 — Vector font rendering (TGFX `third_party/freetype/`, `PAG_USE_FREETYPE`)

**Graphics pipeline (TGFX internal):**
- `pathkit` — Skia-derived 2D path library (`third_party/tgfx/third_party/pathkit/`)
- `skcms` — ICC color profile processing (`third_party/tgfx/third_party/skcms/`)
- `highway` — SIMD acceleration (`third_party/tgfx/third_party/highway/`)
- `concurrentqueue` — lock-free task queue (`third_party/tgfx/third_party/concurrentqueue/`)
- `flatbuffers` — binary serialization in TGFX layers module (`third_party/tgfx/third_party/flatbuffers/`)
- `shaderc` + `SPIRV-Cross` — GLSL → MSL shader compilation for Metal backend (`third_party/tgfx/third_party/shaderc/`, `third_party/tgfx/third_party/SPIRV-Cross/`)
- `zlib` — required by TGFX/shaderc on some paths (`third_party/tgfx/third_party/zlib/`)
- `nlohmann/json` — test utilities in TGFX (`third_party/tgfx/third_party/json/`)

## Configuration

**CMake options** (defined in `CMakeLists.txt:27-67`):

| Option | Default | Description |
|--------|---------|-------------|
| `PAG_USE_OPENGL` | ON | OpenGL GPU backend |
| `PAG_USE_SWIFTSHADER` | OFF | CPU rendering via SwiftShader |
| `PAG_USE_ANGLE` | OFF | Windows D3D-backed OpenGL ES via ANGLE |
| `PAG_USE_QT` | OFF | Qt6 integration (disables Metal/SwiftShader/ANGLE) |
| `PAG_USE_RTTR` | OFF | Runtime type reflection |
| `PAG_USE_HARFBUZZ` | OFF | Text shaping (auto-ON with PAGX) |
| `PAG_USE_C` | OFF | C language API bindings (`src/c/`) |
| `PAG_USE_FREETYPE` | ON non-Apple/non-Web, OFF Apple/Web | Embedded FreeType |
| `PAG_USE_LIBAVC` | ON non-Apple-sim/non-OHOS/non-Web | libavc fallback decoder |
| `PAG_USE_FFAVC` | ON Android | ffavc prebuilt decoder |
| `PAG_USE_THREADS` | ON non-Web or EMSCRIPTEN_PTHREADS | Multithreaded rendering |
| `PAG_USE_SYSTEM_LZ4` | ON Apple | Link Apple's `compression.framework` for LZ4 |
| `PAG_BUILD_PAGX` | OFF | PAGX XML format support (auto-ON by TESTS/CLI/SVG) |
| `PAG_BUILD_SVG` | OFF | SVG import/export (auto-ON by CLI/TESTS) |
| `PAG_BUILD_CLI` | OFF | `pagx-cli` command-line tool |
| `PAG_BUILD_TESTS` | OFF | Enable all modules and GoogleTest targets |
| `PAG_BUILD_SHARED` | ON non-Web | Shared vs static library |
| `PAG_BUILD_FRAMEWORK` | ON Apple | Build Apple framework bundle |

**Dependency option chains** (`CMakeLists.txt:95-125`):
- `PAG_BUILD_CLI` → forces `PAG_BUILD_PAGX` + `PAG_BUILD_SVG`
- `PAG_BUILD_SVG` → forces `PAG_BUILD_PAGX`
- `PAG_BUILD_PAGX` → forces `PAG_USE_HARFBUZZ`
- `PAG_BUILD_TESTS` → forces `PAG_USE_FREETYPE` + `PAG_BUILD_PAGX` + `PAG_BUILD_CLI` + `PAG_BUILD_SVG` + `PAG_USE_HARFBUZZ` + `PAG_USE_SYSTEM_LZ4=OFF` + `PAG_BUILD_SHARED=OFF`
- `PAG_USE_QT|SWIFTSHADER|ANGLE` → forces `PAG_USE_OPENGL=ON`
- `PAG_USE_FFAVC` → overrides `PAG_USE_LIBAVC=OFF`

**Compiler flags** (`CMakeLists.txt:209-216`):
- Clang: `-Werror -Wall -Wextra -Weffc++ -pedantic -Werror=return-type -Wno-unused-command-line-argument`
- MSVC: `/utf-8 /w44251 /w44275`
- Android release: `-ffunction-sections -fdata-sections -Os -fno-exceptions -fno-rtti` (`CMakeLists.txt:425-426`)
- Web: `-fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0` plus `-Oz` (release) or `-O0 -gsource-map` (debug)

**Key build defines added by options:**
- `PAG_DLL` when `PAG_BUILD_SHARED`
- `PAG_USE_LIBAVC`, `PAG_USE_FFAVC`, `PAG_USE_RTTR`, `PAG_USE_HARFBUZZ`
- `PAG_BUILD_PAGX`, `PAG_BUILD_SVG`
- `PAG_BUILD_FOR_WEB` on Web
- `LIBXML_STATIC` (tests/CLI), `XML_STATIC` (Windows + PAGX)
- `GL_SILENCE_DEPRECATION` / `GLES_SILENCE_DEPRECATION` on Apple
- `NOMINMAX`, `_USE_MATH_DEFINES` on Windows
- `SKIP_FRAME_COMPARE` in `PAGUnitTest`, `UPDATE_BASELINE`/`GENERATE_BASELINE_IMAGES` in `UpdateBaseline`

## Platform Requirements

**Development (macOS, primary dev platform):**
- Xcode command-line tools
- Homebrew-installed: node, cmake, ninja, yasm, git-lfs, emscripten (for web)
- `./sync_deps.sh` must be run before first build — invokes `depsync` which clones repos listed in `DEPS` into `third_party/`
- CLion-friendly: when `$ENV{CLION_IDE}` is set on macOS, `PAG_BUILD_TESTS=ON` by default (`CMakeLists.txt:77-86`)

**Production build outputs:**
- **iOS/macOS:** Framework bundle (`PAG_BUILD_FRAMEWORK=ON`), bundle identifier `com.tencent.libpag`, version `4.0.0` (`src/rendering/PAG.cpp`)
- **Android:** AAR `com.tencent.tav:libpag:4.1.0`, min SDK 21, target SDK 33, NDK 28.0.13004108
- **Web:** NPM package `libpag@4.0.0` (`web/package.json`) — ESM/CJS/UMD; targets `wasm` (ST) and `wasm-mt` (MT); WeChat mini-program variant under `web/wechat/`
- **OpenHarmony:** HAR package `@tencent/libpag@1.0.1`
- **CLI:** `pagx-cli` executable, npm `@libpag/pagx@0.2.16` (`cli/npm/package.json`)
- **Windows:** Win32 demo only (`win/Win32Demo.sln`), no packaged SDK
- **Linux:** CLI executable + demo (`linux/CMakeLists.txt`)

**Versions:**
- C++ SDK: `4.0.0` (`src/rendering/PAG.cpp`)
- Android SDK: `4.1.0` (`android/libpag/build.gradle`)
- Web package: `4.0.0` (`web/package.json`)
- TGFX engine: `2.1.1` (`third_party/tgfx/CMakeLists.txt`)
- PAGX CLI npm: `0.2.16` (`cli/npm/package.json`)
- DEPS manifest: `1.3.12` (`DEPS`)

---

*Stack analysis: 2026-04-28*
