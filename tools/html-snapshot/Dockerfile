# html-snapshot HTTP service image.
#
# Wraps `server.js` (the long-running HTTP wrapper around the snapshot
# pipeline) into a self-contained container. The base layer ships:
#   - Node.js 22 + the html-snapshot Node sources (server.js, lib/)
#   - Headless Chromium from the Debian repos (works natively on both
#     linux/amd64 and linux/arm64; avoids puppeteer's amd64-only
#     Chrome-for-Testing bundle — see bench/Dockerfile for the long
#     rationale)
#   - CJK + emoji fonts so non-ASCII pages render correctly
#   - `tini` as PID 1, so SIGTERM/SIGINT reach Node and trigger the
#     graceful shutdown path baked into server.js
#
# The `pagx` binary is NOT bundled. Building it would drag in the full
# libpag C++ toolchain. The server boots fine without it — only
# `format=pagx`/`format=both` requests fail until you provide one of:
#   • bind-mount: `-v /host/path/to/pagx:/usr/local/bin/pagx:ro`
#   • derived image: `FROM html-snapshot-server` then `COPY pagx /usr/local/bin/`
# Either way the default $PAGX_BIN below (=/usr/local/bin/pagx) picks it up
# without extra flags.
#
# Build (from tools/html-snapshot/):
#   docker build -t html-snapshot-server .
#
# Run:
#   docker run --rm -p 8787:8787 --init --shm-size=1g html-snapshot-server
#   # then: curl http://127.0.0.1:8787/health
#
# Run with a host-built pagx binary mounted in:
#   docker run --rm -p 8787:8787 --init --shm-size=1g \
#     -v "$(pwd)/../../cmake-build-debug/pagx:/usr/local/bin/pagx:ro" \
#     html-snapshot-server

FROM node:22-bookworm-slim

# Headless Chromium + its runtime deps + fonts + tini.
# Mirrors the bench Dockerfile's package list; `tini` is added because
# server.js installs SIGTERM/SIGINT handlers and we need PID 1 to forward
# the signal (otherwise `docker stop` will hang until the 10s force-exit
# timer fires).
#
# `libegl1` + `libgles2` + `libfontconfig1` are the runtime libs the
# Linux `pagx` binary itself links against (see top-level CMakeLists.txt
# where the pagx-cli target pulls in EGL + GLESv2 + Fontconfig on Linux).
# Without them a bind-mounted pagx fails at startup with
# `error while loading shared libraries: libEGL.so: cannot open shared
# object file`. They're cheap (<2 MB combined) and harmless if pagx is
# never used, so we install them unconditionally to keep the
# "drop in a pagx binary and it works" UX promised above.
RUN apt-get update && apt-get install -y --no-install-recommends \
      chromium \
      ca-certificates \
      tini \
      fonts-liberation \
      fonts-noto-cjk \
      fonts-noto-color-emoji \
      libasound2 \
      libatk-bridge2.0-0 \
      libatk1.0-0 \
      libc6 \
      libcairo2 \
      libcups2 \
      libdbus-1-3 \
      libdrm2 \
      libegl1 \
      libexpat1 \
      libfontconfig1 \
      libgbm1 \
      libgles2 \
      libglib2.0-0 \
      libgtk-3-0 \
      libnspr4 \
      libnss3 \
      libpango-1.0-0 \
      libx11-6 \
      libx11-xcb1 \
      libxcb1 \
      libxcomposite1 \
      libxcursor1 \
      libxdamage1 \
      libxext6 \
      libxfixes3 \
      libxi6 \
      libxkbcommon0 \
      libxrandr2 \
      libxshmfence1 \
      libxss1 \
      libxtst6 \
      procps \
      wget \
      xdg-utils \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy manifests first so `npm ci` is layer-cached across source-only edits.
COPY package.json package-lock.json* ./

# Point both engines at the apt-installed Chromium and skip their own
# downloads (Chrome-for-Testing has no arm64 build; the playwright
# downloader can't always reach the host's network from CI either).
# `lib/browser-engine.js` reads PLAYWRIGHT_EXECUTABLE_PATH and threads it
# into launchOpts.executablePath; puppeteer reads PUPPETEER_EXECUTABLE_PATH
# directly.
ENV PUPPETEER_SKIP_DOWNLOAD=true \
    PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium \
    PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1 \
    PLAYWRIGHT_EXECUTABLE_PATH=/usr/bin/chromium

# Prefer `npm ci` when a lockfile is present (reproducible installs); fall
# back to `npm install` otherwise so the image still builds in a fresh
# clone where the lockfile is gitignored. Optional deps (playwright) are
# kept so `--browser-engine playwright` works out of the box.
RUN if [ -f package-lock.json ]; then npm ci --omit=dev --no-audit --no-fund; \
    else npm install --omit=dev --no-audit --no-fund; fi \
 && npm cache clean --force

# Copy the runtime sources. snapshot.js is included for parity with the
# CLI but the server only needs server.js + lib/.
COPY lib ./lib
COPY server.js ./server.js
COPY snapshot.js ./snapshot.js

# Server defaults: bind to all interfaces so the published port is
# reachable from outside the container; expose 8787 (the server's default
# listen port). PAGX_BIN points at a conventional drop-in location so
# operators can mount a host-built pagx without extra --pagx-bin flags.
ENV NODE_ENV=production \
    HOST=0.0.0.0 \
    PORT=8787 \
    PAGX_BIN=/usr/local/bin/pagx

EXPOSE 8787

# Liveness probe. `node -e` avoids pulling in curl/wget at runtime and
# keeps the check inside the same Node binary that's already running.
HEALTHCHECK --interval=30s --timeout=5s --start-period=20s --retries=3 \
  CMD node -e "require('http').get('http://127.0.0.1:'+(process.env.PORT||8787)+'/health',r=>process.exit(r.statusCode===200?0:1)).on('error',()=>process.exit(1))"

# tini reaps zombies and forwards SIGTERM/SIGINT to node, which lets
# server.js drain in-flight requests and close the browser on `docker stop`
# before the 10s safety-net timer trips.
ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["node", "server.js"]
