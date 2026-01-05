#!/usr/bin/env node
process.chdir(__dirname);
const fs = require("fs");

process.argv.push("-s");
process.argv.push("../../");
process.argv.push("-o");
process.argv.push("../src");
process.argv.push("-p");
process.argv.push("web");
process.argv.push("pag");
require("../../build_pag");

const getArgValue = (argName, defaultValue = "wasm") => process.argv.includes(argName) ? process.argv[process.argv.indexOf(argName) + 1] : defaultValue;

const arch=getArgValue("-a");
const libPath = (arch==="wasm-mt" ? "../lib-mt" : "../lib");

if (!fs.existsSync(`${libPath}`)) {
    fs.mkdirSync(`${libPath}`, {recursive: true});
}

fs.copyFileSync(`../src/${arch}/libpag.wasm`, `${libPath}/libpag.wasm`);
