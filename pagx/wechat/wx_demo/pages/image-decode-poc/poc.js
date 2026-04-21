/**
 * Copyright (C) 2026 Tencent. All Rights Reserved.
 *
 * Image Decode POC for WeChat Miniprogram.
 *
 * Goal: validate the async-decode-on-JS + direct-consume-on-WebGL path that we plan to use for
 * pagx async image loading, before investing in the full C++ refactor.
 *
 * We verify 3 questions:
 *   Q1. Can wx Canvas2D `createImage()` decode a webp URL end-to-end via `img.src = url`?
 *   Q2. Can `gl.texImage2D(..., wxImage)` consume the decoded WxImage object directly, without
 *       going through `drawImage + getImageData`?
 *   Q3. Do concurrent `img.onload` events actually decode in parallel (wall time < N * serial)?
 */

// IMPORTANT: set by the user. Do not hard-code these in library code; this is POC-only.
const TEST_IMAGE_URLS = [
  'https://test-sign.d.gtimg.com/img/0444/2ab1/54ea00694d0b202c5b040d0872b188f8?sign=1776528441-9a28f16b-0-713983683b1f1fa07bc852cb2ed40ad4&imageMogr2/format/webp',
  'https://test-sign.d.gtimg.com/img/ef19/e1b7/e6ff8b143c18a80c567d0efad69281ef?sign=1776528441-9a28f16b-0-20ca454905b4b11c0ed5772bc550f3c7&imageMogr2/format/webp',
  'https://test-sign.d.gtimg.com/img/9e11/7ea3/1e4ff8888658aa9397986f119ac022b4?sign=1776528441-9a28f16b-0-fde595072661fb5840afe5bfad9a2c57&imageMogr2/format/webp',
  'https://test-sign.d.gtimg.com/img/7034/f35b/725fac9e1ec2b9fc08f8b923ff2dc763?sign=1776528441-9a28f16b-0-153a959ae3eeefc99f07573294e6748f&imageMogr2/format/webp',
];

Page({
  data: {
    running: false,
    logs: [],
    logAnchor: '',
  },

  canvas2d: null,        // wx offscreen-like Canvas2D used for createImage
  canvasWebgl: null,     // wx Canvas used for WebGL texture tests
  gl: null,
  logIdx: 0,

  onLoad() {
    this.log('info', 'POC page loaded. Tap a button to run tests.');
    // Prepare Canvas2D and WebGL canvases once.
    Promise.all([
      this.acquireCanvas2D(),
      this.acquireWebglCanvas(),
    ])
      .then(() => this.log('ok', 'Canvas2D and WebGL canvases are ready.'))
      .catch((err) => this.log('err', 'Canvas init failed: ' + (err && err.message)));
  },

  now() {
    return (typeof performance !== 'undefined' && performance.now)
      ? performance.now() : Date.now();
  },

  log(level, text) {
    const idx = this.logIdx++;
    const line = { idx, level, text };
    // Keep log array reasonably small to avoid setData bloat.
    const MAX = 500;
    const logs = this.data.logs.concat(line);
    if (logs.length > MAX) {
      logs.splice(0, logs.length - MAX);
    }
    this.setData({ logs, logAnchor: 'log-' + idx });
    // Mirror to console for easier copy-paste.
    // eslint-disable-next-line no-console
    console.log('[POC]', text);
  },

  clearLog() {
    this.setData({ logs: [] });
    this.logIdx = 0;
  },

  acquireCanvas2D() {
    return new Promise((resolve, reject) => {
      const query = wx.createSelectorQuery();
      query.select('#poc-canvas2d')
        .node()
        .exec((res) => {
          if (!res || !res[0] || !res[0].node) {
            reject(new Error('canvas2d node not found'));
            return;
          }
          this.canvas2d = res[0].node;
          resolve();
        });
    });
  },

  acquireWebglCanvas() {
    return new Promise((resolve, reject) => {
      const query = wx.createSelectorQuery();
      query.select('#poc-canvas-webgl')
        .node()
        .exec((res) => {
          if (!res || !res[0] || !res[0].node) {
            reject(new Error('webgl canvas node not found'));
            return;
          }
          this.canvasWebgl = res[0].node;
          // WeChat webgl canvas size is in physical pixels; keep it tiny since we do not render.
          this.canvasWebgl.width = 4;
          this.canvasWebgl.height = 4;
          const gl = this.canvasWebgl.getContext('webgl');
          if (!gl) {
            reject(new Error('getContext("webgl") returned null'));
            return;
          }
          this.gl = gl;
          resolve();
        });
    });
  },

  // ---------------------------------------------------------------------------
  // Q1: createImage + onload decode webp via URL.
  // ---------------------------------------------------------------------------
  async runQ1() {
    if (this.data.running) return;
    this.setData({ running: true });
    try {
      this.log('step', '=== Q1: createImage() + img.src = url (webp) ===');
      if (!this.canvas2d) await this.acquireCanvas2D();

      const results = [];
      // Decode URLs one-by-one to get per-image timing.
      for (let i = 0; i < TEST_IMAGE_URLS.length; i++) {
        const url = TEST_IMAGE_URLS[i];
        const t0 = this.now();
        try {
          const img = await this.decodeImageFromUrl(url);
          const t1 = this.now();
          const dt = (t1 - t0).toFixed(1);
          const w = img.width;
          const h = img.height;
          this.log('ok', `#${i} ok: ${w}x${h} in ${dt} ms`);
          results.push({ ok: true, w, h, dt: +dt, img });
        } catch (err) {
          const t1 = this.now();
          this.log('err', `#${i} failed after ${(t1 - t0).toFixed(1)} ms: ${err && err.message}`);
          results.push({ ok: false, err: err && err.message });
        }
      }

      const okCount = results.filter((r) => r.ok).length;
      this.log(okCount === results.length ? 'ok' : 'warn',
        `Q1 summary: ${okCount}/${results.length} decoded successfully.`);

      // Save for potential reuse in Q2.
      this.lastDecoded = results;
    } finally {
      this.setData({ running: false });
    }
  },

  decodeImageFromUrl(url) {
    return new Promise((resolve, reject) => {
      const img = this.canvas2d.createImage();
      let done = false;
      img.onload = () => {
        if (done) return;
        done = true;
        resolve(img);
      };
      img.onerror = (err) => {
        if (done) return;
        done = true;
        // Some bases pass Error, some pass string, some pass event.
        const msg = (err && (err.errMsg || err.message || err.type)) || 'onerror';
        reject(new Error('img decode error: ' + msg));
      };
      img.src = url;
    });
  },

  // ---------------------------------------------------------------------------
  // Q2: WebGL directly consumes WxImage via texImage2D(..., wxImage).
  // ---------------------------------------------------------------------------
  async runQ2() {
    if (this.data.running) return;
    this.setData({ running: true });
    try {
      this.log('step', '=== Q2: gl.texImage2D(..., wxImage) direct consume ===');
      if (!this.canvas2d) await this.acquireCanvas2D();
      if (!this.gl) await this.acquireWebglCanvas();

      // Reuse Q1 results if available, otherwise decode fresh.
      let decoded = this.lastDecoded;
      if (!decoded || !decoded.some((r) => r.ok)) {
        this.log('info', 'No prior decoded images, decoding now...');
        decoded = [];
        for (let i = 0; i < TEST_IMAGE_URLS.length; i++) {
          try {
            const img = await this.decodeImageFromUrl(TEST_IMAGE_URLS[i]);
            decoded.push({ ok: true, w: img.width, h: img.height, img });
          } catch (err) {
            decoded.push({ ok: false });
          }
        }
      }

      const gl = this.gl;
      let directOk = 0;
      let directFail = 0;
      let fallbackOk = 0;

      for (let i = 0; i < decoded.length; i++) {
        const r = decoded[i];
        if (!r || !r.ok) continue;
        const img = r.img;
        this.log('info', `#${i} size=${r.w}x${r.h}`);

        // -- Path A: direct texImage2D(..., wxImage)
        const texA = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, texA);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
        gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
        // Clear any prior gl error so the next getError reflects only this call.
        // eslint-disable-next-line no-unused-expressions
        gl.getError();

        const tA0 = this.now();
        let directErr = null;
        try {
          gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
        } catch (e) {
          directErr = e && (e.message || String(e));
        }
        const tA1 = this.now();
        const glErrA = gl.getError();
        gl.deleteTexture(texA);

        if (directErr) {
          this.log('err', `  A direct texImage2D threw: ${directErr}`);
          directFail++;
        } else if (glErrA !== gl.NO_ERROR) {
          this.log('warn', `  A direct texImage2D glError=0x${glErrA.toString(16)} (unsupported) t=${(tA1 - tA0).toFixed(1)}ms`);
          directFail++;
        } else {
          this.log('ok', `  A direct texImage2D OK in ${(tA1 - tA0).toFixed(1)} ms`);
          directOk++;
        }

        // -- Path B: fallback drawImage + getImageData -> texImage2D(pixels)
        const tB0 = this.now();
        const canvas2d = this.canvas2d;
        canvas2d.width = r.w;
        canvas2d.height = r.h;
        const ctx = canvas2d.getContext('2d', { willReadFrequently: true });
        ctx.clearRect(0, 0, r.w, r.h);
        ctx.drawImage(img, 0, 0, r.w, r.h);
        const imageData = ctx.getImageData(0, 0, r.w, r.h);
        const pixels = new Uint8Array(imageData.data.buffer, imageData.data.byteOffset, imageData.data.byteLength);
        const tB1 = this.now();

        const texB = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, texB);
        const tB2 = this.now();
        let fallbackErr = null;
        try {
          gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, r.w, r.h, 0, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        } catch (e) {
          fallbackErr = e && (e.message || String(e));
        }
        const tB3 = this.now();
        const glErrB = gl.getError();
        gl.deleteTexture(texB);

        if (fallbackErr || glErrB !== gl.NO_ERROR) {
          this.log('err', `  B fallback failed: ${fallbackErr || 'glError=0x' + glErrB.toString(16)}`);
        } else {
          const drawMs = (tB1 - tB0).toFixed(1);
          const uploadMs = (tB3 - tB2).toFixed(1);
          const totalMs = (tB3 - tB0).toFixed(1);
          this.log('ok', `  B fallback OK: drawImage+getImageData=${drawMs}ms, upload=${uploadMs}ms, total=${totalMs}ms`);
          fallbackOk++;
        }
      }

      this.log(directOk > 0 ? 'ok' : 'warn',
        `Q2 summary: direct ok=${directOk}, direct fail=${directFail}, fallback ok=${fallbackOk}`);
      if (directOk === 0 && fallbackOk > 0) {
        this.log('warn', 'WebGL does NOT consume WxImage directly. Production must go through drawImage+getImageData.');
      } else if (directOk > 0 && directFail === 0) {
        this.log('ok', 'WebGL accepts WxImage directly. Zero-copy path is viable!');
      }
    } finally {
      this.setData({ running: false });
    }
  },

  // ---------------------------------------------------------------------------
  // Q3: parallel vs serial decode wall time.
  // ---------------------------------------------------------------------------
  async runQ3() {
    if (this.data.running) return;
    this.setData({ running: true });
    try {
      this.log('step', '=== Q3: parallel vs serial onload ===');
      if (!this.canvas2d) await this.acquireCanvas2D();

      // Serial: decode each URL one after another.
      const tS0 = this.now();
      const serialResults = [];
      for (let i = 0; i < TEST_IMAGE_URLS.length; i++) {
        try {
          const img = await this.decodeImageFromUrl(TEST_IMAGE_URLS[i]);
          serialResults.push({ ok: true, w: img.width, h: img.height });
        } catch (err) {
          serialResults.push({ ok: false });
        }
      }
      const tS1 = this.now();
      const serialMs = tS1 - tS0;
      const serialOk = serialResults.filter((r) => r.ok).length;
      this.log('info', `Serial: ${serialOk}/${TEST_IMAGE_URLS.length} ok in ${serialMs.toFixed(1)} ms`);

      // Parallel: fire all decodes together.
      const tP0 = this.now();
      const parallel = await Promise.all(
        TEST_IMAGE_URLS.map((url) =>
          this.decodeImageFromUrl(url).then(
            (img) => ({ ok: true, w: img.width, h: img.height }),
            () => ({ ok: false })
          )
        )
      );
      const tP1 = this.now();
      const parallelMs = tP1 - tP0;
      const parallelOk = parallel.filter((r) => r.ok).length;
      this.log('info', `Parallel: ${parallelOk}/${TEST_IMAGE_URLS.length} ok in ${parallelMs.toFixed(1)} ms`);

      const speedup = serialMs > 0 ? (serialMs / parallelMs).toFixed(2) : 'n/a';
      this.log('ok', `Q3 summary: serial=${serialMs.toFixed(1)}ms, parallel=${parallelMs.toFixed(1)}ms, speedup=${speedup}x`);
      if (parallelMs < serialMs * 0.8) {
        this.log('ok', 'Concurrent decoding effectively reduces wall time.');
      } else {
        this.log('warn', 'Concurrency does NOT help much. Decoding may be serialized by the system.');
      }
    } finally {
      this.setData({ running: false });
    }
  },

  // ---------------------------------------------------------------------------
  // Q4: split network vs decode by using arraybuffer + tempfile, then compare
  //     hot-cache URL decode to check if WeChat caches remote images.
  // ---------------------------------------------------------------------------
  async runQ4() {
    if (this.data.running) return;
    this.setData({ running: true });
    try {
      this.log('step', '=== Q4: cold (download+temp+decode) vs hot (url cache) ===');
      if (!this.canvas2d) await this.acquireCanvas2D();

      const fs = wx.getFileSystemManager();

      // Cold path: download → write temp → img.src = tempPath
      this.log('info', '-- cold path: wx.request + temp file + createImage --');
      const coldResults = [];
      for (let i = 0; i < TEST_IMAGE_URLS.length; i++) {
        const url = TEST_IMAGE_URLS[i];

        const tDl0 = this.now();
        let bytes = null;
        try {
          bytes = await this.downloadArrayBuffer(url);
        } catch (err) {
          this.log('err', `  #${i} download failed: ${err && err.message}`);
          coldResults.push({ ok: false });
          continue;
        }
        const tDl1 = this.now();
        const dlMs = tDl1 - tDl0;
        const bytesKB = (bytes.byteLength / 1024).toFixed(1);

        // Write to temp file.
        const tempPath = `${wx.env.USER_DATA_PATH}/poc_img_${i}.webp`;
        const tFs0 = this.now();
        try {
          fs.writeFileSync(tempPath, bytes);
        } catch (err) {
          this.log('err', `  #${i} writeFile failed: ${err && err.message}`);
          coldResults.push({ ok: false });
          continue;
        }
        const tFs1 = this.now();
        const fsMs = tFs1 - tFs0;

        // Decode via temp path.
        const tDec0 = this.now();
        let img = null;
        try {
          img = await this.decodeImageFromUrl(tempPath);
        } catch (err) {
          this.log('err', `  #${i} temp-path decode failed: ${err && err.message}`);
          coldResults.push({ ok: false });
          continue;
        }
        const tDec1 = this.now();
        const decMs = tDec1 - tDec0;
        const totalMs = tDec1 - tDl0;

        this.log('ok',
          `  #${i} ${img.width}x${img.height} ${bytesKB}KB | dl=${dlMs.toFixed(1)} fs=${fsMs.toFixed(1)} decode=${decMs.toFixed(1)} total=${totalMs.toFixed(1)} ms`);
        coldResults.push({
          ok: true, w: img.width, h: img.height, bytesKB: +bytesKB,
          dlMs, fsMs, decMs, totalMs,
        });
      }

      // Aggregate cold path.
      const coldOk = coldResults.filter((r) => r.ok);
      if (coldOk.length > 0) {
        const sumDl = coldOk.reduce((s, r) => s + r.dlMs, 0);
        const sumFs = coldOk.reduce((s, r) => s + r.fsMs, 0);
        const sumDec = coldOk.reduce((s, r) => s + r.decMs, 0);
        const sumTotal = coldOk.reduce((s, r) => s + r.totalMs, 0);
        this.log('info',
          `  cold sum: dl=${sumDl.toFixed(1)} fs=${sumFs.toFixed(1)} decode=${sumDec.toFixed(1)} total=${sumTotal.toFixed(1)} ms (${coldOk.length} imgs)`);
        this.log('info',
          `  cold avg per img: dl=${(sumDl / coldOk.length).toFixed(1)} decode=${(sumDec / coldOk.length).toFixed(1)} ms`);
      }

      // Hot path: same URLs, direct img.src = url, should hit WeChat URL cache.
      this.log('info', '-- hot path: img.src = url (same URL, should hit cache) --');
      const hotResults = [];
      for (let i = 0; i < TEST_IMAGE_URLS.length; i++) {
        const url = TEST_IMAGE_URLS[i];
        const t0 = this.now();
        try {
          const img = await this.decodeImageFromUrl(url);
          const t1 = this.now();
          const dt = t1 - t0;
          this.log('ok', `  #${i} ${img.width}x${img.height} url-decode=${dt.toFixed(1)} ms`);
          hotResults.push({ ok: true, dt });
        } catch (err) {
          this.log('err', `  #${i} hot decode failed: ${err && err.message}`);
          hotResults.push({ ok: false });
        }
      }

      const hotOk = hotResults.filter((r) => r.ok);
      if (hotOk.length > 0) {
        const sumHot = hotOk.reduce((s, r) => s + r.dt, 0);
        this.log('info', `  hot sum: ${sumHot.toFixed(1)} ms (${hotOk.length} imgs)`);
        this.log('info', `  hot avg per img: ${(sumHot / hotOk.length).toFixed(1)} ms`);
      }

      // Parallel cold to measure real-world "first load all images" wall time.
      this.log('info', '-- parallel cold: concurrent download+temp+decode --');
      const tP0 = this.now();
      const parallelResults = await Promise.all(
        TEST_IMAGE_URLS.map((url, i) => this.coldDecodeOne(url, i + 100))
      );
      const tP1 = this.now();
      const parallelOkCount = parallelResults.filter((r) => r.ok).length;
      this.log('ok',
        `  parallel cold total: ${(tP1 - tP0).toFixed(1)} ms (${parallelOkCount}/${TEST_IMAGE_URLS.length} ok)`);

      // Clean up temp files.
      for (let i = 0; i < TEST_IMAGE_URLS.length; i++) {
        try { fs.unlinkSync(`${wx.env.USER_DATA_PATH}/poc_img_${i}.webp`); } catch (e) { /* ignore */ }
        try { fs.unlinkSync(`${wx.env.USER_DATA_PATH}/poc_img_${i + 100}.webp`); } catch (e) { /* ignore */ }
      }

      this.log('ok', 'Q4 done. Compare cold.decode vs hot to tell if the URL path is a real re-download.');
    } finally {
      this.setData({ running: false });
    }
  },

  downloadArrayBuffer(url) {
    return new Promise((resolve, reject) => {
      wx.request({
        url,
        responseType: 'arraybuffer',
        success: (res) => {
          if (res.statusCode === 200) {
            resolve(res.data);
          } else {
            reject(new Error('HTTP ' + res.statusCode));
          }
        },
        fail: (err) => reject(new Error((err && err.errMsg) || 'request fail')),
      });
    });
  },

  // Helper: download one URL, write to temp file, decode via tempPath. Used for parallel test.
  async coldDecodeOne(url, id) {
    const fs = wx.getFileSystemManager();
    try {
      const bytes = await this.downloadArrayBuffer(url);
      const tempPath = `${wx.env.USER_DATA_PATH}/poc_img_${id}.webp`;
      fs.writeFileSync(tempPath, bytes);
      const img = await this.decodeImageFromUrl(tempPath);
      return { ok: true, w: img.width, h: img.height };
    } catch (err) {
      return { ok: false, err: err && err.message };
    }
  },

  // ---------------------------------------------------------------------------
  // Run all in sequence.
  // ---------------------------------------------------------------------------
  async runAll() {
    if (this.data.running) return;
    await this.runQ1();
    await this.runQ2();
    await this.runQ3();
    await this.runQ4();
  },
});
