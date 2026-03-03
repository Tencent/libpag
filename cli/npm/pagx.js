#!/usr/bin/env node
const { execFileSync } = require("child_process");
const path = require("path");

const PLATFORMS = {
  darwin: "pagx-darwin",
  linux: "pagx-linux",
  win32: "pagx-win32.exe",
};

const bin = PLATFORMS[process.platform];
if (!bin) {
  console.error(
    "pagx: unsupported platform '" +
      process.platform +
      "'. Supported: " +
      Object.keys(PLATFORMS).join(", ")
  );
  process.exit(1);
}

const binPath = path.join(__dirname, bin);
try {
  execFileSync(binPath, process.argv.slice(2), { stdio: "inherit" });
} catch (e) {
  process.exit(e.status != null ? e.status : 1);
}
