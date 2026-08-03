# PAGX Preview

Local live-reload preview server for rendering and debugging [PAGX](https://pag.io/pagx/latest/) animation files. Supports automatic refresh on file changes and MCP integration for AI coding assistants.

It is exposed as the `pagx preview` subcommand of [`@libpag/pagx`](https://www.npmjs.com/package/@libpag/pagx). End users invoke it as `pagx preview` through the `@libpag/pagx` CLI; this package can also run standalone as `pagx-preview`, a form used for local development and maintainer testing (see [Development](#development)).

## Features

- One-command startup; the browser opens the preview automatically
- Automatic file-change detection and live re-rendering (pushed over SSE)
- Persistent background daemon; multiple files share one server
- Drag and drop a `.pagx` file into the browser window to preview it
- Runs as an MCP server for AI coding assistants (CodeBuddy / Claude Desktop / VS Code Copilot)
- Automatic font download (NotoSansSC + NotoColorEmoji)

## Requirements

- Node.js ≥ 16.7
- A built `pagx-viewer` (multi-threaded or single-threaded variant)

## Quick Start

```bash
# Install the main CLI globally (includes the preview subcommand)
npm install -g @libpag/pagx

# Preview a file
pagx preview /path/to/animation.pagx

# Preview another file (reuses the running server)
pagx preview /path/to/other.pagx

# Stop the background server
pagx preview stop
```

## CLI Options

| Option | Description |
|--------|-------------|
| `--port <n>` | Bind to a specific port (default: system-assigned) |
| `--host <addr>` | Bind host (default: 127.0.0.1) |
| `--fonts <dir>` | Override the fonts directory |
| `--no-open` | Do not open the browser automatically |
| `--foreground` | Run in the foreground (do not detach) |
| `--mcp` | Run as an MCP stdio server (see the MCP section below) |
| `--log` | Print the server log |
| `stop` | Stop the background server |

## MCP Server Mode

`pagx preview` can run as an MCP (Model Context Protocol) server, letting AI coding assistants preview `.pagx` files directly in conversation.

### How it works

When started with `--mcp`, the process communicates over stdin/stdout using the MCP protocol and automatically starts a local HTTP server for WASM rendering. The user does not need to start any service manually — the MCP client manages the process lifecycle.

### Tools

| Tool | Description |
|------|-------------|
| `preview_pagx` | Loads the file and returns a session URL to open in an IDE webview panel or a browser. |
| `reload_file` | Force a reload of the file from disk (file changes auto-reload; this is a manual trigger). |
| `get_document` | Return document information (dimensions, duration). |

All previewing goes through `preview_pagx`, which returns a session URL to open in an IDE webview panel or a browser. Inline MCP Apps widget rendering is unreliable across desktop hosts (see [Known compatibility issues](#known-compatibility-issues)), so the inline widget tool is not exposed for now; the widget resource (`ui://pagx-preview/main`) still exists in the code but is not advertised and can be re-enabled once host support matures.

### Platform deployment

(Note: the MCP configuration differs between development/testing and the published form — see Development > Build and test a tarball locally, step 4.)

#### CodeBuddy IDE

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

macOS: `~/Library/Application Support/Claude/claude_desktop_config.json`
Windows: `%APPDATA%\Claude\claude_desktop_config.json`

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

Create `.vscode/mcp.json` in your project:

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

Note: Copilot uses HTTP transport; start the server manually first (`pagx preview --port <port> <file>`).

### Known compatibility issues

The inline widget has been verified in the official [ext-apps basic-host](https://github.com/modelcontextprotocol/ext-apps/tree/main/examples/basic-host) reference host, but inline MCP Apps support varies across desktop hosts:

| Host | Inline widget status |
|------|------|
| Claude Desktop | The widget iframe may not be mounted by the host (known host-side bug) |
| CodeBuddy IDE | MCP Apps inline widgets are not yet supported |
| VS Code Copilot | Opens in Simple Browser (editor tab), not an inline widget |

Because inline rendering does not work reliably on the major hosts, the inline widget tool is not exposed for now. Previewing goes through `preview_pagx` (open in an IDE webview panel / browser). In all cases the session URL (`http://127.0.0.1:<port>/session/<id>/`) provides a fully functional preview with live reload.

## Development

> The commands in this section target the **standalone** `pagx-preview` command (run via `npm link` or `node src/cli.js`), which is how this package is developed and tested in isolation. End users reach the same tool as `pagx preview` through the `@libpag/pagx` CLI.

### First-time setup

```bash
cd ../pagx-preview
npm install
npm run build                # builds pagx-viewer (single-threaded) + pagx-player and stages artifacts
```

`npm run build` chains all steps: it first builds the upstream `pagx-viewer` (single-threaded `st` variant) and `pagx-player`, then copies the artifacts into `static/`.

Build commands:

| Command | Viewer variant | Use case |
| --- | --- | --- |
| `npm run build` | single-threaded (st) | Default. Works everywhere — MCP widget **and** browser preview. |
| `npm run build:release` | single-threaded (st), release | Same as above, optimized build. |
| `npm run build:mt` | multi-threaded (mt) | Browser preview only; faster rendering. |
| `npm run build:release:mt` | multi-threaded (mt), release | Same, optimized build. |

The single-threaded (`st`) variant is the default: the MCP widget runs in a sandbox iframe without cross-origin isolation, where the `SharedArrayBuffer` that the multi-threaded build depends on is unavailable. The `mt` variant renders faster but only works in the plain browser preview (`pagx-preview file.pagx`, where the server sends the required COOP/COEP headers); inside an MCP host the widget falls back to opening the browser URL.

If the upstream artifacts are already built (or the cloned repo ships them), run `npm run prebuild` to only copy them without rebuilding. `prebuild` automatically:
- Detects the viewer build variant (MT/ST) and copies wasm + glue to `static/viewer/`
- Copies the pagx-player ESM bundle to `static/player/`
- Copies the ext-apps SDK bundle to `static/ext/`
- Bundles the MCP widget with esbuild to `static/mcp-widget.bundle.js`

### Run from source

```bash
node src/cli.js /path/to/file.pagx              # background
node src/cli.js --foreground /path/to/file.pagx  # foreground
node src/cli.js --mcp                            # MCP mode
```

### Test with a global link

```bash
npm link
pagx-preview /path/to/file.pagx
npm unlink -g @libpag/pagx-preview
```

### How end users get the package

End users do not build or pack anything. Once published, the compiled artifacts (`static/viewer/*.wasm`, `static/player/*.js`, etc.) ship with the npm tarball, so a plain install pulls a ready-to-run command — exactly like the `@libpag/pagx` CLI:

```bash
npm install -g @libpag/pagx-preview   # downloads prebuilt artifacts; no compilation needed
```

Building (`npm run build`) is the maintainer step that regenerates the artifacts before a release. The local tarball test below is only for simulating a real install before the package is published to the npm registry.

### Build and test a tarball locally

Reproduces the full experience a user gets from `npm install`, using a local tarball.

```bash
# 1. Build artifacts (release) and pack the tarball
cd ../pagx-preview
npm run build:release
npm pack --dry-run           # inspect the file list that will be published
npm pack                     # writes libpag-pagx-preview-<version>.tgz

# 2. Install the tarball globally to simulate a published state
npm unlink -g @libpag/pagx-preview   # remove any existing dev link first
npm install -g ./libpag-pagx-preview-<version>.tgz

# 3. Verify the CLI
pagx-preview --help
pagx-preview /path/to/file.pagx      # opens a browser preview

# 4. Verify the MCP server
#    Note: this section targets the **standalone** pagx-preview command, whose MCP json differs
#    from the end-user (published) form:
#      - standalone dev/test:     { "command": "pagx-preview", "args": ["--mcp"] }
#      - end user (@libpag/pagx): { "command": "pagx", "args": ["preview", "--mcp"] } (see "Platform deployment" above)
#    Point your MCP client (CodeBuddy / Claude Desktop) at the installed standalone command:
#      { "mcpServers": { "pagx-preview": { "command": "pagx-preview", "args": ["--mcp"] } } }
#    Then ask the assistant to preview a .pagx file. Or smoke-test stdio startup directly:
pagx-preview --mcp < /dev/null       # should start, print nothing to stdout, and wait

# 5. Cleanup
npm uninstall -g @libpag/pagx-preview
rm libpag-pagx-preview-<version>.tgz
rm -rf ~/.pagx                       # clears cached fonts, log, and process lock (fresh-user state)
```

### Publish to npm

> Distribution: this tool ships with `@libpag/pagx` as the `pagx preview` subcommand, so end users do not need to install this package separately.

## Directory Structure

```
pagx-preview/
├── src/
│   ├── cli.js          # CLI entry, argument parsing
│   ├── daemon.js       # background process management, stdio MCP startup
│   ├── server/
│   │   ├── index.js    # Express HTTP server (SSE, static assets, MCP mount)
│   │   ├── session.js  # file-watching session (chokidar)
│   │   ├── fonts.js    # font resolution
│   │   ├── font-cache.js # lazy font download
│   │   └── lock.js     # process lock
│   └── mcp/
│       ├── server.js   # MCP server construction (stdio + HTTP)
│       └── tools.js    # MCP tools + resource definitions
├── static/
│   ├── index.js / index.html / index.css  # browser client
│   ├── viewer/         # pagx-viewer WASM + glue (generated by prebuild)
│   ├── player/         # pagx-player ESM bundle (generated by prebuild)
│   ├── ext/            # ext-apps SDK bundle (generated by prebuild)
│   ├── icons/          # playback control icons
│   ├── mcp-widget.js   # MCP Apps widget source
│   ├── mcp-widget.html # MCP Apps widget HTML shell
│   └── mcp-widget.bundle.js  # bundled widget (generated by prebuild)
├── scripts/
│   ├── prebuild.js     # artifact copy + bundle build
│   └── check-artifacts.js # pre-publish check
└── package.json
```

## Fonts

On first run, NotoSansSC-Regular.otf + NotoColorEmoji.ttf are downloaded automatically to `~/.pagx/fonts/`.
Priority: `--fonts` argument > `PAGX_FONTS_DIR` environment variable > `~/.pagx/fonts/` > `resources/font/` in the libpag repo.

Set `PAGX_FONTS_NO_AUTO_DOWNLOAD=1` to disable the automatic download.

## License

Apache-2.0
