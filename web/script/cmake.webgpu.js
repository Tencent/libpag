#!/usr/bin/env node
process.chdir(__dirname);
const fs = require("fs");

// Mirrors cmake.demo.js but builds the WebGPU variant and preserves the emscripten glue under
// src/<arch>-webgpu/ so that rollup can select it per backend via update.pag.import.js.
process.argv.push("-s");
process.argv.push("../../");
process.argv.push("-o");
process.argv.push("../src");
process.argv.push("-p");
process.argv.push("web");
process.argv.push("-DPAG_USE_WEBGPU=ON");
process.argv.push("pag");
require("./setup.emsdk");
require("../../build_pag");

const getArgValue = (argName, defaultValue = "wasm") => process.argv.includes(argName) ? process.argv[process.argv.indexOf(argName) + 1] : defaultValue;

const arch = getArgValue("-a");
const webgpuArch = `${arch}-webgpu`;
const libPath = (arch === "wasm-mt" ? "../lib-webgpu-mt" : "../lib-webgpu");

fs.mkdirSync(`../src/${webgpuArch}`, {recursive: true});
fs.copyFileSync(`../src/${arch}/libpag.js`, `../src/${webgpuArch}/libpag.js`);
fs.copyFileSync(`../src/${arch}/libpag.wasm`, `../src/${webgpuArch}/libpag.wasm`);

if (!fs.existsSync(`${libPath}`)) {
    fs.mkdirSync(`${libPath}`, {recursive: true});
}

fs.copyFileSync(`../src/${webgpuArch}/libpag.wasm`, `${libPath}/libpag.wasm`);
