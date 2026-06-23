#!/usr/bin/env node
const { execFileSync } = require("child_process");
const fs = require("fs");
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

// Resolve the native binary. Prefer an architecture-specific directory
// (`<platform>-<arch>`, e.g. `linux-arm64`) so a single tarball can ship
// distinct x64/arm64 builds, then fall back to the legacy `<platform>` layout
// (used when the directory holds a universal binary, e.g. macOS).
const candidateDirs = [process.platform + "-" + process.arch, process.platform];

let binPath = null;
for (const dir of candidateDirs) {
  const candidate = path.join(__dirname, dir, binName);
  if (fs.existsSync(candidate)) {
    binPath = candidate;
    break;
  }
}

if (!binPath) {
  console.error(
    "pagx: no native binary found for " +
      process.platform +
      "-" +
      process.arch +
      ". Looked in: " +
      candidateDirs
        .map((dir) => path.join("bin", dir, binName))
        .join(", ") +
      ". The package may have been published without a binary for your " +
      "platform/architecture."
  );
  process.exit(1);
}

// Point the native binary's `--html-snapshot` bridge at the bundled snapshot
// tool so `pagx import --html-snapshot` works without a libpag checkout. The
// launcher lazily installs the headless browser on first use. A user-supplied
// PAGX_HTML_SNAPSHOT_BIN always wins.
//
// The published package keeps this wrapper in `bin/` while `html-snapshot/`
// lives at the package root, so check both layouts (sibling and parent).
if (!process.env.PAGX_HTML_SNAPSHOT_BIN) {
  const candidates = [
    path.join(__dirname, "html-snapshot", "launch.js"),
    path.join(__dirname, "..", "html-snapshot", "launch.js"),
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      process.env.PAGX_HTML_SNAPSHOT_BIN = candidate;
      break;
    }
  }
}

try {
  execFileSync(binPath, process.argv.slice(2), { stdio: "inherit" });
} catch (e) {
  process.exit(e.status != null ? e.status : 1);
}
