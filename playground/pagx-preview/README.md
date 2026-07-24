# PAGX Preview

Local live-reload preview server for [PAGX](https://pag.io/pagx/latest/) files. Provides a
`pagx-preview` CLI that renders a `.pagx` file in the browser and automatically refreshes on
file changes.

Companion tool to [`@libpag/pagx`](https://www.npmjs.com/package/@libpag/pagx) and
[`pagx-viewer`](../pagx-viewer). See [PAGX](https://pag.io/pagx/latest/) for the format
specification.

## Introduction

`pagx-preview` starts a local HTTP server, watches the target `.pagx` file (and any external
resources it references) with `chokidar`, and pushes reload events to the browser over
Server-Sent Events. The rendering itself is delegated to `pagx-viewer` (WebAssembly). The first
invocation spawns a detached daemon so the shell returns immediately; subsequent invocations
reuse the running daemon and open additional tabs for different files.

## Requirements

- Node.js ≥ 16.7
- A built `pagx-viewer` (either multi-threaded or single-threaded variant)

## Quick Start (End Users)

```bash
# Install (from an npm tarball; publishing to the registry is planned)
npm install -g ./libpag-pagx-preview-<version>.tgz

# Open a PAGX file
pagx-preview /path/to/animation.pagx

# Open another file (reuses the same background server, opens a new tab)
pagx-preview /path/to/other.pagx

# Stop the background server
pagx-preview stop

# Inspect the server log
pagx-preview --log
```

On first run, fallback fonts (~18 MB) are lazily downloaded to `~/.pagx/fonts/`. Subsequent
runs use the cached copies.

### CLI options

| Option | Description |
|--------|-------------|
| `--port <n>` | Bind to a specific port (default: system-assigned) |
| `--host <addr>` | Bind host (default: `127.0.0.1`) |
| `--fonts <dir>` | Override the fonts directory |
| `--no-open` | Do not open the browser automatically |
| `--foreground` | Run the server in the foreground instead of detaching |
| `--log` | Print the server log and exit |
| `-h, --help` | Show help |

### Playback controls (in-browser)

- **Space** — Play / Pause
- **Previous / Next frame buttons** — Step one frame at a time
- **Progress slider** — Scrub the timeline
- **Loop toggle** — Sequence loop vs. play-once
- **Drop a `.pagx` file into the window** — Open a one-shot preview in a new tab (not watched)

### Font resolution

Fallback fonts (`NotoSansSC-Regular.otf` + `NotoColorEmoji.ttf`) are resolved in priority order:

1. `--fonts <dir>` CLI argument
2. `PAGX_FONTS_DIR` environment variable
3. `~/.pagx/fonts/` (populated by lazy download from `pag.qq.com/wx_pagx_demo/fonts/`)
4. `resources/font/` in an ancestor `libpag` checkout

Set `PAGX_FONTS_NO_AUTO_DOWNLOAD=1` to disable the lazy download step (for offline / CI hosts).

### Files created at runtime

| Path | Purpose |
|------|---------|
| `~/.pagx/preview.lock` | Currently-running daemon's pid/port (cleared on shutdown) |
| `~/.pagx/preview.log` | Daemon stdout/stderr; truncated on each spawn |
| `~/.pagx/fonts/` | Cached fallback fonts |

## MCP Server Mode

`pagx-preview` can run as an [MCP (Model Context Protocol)](https://modelcontextprotocol.io/)
server, allowing AI coding assistants to preview `.pagx` files directly in conversation. The
server exposes tools (`preview_pagx`, `reload_file`, `get_document`) and an MCP Apps UI
resource that renders the pagx animation inline as a widget.

### How it works

When started with `--mcp`, the process communicates over stdio (JSON-RPC) and automatically
spawns a local HTTP server for the widget to load WASM, fonts, and pagx bytes. The user does
not need to manually start any services — the MCP client handles process lifecycle.

### Platform deployment

#### CodeBuddy IDE / VS Code (CodeBuddy plugin)

Add to `~/.codebuddy/mcp.json`:

```json
{
  "mcpServers": {
    "pagx-preview": {
      "command": "pagx-preview",
      "args": ["--mcp"]
    }
  }
}
```

#### Claude Desktop

Add to `~/Library/Application Support/Claude/claude_desktop_config.json` (macOS) or
`%APPDATA%\Claude\claude_desktop_config.json` (Windows):

```json
{
  "mcpServers": {
    "pagx-preview": {
      "command": "pagx-preview",
      "args": ["--mcp"]
    }
  }
}
```

#### VS Code GitHub Copilot

Add to `.vscode/mcp.json` in your project:

```json
{
  "servers": {
    "pagx-preview": {
      "type": "http",
      "url": "http://127.0.0.1:<port>/mcp"
    }
  }
}
```

Note: Copilot uses HTTP transport. Start the preview server first (`pagx-preview --port <port>
<file>`) then configure the URL.

### Known compatibility issues

The inline MCP Apps widget renders correctly in the official
[ext-apps basic-host](https://github.com/modelcontextprotocol/ext-apps/tree/main/examples/basic-host)
reference implementation. However, due to varying MCP Apps support across hosts:

- **Claude Desktop**: The widget iframe may not be mounted by the host (known issue tracked at
  [claude-ai-mcp#165](https://github.com/anthropics/claude-ai-mcp/issues/165)). When the
  inline widget is blank, open the browser URL returned by `preview_pagx` instead.
- **CodeBuddy IDE**: MCP Apps inline widget rendering is not yet supported. Use the browser URL.
- **VS Code Copilot**: Opens the preview in Simple Browser (editor tab), not as an inline
  widget. Functionally equivalent.

In all cases, the browser URL (`http://127.0.0.1:<port>/session/<id>/`) provides a fully
functional preview with live reload.

## Architecture

- **`src/cli.js`** — Command entry, argument parsing, daemon / foreground dispatch, session
  reuse over HTTP.
- **`src/daemon.js`** — Detached child process handling, log capture, `stop` / `--log`
  commands.
- **`src/server/index.js`** — Express-based HTTP server with SSE, static asset routing, and
  the `/sessions` reuse endpoint.
- **`src/server/session.js`** — Per-file session: watches the entry PAGX and its external
  resources; emits `reload` events to subscribers.
- **`src/server/fonts.js`** / **`font-cache.js`** — Font resolution and lazy download.
- **`src/server/lock.js`** — Cross-invocation lock file at `~/.pagx/preview.lock`.
- **`static/`** — Browser client: vanilla ES modules that boot `pagx-viewer`, subscribe to
  SSE, and drive the playback bar.

## Development

### First-time setup

```bash
# 1. Build pagx-viewer artifacts (either variant works)
cd ../pagx-viewer
npm run build:debug:st       # single-threaded (recommended for IDE embedding)
# or:
# npm run build:debug        # multi-threaded (better rendering performance)

# 2. Install deps and stage viewer artifacts
cd ../pagx-preview
npm install
npm run prebuild             # copies pagx-viewer wasm / glue into static/viewer/
```

`prebuild` auto-detects whichever variant exists in `../pagx-viewer/lib/` and writes
`static/viewer/info.json` so the server knows whether to attach COOP/COEP headers (only
required by the multi-threaded build).

### Run from source

```bash
# Standard run (spawns a detached daemon)
node src/cli.js /path/to/file.pagx

# Foreground run (keeps the server attached to the terminal; Ctrl+C stops it)
node src/cli.js --foreground /path/to/file.pagx
```

### Test as a globally installed command

```bash
# Link the package so the `pagx-preview` binary points at your working tree
npm link
pagx-preview /path/to/file.pagx

# Unlink when done
npm unlink -g @libpag/pagx-preview
```

### Build and test a tarball locally

```bash
# Optional: use the release build of pagx-viewer for a realistic install
cd ../pagx-viewer
npm run build:release:st

# Build the tarball
cd ../pagx-preview
npm run prebuild
npm pack --dry-run           # inspect what will be included
npm pack                     # writes libpag-pagx-preview-<version>.tgz

# Install the tarball as if it were published
npm unlink -g @libpag/pagx-preview   # remove any existing link
npm install -g ./libpag-pagx-preview-<version>.tgz
pagx-preview --help
pagx-preview /path/to/file.pagx

# Cleanup
npm uninstall -g @libpag/pagx-preview
```

### Publish to npm

```bash
# Bump the version in package.json first (semver-compatible; prerelease tags allowed)
npm publish --dry-run        # sanity-check without pushing
npm publish                  # `prepack` auto-runs prebuild + check-artifacts
```

`prepack` (declared in `package.json`) runs `scripts/prebuild.js` and
`scripts/check-artifacts.js` to guarantee the tarball ships with the viewer wasm, glue file,
and playback icons; the release fails loudly otherwise.

### Force a font-download rehearsal

The server prefers a `libpag` checkout's `resources/font/` over the lazy-download cache when
both are available, so testing the download path requires shadowing the checkout:

```bash
pagx-preview stop
export PAGX_FONTS_DIR=/nonexistent      # blocks the checkout fallback
rm -rf ~/.pagx/fonts                    # clear the cache
unset PAGX_FONTS_DIR                    # let the download resolve to the cache
pagx-preview /path/to/file.pagx
pagx-preview --log                      # watch the download progress
```

## Server endpoints

The server exposes a small HTTP surface consumed by the browser client and, for reuse across
CLI invocations, by the CLI itself.

| Path | Purpose |
|------|---------|
| `GET /health` | Liveness probe used by the CLI lock check |
| `POST /sessions` | Create or reuse a session for a filesystem path |
| `GET /session/:id/` | Serve the client `index.html` |
| `GET /session/:id/pagx` | Serve the entry PAGX bytes |
| `GET /session/:id/info` | Session metadata (name, watched vs. one-shot) |
| `GET /session/:id/resources/*` | Serve external resources under the entry file's directory |
| `POST /session/:id/resources` | Browser reports the PAGX's external file list |
| `GET /session/:id/events` | Server-Sent Events stream (`reload`, `fonts-ready`, `focus`) |
| `POST /session/drop` | Upload dropped bytes as a one-shot ephemeral session |
| `GET /fonts/list` | Enumerate available fallback fonts |
| `GET /fonts/:name` | Serve a fallback font |
| `GET /static/*` | Client bundle (viewer wasm/glue, index.js, index.css) |

## Related packages

- [`pagx-viewer`](../pagx-viewer) — The WebAssembly rendering core loaded by the browser
  client.
- [`pagx-playground`](../pagx-playground) — The hosted online viewer at
  [pag.io/pagx](https://pag.io/pagx/). Reuses the same viewer with a richer editing UI.
- [`@libpag/pagx`](https://www.npmjs.com/package/@libpag/pagx) — The main PAGX command-line
  tool (validate, render, optimize, format, etc.). Not yet integrated with `pagx-preview`
  as a subcommand.

## License

Apache-2.0
