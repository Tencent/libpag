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

// Point the native binary's html-snapshot bridge at the bundled snapshot
// tool so `pagx import` of HTML works without a libpag checkout (HTML import
// always renders through snapshot.js). The launcher lazily installs the
// headless browser on first use. A user-supplied PAGX_HTML_SNAPSHOT_BIN always wins.
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

// Intercept the preview sub-command and delegate to pagx-preview (a Node.js-based
// HTTP server + MCP service). The preview module lives under preview/ at the package
// root so it can be bundled and published alongside the native binary.
if (process.argv[2] === 'preview') {
  const previewEntry = path.join(__dirname, '..', 'preview', 'src', 'cli.js');
  if (!fs.existsSync(previewEntry)) {
    console.error('pagx: preview command not available. Preview module not found.');
    process.exit(1);
  }
  const { spawn } = require('child_process');
  const child = spawn('node', [previewEntry, ...process.argv.slice(3)], { stdio: 'inherit' });
  child.on('exit', (code) => process.exit(code != null ? code : 1));
  return;
}

// Inject the preview sub-command into the native binary's --help output. The native binary is
// unaware of preview (it lives in the Node.js wrapper), so we run its help, then splice a preview
// line under the "Commands:" header. Any future native command changes flow through untouched;
// only the preview line is maintained here.
if ((process.argv[2] === '--help' || process.argv[2] === '-h') && process.argv.length === 3) {
  const { spawnSync } = require('child_process');
  const result = spawnSync(binPath, ['--help'], { encoding: 'utf8' });
  const lines = result.stdout.split('\n');
  const output = lines
    .map((line) => {
      if (line.trim() === 'Commands:') {
        return line + '\n  preview        Preview a PAGX file in the browser with live reload';
      }
      return line;
    })
    .join('\n');
  process.stdout.write(output);
  if (result.stderr) process.stderr.write(result.stderr);
  process.exit(result.status || 0);
}

try {
  execFileSync(binPath, process.argv.slice(2), { stdio: "inherit" });
} catch (e) {
  process.exit(e.status != null ? e.status : 1);
}
