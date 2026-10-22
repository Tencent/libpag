const fs = require('fs');
const path = require('path');
const Utils = require("../../third_party/vendor_tools/lib/Utils");

const versionResult = Utils.execSafe("emcc --version");
if (versionResult.includes("Emscripten")) {
    // This file is loaded via require() by the cmake scripts, so exit early with return instead
    // of process.exit() to avoid terminating the parent build process.
    process.stdout.write("emcc is already available in the environment, skipping emsdk installation.\n");
    return;
}

const emsdkPath = path.resolve(__dirname, '../../third_party/emsdk');
if (!fs.existsSync(emsdkPath)) {
    Utils.exec(`git clone https://github.com/emscripten-core/emsdk.git ${Utils.escapeSpace(emsdkPath)}`);
} else {
    let pullResult = Utils.execSafe(`git -C ${Utils.escapeSpace(emsdkPath)} pull`);
    process.stdout.write(pullResult);
}
const emscriptenPath = path.resolve(emsdkPath, 'upstream/emscripten');
process.env.PATH = process.platform === 'win32'
    ? `${emsdkPath};${emscriptenPath};${process.env.PATH}`
    : `${emsdkPath}:${emscriptenPath}:${process.env.PATH}`;

// Pin the emscripten version instead of using "latest": the WebGPU backend requires
// emsdk >= 4.0.15 and < 4.0.18 (enforced at configure time in the root CMakeLists.txt).
Utils.exec("emsdk install 4.0.15", emsdkPath);
Utils.exec("emsdk activate 4.0.15", emsdkPath);

const emsdkEnv = process.platform === 'win32' ? "emsdk_env.bat" : "source emsdk_env.sh";
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
