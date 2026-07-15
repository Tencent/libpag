[English](./README.md) | [中文](./README.zh_CN.md)

# PAGX Viewer for WeChat Mini Program

A WeChat Mini Program runtime for rendering PAGX files. The C++ rendering core
is compiled to WebAssembly via Emscripten, and the TypeScript bindings are
bundled into a single ESM module that can be consumed by a mini program.

## Directory Layout

```
pagx/wechat/
├── CMakeLists.txt       # CMake entry for building the wasm module
├── src/                 # C++ sources bound to JS (PAGXView, binding.cpp)
├── ts/                  # TypeScript sources (pagx.ts, pagx-view.ts, gesture-manager.ts, ...)
├── script/              # Build scripts (cmake / rollup / emsdk setup / file copy)
├── wasm/                # Raw CMake output (pagx-viewer.wasm, pagx-viewer.js)
├── lib/                 # Final bundled JS + compressed wasm (.br)
├── wx_demo/             # Sample WeChat Mini Program consuming the viewer
├── GETTING_STARTED.md   # Quick-start guide and full API reference for consumers
├── CHANGELOG.md         # Release history
└── package.json         # Build config + npm publish config
```

## Prerequisites

- **Node.js** 16 or later, with `npm`
- **CMake** 3.13+ and **Ninja** (used by the CMake wrapper script)
- **Brotli** CLI on `PATH` (for compressing the final wasm)
  - macOS: `brew install brotli`
  - Debian/Ubuntu: `sudo apt-get install brotli`
- A C/C++ toolchain available on the host (for any native tooling invoked
  during the CMake configure step)

You do **not** need to install Emscripten manually. The build script clones
and activates `emsdk 3.1.20` automatically into
`third_party/emsdk` the first time it runs.

Before building, make sure the libpag third-party dependencies have been
synced from the repository root:

```bash
cd /path/to/libpag
./sync_deps.sh
```

## One-Shot Build

From this directory (`pagx/wechat/`):

```bash
npm install
npm run build
```

This single command runs the full pipeline:

1. `node script/cmake.wx.js -a wasm` — configures and builds
   `pagx-viewer.wasm` / `pagx-viewer.js` via Emscripten + CMake. Output is
   placed under `wasm/` and then copied to `lib/`.
2. `brotli -f ./lib/pagx-viewer.wasm` — produces `lib/pagx-viewer.wasm.br`.
3. `rollup -c ./script/rollup.wx.js` — bundles the TypeScript sources
   (`ts/pagx.ts`, `ts/gesture-manager.ts`) into `lib/pagx-viewer.js` and
   `wx_demo/utils/gesture-manager.js`.
4. `node script/copy-files.js` — copies `lib/pagx-viewer.js` and
   `lib/pagx-viewer.wasm.br` into `wx_demo/utils/` so the demo mini program
   picks up the fresh build.
5. `tsc -p ./tsconfig.type.json` — emits `.d.ts` type definitions.

## Build Individual Steps

Only rebuild the wasm module:

```bash
node script/cmake.wx.js -a wasm
```

Only rebuild the JS bundle (no wasm):

```bash
npm run build:js
```

Clean wasm build cache (forces a full rebuild next time):

```bash
npm run clean
```

## Output Artifacts

After a successful build, the distributable files are:

| File | Purpose |
|------|---------|
| `lib/pagx-viewer.js` | Bundled ESM glue + TypeScript bindings (core renderer) |
| `lib/pagx-viewer.wasm` | Uncompressed WebAssembly module |
| `lib/pagx-viewer.wasm.br` | Brotli-compressed wasm shipped to the mini program |
| `lib/pagx-check.js` | Standalone render-risk evaluator (optional, see below) |
| `wx_demo/utils/pagx-viewer.js` | Demo copy, kept in sync by `copy-files.js` |
| `wx_demo/utils/pagx-viewer.wasm.br` | Demo copy, kept in sync by `copy-files.js` |

### `pagx-check.js` — Render Risk Evaluator

`pagx-check.js` is an **optional** standalone module that evaluates whether a
PAGX file is likely to render smoothly on the current device. It is **not**
required for rendering and does not depend on the WASM module or WebGL context.

Hosts that want pre-render stutter detection can import it separately:

```javascript
const { CheckPagx } = require('./pagx-check');
const result = await CheckPagx(pagxFileBytes);
// result.score >= threshold means smooth rendering is expected
```

If your application does not need the risk check, simply omit `pagx-check.js`
from your deployment — it will not affect rendering functionality.

## Running the Demo

`wx_demo/` is a minimal WeChat Mini Program that consumes the built viewer.
Open the directory in WeChat Developer Tools (微信开发者工具) and click
**Run**. Any time you rebuild with `npm run build`, the demo's
`utils/` folder is refreshed automatically.

## Troubleshooting

- **`emsdk` clone/update fails** — the script requires network access to
  `github.com/emscripten-core/emsdk`. If you are behind a firewall, clone
  `emsdk` manually to `third_party/emsdk` and let the script activate
  version 3.1.20.
- **`brotli: command not found`** — install the Brotli CLI (see
  [Prerequisites](#prerequisites)).
- **Wasm rebuild is skipped** — delete `.pagx-viewer.wasm.md5` and the
  build directory under `wasm/` (or run `npm run clean`) to force a rebuild.
- **`tgfx` not found** — make sure `./sync_deps.sh` has been run at the
  libpag repository root so that `third_party/tgfx` exists. You can also
  override its location via `-DTGFX_DIR=/path/to/tgfx` when invoking CMake
  directly.
