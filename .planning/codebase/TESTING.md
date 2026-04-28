# Testing Patterns

**Analysis Date:** 2025-05-27

## Test Framework

**Runner:** Google Test (gtest)
- Config: CMake target `PAGFullTest` (all tests + screenshot comparison) / `PAGUnitTest` (no screenshot comparison, `SKIP_FRAME_COMPARE` defined)
- Base header: `test/src/base/PAGTest.h`

**Assertion Library:** Google Test built-in (`ASSERT_*`, `EXPECT_*`)

**Run Commands:**
```bash
# Build (required before running)
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest

# Run all tests
./cmake-build-debug/PAGFullTest

# Run a specific test case
./cmake-build-debug/PAGFullTest --gtest_filter="PAGFileTest.TestPAGFileBase"

# Run all tests in a suite
./cmake-build-debug/PAGFullTest --gtest_filter="PAGFileTest.*"
```

## Test File Organization

**Location:** `test/src/` — all test `.cpp` files co-located in one directory

**Naming:** `{SuiteName}Test.cpp` (e.g., `PAGFileTest.cpp`, `PAGFilterTest.cpp`, `PAGSurfaceTest.cpp`)

**Utilities:** `test/src/utils/` — shared helpers for all test files:
- `Baseline.h` / `Baseline.cpp` — screenshot comparison logic
- `TestUtils.h` / `TestUtils.cpp` — `LoadPAGFile()`, `MakeSnapshot()`, `GetLayer()`, `MakeImage()`, etc.
- `OffscreenSurface.h` / `OffscreenSurface.cpp` — off-screen GPU surface factory
- `DevicePool.h` / `DevicePool.cpp` — GL device management
- `ProjectPath.h` / `ProjectPath.cpp` — absolute path resolution
- `Semaphore.h` / `Semaphore.cpp` — synchronization primitive for multi-thread tests
- `TestDir.h` / `TestDir.cpp` — test output directory management

**Framework infrastructure:** `test/src/base/PAGTest.h` / `PAGTest.cpp`

## Test Structure

**Test Base Classes:**

| Class | Defined In | Purpose |
|-------|------------|---------|
| `pag::PAGTest` | `test/src/base/PAGTest.h` | Base for all PAG rendering tests; tracks `hasFailure` global |
| `pag::PAGXTest` | `test/src/base/PAGTest.h` | Extends PAGTest; creates `GLDevice` + `Context` in `SetUp()`, unlocks in `TearDown()` |
| `pag::CLITest` | `test/src/base/PAGTest.h` | Base for CLI-only tests; no GPU setup |

**Test Macros:**

```cpp
// Standard rendering test (uses PAGTest base)
PAG_TEST(SuiteName, TestName) {
  // test body
}

// PAGX XML test (uses PAGXTest base — GLDevice already set up)
PAGX_TEST(SuiteName, TestName) {
  // this->device and this->context are available
}

// CLI test (uses CLITest base — no GPU context)
CLI_TEST(SuiteName, TestName) {
  // test body
}
```

**Standard Setup Macros:**

```cpp
// Default setup: loads resources/apitest/test.pag, creates shared OffscreenSurface
PAG_SETUP(pagSurface, pagPlayer, pagFile);
// Expands to:
//   auto pagFile = LoadPAGFile("resources/apitest/test.pag");
//   auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
//   auto pagPlayer = std::make_shared<PAGPlayer>();
//   pagPlayer->setSurface(pagSurface); pagPlayer->setComposition(pagFile);

// Isolated setup: creates PAGSurface::MakeOffscreen() instead of shared pool
PAG_SETUP_ISOLATED(pagSurface, pagPlayer, pagFile);

// Custom path setup
PAG_SETUP_WITH_PATH(pagSurface, pagPlayer, pagFile, "resources/apitest/custom.pag");
```

**Typical Test Pattern:**
```cpp
/**
 * 用例描述: CornerPin用例
 */
PAG_TEST(PAGFilterTest, CornerPin) {
  auto pagFile = LoadPAGFile("resources/filter/cornerpin.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/CornerPin"));
}
```

## Screenshot Testing

**Mechanism:** `Baseline::Compare(surface, key)` compares rendered output against stored baseline images.

**Key format:** `"{Folder}/{Name}"` — e.g., `"PAGFilterTest/CornerPin"`, `"PAGSurfaceTest/Mask"`

**Output path:** `test/out/{Folder}/{Name}.webp`

**Baseline path:** `test/out/{Folder}/{Name}_base.webp`

**Version files:**
- `test/baseline/version.json` — committed repository baselines
- `test/baseline/.cache/version.json` — local cache of accepted versions

**Comparison logic:**
- Both repo and cache versions exist AND differ → skip comparison, return `true` (change accepted)
- Otherwise → compare rendered output against `_base.webp`; fail if missing or pixels differ

**Accepting baseline changes:**
- **Never** manually run `accept_baseline.sh` or modify `version.json` directly
- The only permitted workflow is the user running the `/accept-baseline` slash command

**Baseline.Compare overloads** (`test/src/utils/Baseline.h`):
```cpp
Baseline::Compare(std::shared_ptr<tgfx::Surface> surface, const std::string& key);
Baseline::Compare(const tgfx::Bitmap& bitmap, const std::string& key);
Baseline::Compare(const tgfx::Pixmap& pixmap, const std::string& key);
Baseline::Compare(const std::shared_ptr<PAGSurface>& surface, const std::string& key);
Baseline::Compare(const std::shared_ptr<ByteData>& byteData, const std::string& key);
```

**Test output images** are written as WebP at 100% quality via `tgfx::ImageCodec::Encode()`.

## Test Coverage

**Test Suites** (`test/src/`):

| File | Suite | Coverage Area |
|------|-------|---------------|
| `PAGFileTest.cpp` | `PAGFileTest` | PAGFile load, metadata, text/image layer access |
| `PAGPlayerTest.cpp` | `PAGPlayerTest` | PAGPlayer composition, flush, surface switching |
| `PAGSurfaceTest.cpp` | `PAGSurfaceTest` | Surface from texture, pixel readback |
| `PAGFilterTest.cpp` | `PAGFilterTest` | All 30+ filter effects (CornerPin, Bulge, blur, etc.) |
| `PAGLayerTest.cpp` | `PAGLayerTest` | Layer properties and manipulation |
| `PAGImageTest.cpp` | `PAGImageTest` | PAGImage creation and replacement |
| `PAGImageLayerTest.cpp` | `PAGImageLayerTest` | Image layer behavior |
| `PAGTextLayerTest.cpp` | `PAGTextLayerTest` | Text layer editing and rendering |
| `PAGShapeLayerTest.cpp` | `PAGShapeLayerTest` | Shape layer rendering |
| `PAGSolidLayerTest.cpp` | `PAGSolidLayerTest` | Solid layer rendering |
| `PAGCompositionTest.cpp` | `PAGCompositionTest` | Composition nesting and playback |
| `PAGBlendTest.cpp` | `PAGBlendTest` | Blend mode rendering |
| `PAGFontTest.cpp` | `PAGFontTest` | Font loading and fallback |
| `PAGDiskCacheTest.cpp` | `PAGDiskCacheTest` | Disk cache behavior |
| `PAGSequenceTest.cpp` | `PAGSequenceTest` | Bitmap/video sequence decoding |
| `PAGTimeStretchTest.cpp` | `PAGTimeStretchTest` | Time remapping |
| `PAGTimeUtilsTest.cpp` | `PAGTimeUtilsTest` | Time utility functions |
| `PAGGradientColorTest.cpp` | `PAGGradientColorTest` | Gradient color rendering |
| `PAGSimplePathTest.cpp` | `PAGSimplePathTest` | Path simplification |
| `PAGCompareFrameTest.cpp` | `PAGCompareFrameTest` | Frame-by-frame comparison |
| `PAGFuzzTest.cpp` | `PAGFuzzTest` | Fuzz / malformed input handling |
| `AsyncDecodeTest.cpp` | `AsyncDecode` | Async video/bitmap decoding |
| `MultiThreadCase.cpp` | `SimpleMultiThreadCase` | Concurrent multi-PAGPlayer rendering |
| `PAGXTest.cpp` | Various PAGX suites | PAGX import/export, layout, CLI operations |
| `PAGXSVGTest.cpp` | `PAGXSVGTest` | SVG export correctness |
| `PAGXCliTest.cpp` | Various CLI suites | CLI command execution (embed, export, verify, etc.) |

## Running Tests

**Build requirement:** Must pass `-DPAG_BUILD_TESTS=ON` to enable all modules.

```bash
# Standard build + run
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest
./cmake-build-debug/PAGFullTest

# Filter to one suite
./cmake-build-debug/PAGFullTest --gtest_filter="PAGFilterTest.*"

# Filter to one case
./cmake-build-debug/PAGFullTest --gtest_filter="PAGFileTest.TestPAGFileBase"

# Build with local TGFX source (when debugging rendering engine)
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DTGFX_DIR=../tgfx -B cmake-build-debuglocal
cmake --build cmake-build-debuglocal --target PAGFullTest
```

**Non-zero exit code from PAGFullTest is normal when tests fail** — do not re-run the same command.

**Test resources** live in `resources/` directory. Access via:
```cpp
LoadPAGFile("resources/apitest/test.pag");     // resolved to absolute path internally
MakePAGImage("resources/apitest/rotation.jpg");
```

**Numeric test values** (font sizes, coordinates, matrices) must use integers — no floating-point literals.

---

*Testing analysis: 2025-05-27*
