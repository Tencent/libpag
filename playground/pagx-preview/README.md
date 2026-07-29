# PAGX Preview

Local live-reload preview server for [PAGX](https://pag.io/pagx/latest/) files. Powers the
`pagx preview` subcommand of [`@libpag/pagx`](https://www.npmjs.com/package/@libpag/pagx),
rendering a `.pagx` file in the browser and automatically refreshing on file changes.

End users invoke it as `pagx preview` through the `@libpag/pagx` CLI. This package can also run
standalone as `pagx-preview` — that form is used for local development and maintainer testing
(see [Development](#development)).

Companion tool to [`pagx-viewer`](../pagx-viewer). See [PAGX](https://pag.io/pagx/latest/) for
the format specification.

## Introduction

`pagx preview` starts a local HTTP server, watches the target `.pagx` file (and any external
resources it references) with `chokidar`, and pushes reload events to the browser over
Server-Sent Events. The rendering itself is delegated to `pagx-viewer` (WebAssembly). The first
invocation spawns a detached daemon so the shell returns immediately; subsequent invocations
reuse the running daemon and open additional tabs for different files.

## Requirements

- Node.js ≥ 16.7
- A built `pagx-viewer` (either multi-threaded or single-threaded variant)

## Quick Start (End Users)

```bash
# Install the main PAGX CLI (ships the preview subcommand)
npm install -g @libpag/pagx

# Open a PAGX file
pagx preview /path/to/animation.pagx

# Open another file (reuses the same background server, opens a new tab)
pagx preview /path/to/other.pagx

# Stop the background server
pagx preview stop

# Inspect the server log
pagx preview --log
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

`pagx preview` can run as an [MCP (Model Context Protocol)](https://modelcontextprotocol.io/)
server, allowing AI coding assistants to preview `.pagx` files directly in conversation.

### Tools

| Tool | Purpose |
| --- | --- |
| `preview_pagx` | **Default preview.** Loads the file and returns a session url to open in an IDE webview panel or a browser. Does not render an inline widget. |
| `preview_pagx_widget` | **Inline widget preview.** Renders the animation directly in the conversation (a small in-chat window). Use only when the user explicitly asks for an inline / small-window ("小窗") preview. |
| `reload_file` | Force a full reload of the pagx from disk (the preview auto-reloads on file change; this is a manual trigger). |
| `get_document` | Return a summary (dimensions, duration) of the loaded document. |

`preview_pagx` is the default because inline widget rendering is unreliable across desktop
hosts (see [Known compatibility issues](#known-compatibility-issues)). Only `preview_pagx_widget`
carries the MCP Apps UI resource (`ui://pagx-preview/main`) that makes a supporting host mount
the inline iframe; `preview_pagx` deliberately omits it so it never triggers a broken widget.

### How it works

When started with `--mcp`, the process communicates over stdio (JSON-RPC) and automatically
spawns a local HTTP server to load WASM, fonts, and pagx bytes. The user does not need to
manually start any services — the MCP client handles process lifecycle.

### Platform deployment

#### CodeBuddy IDE / VS Code (CodeBuddy plugin)

Add to `~/.codebuddy/mcp.json`:

```json
{
  "mcpServers": {
    "pagx-preview": {
      "command": "pagx",
      "args": ["preview", "--mcp"]
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
      "command": "pagx",
      "args": ["preview", "--mcp"]
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

Note: Copilot uses HTTP transport. Start the preview server first (`pagx preview --port <port>
<file>`) then configure the URL.

### Known compatibility issues

The inline widget (`preview_pagx_widget`) renders correctly in the official
[ext-apps basic-host](https://github.com/modelcontextprotocol/ext-apps/tree/main/examples/basic-host)
reference implementation, but inline MCP Apps support varies across desktop hosts:

- **Claude Desktop**: The widget iframe may not be mounted by the host (known issue tracked at
  [claude-ai-mcp#165](https://github.com/anthropics/claude-ai-mcp/issues/165)).
- **CodeBuddy IDE**: MCP Apps inline widget rendering is not yet supported.
- **VS Code Copilot**: Opens the preview in Simple Browser (editor tab), not as an inline widget.

Because of this, `preview_pagx` (open in an IDE webview panel / browser) is the default and
`preview_pagx_widget` is opt-in. In all cases the session url
(`http://127.0.0.1:<port>/session/<id>/`) provides a fully functional preview with live reload.

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

> The commands in this section drive the **standalone** `pagx-preview` binary (via `npm link` or
> `node src/cli.js`), which is how this package is developed and tested in isolation. End users
> instead reach the same tool as `pagx preview` through the `@libpag/pagx` CLI.

### First-time setup

```bash
cd ../pagx-preview
npm install
npm run build                # builds pagx-viewer (st) + pagx-player, then stages artifacts
```

`npm run build` chains everything: it builds the upstream `pagx-viewer` (single-threaded `st`
variant) and `pagx-player` packages, then copies their artifacts into `static/`.

Build commands:

| Command | Viewer variant | Use case |
| --- | --- | --- |
| `npm run build` | single-threaded (st) | Default. Works everywhere — MCP widget **and** browser preview. |
| `npm run build:release` | single-threaded (st), release | Same as above, optimized build. |
| `npm run build:mt` | multi-threaded (mt) | Browser preview only; faster rendering. |
| `npm run build:release:mt` | multi-threaded (mt), release | Same, optimized build. |

The single-threaded (`st`) variant is the default because the MCP widget runs in a sandbox
iframe with no cross-origin isolation, so the multi-threaded build's `SharedArrayBuffer` is
unavailable there. The `mt` variant renders faster but only works in the plain browser preview
(`pagx-preview file.pagx`), where the server sends the required COOP/COEP headers; inside an MCP
host the widget then falls back to opening the browser URL.

If you already have the upstream artifacts built (or a clean checkout that ships them), run
`npm run prebuild` instead to only stage them without rebuilding. `prebuild` auto-detects
whichever variant exists in `../pagx-viewer/lib/` and writes `static/viewer/info.json` so the
server knows whether to attach COOP/COEP headers (only required by the multi-threaded build).

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

### How end users get the package

End users never build or pack anything. Once published, the compiled artifacts
(`static/viewer/*.wasm`, `static/player/*.js`, …) ship inside the npm tarball, so a plain
install pulls a ready-to-run command — exactly like the `@libpag/pagx` CLI:

```bash
npm install -g @libpag/pagx-preview   # downloads prebuilt artifacts; no compilation needed
```

Building (`npm run build`) is a maintainer step that regenerates those artifacts before a
release. The steps below reproduce a published install locally with a tarball, which is only
needed until the package is actually on the npm registry.

### Build and test a tarball locally

Reproduces exactly what an end user would get from `npm install`, using a local tarball instead
of the registry.

```bash
# 1. Build artifacts (release variant) and pack the tarball
cd ../pagx-preview
npm run build:release
npm pack --dry-run           # inspect the file list that will be published
npm pack                     # writes libpag-pagx-preview-<version>.tgz

# 2. Install the tarball globally as if it were published
npm unlink -g @libpag/pagx-preview   # remove any existing dev link first
npm install -g ./libpag-pagx-preview-<version>.tgz

# 3. Verify the CLI works
pagx-preview --help
pagx-preview /path/to/file.pagx      # opens a browser preview

# 4. Verify the MCP server works
#    Point your MCP client (CodeBuddy / Claude Desktop) at the installed binary:
#      { "mcpServers": { "pagx-preview": { "command": "pagx-preview", "args": ["--mcp"] } } }
#    Then ask the assistant to preview a .pagx file. Or smoke-test stdio startup directly:
pagx-preview --mcp < /dev/null       # should start, print nothing to stdout, and wait

# 5. Cleanup
npm uninstall -g @libpag/pagx-preview
rm libpag-pagx-preview-<version>.tgz
rm -rf ~/.pagx                       # clears cached fonts, log, and process lock (fresh-user state)
```

### Publish to npm

```bash
# 1. Bump the version in package.json (semver; use a prerelease id for alpha/beta,
#    e.g. 0.1.0-alpha.2).
npm publish --dry-run        # sanity-check the file list without pushing

# 2. Publish. For a prerelease, pin a dist-tag so it does NOT become the default
#    "latest" that a plain `npm install @libpag/pagx-preview` would pull.
npm login                    # if not already authenticated
npm publish --tag alpha      # `prepack` auto-runs prebuild(--release) + check-artifacts

# 3. Once a build is stable, promote that exact version to "latest".
npm dist-tag add @libpag/pagx-preview@<version> latest
```

`prepack` (declared in `package.json`) runs `scripts/prebuild.js --build --release` and
`scripts/check-artifacts.js` to guarantee the tarball ships with the viewer wasm, glue file,
and playback icons; the release fails loudly otherwise.

#### Verify a published release

After `npm publish` succeeds, confirm the registry actually serves what you expect:

```bash
# 1. Check the version and dist-tags landed on the registry.
npm view @libpag/pagx-preview dist-tags
npm view @libpag/pagx-preview@alpha version

# 2. Inspect the shipped file list without installing (no *.map / *.wasm.symbols expected).
npm pack @libpag/pagx-preview@alpha --dry-run

# 3. Install exactly what the registry serves. A prerelease needs the tag (or an explicit
#    version) — a bare `npm i -g @libpag/pagx-preview` would still resolve to "latest".
npm install -g @libpag/pagx-preview@alpha

# 4. Smoke-test the installed command.
pagx-preview --help
pagx-preview /path/to/file.pagx      # browser preview renders + live-reloads
pagx-preview --mcp < /dev/null       # MCP stdio server starts, prints nothing, and waits
```

#### Internal / pre-release trial (Tencent registry)

The canonical package is `@libpag/pagx-preview` on the public npm registry (matching its sibling
`@libpag/pagx`). For an internal-only trial *before* the public release, publish a throwaway
build to the Tencent registry instead. That registry only accepts the `@tencent` scope, so
temporarily override the package name for the publish command — **do not commit this change**;
the repository keeps the `@libpag` identity as the canonical/public one.

```bash
# 1. Temporarily rename the scope for this publish only.
npm pkg set name=@tencent/pagx-preview

# 2. Publish to the Tencent registry with the alpha tag.
npm publish --tag alpha --registry https://mirrors.tencent.com/npm/

# 3. Restore the canonical name so it never lands in git.
npm pkg set name=@libpag/pagx-preview
```

Colleagues install the trial build with:

```bash
npm install -g @tencent/pagx-preview@alpha --registry https://mirrors.tencent.com/npm/
```

Once the tool is proven, publish the canonical `@libpag/pagx-preview` to the public registry (see
above). Note that `@libpag/pagx` already dispatches `pagx preview ...` to this tool, so end users
reach it through the main CLI without installing this package directly.

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
  tool (validate, render, optimize, format, etc.). Exposes this tool as its `pagx preview`
  subcommand.

## License

Apache-2.0
