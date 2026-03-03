#!/usr/bin/env node
const { execFileSync } = require("child_process");
const path = require("path");

const SUPPORTED = ["darwin", "linux", "win32"];

if (!SUPPORTED.includes(process.platform)) {
  console.error(
    "pagx: unsupported platform '" +
      process.platform +
      "'. Supported: " +
      SUPPORTED.join(", ")
  );
  process.exit(1);
}

const binName = process.platform === "win32" ? "pagx.exe" : "pagx";
const binPath = path.join(__dirname, process.platform, binName);
try {
  execFileSync(binPath, process.argv.slice(2), { stdio: "inherit" });
} catch (e) {
  process.exit(e.status != null ? e.status : 1);
}
