# libpag Web Test

Runs the native `test/src` Google Test suites inside the browser against the **WebGL** or
**WebGPU** backend, following tgfx's `web/test` pattern: the test binary is compiled to
WebAssembly with Emscripten and driven by Puppeteer; screenshot outputs and the baseline
MD5 cache are exported from the virtual filesystem through CDP downloads.

## Prerequisites

- Node.js and the emsdk pinned in `web/emsdk-versions.txt`:

  ```bash
  node web/script/setup.emsdk.js
  source web/emsdk/emsdk_env.sh
  cd web/test && npm install
  ```

- The tgfx JS bindings (built once; the wasm side reads browser-only capabilities through
  `Module.tgfx` / `Module.ScalerContext`, injected by these bindings at page load):

  ```bash
  cd third_party/tgfx/web && npm install && npm run build:tgfx
  ```

  The `copy:*` scripts pick up `tgfx.esm.js` from there automatically.

## Usage

```bash
# From the project root: seed the baseline cache from main (run once, then after
# every accepted baseline change):
./update_baseline.sh USE_WEBGL           # or USE_WEBGPU
./update_baseline.sh USE_WEBGL --skip-images   # CI mode: md5 cache only

# Run the tests against the current branch:
./autotest.sh USE_WEBGL                  # or USE_WEBGPU
# or directly from web/test:
npm run test:webgl                       # build + run
npm run autotest:webgl                   # run only (reuses build-webgl)
```

## How it works

1. `emcmake cmake` builds `pag-test` (an Emscripten executable, entry point
   `src/WebTestRunner.cpp` exporting `RunAllTests`/`RunTest`).
2. `run_test.js` starts http-server, launches headless Chrome through Puppeteer
   (with `--use-webgpu-adapter=swiftshader` on GPU-less Linux), and opens `index.html`.
3. `index.html` instantiates the module (WebGL: default GL context on
   `#pag-test-canvas`; WebGPU: `preinitializedWebGPUDevice`), calls
   `ccall('RunAllTests')`, then packs `baseline-out/`, `test/out/`, and the md5 cache
   as zips that the harness extracts back into the repository.
4. The baseline comparison follows the native semantics: the repository
   `test/baseline/version.json` is compared against the per-backend snapshot in
   `test/baseline/.cache/{webgl,webgpu}/version.json`; equal versions compare MD5s
   against `.cache/{backend}/md5.json`, differing versions skip comparison.

Without a seeded cache every screenshot comparison silently passes (a CMake WARNING
is emitted at configure time). Always run `update_baseline.sh` first.

## Excluded on the web (phase 1)

| File | Reason |
|---|---|
| `MultiThreadCase.cpp`, `PAGANRRegressionTest.cpp`, `PAGDiskCacheTest.cpp` | require pthreads (single-threaded build) / MEMFS disk-cache semantics |
| `PAGVideoSequenceTest.cpp`, `VideoSequenceFallbackTest.cpp` | require the ffavc software decoder, which has no wasm build |
| `AsyncDecodeTest.cpp` | asserts sequence/image cache counts driven by the native 500ms prefetch window; the web build prefetches the whole timeline (`RenderCache::prepareLayers`), so the counts permanently differ |
| `PAGXSVGTest.cpp` | the `resources/apitest/SVG` corpus is 492MB; not preloaded |
| `PAGXCliTest.cpp` | CLI sources are not compiled into the web test |
| `PAGXHTMLImporterTest.cpp` | uses `dynamic_cast`; RTTI cannot be enabled because tgfx's embind typeinfo is no-rtti. Covered natively |
| `*.mm` (Metal/Cocoa) | platform tests never compile under Emscripten |

Race sections inside `PAGTextLayerTest`/`PAGImageLayerTest` are skipped through
`__EMSCRIPTEN__` guards; `PAGFrameCompareTest` runs its corpus serially.

## Known limitations

- RTTI stays off for the whole web build: tgfx's emscripten bindings rely on synthetic no-rtti
  typeinfo, so enabling RTTI anywhere in the same embind registry breaks all JS-to-C++ calls
  (BindingError: unknown type). This is why the dynamic_cast-based `PAGXHTMLImporterTest`
  cannot run on the web.
- Video-sequence decoding is unavailable (no wasm ffavc), so PAG files containing
  video sequences render those layers empty.
- `resources/apitest/SVG` is never preloaded; the SVG import code is still compiled in.
- The WebGPU build enables Asyncify so the synchronous `readPixels()` used by
  `Baseline::Compare` can await buffer mapping.
