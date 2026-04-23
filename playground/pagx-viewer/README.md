# PAGX Viewer

A lightweight WebAssembly-based SDK for rendering and viewing PAGX files in the browser.

## Features

- 🚀 WebAssembly powered for high performance
- 📦 Multiple module formats (ESM, CJS, UMD)
- 🎨 Custom font support with emoji rendering
- 🔍 Zoom and pan controls
- 📁 External file loading support

## Installation

## Quick Start

```typescript
import { PAGXInit } from './lib/pagx-viewer.esm';

// Initialize the WASM module
const PAGX = await PAGXInit({
  locateFile: (file) => `/lib/${file}`,
});

// Create a view from a canvas element
const view = PAGX.PAGXView.init('#pagx-canvas');

// Register fonts (optional but recommended for text rendering)
view.registerFonts(fontData, emojiFontData);

// Step 1: Parse PAGX data
view.parsePAGX(pagxData);

// Step 2: Get external file paths (e.g., images)
const paths = view.getExternalFilePaths();
const count = paths.size();

// Step 3: Download and inject external files
for (let i = 0; i < count; i++) {
  const filePath = paths.get(i);
  const response = await fetch(baseUrl + filePath);
  const fileData = new Uint8Array(await response.arrayBuffer());
  view.loadFileData(filePath, fileData);
}
paths.delete(); // Free memory

// Step 4: Build layers after all external files are loaded
view.buildLayers();

// Update canvas size
view.updateSize();

// Start render loop
view.start();
```

## API Reference

### PAGXInit(options?)

Initializes the PAGX Viewer WebAssembly module.

```typescript
function PAGXInit(options?: ModuleOption): Promise<PAGXModule>;
```

#### ModuleOption

| Property | Type | Description |
|----------|------|-------------|
| `locateFile` | `(file: string) => string` | Custom function to locate the WASM file |
| `wasmBinary` | `ArrayBuffer` | Pre-fetched WASM binary data |
| `mainScriptUrlOrBlob` | `string \| Blob` | URL or Blob for multi-threaded builds |

#### Example with pre-fetched WASM

```typescript
// Fetch WASM with progress tracking
const wasmResponse = await fetch('/wasm/pagx-viewer.wasm');
const wasmBuffer = await wasmResponse.arrayBuffer();

const PAGX = await PAGXInit({
  locateFile: (file) => `/wasm/${file}`,
  wasmBinary: wasmBuffer,
});
```

### PAGXView

The main view class for rendering PAGX content.

#### Static Methods

##### `PAGXView.init(canvas)`

Creates a PAGXView from a canvas element.

```typescript
static init(canvas: string | HTMLCanvasElement): PAGXView | null;
```

- `canvas`: CSS selector (e.g., `'#my-canvas'`) or HTMLCanvasElement
- Returns: PAGXView instance or `null` if creation failed

#### Instance Properties

| Property | Type | Description |
|----------|------|-------------|
| `isRunning` | `boolean` | Whether the render loop is running |
| `isDestroyed` | `boolean` | Whether the view has been destroyed |

#### Instance Methods

##### `registerFonts(fontData, emojiFontData)`

Registers fallback fonts for text rendering.

```typescript
registerFonts(fontData: Uint8Array, emojiFontData: Uint8Array): void;
```

- `fontData`: Primary font data (e.g., NotoSansSC)
- `emojiFontData`: Emoji font data (e.g., NotoColorEmoji)

##### `loadPAGX(data)`

Loads a PAGX document (for files without external resources).

```typescript
loadPAGX(data: Uint8Array): void;
```

##### `parsePAGX(data)`

Parses PAGX data without building layers. Use this for files with external resources.

```typescript
parsePAGX(data: Uint8Array): void;
```

##### `getExternalFilePaths()`

Returns the list of external file paths referenced by the PAGX document.

```typescript
getExternalFilePaths(): StringVector;
```

> **Important**: Must call `delete()` on the returned vector when done to free memory.

##### `loadFileData(path, data)`

Loads external file data referenced by the PAGX document.

```typescript
loadFileData(path: string, data: Uint8Array): boolean;
```

##### `buildLayers()`

Builds the layer tree after parsing and loading external files.

```typescript
buildLayers(): void;
```

##### `updateSize()`

Updates the view size to match the canvas dimensions. Call this after canvas resize.

```typescript
updateSize(): void;
```

##### `updateZoomScaleAndOffset(zoom, offsetX, offsetY)`

Updates the zoom scale and content offset.

```typescript
updateZoomScaleAndOffset(zoom: number, offsetX: number, offsetY: number): void;
```

- `zoom`: Zoom scale (1.0 = 100%)
- `offsetX`: Horizontal offset in pixels
- `offsetY`: Vertical offset in pixels

##### `contentWidth()` / `contentHeight()`

Returns the original dimensions of the PAGX content.

```typescript
contentWidth(): number;
contentHeight(): number;
```

##### `draw()`

Draws the current frame immediately.

```typescript
draw(): void;
```

##### `start()` / `stop()`

Controls the render loop.

```typescript
start(): void;
stop(): void;
```

##### `destroy()`

Destroys the view and releases resources.

```typescript
destroy(): void;
```

## Usage Examples

### Basic Usage

```typescript
import { PAGXInit } from 'pagx-viewer';

async function main() {
  // Initialize module
  const PAGX = await PAGXInit({
    locateFile: (file) => `/wasm/${file}`,
  });

  // Create view
  const view = PAGX.PAGXView.init('#pagx-canvas');
  if (!view) {
    console.error('Failed to create PAGXView');
    return;
  }

  // Load PAGX file
  const response = await fetch('/sample.pagx');
  const data = new Uint8Array(await response.arrayBuffer());
  view.loadPAGX(data);

  // Setup canvas size
  const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
  canvas.width = canvas.clientWidth * window.devicePixelRatio;
  canvas.height = canvas.clientHeight * window.devicePixelRatio;
  view.updateSize();

  // Start rendering
  view.start();
}
```

### Loading PAGX with External Files

```typescript
import { PAGXInit } from 'pagx-viewer';

async function loadPAGXWithExternalFiles(pagxUrl: string, baseUrl: string) {
  const PAGX = await PAGXInit({
    locateFile: (file) => `/wasm/${file}`,
  });

  const view = PAGX.PAGXView.init('#pagx-canvas');

  // Fetch and parse PAGX data
  const response = await fetch(pagxUrl);
  const pagxData = new Uint8Array(await response.arrayBuffer());
  view.parsePAGX(pagxData);

  // Load external files
  const paths = view.getExternalFilePaths();
  const count = paths.size();

  for (let i = 0; i < count; i++) {
    const filePath = paths.get(i);
    const fileUrl = baseUrl + filePath;
    
    try {
      const fileResponse = await fetch(fileUrl);
      const fileData = new Uint8Array(await fileResponse.arrayBuffer());
      view.loadFileData(filePath, fileData);
    } catch (error) {
      console.warn(`Failed to load external file: ${filePath}`);
    }
  }

  // Free the paths vector
  paths.delete();

  // Build layers after all external files are loaded
  view.buildLayers();
  view.updateSize();
  view.start();
}
```

### Implementing Zoom and Pan

```typescript
const MIN_ZOOM = 0.001;
const MAX_ZOOM = 1000.0;

let zoom = 1.0;
let offsetX = 0;
let offsetY = 0;

// Handle mouse wheel for zoom
canvas.addEventListener('wheel', (e) => {
  e.preventDefault();
  
  if (e.ctrlKey || e.metaKey) {
    // Zoom
    const rect = canvas.getBoundingClientRect();
    const pixelX = (e.clientX - rect.left) * window.devicePixelRatio;
    const pixelY = (e.clientY - rect.top) * window.devicePixelRatio;
    
    const scale = Math.exp(-e.deltaY / 800);
    const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, zoom * scale));
    
    // Zoom around cursor position
    offsetX = (offsetX - pixelX) * (newZoom / zoom) + pixelX;
    offsetY = (offsetY - pixelY) * (newZoom / zoom) + pixelY;
    zoom = newZoom;
  } else {
    // Pan
    offsetX -= e.deltaX * window.devicePixelRatio;
    offsetY -= e.deltaY * window.devicePixelRatio;
  }
  
  view.updateZoomScaleAndOffset(zoom, offsetX, offsetY);
}, { passive: false });

// Reset view
function resetView() {
  zoom = 1.0;
  offsetX = 0;
  offsetY = 0;
  view.updateZoomScaleAndOffset(zoom, offsetX, offsetY);
}
```

### Handling Visibility Changes

```typescript
// Pause rendering when tab is hidden
document.addEventListener('visibilitychange', () => {
  if (document.hidden) {
    view.stop();
  } else {
    view.start();
  }
});

// Stop rendering before page unload
window.addEventListener('beforeunload', () => {
  view.stop();
});
```

### Canvas Resize Handling

```typescript
function updateCanvasSize() {
  const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
  const container = canvas.parentElement;
  const rect = container.getBoundingClientRect();
  const scaleFactor = window.devicePixelRatio;
  
  canvas.width = rect.width * scaleFactor;
  canvas.height = rect.height * scaleFactor;
  canvas.style.width = rect.width + 'px';
  canvas.style.height = rect.height + 'px';
  
  view.updateSize();
}

// Debounced resize handler
let resizeTimeout: number;
window.addEventListener('resize', () => {
  clearTimeout(resizeTimeout);
  resizeTimeout = window.setTimeout(updateCanvasSize, 300);
});
```

## Browser Requirements

- Chrome 69+
- Firefox 79+
- Safari 15+
- Edge 79+

Requires WebGL2 and WebAssembly support.

## Module Formats

The library is available in multiple formats:

| Format | File | Usage |
|--------|------|-------|
| ESM | `pagx-viewer.esm.js` | `import { PAGXInit } from 'pagx-viewer'` |
| CJS | `pagx-viewer.cjs.js` | `const { PAGXInit } = require('pagx-viewer')` |
| UMD | `pagx-viewer.umd.js` | Browser `<script>` tag |
| Minified | `pagx-viewer.min.js` | Production use |

## License

Apache-2.0
