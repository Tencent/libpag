#!/usr/bin/env node
process.chdir(__dirname);
const fs = require("fs");

// Set BACKEND=webgpu (see cmake.webgpu.js) to build the WebGPU variant: it appends
// -DPAG_USE_WEBGPU=ON and stages the emscripten glue under src/wasm-webgpu[-mt]/ so that
// rollup can select it per backend via update.pag.import.js. The WebGPU build uses a
// dedicated intermediate output directory so src/wasm[-mt]/ always keeps the WebGL artifacts
// and a JS-only rebuild (build:js:st, dev:st) can never bundle mismatched WebGPU glue.
const isWebGPU = process.env.BACKEND === "webgpu";
const tmpOutputDir = "../src-webgpu-tmp";

process.argv.push("-s");
process.argv.push("../../");
process.argv.push("-o");
process.argv.push(isWebGPU ? tmpOutputDir : "../src");
process.argv.push("-p");
process.argv.push("web");
if (isWebGPU) {
    process.argv.push("-DPAG_USE_WEBGPU=ON");
}
process.argv.push("pag");
require("./setup.emsdk");
require("../../build_pag");

const getArgValue = (argName, defaultValue = "wasm") => process.argv.includes(argName) ? process.argv[process.argv.indexOf(argName) + 1] : defaultValue;

const arch = getArgValue("-a");
const { libDir, wasmDir } = require("./targets").resolveTarget(arch, process.env.BACKEND);

const assertFileExists = (filePath) => {
    if (!fs.existsSync(filePath)) {
        console.error(`Expected build artifact is missing: ${filePath}`);
        process.exit(1);
    }
};

const copyWasmToLib = (srcWasmPath, libDir) => {
    assertFileExists(srcWasmPath);
    fs.mkdirSync(libDir, {recursive: true});
    fs.copyFileSync(srcWasmPath, `${libDir}/libpag.wasm`);
};

if (isWebGPU) {
    // The staging directory must be cleaned up even when a copy fails, otherwise the next build
    // picks up stale artifacts from src-webgpu-tmp.
    try {
        const stagedJs = `${tmpOutputDir}/${arch}/libpag.js`;
        const stagedWasm = `${tmpOutputDir}/${arch}/libpag.wasm`;
        assertFileExists(stagedJs);
        assertFileExists(stagedWasm);
        fs.mkdirSync(`../src/${wasmDir}`, {recursive: true});
        fs.copyFileSync(stagedJs, `../src/${wasmDir}/libpag.js`);
        fs.copyFileSync(stagedWasm, `../src/${wasmDir}/libpag.wasm`);
        copyWasmToLib(`../src/${wasmDir}/libpag.wasm`, `../${libDir}`);
    } finally {
        fs.rmSync(tmpOutputDir, {recursive: true, force: true});
    }
} else {
    copyWasmToLib(`../src/${wasmDir}/libpag.wasm`, `../${libDir}`);
}
