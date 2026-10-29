#!/usr/bin/env node
// Thin wrapper around cmake.demo.js that builds the WebGPU variant. The BACKEND env var tells
// cmake.demo.js to append -DPAG_USE_WEBGPU=ON and stage the glue under src/wasm-webgpu[-mt]/.
process.env.BACKEND = "webgpu";
require("./cmake.demo");
