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
| `PAGXSVGTest.cpp` | the `resources/apitest/SVG` corpus is 492MB; not preloaded |
| `PAGXCliTest.cpp` | CLI sources are not compiled into the web test |
| `*.mm` (Metal/Cocoa) | platform tests never compile under Emscripten |

Race sections inside `PAGTextLayerTest`/`PAGImageLayerTest` are skipped through
`__EMSCRIPTEN__` guards; `PAGFrameCompareTest` runs its corpus serially.

## Known limitations

- The web test build enables RTTI (`PAG_WEB_ENABLE_RTTI`) for the whole library: the legacy
  `PAGXHTMLImporterTest` suite uses `dynamic_cast`, which requires typeinfo on objects created
  by library code. This only affects the test binary; the product web build stays no-RTTI.
- Video-sequence decoding is unavailable (no wasm ffavc), so PAG files containing
  video sequences render those layers empty.
- `resources/apitest/SVG` is never preloaded; the SVG import code is still compiled in.
- The WebGPU build enables Asyncify so the synchronous `readPixels()` used by
  `Baseline::Compare` can await buffer mapping.
