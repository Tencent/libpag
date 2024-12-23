#!/usr/bin/env node
process.chdir(__dirname);
const fs = require("fs");

process.argv.push("-s");
process.argv.push("../../");
process.argv.push("-o");
process.argv.push("../src");
process.argv.push("-p");
process.argv.push("web");
process.argv.push("-a");
process.argv.push("wasm");
process.argv.push("pag");
require("../../build_pag");

if (!fs.existsSync("../lib")) {
    fs.mkdirSync("../lib", {recursive: true});
}
if (!fs.existsSync("../wechat/lib")) {
    fs.mkdirSync("../wechat/lib", {recursive: true});
}
fs.copyFileSync("../src/wasm/libpag.wasm", "../lib/libpag.wasm")
fs.copyFileSync("../src/wasm/libpag.wasm", "../wechat/lib/libpag.wasm")

