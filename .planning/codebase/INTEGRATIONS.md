# External Integrations

**Analysis Date:** 2026-04-28

libpag is an offline C++ rendering library — it has no network APIs, no authentication, no cloud services, and no webhooks. "Integrations" here means **OS-level system frameworks, GPU backends, hardware decoders, and font services** that the library binds to on each supported platform.

## Third-party Libraries

| Library | Version | Source | Used For | Condition |
|---------|---------|--------|----------|-----------|
| `tgfx` | 2.1.1 | `https://github.com/libpag/tgfx.git` → `third_party/tgfx/` | 2D GPU rendering engine | Always |
| `lz4` | 1.10.0 | `https://github.com/lz4/lz4.git` → `third_party/lz4/` | Sequence frame disk cache compression | Always (or system `compression.framework` on Apple) |
| `libavc` | — | `https://github.com/libpag/libavc.git` → `third_party/libavc/` | H.264 software decoder fallback | `PAG_USE_LIBAVC` (non-Android/iOS/OHOS) |
| `ffavc` | — | Prebuilt `.so` in `android/libpag/libs/${ANDROID_ABI}/` | H.264 decoder on Android | `PAG_USE_FFAVC` (Android) |
| `harfbuzz` | 10.1.0 | `https://github.com/harfbuzz/harfbuzz.git` → `third_party/harfbuzz/` | Text shaping / glyph layout | `PAG_USE_HARFBUZZ` (auto when PAGX) |
| `expat` | 2.6.3 | `https://github.com/libexpat/libexpat.git` → `third_party/expat/` | XML parsing for PAGX format | `PAG_BUILD_PAGX` |
| `SheenBidi` | 3.0.0 | `https://github.com/Tehreer/SheenBidi.git` → `third_party/SheenBidi/` | Unicode BiDi algorithm | `PAG_BUILD_PAGX` |
| `libxml2` | 2.15.2 | `https://github.com/GNOME/libxml2.git` → `third_party/libxml2/` | XPath + XML for CLI/tests | CLI + Tests only |
| `rttr` | — | `https://github.com/rttrorg/rttr.git` → `third_party/rttr/` | Runtime type reflection | `PAG_USE_RTTR` (opt-in) |
| `vendor_tools` | — | `https://github.com/libpag/vendor_tools.git` → `third_party/vendor_tools/` | CMake vendor build helpers | Always |
| `libpng` | 1.6.47 | TGFX `third_party/libpng/` | PNG decode/encode | `PAG_USE_PNG_*` (via TGFX) |
| `libjpeg-turbo` | 2.1.1 | TGFX `third_party/libjpeg-turbo/` | JPEG decode/encode | `PAG_USE_JPEG_*` (via TGFX) |
| `libwebp` | 1.x | TGFX `third_party/libwebp/` | WebP decode/encode | `PAG_USE_WEBP_*` (via TGFX) |
| `freetype` | 2.13.3 | TGFX `third_party/freetype/` | Vector font rasterization | `PAG_USE_FREETYPE` (via TGFX) |
| `pathkit` | — | TGFX `third_party/pathkit/` | 2D path operations (Skia-derived) | Always (TGFX) |
| `skcms` | — | TGFX `third_party/skcms/` | ICC color profile handling | Always (TGFX) |
| `highway` | — | TGFX `third_party/highway/` | SIMD acceleration | Always (TGFX) |
| `concurrentqueue` | — | TGFX `third_party/concurrentqueue/` | Lock-free task queuing | Always (TGFX) |
| `flatbuffers` | — | TGFX `third_party/flatbuffers/` | Binary serialization in layers module | `TGFX_BUILD_LAYERS` |
| `shaderc` | — | TGFX `third_party/shaderc/` | GLSL → SPIR-V compilation for Metal | TGFX Metal backend |
| `SPIRV-Cross` | — | TGFX `third_party/SPIRV-Cross/` | SPIR-V → MSL for Metal | TGFX Metal backend |
| `zlib` | — | TGFX `third_party/zlib/` | Compression support (shaderc dep) | TGFX |
| `nlohmann/json` | — | TGFX `third_party/json/` | JSON in TGFX test utilities | TGFX tests |
| `googletest` | 1.16.0 | TGFX `third_party/googletest/` | C++ unit test framework | `PAG_BUILD_TESTS` |

## APIs & External Services

**External network services:**
- None. libpag does not make HTTP requests, open sockets, or contact any remote service at runtime. `libxml2` is configured with `LIBXML2_WITH_HTTP=OFF` explicitly.

## Data Storage

**Databases:**
- None.

**File Storage:**
- Local filesystem only. The library reads `.pag` binary files and `.pagx` XML files from paths supplied by the caller.
- Disk cache for decoded sequence frames — `PAGDiskCache` API, implemented via `SequenceFile` (`src/rendering/caches/SequenceFile.cpp`), compressed with LZ4.
- Test resources: `resources/` directory (`test/src/` references via `TestConstants::PAG_ROOT`).
- Test output: `test/out/{folder}/{name}.webp` vs baseline `{name}_base.webp`, version-tracked via `test/baseline/version.json` (repo) and `test/baseline/.cache/version.json` (local).

**Caching:**
- In-memory render caches: `RenderCache`, `RasterCache`, `ShapeCache`, `TextCache` in `src/rendering/caches/`.
- Disk cache wraps the `SequenceFile` LZ4 format.

## Authentication & Identity

- None. libpag has no user concept, no auth provider, no credentials.
- `android/libpag/build.gradle` references JReleaser/Sonatype credentials for **publishing** only, not runtime use.

## Monitoring & Observability

**Error Tracking:**
- None (no Sentry / Crashlytics / Bugsnag).

**Logs:**
- Android: `#include <android/log.h>` via linked `log` library (`CMakeLists.txt:407-408`)
- OpenHarmony: `hilog_ndk.z` library (`CMakeLists.txt:485`)
- Trace image debug output: `src/platform/android/JTraceImage.cpp` (optional debug feature)
- Otherwise errors are returned as `DecodingResult` error codes or `nullptr` from factory methods

## CI/CD & Deployment

**Hosting:**
- Android SDK → Maven Central via JReleaser (`com.tencent.tav:libpag`, `android/libpag/build.gradle:145-161`)
- Web SDK → NPM (`libpag@4.0.0`, `web/package.json`)
- CLI → NPM (`@libpag/pagx@0.2.16`, `cli/npm/package.json`)
- iOS/macOS → distributed as prebuilt frameworks (no CocoaPods/SPM in this tree)
- OHOS → HAR package (`@tencent/libpag@1.0.1`)

**CI Pipeline:**
- Build scripts: `autotest.sh`, `build_pag`, `build_vendor`, `codecov.sh` at repo root.
- No `.github/`, `.gitlab-ci.yml`, or `Jenkinsfile` observed.

## Environment Configuration

**Runtime env vars:**
- None required by the library itself at runtime.
- `$ENV{CLION_IDE}` inspected at CMake configure time only (`CMakeLists.txt:77-81`).

**Secrets location:**
- Android publishing: JReleaser GPG keys expected via standard JReleaser env vars (not stored in repo)
- OHOS signing: `ohos/local.signingconfig.sample.json` — sample only, real file gitignored

## Webhooks & Callbacks

**Incoming:** None.

**Outgoing:** None at the network level.

**In-process callbacks:**
- `PAGAnimator::Listener` — animation lifecycle events
- `PAGPlayer` / `PAGSurface` callbacks delivered to host app via platform bindings (iOS `PAGAnimationCallback.mm`, Android `JPAGAnimator.cpp`)

---

## Platform Integrations (the primary integration surface)

### iOS

**System frameworks linked** (`CMakeLists.txt:323-343`):
- `UIKit`, `Foundation`, `QuartzCore`, `CoreGraphics` — UI + 2D primitives
- `CoreText` — font loading / text shaping on Apple
- `VideoToolbox`, `CoreMedia`, `CoreVideo` — hardware H.264 decode
- `ImageIO` — image encode/decode
- `OpenGLES` (when `PAG_USE_OPENGL=ON`) **or** `Metal` (when `PAG_USE_OPENGL=OFF`)
- `iconv` — character encoding conversion

**GPU context:**
- OpenGL ES via EAGLWindow / EAGLDevice used in `src/platform/ios/private/GPUDrawable.h`
- Metal is the TGFX Metal backend when OpenGL is disabled
- `CAEAGLLayer`-backed `PAGView` (`src/platform/ios/PAGView.mm`)

**Hardware video decoding** (`src/platform/ios/private/HardwareDecoder.h`):
- `VTDecompressionSessionRef` from `<VideoToolbox/VideoToolbox.h>`
- Uses `CMSampleBufferRef` for input bitstream, yields `CVPixelBufferRef` for GPU upload

**Font loading:**
- Native shaper uses `<CoreText/CoreText.h>` via `src/platform/cocoa/private/NativeTextShaper.mm` (shared between iOS and macOS)

**Display sync:**
- `CADisplayLink` wrapped in `src/platform/ios/private/NativeDisplayLink.mm`

**Public ObjC headers:**
- Umbrella: `src/platform/ios/libpag.h`
- Module map: `ios/libpag.modulemap`
- Bundle identifier: `com.tencent.libpag`

### macOS

**System frameworks linked** (`CMakeLists.txt:367-393`):
- `ApplicationServices`, `QuartzCore`, `Cocoa`, `Foundation`
- `VideoToolbox`, `CoreMedia` — hardware H.264 decode
- `OpenGL` (when `PAG_USE_OPENGL=ON`) **or** `Metal`
- `iconv`

**GPU context:**
- Desktop OpenGL via `NSOpenGLView`-backed `PAGView` (`src/platform/mac/PAGView.m`)

**Hardware video decoding:**
- Same VideoToolbox pipeline as iOS; separate impl at `src/platform/mac/private/HardwareDecoder.h/.mm`

**Font loading:** Shared CoreText path via `src/platform/cocoa/private/NativeTextShaper.mm`.

**Module map:** `mac/libpag.modulemap`.

### Android

**System libraries linked** (`CMakeLists.txt:406-415, 429-434`):
- `log` — `<android/log.h>` logging
- `android` — `ANativeWindow` and core NDK
- `jnigraphics` — `AndroidBitmap_*` pixel access
- `mediandk` — `<media/NdkMediaCodec.h>`, `<media/NdkMediaFormat.h>` hardware video decoder
- `GLESv2`, `GLESv3`, `EGL` — OpenGL ES context

**GPU context:**
- EGL surface via `ANativeWindow` — `src/platform/android/GPUDrawable.cpp` wraps `tgfx::EGLWindow`
- Delivered to Java as `android.view.Surface` through `JPAGSurface.cpp`

**Hardware video decoding** (`src/platform/android/HardwareDecoder.h`):
- `AMediaCodec` + `AMediaCodecBufferInfo` from `<media/NdkMediaCodec.h>`
- Uses `ANativeWindow` from `<android/native_window_jni.h>` to feed a `SurfaceTexture`
- Fallback software decoder: ffavc (prebuilt `libffavc.so` in `android/libpag/libs/${ANDROID_ABI}/`)

**Font loading:** `src/platform/android/FontConfigAndroid.cpp` — walks Android system font config XML, registers fallback fonts with TGFX.

**Display sync:** `src/platform/android/NativeDisplayLink.cpp` — wraps `android.view.Choreographer` via JNI.

**JNI bridge files:** All `J*.cpp` in `src/platform/android/` — `JPAG.cpp`, `JPAGPlayer.cpp`, `JPAGFile.cpp`, `JPAGImage.cpp`, `JPAGImageLayer.cpp`, `JPAGShapeLayer.cpp`, `JPAGTextLayer.cpp`, `JPAGSolidLayer.cpp`, `JPAGAnimator.cpp`, `JPAGDecoder.cpp`, `JPAGDiskCache.cpp`, `JPAGFont.cpp`, `JPAGImageView.cpp`, `JPAGSurface.cpp`, `JPAGComposition.cpp`, `JPAGLayer.cpp`, `JVideoDecoder.cpp`, `JVideoSurface.cpp`, `JNativeTask.cpp`, `JTraceImage.cpp`. Helpers: `JNIHelper.h/.cpp`.

**Link-time optimization:**
- Symbol export script `android/libpag/export.def` + `-Wl,--gc-sections --version-script` (`CMakeLists.txt:425`)
- `-fno-exceptions -fno-rtti -Os` for minimum binary size

**Android artifact:** `com.tencent.tav:libpag:4.1.0` (AAR), min SDK 21, target SDK 33, NDK 28.0.13004108. Depends on `androidx.exifinterface:exifinterface:1.3.3`.

### OpenHarmony (OHOS)

**System libraries linked** (`CMakeLists.txt:474-500`):
- GPU: `GLESv3`, `EGL`
- Windowing / graphics: `native_buffer`, `native_window`, `native_image`, `native_display_soloist`
- Pixel buffers: `pixelmap_ndk.z`, `image_source_ndk.z`, `pixelmap`, `image_source`
- NAPI runtime: `ace_ndk.z`, `ace_napi.z`
- Logging: `hilog_ndk.z`
- Raw files: `rawfile.z`
- Hardware video decode: `native_media_codecbase`, `native_media_core`, `native_media_vdec`

**GPU context:** EGL via `tgfx::EGLWindow` (`src/platform/ohos/GPUDrawable.h`).

**Hardware video decoding:** `OHOSVideoDecoder.cpp` uses `native_media_vdec` / `native_media_codecbase`. Software fallback: `OHOSSoftwareDecoderWrapper.cpp`.

**XComponent integration:** `XComponentHandler.cpp` — bridges surface lifecycle from OHOS XComponent.

**NAPI bridge:** All `JPAG*.cpp` in `src/platform/ohos/` mirror the Android JNI layer via NAPI. Helper: `JsHelper.h/.cpp`.

**Link-time symbol export:** `ohos/libpag/export.def` + `--gc-sections --version-script` (`CMakeLists.txt:503`).

**Package:** `@tencent/libpag@1.0.1` HAR.

### Web (Emscripten / WASM)

**Runtime environment** (`CMakeLists.txt:639-660`):
- Emscripten-compiled WASM + JS glue
- WebGL 2 (`-sMAX_WEBGL_VERSION=2`)
- ES6 module export with name `PAGInit` (`-sEXPORT_NAME='PAGInit' -sEXPORT_ES6=1 -sMODULARIZE=1`)
- Environments: `web,worker`
- Memory: `-sALLOW_MEMORY_GROWTH=1`; multithreaded build adds `-sUSE_PTHREADS=1 -sINITIAL_MEMORY=32MB -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency`
- `-fno-rtti` (RTTI disabled on web)

**GPU context:** Canvas-bound WebGL 2. `src/platform/web/GPUDrawable.cpp` calls `emscripten_get_canvas_element_size()` targeting a named canvas element.

**Hardware video decoding:** Delegated to JavaScript (`src/platform/web/HardwareDecoder.cpp`) via `WebVideoSequenceDemuxer`. `WebSoftwareDecoderFactory` provides JS-implemented software decoders.

**Text shaping:** `src/platform/web/NativeTextShaper.cpp` delegates to JS callbacks (Canvas/DOM font metrics). Unicode emoji table: `src/platform/web/UnicodeEmojiTable.hh`.

**JS binding layer:** `src/platform/web/PAGWasmBindings.cpp` uses `emscripten::bind` and `emscripten::val` to expose C++ classes as JS objects (`PAGFile`, `PAGPlayer`, `PAGSurface`, `PAGLayer` hierarchy, etc.).

**Build tooling:** Rollup + TypeScript in `web/src/`. Variants: `wasm` (ST), `wasm-mt` (MT), `wechat` (WeChat Mini Program with Brotli WASM).

**Blocked Emscripten version:** `4.0.11` (`CMakeLists.txt:643-648`) — build fatals if detected.

### Windows

**System libraries linked** (`CMakeLists.txt:438-459`):
- `opengl32` (native GL) **or** ANGLE static libs from `${TGFX_DIR}/vendor/angle/` when `PAG_USE_ANGLE=ON`
- `Bcrypt` — cryptographic primitives (required transitively)
- `ws2_32` — Winsock (required transitively)

**GPU context:** WGL on Win32 HWND, or ANGLE-backed GLES3 on D3D11. `src/platform/win/GPUDrawable.cpp`.

**Hardware video decoding:** None in `src/platform/win/`. Falls back to libavc software decoder.

**Font loading:** TGFX/FreeType.

**Preprocessor:** `NOMINMAX`, `_USE_MATH_DEFINES`, 64-bit forced.

**Demo project:** `win/Win32Demo.sln`.

### Linux

**System libraries linked** (`CMakeLists.txt:460-473`):
- `pthread` / `Threads::Threads`, `dl`
- `GLESv2`, `EGL`
- `Fontconfig` (CLI only, `CMakeLists.txt:771-773`) via `find_package(Fontconfig REQUIRED)`

**GPU context:** EGL + GLESv2 (`src/platform/linux/GPUDrawable.cpp`).

**Hardware video decoding:** None native; libavc software decoder only.

**Font loading:** System fontconfig (CLI); TGFX/FreeType otherwise.

**Compile flags:** `-fPIC -pthread`.

### Qt

**Frameworks linked** (`CMakeLists.txt:242-245`):
- `Qt6::Widgets`, `Qt6::OpenGL`, `Qt6::Quick`

**GPU context:** `QGLWindow` via `src/platform/qt/GPUDrawable.cpp`.

**Notes:** When Qt is enabled, SwiftShader and ANGLE are force-disabled; OpenGL is force-enabled. On macOS, `HardwareDecoder.mm` is still included for VideoToolbox decoding.

### SwiftShader (optional CPU backend)

**Libraries:** Shared libs globbed from `${TGFX_DIR}/vendor/swiftshader/${PLATFORM}/${ARCH}/*` (`CMakeLists.txt:253-258`). Used for headless/CI rendering.

---

## Hardware Video Decoding Summary

| Platform | Hardware API | Fallback |
|----------|-------------|----------|
| iOS | `VTDecompressionSession` (VideoToolbox) | none |
| macOS | `VTDecompressionSession` (VideoToolbox) | libavc |
| Android | `AMediaCodec` (NdkMediaCodec) | ffavc (`libffavc.so`) |
| OpenHarmony | `native_media_vdec` | OHOS software decoder wrapper |
| Web | JS-side decoders via `WebVideoSequenceDemuxer` | JS software decoder factory |
| Windows | none native | libavc |
| Linux | none native | libavc |

Software fallback chain controlled at configure time via `PAG_USE_LIBAVC` and `PAG_USE_FFAVC`. The `VideoDecoder` abstract base lives at `src/rendering/video/VideoDecoder.h`.

## Font Loading Summary

| Platform | Font source |
|----------|-------------|
| iOS / macOS | CoreText (`src/platform/cocoa/private/NativeTextShaper.mm`) |
| Android | Platform font config XML (`src/platform/android/FontConfigAndroid.cpp`) |
| Linux (CLI) | fontconfig |
| OHOS | TGFX default (FreeType) |
| Web | JS-side text shaping (`src/platform/web/NativeTextShaper.cpp`) |
| Windows | TGFX default (FreeType) |

## Format Support

| Format | Codec / Parser | Direction |
|--------|----------------|-----------|
| `.pag` | Tag-based binary codec (`src/codec/`) — 100+ tag types, LZ4-compressed blocks, versioned tags | Read + Write |
| `.pagx` | XML via Expat (`src/pagx/xml/`) + HarfBuzz + SheenBidi for text | Read + Write (`PAGXImporter` / `PAGXExporter`) |
| `.svg` | `SVGImporter.cpp` / `SVGExporter.cpp` (`src/pagx/svg/`), path parsing via `SVGPathParser.cpp` | Read + Write (`PAG_BUILD_SVG`) |
| `.webp` | TGFX/libwebp (baseline screenshots, sequence frames) | Read + Write |
| `.png` | TGFX/libpng | Read + Write |
| `.jpg` | TGFX/libjpeg-turbo | Read + Write |
| `.mp4` / H.264 | Hardware decoder per platform + libavc/ffavc fallback | Read (decode only) |

---

*Integration audit: 2026-04-28*
