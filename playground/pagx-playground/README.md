# PAGX Playground

English | [简体中文](./README.zh_CN.md)

An interactive web demo for viewing and testing PAGX files in the browser.

## Introduction

PAGX Playground is a web-based interactive demo that uses the [pagx-viewer](./pagx-viewer) SDK to render
and preview PAGX files. It provides a visual interface for loading PAGX files, viewing samples, and
browsing the PAGX specification documentation.

## Features

- Load and preview PAGX files directly in the browser
- Sample browser with pre-loaded PAGX files
- Interactive rendering controls
- PAGX specification documentation viewer
- Skill documentation integration

## Directory Structure

```
pagx-playground/
├── static/          # Static assets (HTML, CSS, images)
├── pages/           # HTML templates for documentation pages
├── scripts/         # Build and publish scripts
├── src/             # TypeScript source code
└── wasm-mt/         # WebAssembly build output (generated during build)
```

> Note: Font files are served directly from `libpag/resources/font` during development and
> copied to the output directory during publish. They are not tracked by git.

## Build

### Prerequisites

First, ensure you have installed all the tools and dependencies listed in the
[README.md](../../README.md#Development) in the project root, including Emscripten.

> Important: The playground does not compile any WebAssembly itself. It consumes the compiled
> WASM artifacts from [pagx-viewer](../pagx-viewer). You must build pagx-viewer first, and the
> multi-threaded (MT) / single-threaded (ST) choice is made there, not here. Read the
> [pagx-viewer README](../pagx-viewer/README.md) before building the playground.

### Build pagx-viewer First

The MT and ST WASM builds are produced in pagx-viewer, not in the playground. Pick the flavor
you need there:

```bash
# In playground/pagx-viewer
npm install
npm run build:release       # multi-threaded (default, requires SharedArrayBuffer)
npm run build:release:st    # single-threaded (no SharedArrayBuffer required)
```

See the [pagx-viewer README](../pagx-viewer/README.md) for the full build matrix.

### Install Dependencies

```bash
cd playground/pagx-playground
npm install
```

### Build

Build the playground against the viewer artifacts. Use the plain commands to bundle against the
multi-threaded viewer build, or the `:st` variants for the single-threaded one:

```bash
# Bundle against the multi-threaded viewer artifacts (default)
npm run build
npm run build:release

# Bundle against the single-threaded viewer artifacts
npm run build:st
npm run build:release:st

# Clean build artifacts
npm run clean
```

The `:st` variants only tell the playground bundler to pick up the single-threaded viewer
artifacts; the actual MT/ST WASM compilation happens in pagx-viewer. Multi-threaded builds rely
on `SharedArrayBuffer`, which requires the `Cross-Origin-Opener-Policy` and
`Cross-Origin-Embedder-Policy` headers. The development server detects the build type
automatically and only sends these headers for multi-threaded builds, so no manual configuration
is needed when switching between the two.

## Development

### Run Development Server

Start a local development server:

```bash
npm run server
```

The server automatically opens the playground in your default browser and applies the correct
COOP/COEP headers based on the detected build type (see [Build](#build)).

### LAN Sharing

By default the server binds to `localhost` only. To share it with other devices on the same
network (e.g. testing on a phone), enable LAN binding with the `PAGX_LAN` environment variable:

```bash
PAGX_LAN=1 npm run server
```

The console then prints a `LAN access` URL. Only enable this on trusted networks, as it exposes
the local file server to any device on the LAN.

## Publish

Publish the playground to the output directory:

```bash
npm run publish
```

This will build the release version and copy all necessary files to the output directory,
including:
- Static assets (HTML, CSS, images)
- WASM files
- Font files
- Sample PAGX files
- Specification documentation
- Skill documentation

### Publish Options

```bash
# Publish to a custom output directory
npm run publish -- -o /path/to/output

# Skip build step (use pre-built files)
npm run publish -- --skip-build
```

## Browser Requirements

- Chrome 69+
- Firefox 79+
- Safari 15+
- Edge 79+

Requires WebGL2 and WebAssembly support.

## License

Apache-2.0
