<!-- refreshed: 2025-07-15 -->
# Architecture

**Analysis Date:** 2025-07-15

## System Overview

```text
┌──────────────────────────────────────────────────────────────────────────┐
│                        Public API Layer                                   │
│  include/pag/pag.h  include/pag/file.h  include/pag/decoder.h            │
│  C API: include/pag/c/  │  PAGX API: include/pagx/                        │
└──────────┬──────────────┬───────────────────────┬────────────────────────┘
           │              │                       │
           ▼              ▼                       ▼
┌──────────────┐  ┌──────────────────┐  ┌──────────────────────────────┐
│  Codec Layer │  │  Rendering Layer │  │    PAGX / CLI Tools          │
│ src/codec/   │  │  src/rendering/  │  │  src/pagx/  src/cli/         │
│  (decode /   │  │  (playback +     │  │  (XML import/export,         │
│   encode     │  │   GPU submit)    │  │   SVG, command-line tool)    │
│   PAG binary)│  │                  │  │                              │
└──────┬───────┘  └────────┬─────────┘  └──────────────────────────────┘
       │                   │
       ▼                   ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                       Data Model  src/base/                              │
│  Composition, Layer, Shape, Effect, Keyframe, Transform, Sequence        │
└──────────────────────────────────────┬──────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│              Text Layout Engine  src/renderer/                           │
│  HarfBuzz shaping, line breaking, BiDi, font embedding                   │
└──────────────────────────────────────┬──────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│             TGFX 2D Graphics Engine  (third_party/tgfx)                  │
│  GPU backend (OpenGL/Metal/Vulkan), path rendering, image decoding        │
└─────────────────────────────────────────────────────────────────────────┘
           │
           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│           Platform Abstraction  src/platform/                            │
│  android/ ios/ mac/ cocoa/ win/ linux/ ohos/ web/ qt/ swiftshader/       │
└─────────────────────────────────────────────────────────────────────────┘
```

## Core Subsystems

| Subsystem | Location | Responsibility |
|-----------|----------|----------------|
| Data Model | `src/base/` | PAG scene graph: Compositions, Layers, Shapes, Effects, Keyframes |
| Codec | `src/codec/` | Tag-based binary PAG format encode/decode; LZ4 compression; MP4 muxing |
| Rendering | `src/rendering/` | PAGPlayer/PAGSurface/PAGStage, frame caches, GPU draw submission |
| Text Layout | `src/renderer/` | HarfBuzz text shaping, BiDi, line breaking, font embedding |
| PAGX | `src/pagx/` | XML format import/export, SVG support, layout engine |
| CLI | `src/cli/` | `pagx-cli` commands: embed, export, import, font, render, verify, etc. |
| Platform | `src/platform/` | GPU context init, hardware video decode, system font loading, event loop |
| C Bindings | `src/c/` | C language API wrapping the C++ public API |
| Public C++ API | `include/pag/` | `PAGPlayer`, `PAGFile`, `PAGSurface`, `PAGImage`, `PAGFont`, etc. |
| PAGX Public API | `include/pagx/` | `PAGXImporter`, `PAGXExporter`, `PAGXDocument`, node/type headers |

## Data Flow

### Primary Playback Path

1. **Load PAG file** — `PAGFile::Load(path)` (`include/pag/file.h`, `src/rendering/layers/PAGFile.cpp`)
2. **Binary decode** — `Codec::Decode()` reads tag stream (`src/codec/Codec.cpp`)
3. **Build object tree** — `Composition`, `Layer`, `Shape` objects in `src/base/`
4. **Create player** — `PAGPlayer` owns `PAGStage` + `RenderCache` (`src/rendering/PAGPlayer.cpp`)
5. **Attach surface** — `PAGPlayer::setSurface(PAGSurface)` wraps platform `Drawable`
6. **Set progress** — `PAGPlayer::setProgress(t)` updates animation state on `PAGStage`
7. **Render** — `PAGPlayer::flush()` → `PAGStage::draw()` → per-layer renderers → TGFX GPU submit

### PAGX Conversion Path

1. **Import** — `PAGXImporter::Import(path)` parses XML via Expat (`src/pagx/PAGXImporter.cpp`)
2. **Build PAGX scene graph** — `PAGXDocument` + `pagx::Node` tree (`include/pagx/`)
3. **Export to PAG** — `PAGXExporter::Export()` serializes binary PAG (`src/pagx/PAGXExporter.cpp`)
4. **CLI command** — `CommandImport` / `CommandExport` wrappers in `src/cli/`

### Render Frame Flow (detail)

```
PAGPlayer::flush()
  └─ RenderCache::beginFrame()          [src/rendering/caches/RenderCache.cpp]
  └─ PAGStage::draw(recorder)           [src/rendering/layers/PAGStage.cpp]
       └─ PAGLayer::draw()              [src/rendering/layers/PAGLayer.cpp]
            └─ LayerRenderer::draw()    [src/rendering/renderers/LayerRenderer.cpp]
                 ├─ ContentCache hit?   [src/rendering/caches/]
                 ├─ ShapeRenderer / TextRenderer / FilterRenderer
                 └─ TrackMatteRenderer / MaskRenderer
  └─ RenderCache::attachToContext()     → TGFX draw calls → GPU submit
```

### State Management

- All mutations guarded by `rootLocker` (a shared mutex on `PAGStage`)
- `LockGuard` / `ScopedLock` used consistently in `PAGPlayer` public methods
- Cache invalidation via `ContentVersion` dirty-tracking (`src/rendering/layers/ContentVersion.h`)

## Layer Hierarchy

### Composition Classes (`src/base/`)

```
Composition (abstract)
├─ VectorComposition          — vector layers, shapes, text
├─ BitmapComposition          — raster bitmap sequence
└─ VideoComposition           — H.264/H.265 video sequence
```

### Layer Classes (`src/base/`)

```
Layer (abstract)
├─ ImageLayer                 — bitmap image content
├─ ShapeLayer                 — vector shape groups
├─ SolidLayer                 — solid color fill
├─ TextLayer                  — text with animator
├─ PreComposeLayer            — nested composition reference
└─ CameraLayer                — 3D camera
```

### PAG Runtime Layer Classes (`src/rendering/layers/`)

```
PAGLayer (public API wrapper around data-model Layer)
├─ PAGComposition             — wraps Composition; supports child layers
│    ├─ PAGFile               — top-level file wrapper
│    └─ PAGStage              — root render graph (owns RenderCache)
├─ PAGImageLayer
├─ PAGShapeLayer
├─ PAGSolidLayer
└─ PAGTextLayer
```

### Renderer Classes (`src/rendering/renderers/`)

| Renderer | Handles |
|----------|---------|
| `LayerRenderer` | Dispatch to typed renderers; apply transforms/effects |
| `ShapeRenderer` | Vector shape drawing via TGFX paths |
| `TextRenderer` | Text layout integration; glyph drawing |
| `FilterRenderer` | Effect/filter chain application |
| `CompositionRenderer` | Nested composition flattening |
| `MaskRenderer` | Alpha/luma mask application |
| `TrackMatteRenderer` | Track matte compositing |
| `TransformRenderer` | Transform/opacity application |
| `TextAnimatorRenderer` | Per-character text animation |

### Cache Classes (`src/rendering/caches/`)

| Cache | Stores |
|-------|--------|
| `RenderCache` | Master cache coordinator; owns GPU context attachment |
| `LayerCache` | Per-layer transform/content snapshots |
| `ShapeContentCache` | Pre-rasterized shape geometry |
| `TextContentCache` | Shaped text glyph runs |
| `ImageContentCache` | Decoded image bitmaps |
| `CompositionCache` | Flattened composition snapshots |
| `SequenceFile` | Disk-backed video/bitmap sequence cache |
| `DiskCache` | General disk I/O cache with worker thread |

## Key Design Patterns

### 1. Data Model / Runtime Wrapper Split
`src/base/` contains pure data (`Layer`, `Composition`, `Shape`, etc.) with no rendering logic. `src/rendering/layers/` wraps these in `PAGLayer`/`PAGComposition` runtime objects that hold cache state and expose the public API. Data objects are decoded once; runtime wrappers are created per `PAGPlayer` instance.

### 2. Tag-Based Codec
The PAG binary format uses a tag-stream design (`src/codec/tags/`). Each feature (effect, shape type, layer style) maps to one or more numeric tag IDs. New features add new tags without breaking backward compatibility. Tags above a known version are skipped by older decoders.

### 3. Dirty-Region Cache Invalidation
`ContentVersion` counters (`src/rendering/layers/ContentVersion.h`) track when layer content changes. Renderers check version numbers before re-rasterizing, enabling frame-to-frame content reuse without explicit cache invalidation calls.

### 4. Platform Abstraction via `Drawable`
`PAGSurface` holds a `Drawable` interface (`src/rendering/drawables/`). Each platform implements `Drawable` to provide a TGFX `Surface` backed by the platform's GPU context (EGL on Android, CAMetalLayer on iOS/Mac, etc.).

### 5. Sequence Decode Pipeline
Video/bitmap sequences have a dedicated async pipeline: `SequenceInfo` → `SequenceImageQueue` → hardware decoder (via platform layer) → `RenderCache` frame promotion. `DiskIOWorker` handles background I/O for disk-cached sequences.

### 6. No Exceptions / No RTTI
The codebase prohibits `throw`/`try`/`catch` and `dynamic_cast`. Error propagation uses return values, `nullptr`, or output parameters.

## Error Handling

**Strategy:** Return `nullptr` / empty objects on failure; no exceptions.

**Patterns:**
- `PAGFile::Load()` returns `nullptr` on decode failure
- `std::shared_ptr` used for all heap-allocated public objects (automatic lifetime)
- Platform implementations return empty on silent failure (e.g., `src/cli/`, `src/pagx/SystemFonts.cpp`)

## Cross-Cutting Concerns

**Logging:** No `LOG` macros in `src/cli/` or `src/pagx/SystemFonts.cpp`; silent empty-return convention.
**Thread safety:** `rootLocker` mutex on `PAGStage`; `LockGuard`/`ScopedLock` wrappers.
**Memory:** `std::shared_ptr` ownership throughout public API; `RenderCache` manages GPU resource lifetime.
**GPU:** All GPU calls go through TGFX (`third_party/tgfx`); libpag never calls GPU APIs directly.

---

*Architecture analysis: 2025-07-15*
