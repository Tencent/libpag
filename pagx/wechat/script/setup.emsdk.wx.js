/*
* WeChat Mini Program compilation depends on emsdk version 3.1.20.
* This script is used to download and activate the specified version of emsdk.
*
* WARNING: Do NOT upgrade the emsdk version without thorough testing on real devices.
* WeChat's WXWebAssembly runtime has limited compatibility with Emscripten's generated
* JS glue code. Versions above 3.1.20 produce API calls (e.g. newer TextDecoder usage,
* BigInt integration, or ES module patterns) that WXWebAssembly does not support,
* causing silent failures or crashes at runtime. This version is a hard constraint
* determined by the WeChat Mini Program platform, not a development preference.
*/

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import Utils from "../../../third_party/vendor_tools/lib/Utils.js";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const emsdkPath = path.resolve(__dirname, '../../../third_party/emsdk');
if (!fs.existsSync(emsdkPath)) {
    try {
        Utils.exec(`git clone https://github.com/emscripten-core/emsdk.git ${emsdkPath}`);
    } catch (error) {
        console.error('clone emsdk failed:', error);
        process.exit(1);
    }
} else {
    Utils.exec(`git -C ${emsdkPath} pull`);
}
const emscriptenPath = path.resolve(emsdkPath, 'upstream/emscripten');
process.env.PATH = process.platform === 'win32'
    ? `${emsdkPath};${emscriptenPath};${process.env.PATH}`
    : `${emsdkPath}:${emscriptenPath}:${process.env.PATH}`;
Utils.exec("emsdk install 3.1.20", emsdkPath);
Utils.exec("emsdk activate 3.1.20", emsdkPath);
const emsdkEnv = process.platform === 'win32' ? "emsdk_env.bat" : ". emsdk_env.sh";
let result = Utils.execSafe(emsdkEnv, emsdkPath);
let lines = result.split("\n");
for (let line of lines) {
    let values = line.split("=");
    if (values.length > 1) {
        process.stdout.write(line);
        let key = values[0].trim();
        process.env[key] = values[1].trim();
    }
}
