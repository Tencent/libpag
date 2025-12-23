# CODEBUDDY.md

This file provides guidance to CodeBuddy Code when working with code in this repository.

@./rules/Rules.md

## Project Overview

libpag is a real-time rendering library for PAG (Portable Animated Graphics) files. It renders vector and raster animations across iOS, Android, macOS, Windows, Linux, OpenHarmony, and Web platforms.

### Source Structure

| Directory | Purpose |
|-----------|---------|
| `include/pag/` | Public API headers |
| `src/base/` | Base data structures (properties, keyframes, effects, transforms) |
| `src/codec/` | PAG file format parsing (tag-based binary codec) |
| `src/rendering/` | Rendering pipeline (renderers, caches, graphics, filters) |
| `src/platform/` | Platform-specific code (cocoa, android, ios, mac, ohos, web, win) |
| `test/src/` | Google Test-based test cases |
| `third_party/tgfx/` | GPU abstraction layer (OpenGL, Metal, Vulkan) |

File extensions: `.h` (headers), `.cpp` (implementation), `.mm` (Objective-C++). Source files are auto-collected via `file(GLOB)` - no CMake changes needed for new files.

### Core Classes

- **PAGFile**: Load and manage PAG animation files
- **PAGComposition**: Container for layers with hierarchical structure
- **PAGLayer**: Base class for all layers (PAGImageLayer, PAGTextLayer, PAGSolidLayer, PAGShapeLayer)
- **PAGPlayer**: Orchestrates playback, manages composition and surface
- **PAGSurface**: GPU rendering target abstraction

### Rendering Pipeline

1. Load PAG file → Parse binary codec (tag-based format)
2. Build PAGComposition tree from parsed data
3. PAGPlayer manages playback state and progress
4. For each frame: update timelines → render layer tree via LayerRenderer → record drawing commands via Recorder → create immutable Graphic → execute GPU commands via tgfx → present to surface

### GPU Layer (tgfx)

tgfx is the GPU abstraction layer supporting multiple backends (OpenGL, Metal, Vulkan). Rendering uses a deferred pattern: commands are recorded, then executed on GPU.


## Build Commands

### Dependencies

```bash
# macOS - Install dependencies automatically
./sync_deps.sh

# Other platforms - Install depsync and run manually
npm install -g depsync
depsync
```

### Build Verification

After modifying code, use this command to verify compilation. Must pass `-DPAG_BUILD_TESTS=ON` to enable ALL modules (layers, svg, pdf, etc.).

```bash
cmake -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest -- -j 12
```

### Code Formatting (REQUIRED before commit)
```bash
./codeformat.sh
```
Run this before every commit. Ignore any error output - the script completes formatting regardless of reported errors.


## Testing

- Location: `/test/src/*Test.cpp`, based on Google Test framework
- Test code can access all private members via compile flags (no friend class needed)

### Run Tests
```bash
./cmake-build-debug/PAGFullTest                                        # Run all tests
./cmake-build-debug/PAGFullTest --gtest_filter=CanvasTest.drawRect     # Run specific test
./cmake-build-debug/PAGFullTest --gtest_filter=Canvas*                 # Run pattern match
```

### Screenshot Tests

- Use `Baseline::Compare(pixels, key)` where key format is `{folder}/{name}`, e.g., `PAGSurfaceTest/Mask`
- Screenshots output to `test/out/{folder}/{name}.webp`, baseline is `{name}_base.webp` in same directory
- Comparison mechanism: compares version numbers in `test/baseline/version.json` (repo) vs `.cache/version.json` (local) for the same key; only performs baseline comparison when versions match, otherwise skips and returns success

### Updating Baselines

- To accept screenshot changes, copy `test/out/version.json` to `test/baseline/`, **but MUST satisfy both**:
  - Use `version.json` output from running ALL test cases in `PAGFullTest`, never use partial test output
  - User explicitly confirms accepting all screenshot changes
- `UpdateBaseline` or `update_baseline.sh` syncs `test/baseline/version.json` to `.cache/` and generates local baseline cache. CMake warns when version.json files differ. **NEVER run this command automatically**