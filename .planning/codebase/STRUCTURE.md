# Codebase Structure

**Analysis Date:** 2025-07-15

## Directory Layout

```
libpag/
├── include/                        # Public headers (installed with the library)
│   ├── pag/                        # Core C++ API
│   │   ├── pag.h                   # Master include: PAGPlayer, PAGFile, PAGSurface, PAGImage…
│   │   ├── file.h                  # PAGFile, PAGComposition, PAGLayer types
│   │   ├── decoder.h               # PAGDecoder (frame-by-frame decode to pixels)
│   │   ├── gpu.h                   # GPU backend types (BackendTexture, BackendSemaphore)
│   │   ├── types.h                 # Shared value types (Color, Point, Rect, Matrix, etc.)
│   │   ├── defines.h               # PAG_API export macros
│   │   └── c/                      # C language bindings (pag_player.h, pag_file.h, …)
│   └── pagx/                       # PAGX XML format API (~100 headers)
│       ├── PAGXDocument.h          # Top-level XML document model
│       ├── PAGXImporter.h          # PAGXImporter: XML → PAGFile
│       ├── PAGXExporter.h          # PAGXExporter: PAGFile → XML
│       ├── SVGImporter.h           # SVGImporter
│       ├── SVGExporter.h           # SVGExporter
│       ├── FontConfig.h            # Font embedding/lookup config
│       ├── nodes/                  # Scene graph node types (Layer, Group, Text, Shape…)
│       └── types/                  # PAGX enum/value types (BlendMode, Alignment, etc.)
│
├── src/                            # Implementation sources
│   ├── base/                       # PAG data model (no rendering logic)
│   │   ├── Composition.cpp         # Base Composition + Vector/Bitmap/VideoComposition
│   │   ├── Layer.cpp               # Base Layer + Image/Shape/Solid/Text/PreCompose/Camera
│   │   ├── File.cpp                # File-level metadata
│   │   ├── shapes/                 # Shape primitives (Rectangle, Ellipse, ShapePath, Fill…)
│   │   ├── effects/                # Effect data (blur, drop shadow, glow, etc.)
│   │   ├── keyframes/              # Keyframe interpolation types
│   │   ├── layerStyles/            # Layer style data (stroke, outer glow, etc.)
│   │   ├── text/                   # Text document, animator, selector data
│   │   └── utils/                  # TGFXCast, TimeUtil, math helpers
│   │
│   ├── codec/                      # Binary PAG format encode/decode
│   │   ├── Codec.cpp               # Entry point: Codec::Decode / Codec::Encode
│   │   ├── CodecContext.cpp        # Decode/encode state context
│   │   ├── AttributeHelper.cpp     # Typed attribute read/write helpers
│   │   ├── DataTypes.cpp           # Primitive type serialization
│   │   ├── tags/                   # Per-feature tag handlers
│   │   │   ├── effects/            # Effect tag readers/writers
│   │   │   ├── shapes/             # Shape tag readers/writers
│   │   │   ├── layerStyles/        # Layer style tag readers/writers
│   │   │   └── text/               # Text tag readers/writers
│   │   ├── mp4/                    # MP4/H.264 muxing for video sequences
│   │   └── utils/                  # Codec utility helpers
│   │
│   ├── rendering/                  # Real-time rendering pipeline
│   │   ├── PAGPlayer.cpp           # PAGPlayer: playback control, owns PAGStage + RenderCache
│   │   ├── PAGSurface.cpp          # PAGSurface: rendering target, wraps Drawable
│   │   ├── PAGSurfaceFactory.cpp   # Platform-agnostic surface creation helpers
│   │   ├── PAGAnimator.cpp         # Animation frame timing
│   │   ├── PAGDecoder.cpp          # Frame-by-frame offline decoder
│   │   ├── PAGFont.cpp             # Font registration/lookup
│   │   ├── PAG.cpp                 # Global PAG init / version info
│   │   ├── FontManager.cpp         # System/embedded font manager
│   │   ├── Performance.cpp         # Render performance metrics
│   │   ├── FileReporter.cpp        # Telemetry/error reporting
│   │   ├── layers/                 # PAGLayer runtime wrappers
│   │   │   ├── PAGStage.cpp        # Root scene graph node; owns RenderCache attachment
│   │   │   ├── PAGLayer.cpp        # Base PAGLayer (wraps data-model Layer)
│   │   │   ├── PAGComposition.cpp  # PAGComposition (wraps Composition)
│   │   │   ├── PAGFile.cpp         # PAGFile (top-level file runtime wrapper)
│   │   │   ├── PAGImageLayer.cpp
│   │   │   ├── PAGShapeLayer.cpp
│   │   │   ├── PAGSolidLayer.cpp
│   │   │   ├── PAGTextLayer.cpp
│   │   │   └── ContentVersion.cpp  # Dirty-version counter for cache invalidation
│   │   ├── caches/                 # Frame and content caches
│   │   │   ├── RenderCache.cpp     # Master cache; manages GPU resource lifecycle
│   │   │   ├── LayerCache.cpp      # Per-layer snapshot cache
│   │   │   ├── ShapeContentCache.cpp
│   │   │   ├── TextContentCache.cpp
│   │   │   ├── ImageContentCache.cpp
│   │   │   ├── CompositionCache.cpp
│   │   │   ├── DiskCache.cpp       # Disk-backed cache
│   │   │   ├── DiskIOWorker.cpp    # Background I/O thread
│   │   │   ├── SequenceFile.cpp    # Video/bitmap sequence disk cache
│   │   │   ├── TextBlock.cpp       # Shaped text block cache
│   │   │   └── GraphicContent.cpp
│   │   ├── renderers/              # Per-content-type rendering logic
│   │   │   ├── LayerRenderer.cpp   # Dispatch + transform/effect chain
│   │   │   ├── ShapeRenderer.cpp
│   │   │   ├── TextRenderer.cpp
│   │   │   ├── FilterRenderer.cpp  # Effect/filter application
│   │   │   ├── CompositionRenderer.cpp
│   │   │   ├── MaskRenderer.cpp
│   │   │   ├── TrackMatteRenderer.cpp
│   │   │   ├── TransformRenderer.cpp
│   │   │   ├── TextAnimatorRenderer.cpp
│   │   │   └── TextSelectorRenderer.cpp
│   │   ├── filters/                # GPU filter implementations (32+ types)
│   │   │   ├── gaussianblur/       # Gaussian blur passes
│   │   │   ├── glow/               # Glow/outer glow filters
│   │   │   ├── layerstyle/         # Drop shadow, stroke, etc.
│   │   │   ├── utils/              # Filter utilities
│   │   │   ├── LayerStylesFilter.cpp
│   │   │   ├── MotionBlurFilter.cpp
│   │   │   ├── DisplacementMapFilter.cpp
│   │   │   └── … (20+ more filter files)
│   │   ├── graphics/               # Low-level graphic primitives
│   │   │   ├── Recorder.h          # Draw command recorder
│   │   │   ├── Picture.h           # Raster/GPU picture wrapper
│   │   │   ├── Shape.h             # Vector path wrapper
│   │   │   ├── Snapshot.h          # GPU texture snapshot
│   │   │   └── ImageProxy.h        # Lazy image decode proxy
│   │   ├── sequences/              # Async sequence decode pipeline
│   │   │   ├── SequenceInfo.h      # Sequence metadata
│   │   │   └── SequenceImageQueue.h # Async frame queue
│   │   ├── drawables/              # Platform Drawable implementations
│   │   ├── editing/                # Runtime layer editing helpers
│   │   ├── video/                  # Video sequence decode coordination
│   │   └── utils/                  # Render utilities (ScaleMode, LockGuard, shaper/)
│   │
│   ├── renderer/                   # Text layout engine (HarfBuzz/BiDi integration)
│   │   ├── TextShaper.cpp          # HarfBuzz shaping
│   │   ├── LineBreaker.cpp         # Unicode line breaking
│   │   ├── BidiResolver.cpp        # Bidirectional text
│   │   ├── GlyphRunRenderer.cpp    # Glyph run draw calls
│   │   ├── LayerBuilder.cpp        # Text layer build from shaped glyphs
│   │   ├── FontEmbedder.cpp        # Font data embedding
│   │   ├── ImageEmbedder.cpp       # Inline image embedding
│   │   ├── PunctuationSquash.cpp   # CJK punctuation compression
│   │   └── ToTGFX.cpp              # Convert to TGFX draw calls
│   │
│   ├── pagx/                       # PAGX XML format support
│   │   ├── PAGXImporter.cpp        # XML → PAGFile conversion (Expat parser)
│   │   ├── PAGXExporter.cpp        # PAGFile → XML serialization
│   │   ├── PAGXDocument.cpp        # PAGX document model
│   │   ├── FontConfig.cpp          # Font config for PAGX
│   │   ├── SystemFonts.cpp         # OS font enumeration (silent fail)
│   │   ├── TextLayout.cpp          # PAGX text layout engine
│   │   ├── LayoutContext.cpp       # Layout pass context
│   │   ├── LayoutNode.cpp          # Node layout calculations
│   │   ├── PathData.cpp            # Path data handling
│   │   ├── nodes/                  # PAGX node implementations
│   │   ├── svg/                    # SVG import/export
│   │   ├── utils/                  # PAGX utility helpers
│   │   └── xml/                    # Expat XML wrapper
│   │
│   ├── cli/                        # pagx-cli command-line tool
│   │   ├── main.cpp                # CLI entry point, command dispatch
│   │   ├── CliUtils.cpp            # Shared CLI helpers
│   │   ├── CommandBounds.cpp       # `bounds` command
│   │   ├── CommandEmbed.cpp        # `embed` command (embed fonts/images into PAGX)
│   │   ├── CommandExport.cpp       # `export` command (PAGX → PAG)
│   │   ├── CommandFont.cpp         # `font` command (list/verify fonts)
│   │   ├── CommandFormat.cpp       # `format` command
│   │   ├── CommandImport.cpp       # `import` command (PAG → PAGX)
│   │   ├── CommandLayout.cpp       # `layout` command
│   │   ├── CommandRender.cpp       # `render` command (render to image)
│   │   ├── CommandResolve.cpp      # `resolve` command
│   │   ├── CommandVerify.cpp       # `verify` command
│   │   ├── FormatUtils.cpp         # Output formatting helpers
│   │   ├── LayoutUtils.cpp         # Layout computation helpers
│   │   └── XPathQuery.cpp          # XPath-like XML query helper
│   │
│   ├── platform/                   # Platform-specific implementations
│   │   ├── Platform.cpp / .h       # Platform abstraction interface
│   │   ├── android/                # JNI bindings, hardware video, EGL context
│   │   ├── cocoa/                  # Shared iOS+macOS: font loading, Metal context
│   │   │   └── private/
│   │   ├── ios/                    # iOS-specific: UIKit integration, VideoToolbox
│   │   │   └── private/
│   │   ├── mac/                    # macOS-specific: AppKit, CVDisplayLink
│   │   │   └── private/
│   │   ├── win/                    # Windows: DXGI/OpenGL context, DirectWrite fonts
│   │   ├── linux/                  # Linux: EGL/GLX context, Fontconfig
│   │   ├── ohos/                   # OpenHarmony
│   │   ├── web/                    # Emscripten/WASM, WebGL
│   │   ├── qt/                     # Qt platform abstraction
│   │   └── swiftshader/            # CPU rendering via SwiftShader
│   │
│   └── c/                          # C language API bindings
│       ├── pag_player.cpp
│       ├── pag_file.cpp
│       ├── pag_surface.cpp
│       ├── pag_image.cpp
│       ├── pag_layer.cpp
│       ├── pag_composition.cpp
│       ├── pag_text_document.cpp
│       ├── pag_font.cpp
│       ├── pag_decoder.cpp
│       ├── pag_animator.cpp
│       ├── pag_types.cpp
│       └── ext/                    # EGL extension bindings
│
├── test/
│   ├── src/                        # Google Test cases
│   │   ├── PAGFileTest.cpp         # Core file loading and rendering
│   │   ├── PAGPlayerTest.cpp       # Player control and lifecycle
│   │   ├── PAGSurfaceTest.cpp      # Surface and GPU backend
│   │   ├── PAGTextLayerTest.cpp    # Text rendering
│   │   ├── PAGFilterTest.cpp       # Filter effects
│   │   ├── PAGXTest.cpp            # PAGX format round-trip
│   │   ├── PAGXCliTest.cpp         # CLI command tests
│   │   ├── PAGXSVGTest.cpp         # SVG import/export
│   │   ├── PAGFontTest.cpp         # Font loading/embedding
│   │   ├── PAGImageLayerTest.cpp
│   │   ├── PAGShapeLayerTest.cpp
│   │   ├── PAGSequenceTest.cpp
│   │   ├── PAGBlendTest.cpp
│   │   ├── PAGDiskCacheTest.cpp
│   │   ├── AsyncDecodeTest.cpp
│   │   ├── base/                   # Base/utility test helpers
│   │   └── utils/                  # Test utility code
│   ├── baseline/                   # Screenshot baseline version tracking
│   │   └── version.json            # Baseline version registry (do not modify manually)
│   └── out/                        # Generated test output screenshots (gitignored)
│
├── resources/                      # Test fixture files
│   ├── AE/                         # After Effects exported PAG/PAGX files
│   ├── cli/                        # CLI test inputs
│   ├── font/                       # Test font files
│   ├── svg/                        # SVG test files
│   ├── text/                       # Text test resources
│   └── …                           # Other fixture categories
│
├── third_party/                    # Vendored dependencies (not committed in full)
│   ├── tgfx/                       # TGFX 2D GPU engine (core dependency)
│   ├── harfbuzz/                   # Text shaping
│   ├── expat/                      # XML parsing (used by PAGX)
│   ├── sheenbidi/                  # Unicode BiDi algorithm
│   ├── lz4/                        # LZ4 compression (codec layer)
│   └── libavc/                     # H.264 software decoder
│
├── android/                        # Android Studio project / AAR build
├── ios/                            # Xcode project / iOS framework build
├── mac/                            # macOS framework build
├── win/                            # Windows build helpers
├── linux/                          # Linux build helpers
├── web/                            # Web/WASM build
├── ohos/                           # OpenHarmony build
├── viewer/                         # Desktop PAG viewer application
├── exporter/                       # After Effects exporter plugin
├── playground/                     # Development scratch area
├── spec/                           # PAG format specification
├── assets/                         # Asset lists and metadata
├── CMakeLists.txt                  # Root CMake build definition
├── DEPS                            # Dependency version pins
├── vendor.json                     # Third-party vendor manifest
├── sync_deps.sh                    # Dependency sync script (run before first build)
├── codeformat.sh                   # clang-format + header guard formatter
└── accept_baseline.sh              # Screenshot baseline acceptance (run via /accept-baseline)
```

## Key Files

| File | Purpose |
|------|---------|
| `include/pag/pag.h` | Master public C++ API header |
| `include/pag/file.h` | PAGFile, PAGComposition, layer type declarations |
| `include/pagx/PAGXImporter.h` | PAGX XML import entry point |
| `include/pagx/PAGXExporter.h` | PAGX XML export entry point |
| `src/rendering/PAGPlayer.cpp` | Animation playback engine |
| `src/rendering/PAGSurface.cpp` | GPU rendering surface |
| `src/rendering/layers/PAGStage.cpp` | Root render scene graph |
| `src/rendering/caches/RenderCache.cpp` | GPU resource / frame cache |
| `src/rendering/renderers/LayerRenderer.cpp` | Per-layer render dispatch |
| `src/codec/Codec.cpp` | Binary PAG encode/decode entry |
| `src/pagx/PAGXImporter.cpp` | PAGX XML parser |
| `src/pagx/SystemFonts.cpp` | OS font enumeration (silent on failure) |
| `src/cli/main.cpp` | `pagx-cli` entry point |
| `src/platform/Platform.h` | Platform abstraction interface |
| `CMakeLists.txt` | Build system root |
| `test/baseline/version.json` | Screenshot baseline version registry |

## Module Boundaries

| Module | Allowed Dependencies | Forbidden |
|--------|---------------------|-----------|
| `src/base/` | Standard library, `include/pag/types.h` | rendering, codec, pagx, platform |
| `src/codec/` | `src/base/`, LZ4, MP4 utils | rendering, pagx, platform |
| `src/rendering/` | `src/base/`, `src/renderer/`, TGFX, platform `Drawable` | codec (via `PAGFile::Load` result), pagx |
| `src/renderer/` | `src/base/`, HarfBuzz, TGFX | rendering (no upward deps) |
| `src/pagx/` | `src/base/`, `src/codec/`, Expat, `src/renderer/` | rendering (uses codec output) |
| `src/cli/` | `src/pagx/`, `src/rendering/`, `include/pagx/` | platform internals |
| `src/platform/` | `src/rendering/` drawables, TGFX | `src/base/` data model directly |
| `src/c/` | `include/pag/` public API only | internal rendering headers |

## Public API Surface

### C++ API (`include/pag/`)

- **`PAGPlayer`** — playback control: `setComposition`, `setProgress`, `flush`, `setSurface`
- **`PAGFile`** — file loading: `PAGFile::Load(path)`, `PAGFile::Load(bytes, size)`
- **`PAGSurface`** — rendering target: `PAGSurface::MakeFromWindow(...)`, `PAGSurface::MakeOffscreen(...)`
- **`PAGImage`** — image replacement: `PAGImage::FromPath(...)`, `PAGImage::FromPixels(...)`
- **`PAGFont`** — font registration: `PAGFont::RegisterFont(...)`
- **`PAGDecoder`** — frame-by-frame decode: `PAGDecoder::Make(file, ...)`, `copyFrameTo(pixels, ...)`
- **`PAGAnimator`** — animation timing helper
- **`PAGComposition`** / **`PAGLayer`** / typed sublayers — scene graph manipulation

### PAGX API (`include/pagx/`)

- **`PAGXImporter`** — `PAGXImporter::Import(path)` → `std::shared_ptr<PAGFile>`
- **`PAGXExporter`** — `PAGXExporter::Export(file, path)`
- **`PAGXDocument`** — XML document model
- **`SVGImporter`** / **`SVGExporter`** — SVG conversion
- **`FontConfig`** — font embedding/lookup configuration
- **Node types** in `include/pagx/nodes/` — Layer, Group, Text, Image, Shape primitives
- **Value types** in `include/pagx/types/` — enums and structs used by node properties

### C API (`include/pag/c/`)

Thin C wrappers over the C++ API. All types are opaque pointers (`pag_player_t*`, etc.). Intended for FFI use from non-C++ languages.

## Naming Conventions

**Files:**
- C++ class files: `ClassName.cpp` / `ClassName.h` (PascalCase)
- CLI command files: `CommandVerb.cpp` / `CommandVerb.h`
- C API files: `pag_noun.cpp` / `pag_noun.h` (snake_case)

**Directories:**
- Lowercase, plural for categories (`renderers/`, `caches/`, `filters/`, `layers/`)
- PascalCase for platform names (`android/`, `cocoa/`, `ios/`)

## Where to Add New Code

**New PAG data type / shape / effect:**
- Data model: `src/base/shapes/` or `src/base/effects/`
- Codec tag: `src/codec/tags/shapes/` or `src/codec/tags/effects/`
- Renderer: `src/rendering/renderers/`
- Test: `test/src/PAGShapeLayerTest.cpp` or new `test/src/PAG{Feature}Test.cpp`

**New PAGX node type:**
- Public header: `include/pagx/nodes/NewNode.h`
- Implementation: `src/pagx/nodes/`
- Import/export support: `src/pagx/PAGXImporter.cpp` + `src/pagx/PAGXExporter.cpp`

**New CLI command:**
- `src/cli/CommandNewVerb.cpp` + `src/cli/CommandNewVerb.h`
- Register in `src/cli/main.cpp`
- Test: `test/src/PAGXCliTest.cpp`

**New filter effect:**
- `src/rendering/filters/NewFilter.cpp` + `.h`
- Wire into `LayerStylesFilter` or `FilterRenderer` as appropriate

**New platform:**
- `src/platform/{platform}/` directory
- Implement `Drawable` interface for GPU context + surface
- Implement font loading + video decode hooks

---

*Structure analysis: 2025-07-15*
