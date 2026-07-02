# Linux benchmark container for tools/html-snapshot.
#
# Builds a headless-Chromium-capable Node environment and runs
# `node snapshot.js` against a mounted set of HTML files. Resource
# consumption (peak RSS across the full Chromium process tree, total
# CPU time, wall clock) is captured by bench/sampler.js.
#
# Build (from repo root):
#   docker build -f tools/html-snapshot/bench/Dockerfile -t html-snapshot-bench tools/html-snapshot
#
# Run (mount inputs read-only, outputs writable):
#   docker run --rm \
#     -v "/path/to/tmp_case":/inputs:ro \
#     -v "$(pwd)/bench-out":/out \
#     html-snapshot-bench \
#       /bench/run-in-container.sh /inputs /out

FROM node:22-bookworm-slim

# Why we use distro Chromium instead of puppeteer's bundled
# Chrome-for-Testing build:
#   Chrome-for-Testing currently ships Linux builds only for x86_64.
#   On ARM64 hosts (Apple silicon / Graviton / Ampere) puppeteer still
#   downloads the x86_64 binary into a `linux_arm-*` directory and
#   then fails to exec it under binfmt_misc QEMU because the x86_64
#   dynamic linker isn't installed. Debian Bookworm's `chromium`
#   package, by contrast, is built natively for arm64 (and amd64) and
#   ships from the security tracker, so we get a real headless Chromium
#   on every architecture for free. We disable puppeteer's own
#   downloader (PUPPETEER_SKIP_DOWNLOAD=true) and point it at the
#   distro binary via PUPPETEER_EXECUTABLE_PATH below — this also
#   shaves ~280 MB off the image vs. keeping both binaries around.
#
# Chromium runtime deps for headless puppeteer. With distro chromium
# most of the libs come in as transitive dependencies of the chromium
# package itself, but we keep the explicit list (mirrors puppeteer's
# Linux troubleshooting guide) so the container still works if the
# image is later switched back to chrome-for-testing on x86_64.
# Fonts: CJK + emoji because tmp_case/ is mostly Chinese slides.
RUN apt-get update && apt-get install -y --no-install-recommends \
    chromium \
    ca-certificates \
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
    libexpat1 \
    libgbm1 \
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
    time \
    wget \
    xdg-utils \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy manifests first to maximise layer caching across rebuilds.
COPY package.json package-lock.json* ./

# Tell both engines' postinstall hooks to skip their bundled Chromium
# download (see distro-chromium rationale above) and reuse the
# apt-installed binary at runtime:
#   - puppeteer reads PUPPETEER_EXECUTABLE_PATH automatically.
#   - playwright doesn't honour an env var natively, so the same value
#     is also exposed as PLAYWRIGHT_EXECUTABLE_PATH; the bench's
#     `lib/browser-engine.js` adapter reads it and threads it through
#     as the launchOpts.executablePath, which keeps the docker image
#     small and works on both linux-amd64 and linux-arm64 (Chromium
#     for Testing only ships linux-x64, so a downloaded playwright
#     chromium would fail to launch on ARM containers anyway).
# Optional dependencies are kept enabled so playwright lands in
# node_modules and the `--engine playwright` bench path works without
# a second `npm install` step.
ENV PUPPETEER_SKIP_DOWNLOAD=true
ENV PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium
ENV PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1
ENV PLAYWRIGHT_EXECUTABLE_PATH=/usr/bin/chromium
RUN npm install --no-audit --no-fund

# Copy the rest of the tool (lib/, snapshot.js). bench/ itself is
# copied to /bench so the in-container entrypoint can find sampler.js.
COPY lib ./lib
COPY snapshot.js ./snapshot.js
COPY bench /bench
RUN chmod +x /bench/*.sh /bench/*.js || true

ENV NODE_ENV=production
WORKDIR /work
ENTRYPOINT ["/bin/bash"]
