const fs = require('fs');
const path = require('path');
const Utils = require("../../third_party/vendor_tools/lib/Utils");

// The version policy is shared with the configure-time check in the root CMakeLists.txt.
const readVersionPolicy = () => {
    const policyFile = path.resolve(__dirname, '../emsdk-versions.txt');
    const policy = {};
    for (const line of fs.readFileSync(policyFile, 'utf8').split('\n')) {
        const match = line.match(/^([A-Z_]+)=(.*)$/);
        if (match) {
            policy[match[1]] = match[2].trim();
        }
    }
    return policy;
};

const versionPolicy = readVersionPolicy();

const parseVersion = (text) => {
    const match = text.match(/(\d+)\.(\d+)\.(\d+)/);
    return match ? [Number(match[1]), Number(match[2]), Number(match[3])] : null;
};

const compareVersions = (a, b) => {
    for (let i = 0; i < 3; i++) {
        if (a[i] !== b[i]) {
            return a[i] - b[i];
        }
    }
    return 0;
};

const minVersion = parseVersion(versionPolicy.MIN);
const maxExclusiveVersion = parseVersion(versionPolicy.MAX_EXCLUSIVE);
const envVersion = parseVersion(Utils.execSafe("emcc --version"));
if (envVersion) {
    if (compareVersions(envVersion, minVersion) >= 0 && compareVersions(envVersion, maxExclusiveVersion) < 0) {
        // This file is loaded via require() by the cmake scripts, so exit early with return
        // instead of process.exit() to avoid terminating the parent build process.
        process.stdout.write(`emcc ${envVersion.join('.')} is already available and supported, skipping emsdk installation.\n`);
        return;
    }
    // An unsupported emcc in PATH must not short-circuit the installation of the pinned
    // version, otherwise the configure-time check fails later with a less actionable error.
    process.stdout.write(`emcc ${envVersion.join('.')} is outside the supported window [${versionPolicy.MIN}, ${versionPolicy.MAX_EXCLUSIVE}), installing the pinned ${versionPolicy.PIN} instead.\n`);
}

const emsdkPath = path.resolve(__dirname, '../emsdk');
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
// the window declared in web/emsdk-versions.txt (enforced at configure time in the root
// CMakeLists.txt).
Utils.exec(`emsdk install ${versionPolicy.PIN}`, emsdkPath);
Utils.exec(`emsdk activate ${versionPolicy.PIN}`, emsdkPath);

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
