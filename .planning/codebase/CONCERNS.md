# Codebase Concerns

**Analysis Date:** 2025-01-27

---

## Tech Debt

### Raw Memory Management in Codec Layer

- Issue: `src/codec/` and `src/codec/tags/` extensively use raw `new[]`/`delete[]` and manual `delete` for object lifetimes, instead of `std::unique_ptr` or `std::vector`. This includes `DecodeStream`, `EncodeStream`, `CodecContext`, `LayerTag`, `Attributes.h`, and `AttributeHelper.h`.
- Files: `src/codec/utils/EncodeStream.cpp`, `src/codec/utils/DecodeStream.h`, `src/codec/utils/EncodeStream.h`, `src/codec/tags/LayerTag.cpp`, `src/codec/AttributeHelper.h`, `src/codec/Attributes.h`, `src/codec/CodecContext.cpp`, `src/codec/DataTypes.cpp`
- Impact: Memory leak risk on early-return error paths; manual ownership tracking required when extending the codec.
- Fix approach: Migrate raw arrays to `std::vector<uint8_t>` or `std::unique_ptr<uint8_t[]>`; use RAII wrappers for C-style allocations.

### `mutable` Members in Stream Classes (Convention Violation)

- Issue: `DecodeStream::context` and `EncodeStream::context` are declared `mutable StreamContext*`, used to record errors from `const` read/write methods. This violates the project convention to avoid `mutable`.
- Files: `src/codec/utils/DecodeStream.h:233`, `src/codec/utils/EncodeStream.h:228`
- Impact: Misleads callers about const-correctness; mutating error state through const methods obscures whether a read produced side effects.
- Fix approach: Make all error-producing read/write methods non-const, passing context explicitly or storing it non-mutably.

### Lambda Usage in MP4Generator and SVGExporter

- Issue: `src/codec/mp4/MP4Generator.cpp` contains ~17 lambdas (`[&]`, `[this]`) for all write-function callbacks. `src/pagx/svg/SVGExporter.cpp` and `src/platform/web/NativeTextShaper.cpp` also use lambdas. This violates the project convention to avoid lambdas in favor of explicit methods.
- Files: `src/codec/mp4/MP4Generator.cpp`, `src/pagx/svg/SVGExporter.cpp`, `src/platform/web/NativeTextShaper.cpp`
- Impact: Harder to read call stacks in debuggers; inconsistency with the rest of the codebase.
- Fix approach: Refactor each lambda into a named private method or static function.

### `std::function` Usage in Codec Attribute Helpers

- Issue: `src/codec/AttributeHelper.h` stores `std::function<void(DecodeStream*, void*)>` and `std::function<bool(EncodeStream*, void*)>` as fields in `CustomAttribute`, and `src/codec/tags/ShapeTag.cpp` uses `std::unordered_map<TagCode, std::function<ReadShapeHandler>>`. `std::function` carries heap allocation and type-erasure overhead.
- Files: `src/codec/AttributeHelper.h`, `src/codec/tags/ShapeTag.cpp`
- Impact: Performance cost in hot codec paths; indirection obscures static dispatch.
- Fix approach: Replace with function pointer templates or virtual dispatch where appropriate.

### Dead Code: OHOS Hardware Texture Path Disabled with `if (false)`

- Issue: `OHOSVideoDecoder::onRenderFrame()` has the hardware NativeBuffer texture path guarded by `if (false && codecCategory == HARDWARE)`. The code exists but is permanently bypassed, requiring asynchronous HarmonyOS hardware decoding to be re-enabled once the platform fixes a jitter bug.
- Files: `src/platform/ohos/OHOSVideoDecoder.cpp:264`
- Impact: HarmonyOS hardware video decoding silently falls back to software YUV path, reducing performance on real devices. The dead code path may bitrot.
- Fix approach: Re-enable when HarmonyOS fixes the jitter issue; track with an upstream HarmonyOS issue link.

---

## Complexity Hotspots

### CommandVerify.cpp — 2841 Lines

- Files: `src/cli/CommandVerify.cpp`
- Problem: Single file handles the entire `pagx verify` command: layer validation, painter deduplication, geometry analysis, heuristic rule engine. No sub-module decomposition.
- Why it's a concern: Every new verification rule adds to an already large file; hard to unit-test individual rules in isolation.
- Improvement path: Extract each verification check into a separate `Verifier` class or function file under `src/cli/verify/`.

### SVGImporter.cpp — 2761 Lines

- Files: `src/pagx/svg/SVGImporter.cpp`
- Problem: Handles the complete SVG-to-PAGX translation: element parsing, geometry import, gradient import, text layout, group merging, unknown-node preservation. Monolithic single-file implementation.
- Why it's a concern: SVG spec coverage is incomplete; adding new SVG features requires navigating 2700+ lines.
- Improvement path: Split by SVG element category (shapes, text, gradients, groups) into sub-files.

### PAGXImporter.cpp — 2202 Lines

- Files: `src/pagx/PAGXImporter.cpp`
- Problem: Parses every PAGX XML node type in a single file with deeply nested `if`/`switch` logic per attribute.
- Why it's a concern: Adding new PAGX node attributes or tag versions requires searching through 2200 lines; error-prone when tag versions accumulate.
- Improvement path: Generate per-type parse functions from a schema or split into per-node-category files.

### TagCode Enum — 94 Versioned Tags in Public API

- Files: `include/pag/file.h:51–152`
- Problem: The `TagCode` enum has 94 defined codes with version suffixes (`V2`, `V3`, `Extra`, `ExtraV2`). Reserved ranges (`34~44`) indicate past breakage. The public header exposes the full enum to all consumers and pulls in RTTR headers when `PAG_USE_RTTR` is defined.
- Why it's a concern: Every new format feature requires a new `V(N+1)` tag, and decoders must handle all historical versions forever. Version proliferation makes the tag table hard to audit.
- Improvement path: Document a tag retirement policy; consider a single extensible tag with a version field for new additions.

### TextLayout.cpp — 1585 Lines

- Files: `src/pagx/TextLayout.cpp`
- Problem: Handles HarfBuzz shaping, line breaking, SheenBidi BiDi resolution, and glyph metric computation all in one file.
- Why it's a concern: Complex bidirectional text + shaping logic is notoriously hard to test; changes risk regressions across scripts.
- Improvement path: Unit-test individual shaping cases; split BiDi logic into `BidiLayout.cpp`.

---

## Missing Abstractions

### No Linux Platform Implementation

- Problem: `CMakeLists.txt` references `src/platform/linux/` (line 471) for native Linux builds, but the directory does not exist. Linux is listed as a supported platform in documentation, but there is no `NativePlatform`, font loading, or GPU context code for it.
- Impact: Linux builds without `USE_NATIVE_PLATFORM` fall through to the Qt backend (`src/platform/qt/`), which only provides a GPU drawable stub without font loading or display link.
- Fix approach: Implement a `src/platform/linux/` with at minimum `NativePlatform.cpp` (fontconfig font loading) and `GPUDrawable.cpp` (EGL context).

### No Notification Mechanism for PAGImage Scale Factor Invalidation

- Problem: `ImageReplacement::getScaleFactor()` recomputes the content matrix on every call but has no mechanism to notify upper layers when the `PAGImage` scale mode or matrix changes, requiring callers to re-query.
- Files: `src/rendering/editing/ImageReplacement.cpp:52–57`
- Impact: Callers may cache a stale scale factor after a PAGImage replacement update.
- Fix approach: Implement a dirty/observer notification from `PAGImage` to `ImageReplacement` when scale mode or matrix changes.

### Platform Abstraction Has No Default Display Link on Most Platforms

- Problem: `Platform::createDisplayLink()` returns `nullptr` by default; only iOS/macOS/Android/OHOS implement it. The Qt and Win platforms do not, limiting animation-loop integration on desktop.
- Files: `src/platform/Platform.h:92`, `src/platform/qt/NativePlatform.cpp`, `src/platform/win/NativePlatform.cpp`
- Impact: `PAGAnimator` cannot drive itself automatically on Win/Qt — callers must implement their own timer loop.
- Fix approach: Provide a generic timer-based `DisplayLink` fallback in `src/platform/Platform.cpp`.

---

## Risk Areas

### Codec Error Handling: `throwException` Is Not a Real Exception

- Problem: `StreamContext::throwException()` (named misleadingly) merely appends to an `errorMessages` vector and returns a bool. Callers use the `PAGThrowError` macro to log and record errors, but decoding continues unless the caller explicitly checks `hasException()`. Corrupted data can cause reads past the end of a stream before the error is noticed.
- Files: `src/codec/utils/StreamContext.h`
- Impact: Malformed or fuzzer-generated PAG files may partially decode into undefined state before errors surface; the 116 fuzz corpus files (`resources/fuzz/`) suggest this is a known concern.
- Fix approach: Make `checkEndOfFile` abort reads immediately by returning early (it already exists in `DecodeStream`); audit all callers of `PAGThrowError` to ensure they propagate the error upward promptly.

### Fuzz Corpus Is Static (116 Files)

- Problem: `test/src/PAGFuzzTest.cpp` loads every file in `resources/fuzz/` and decodes them. The corpus is manually curated (116 files) with no continuous fuzzing infrastructure.
- Files: `test/src/PAGFuzzTest.cpp`, `resources/fuzz/`
- Impact: Coverage-guided fuzzing (libFuzzer/AFL) is not integrated; new codec paths added for future tags may introduce vulnerabilities that the static corpus won't discover.
- Fix approach: Add a `PAGFuzzTarget` build target for libFuzzer; seed with the existing corpus.

### OHOS Async Decode Condition Variable Deadlock Risk

- Problem: `OHOSVideoDecoder::onSendBytes()` and `onRenderFrame()` use `condition_variable::wait()` with a lambda capturing `this`. If the codec callback thread fails to push to the queue (codec error, shutdown race), `onSendBytes` will block indefinitely.
- Files: `src/platform/ohos/OHOSVideoDecoder.cpp:165–168`, `src/platform/ohos/OHOSVideoDecoder.cpp:208`
- Impact: Potential hang on OHOS when hardware codec enters an error state.
- Fix approach: Add a timeout or a cancellation flag to the `wait()` calls to allow graceful teardown.

### RTTR Macro Pollution in Public Header

- Problem: `include/pag/file.h` conditionally includes 9 RTTR headers and defines `RTTR_AUTO_REGISTER_CLASS` on all public types when `PAG_USE_RTTR` is defined. This couples the public API to a reflection library that most consumers do not need.
- Files: `include/pag/file.h:25–44`
- Impact: Increases compile time for all consumers; RTTR headers pull in `clang diagnostic` suppressions globally.
- Fix approach: Move RTTR registration to a separate `include/pag/file_rttr.h` that consumers opt in to explicitly.

### `reinterpret_cast` Across HarfBuzz C Callbacks

- Problem: `src/renderer/TextShaper.cpp` uses `reinterpret_cast` to store and retrieve typed pointers (`DataPointer<tgfx::Data>*`, `DataPointer<tgfx::Typeface>*`) in HarfBuzz `void*` user-data slots, then `delete`s them in destroy callbacks.
- Files: `src/renderer/TextShaper.cpp:41–49`
- Impact: Type confusion if a wrong destroy callback is called; manual ownership requires careful pairing of create/destroy across C API boundaries.
- Fix approach: Wrap user-data in a typed destructor struct; consider `std::unique_ptr` with a custom deleter where the C API allows.

---

## Known Issues (TODO/FIXME Analysis)

### [OHOS] Hardware Video Decode Disabled

- Location: `src/platform/ohos/OHOSVideoDecoder.cpp:262`
- Owner: kevingpqi
- Issue: Asynchronous hardware decoding on HarmonyOS causes video jitter. Hardware texture path is disabled with `if (false && ...)` until HarmonyOS platform fixes the issue.
- Risk: HarmonyOS users always run software decode; no automatic re-enablement when platform is fixed.

### [Rendering] PAGPlayer `renderingTime()` Metric Stale

- Location: `src/rendering/PAGPlayer.cpp:395`
- Owner: domrjchen
- Issue: Performance monitoring panel of PAGViewer has not been updated to display new timing properties added to `RenderCache`.
- Risk: Developers using `renderingTime()` may miss new breakdown metrics; documentation mismatch.

### [Editing] PAGImage Scale Factor Notification Missing

- Location: `src/rendering/editing/ImageReplacement.cpp:53`
- Owner: domrjchen
- Issue: No notification mechanism exists to invalidate/reset `scaleFactor` when `PAGImage` scale mode or matrix changes.
- Risk: Callers may receive stale scale factors after image replacement updates.

### [Rendering] BulgeFilter SwiftShader Anomaly

- Location: `src/rendering/filters/BulgeFilter.cpp:130`
- Issue: Using `mix()` to eliminate branch statements in the BulgeFilter GLSL shader causes visual artifacts on SwiftShader. Workaround is an explicit `if (distance <= 1.0)` branch. Root cause unresolved.
- Risk: If SwiftShader is used for CI screenshot testing, BulgeFilter output may differ from GPU results.

---

## Platform Gaps

### Linux

- Missing: `src/platform/linux/` directory referenced in `CMakeLists.txt:471` does not exist.
- Consequence: Linux native platform builds (`USE_NATIVE_PLATFORM`) will fail at cmake configuration. Linux builds fall back to Qt platform with no font loading.
- Coverage: No Linux-specific tests exist.

### Qt / Desktop Windows

- Partial: `src/platform/qt/` has only 4 files: `GPUDrawable.{cpp,h}`, `NativePlatform.{cpp,h}`. There is no font loading, no display link, and no hardware video decoder.
- Consequence: Qt builds require the host app to manually set fallback fonts and drive the animation loop; hardware video will never decode.

### Web (Emscripten)

- Partial: Hardware decode on Web (`src/platform/web/HardwareDecoder.cpp`) is a stub that delegates entirely to JavaScript via `PAGWasmBindings.cpp`. The WASM binding file is 676 lines of hand-maintained JS–C++ glue with no automated API sync.
- Risk: Adding new C++ API methods requires manually extending `PAGWasmBindings.cpp`; easy to miss.

### OpenHarmony (OHOS) Hardware Decode

- Partial: Hardware texture path (`OH_NativeBuffer`) is disabled (see Known Issues). Only software YUV path is active.
- Consequence: Video playback on HarmonyOS devices runs software decode regardless of hardware availability.

---

## Test Coverage Gaps

### PAGDecoder / PAGAnimator / PAGImageView

- What's not tested: The newer high-level APIs (`PAGDecoder`, `PAGAnimator`, `PAGImageView`) have only 13 test references combined across all test files. No screenshot baseline tests for these APIs.
- Files: `src/rendering/PAGDecoder.cpp`, `src/rendering/PAGAnimator.cpp`, `test/src/PAGPlayerTest.cpp`
- Risk: Regressions in async frame delivery or animation timing may go undetected.
- Priority: Medium

### BitmapSequence and VideoSequence HitTest

- What's not tested: `test/src/PAGCompositionTest.cpp:530–532` has explicit TODO markers noting that `BitmapSequenceContent` and `VideoSequenceContent` HitTest cases are missing.
- Files: `test/src/PAGCompositionTest.cpp:530`
- Risk: Pixel-level hit testing on sequence layers may return wrong results silently.
- Priority: Medium

### Filter Edge Cases (SwiftShader, MotionBlur, BulgeFilter)

- What's not tested: `BulgeFilter` has a known SwiftShader rendering anomaly with no regression test. `MotionBlurFilter` has only 5 test references. No filter tests specifically target SwiftShader output.
- Files: `src/rendering/filters/BulgeFilter.cpp`, `src/rendering/filters/MotionBlurFilter.cpp`
- Risk: Filter visual regressions on CPU-only rendering paths (used in CI via SwiftShader) may produce incorrect baselines.
- Priority: Low

### Codec Decode Error Paths

- What's not tested: The `StreamContext::throwException()` error accumulation and early-exit behavior has no dedicated unit tests. Only fuzz corpus replay exercises error paths indirectly.
- Files: `src/codec/utils/StreamContext.h`, `src/codec/utils/DecodeStream.h`
- Risk: New codec tags introduced without testing decode-error propagation may silently produce corrupt object trees.
- Priority: High

---

*Concerns audit: 2025-01-27*
