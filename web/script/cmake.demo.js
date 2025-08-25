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
const filePath=getArgValue("-a");

if (!fs.existsSync(`../lib/${filePath}`)) {
    fs.mkdirSync(`../lib/${filePath}`, {recursive: true});
}
if (!fs.existsSync("../wechat/lib")) {
    fs.mkdirSync("../wechat/lib", {recursive: true});
}

fs.copyFileSync(`../src/${filePath}/libpag.wasm`, `../lib/${filePath}/libpag.wasm`);