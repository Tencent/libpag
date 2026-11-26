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

> The playground does not compile any WebAssembly itself — the WASM comes from
> [pagx-viewer](../pagx-viewer) and the player ESM bundle from [pagx-player](../pagx-player).
> You no longer need to build those by hand: the `build` commands below compile both upstream
> packages automatically before bundling the playground. The multi-threaded (MT) /
> single-threaded (ST) choice is made via the command variant (`:st` = single-threaded).

### Install Dependencies

```bash
cd playground/pagx-playground
npm install
```

### Build and Run

Each `build` command compiles the upstream `pagx-viewer` (MT or ST) and `pagx-player` first,
then bundles the playground. Pick the variant you need, then start the dev server:

```bash
# Multi-threaded viewer (default, requires SharedArrayBuffer / COOP+COEP)
npm run build
npm run build:release

# Single-threaded viewer (no SharedArrayBuffer required)
npm run build:st
npm run build:release:st

# Then serve
npm run server
```

So the common single-threaded dev loop is just:

```bash
npm run build:release:st && npm run server
```

`clean` removes build artifacts:

```bash
npm run clean
```

If you already have the upstream artifacts built (or a clean checkout that ships them) and want
to skip recompiling, use the copy-only `bundle` commands instead — they stage the existing
artifacts and bundle without rebuilding pagx-viewer / pagx-player:

```bash
npm run bundle        # multi-threaded, copy-only
npm run bundle:st     # single-threaded, copy-only
```

The `:st` variants select the single-threaded viewer flavor; the actual MT/ST WASM compilation
happens in pagx-viewer (triggered automatically by the `build` commands). Multi-threaded builds
rely on `SharedArrayBuffer`, which requires the `Cross-Origin-Opener-Policy` and
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
