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

### Install Dependencies

```bash
cd playground/pagx-playground
npm install
```

### Build

```bash
# Build the TypeScript source
npm run build

# Build release version (minified)
npm run build:release

# Clean build artifacts
npm run clean
```

## Development

### Run Development Server

Start a local development server with hot reload:

```bash
npm run server
```

The server will automatically open http://localhost:8081 in your browser.

> Note: The development server requires SharedArrayBuffer for WASM threading.
> If you encounter issues, ensure your browser supports the required COOP/COEP headers.

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

## Use with PAGX Viewer

The playground consumes the compiled artifacts from [pagx-viewer](../pagx-viewer).
Make sure to build the pagx-viewer first before building the playground:

```bash
# In playground/pagx-viewer
npm run build:release

# Then in playground/pagx-playground
npm run build:release
```

## Browser Requirements

- Chrome 69+
- Firefox 79+
- Safari 15+
- Edge 79+

Requires WebGL2 and WebAssembly support.

## License

Apache-2.0
