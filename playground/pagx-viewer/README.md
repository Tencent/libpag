# PAGX Viewer

English | [简体中文](./README.zh_CN.md)

A lightweight WebAssembly-based SDK for rendering and viewing PAGX files in the browser.

## Introduction

PAGX Viewer wraps the libpag PAGX rendering engine into a small, dependency-free JavaScript/TypeScript
SDK. It compiles the C++ rendering core to WebAssembly and exposes a minimal TypeScript API for
loading a PAGX file, registering fonts, handling zoom/pan, and driving the render loop from a
`<canvas>` element.

The SDK is composed of two files after build:

- `pagx-viewer.esm.js` (or `.cjs.js` / `.umd.js` / `.min.js`) — JavaScript glue code
- `pagx-viewer.wasm` — WebAssembly binary

## Quick Start

```typescript
import { PAGXInit } from 'pagx-viewer.esm'; // pagx-viewer.esm.js

// Initialize the WASM module.
const PAGX = await PAGXInit({
  locateFile: (file) => `/lib/${file}`,
});

// Create a view from a canvas element.
const view = PAGX.PAGXView.init('#pagx-canvas');

// Optional: set a custom background color (defaults to a checkerboard pattern).
// The color supports #RGB / #RGBA / #RRGGBB / #RRGGBBAA, and an optional alpha
// parameter (0.0 - 1.0) can override the transparency.
view.setBackgroundColor('#ffffff');

// Register fallback fonts for text rendering.
view.registerFonts(fontData, emojiFontData);

// Step 1: Parse PAGX data without building layers.
view.parsePAGX(pagxData);

// Step 2: Get external file paths (e.g., images) referenced by the document.
const paths = view.getExternalFilePaths();
const count = paths.size();

// Step 3: Download each external file and inject it back into the view.
for (let i = 0; i < count; i++) {
  const filePath = paths.get(i);
  const response = await fetch(baseUrl + filePath);
  const fileData = new Uint8Array(await response.arrayBuffer());
  view.loadFileData(filePath, fileData);
}
paths.delete(); // Always free the returned vector.

// Step 4: Build the layer tree after all external files are loaded.
view.buildLayers();

// Update canvas size to match the current canvas dimensions.
view.updateSize();

// Optional: update the zoom scale and content offset when the user zooms or pans.
// view.updateZoomScaleAndOffset(zoom, offsetX, offsetY);

// Start the render loop.
view.start();
```

For PAGX files without external resources, you can replace `parsePAGX` + `buildLayers` with a
single `view.loadPAGX(pagxData)` call. See [`src/ts/pagx-view.ts`](./src/ts/pagx-view.ts) for the
full API surface.

## Build

### Prerequisites

First, ensure you have installed all the tools and dependencies listed in the
[README.md](../../README.md#Development) in the project root, including Emscripten.

### Install Dependencies

```bash
# ./playground/pagx-viewer
npm install
```

### Build

The SDK ships with two top-level build commands. Both compile the C++ sources to WebAssembly via
Emscripten and bundle the TypeScript sources with Rollup; they differ only in optimization level
and debug information.

```bash
# ./playground/pagx-viewer

# Debug build: no minification, keeps source maps and DWARF debug info.
# Use this during development so you can step through C++ and TS in DevTools.
npm run build:debug

# Release build: minified JS, stripped WASM, optimized for production.
# Use this when integrating the SDK into a shipping product.
npm run build:release
```

Both commands generate the following artifacts under `lib/`:

| File | Format | Usage |
|------|--------|-------|
| `pagx-viewer.esm.js` | ESM | `import { PAGXInit } from 'pagx-viewer'` |
| `pagx-viewer.cjs.js` | CJS | `const { PAGXInit } = require('pagx-viewer')` |
| `pagx-viewer.umd.js` | UMD | Browser `<script>` tag |
| `pagx-viewer.min.js` | UMD (minified) | Production use |
| `pagx-viewer.wasm` | WebAssembly | Runtime dependency |

To debug the C++ side, install the
[C/C++ DevTools Support (DWARF)](https://chrome.google.com/webstore/detail/cc%20%20-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb)
Chrome extension, then open DevTools → Settings → Experiments and enable
"WebAssembly Debugging: Enable DWARF support". You can now set breakpoints in the C++ sources
directly in DevTools.

### Build Individual Targets

If you only need a subset of the pipeline (for example, recompiling WASM without rebuilding the JS
bundles), you can invoke the individual steps:

| Command | Description |
|---------|-------------|
| `npm run build:wasm` | Build only the WebAssembly binary (release) |
| `npm run build:wasm:debug` | Build only the WebAssembly binary (debug) |
| `npm run build:js` | Build only the JavaScript bundles (debug) |
| `npm run build:js:release` | Build only the JavaScript bundles (release) |
| `npm run build:types` | Emit TypeScript declaration files |
| `npm run clean` | Remove build artifacts and caches |

## Use with PAGX Playground

The companion [`pagx-playground`](../pagx-playground) project consumes the compiled artifacts
directly from `pagx-viewer/lib`. After running `npm run build:release` here, you can switch to the
playground directory and run its own build/publish scripts.

## Browser Requirements

- Chrome 69+
- Firefox 79+
- Safari 15+
- Edge 79+

Requires WebGL2 and WebAssembly support.

## License

Apache-2.0
