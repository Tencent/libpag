# CODEBUDDY.md

This file provides guidance to CodeBuddy Code when working with code in this repository.

@./rules/Rules.md

## Project Overview

libpag is a real-time rendering library for PAG (Portable Animated Graphics) files. It renders vector and raster animations across iOS, Android, macOS, Windows, Linux, OpenHarmony, and Web platforms.

### Key Directories

| Directory | Purpose |
|-----------|---------|
| `include/pag/` | Public API headers |
| `src/base/` | Base data structures (properties, keyframes, effects, transforms) |
| `src/codec/` | PAG file format parsing (tag-based binary codec) |
| `src/rendering/` | Rendering pipeline (renderers, caches, graphics, filters) |
| `src/platform/` | Platform-specific code (cocoa, android, ios, mac, ohos, web, win) |
| `test/src/` | Google Test-based test cases |
| `third_party/tgfx/` | GPU abstraction layer (OpenGL, Metal, Vulkan) |

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

### Build and Run Tests

```bash
mkdir build && cd build
cmake -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target PAGFullTest -- -j 12
./PAGFullTest
```

### Run Specific Test

```bash
./PAGFullTest --gtest_filter="TestSuiteName.TestName"
```

### Code Formatting

```bash
./codeformat.sh
```

Run before committing. Ignore output errors - the formatting completes regardless.

## Testing

- Location: `/test/src/*Test.cpp`, based on Google Test framework
- Test code can access all private members via compile flags (no friend class needed)

### Screenshot Tests

- Use `Baseline::Compare(pixels, key)` where key format is `{folder}/{name}`, e.g., `PAGSurfaceTest/Mask`
- Screenshots output to `test/out/{folder}/{name}.webp`, baseline is `{name}_base.webp` in same directory
- Comparison mechanism: compares version numbers in `test/baseline/version.json` (repo) vs `.cache/version.json` (local) for the same key; only performs baseline comparison when versions match, otherwise skips and returns success

### Updating Baselines

- To accept screenshot changes, copy `test/out/version.json` to `test/baseline/`, **but MUST satisfy both**:
  - Use `version.json` output from running ALL test cases in `PAGFullTest`, never use partial test output
  - User explicitly confirms accepting all screenshot changes
- `UpdateBaseline` or `update_baseline.sh` syncs `test/baseline/version.json` to `.cache/` and generates local baseline cache. CMake warns when version.json files differ. **NEVER run this command automatically**

## Key CMake Options

- `-DPAG_BUILD_TESTS=ON`: Enable test targets (required for full module compilation)
- `-DCMAKE_BUILD_TYPE=Debug`: Debug build with profiling enabled

## File Conventions

- Headers: `.h`, Implementation: `.cpp`, Objective-C++: `.mm`
- Test files: `*Test.cpp` in `/test/src/`
- Source files auto-collected via `file(GLOB)` - no CMake changes needed for new files
