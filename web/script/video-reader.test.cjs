// Copyright (C) 2026 Tencent. All rights reserved.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('typescript');

const sourcePath = path.resolve(__dirname, '../src/core/video-reader.ts');
const compiledSource = ts.transpileModule(fs.readFileSync(sourcePath, 'utf8'), {
  compilerOptions: { target: ts.ScriptTarget.ES2020, module: ts.ModuleKind.CommonJS },
  fileName: sourcePath,
}).outputText;

class Events {
  listeners = new Map();

  addEventListener(event, handler) {
    if (!this.listeners.has(event)) this.listeners.set(event, new Set());
    this.listeners.get(event).add(handler);
  }

  removeEventListener(event, handler) {
    this.listeners.get(event)?.delete(handler);
  }

  emit(event) {
    for (const handler of Array.from(this.listeners.get(event) || [])) handler({ type: event });
  }

  count() {
    let count = 0;
    for (const handlers of this.listeners.values()) count += handlers.size;
    return count;
  }
}

class Deferred {
  constructor() {
    this.promise = new Promise(this.capture.bind(this));
  }

  capture(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }
}

class Video extends Events {
  style = {};
  readyState = 3;
  paused = true;
  seeking = false;
  playbackRate = 1;
  src = 'https://example.test/external.mp4';
  error = null;
  playCalls = 0;
  pauseCalls = 0;
  loadCalls = 0;
  seekCalls = 0;
  autoSeek = false;
  playMode = 'ready';
  currentTimeValue = 1;
  playResult = null;

  get currentTime() {
    return this.currentTimeValue;
  }

  set currentTime(value) {
    this.seekCalls++;
    this.currentTimeValue = value;
    this.seeking = true;
    if (this.autoSeek) this.finishSeek();
  }

  finishSeek() {
    this.seeking = false;
    this.emit('seeked');
  }

  play() {
    this.playCalls++;
    if (this.playMode === 'reject') return Promise.reject(new Error('Playback rejected.'));
    this.paused = false;
    if (this.playMode === 'pending') {
      this.playResult = new Deferred();
      return this.playResult.promise;
    }
    return Promise.resolve();
  }

  pause() {
    this.pauseCalls++;
    this.paused = true;
  }

  removeAttribute(name) {
    if (name === 'src') this.src = '';
  }

  load() {
    this.loadCalls++;
  }
}

class Clock {
  time = 0;
  nextID = 0;
  timers = new Map();
  frames = new Map();

  setTimeout(callback, delay) {
    const id = ++this.nextID;
    this.timers.set(id, { callback, time: this.time + delay });
    return id;
  }

  clearTimeout(id) {
    this.timers.delete(id);
  }

  requestAnimationFrame(callback) {
    const id = ++this.nextID;
    this.frames.set(id, callback);
    return id;
  }

  cancelAnimationFrame(id) {
    this.frames.delete(id);
  }

  advance(delay) {
    this.time += delay;
    for (const [id, timer] of Array.from(this.timers)) {
      if (timer.time <= this.time) {
        this.timers.delete(id);
        timer.callback();
      }
    }
  }

  renderFrame() {
    for (const [id, callback] of Array.from(this.frames)) {
      this.frames.delete(id);
      callback(this.time);
    }
  }
}

class Owner {
  isDestroyed = false;
  readers = new Set();

  constructor(playing = false) {
    this.playing = playing;
  }

  isVideoPlaying() {
    return this.playing;
  }

  linkVideoReader(reader) {
    this.readers.add(reader);
  }

  unlinkVideoReader(reader) {
    this.readers.delete(reader);
  }
}

function createEnvironment(flags = {}) {
  const clock = new Clock();
  const document = new Events();
  document.visibilityState = 'visible';
  const videos = [];
  document.createElement = function createElement(name) {
    assert.equal(name, 'video');
    const video = new Video();
    videos.push(video);
    return video;
  };
  const window = new Events();
  const revokedURLs = [];
  const bridgeCallbacks = [];
  window.WeixinJSBridge = {
    invoke(method, options, complete) {
      assert.equal(method, 'getNetworkType');
      bridgeCallbacks.push(complete);
    },
  };
  const wrongOwner = new Owner();
  const imports = {
    '../constant': {
      VIDEO_DECODE_WAIT_FRAME: 1,
      VIDEO_DECODE_SEEK_TIMEOUT_FRAME: 12,
      VIDEO_PLAYBACK_RATE_MAX: 4,
      VIDEO_PLAYBACK_RATE_MIN: 0.25,
    },
    '../utils/ua': { IPHONE: false, WECHAT: false, SAFARI_OR_IOS_WEBVIEW: false, ...flags },
    '../utils/video-listener': {
      addListener(video, event, handler) {
        video.addEventListener(event, handler);
      },
      removeListener(video, event, handler) {
        video.removeEventListener(event, handler);
      },
    },
    '../utils/type-utils': {
      isInstanceOf(value, type) {
        return typeof type === 'function' && value instanceof type;
      },
    },
    '../pag-module': { PAGModule: { currentPlayer: wrongOwner } },
  };
  function loadImport(name) {
    assert.ok(Object.hasOwn(imports, name), `Unexpected import: ${name}`);
    return imports[name];
  }
  const exports = {};
  vm.runInNewContext(
    compiledSource,
    {
      exports,
      require: loadImport,
      document,
      window,
      HTMLVideoElement: Video,
      HTMLMediaElement: { HAVE_FUTURE_DATA: 3 },
      Blob,
      URL: {
        createObjectURL() {
          return `blob:owned-${videos.length}`;
        },
        revokeObjectURL(url) {
          revokedURLs.push(url);
        },
      },
      setTimeout: clock.setTimeout.bind(clock),
      clearTimeout: clock.clearTimeout.bind(clock),
      requestAnimationFrame: clock.requestAnimationFrame.bind(clock),
      cancelAnimationFrame: clock.cancelAnimationFrame.bind(clock),
    },
    { filename: sourcePath },
  );
  return { ...exports, clock, document, window, videos, revokedURLs, bridgeCallbacks, wrongOwner };
}

function makeReader(environment, video = new Video(), owner = new Owner(), ranges = []) {
  return new environment.VideoReader(video, 100, 100, 30, ranges, owner);
}

async function drainMicrotasks() {
  for (let count = 0; count < 16; count++) await Promise.resolve();
}

function assertClean(environment, video) {
  assert.equal(environment.clock.timers.size, 0);
  assert.equal(environment.clock.frames.size, 0);
  assert.equal(environment.document.count(), 0);
  assert.equal(video.count(), 0);
}

test('same-frame preparation follows the latest play intent and playback rate', async function () {
  const environment = createEnvironment();
  const video = new Video();
  const reader = makeReader(environment, video);
  assert.equal(await reader.prepare(30, 1), true);
  assert.equal(video.playCalls, 0);
  reader.isPlaying = true;
  assert.equal(await reader.prepare(30, 1.5), true);
  assert.equal(video.playCalls, 1);
  assert.equal(video.paused, false);
  assert.equal(video.playbackRate, 1.5);
  reader.pause();
  assert.equal(await reader.prepare(30, 0.5), true);
  assert.equal(video.paused, true);
  assert.equal(video.playbackRate, 0.5);
  assertClean(environment, video);
});

test('pause cancels an in-flight seek without a late play or frame commit', async function () {
  const environment = createEnvironment();
  const video = new Video();
  const reader = makeReader(environment, video, new Owner(true));
  const pending = reader.prepare(60, 1);
  await drainMicrotasks();
  assert.equal(video.seeking, true);
  reader.pause();
  assert.equal(await pending, false);
  assert.equal(reader.getCurrentFrame(), -1);
  video.finishSeek();
  await drainMicrotasks();
  assert.equal(video.playCalls, 0);
  assert.equal(await reader.prepare(60, 1), true);
  assert.equal(video.paused, true);
  assertClean(environment, video);
});

test('synchronous seeked is observed and the seek success path updates playback rate', async function () {
  const environment = createEnvironment();
  const video = new Video();
  video.autoSeek = true;
  const reader = makeReader(environment, video, new Owner(true));
  assert.equal(await reader.prepare(60, 2), true);
  assert.equal(reader.getCurrentFrame(), 60);
  assert.equal(reader.isSought, true);
  assert.equal(video.playbackRate, 2);
  assert.equal(video.playCalls, 1);
  assertClean(environment, video);
});

test('canplay timeout and media errors settle false without committing a frame', async function () {
  for (const failure of ['timeout', 'error']) {
    const environment = createEnvironment();
    const video = new Video();
    video.readyState = 0;
    video.currentTimeValue = 0;
    const reader = makeReader(environment, video);
    const pending = reader.prepare(0, 1);
    if (failure === 'timeout') {
      environment.clock.advance(1000);
    } else {
      video.error = new Error('Media failed.');
      video.emit('error');
    }
    assert.equal(await pending, false);
    assert.ok(reader.getError());
    assert.equal(reader.getCurrentFrame(), -1);
    assertClean(environment, video);
  }
});

test('timed-out seek is not cached and retry waits until the current seek finishes', async function () {
  const environment = createEnvironment();
  const video = new Video();
  const reader = makeReader(environment, video);
  const pending = reader.prepare(60, 1);
  await drainMicrotasks();
  environment.clock.advance(400);
  assert.equal(await pending, false);
  assert.equal(reader.getCurrentFrame(), -1);
  assert.match(String(reader.getError()), /timed out/);
  const retry = reader.prepare(60, 1);
  await drainMicrotasks();
  video.emit('seeked');
  await drainMicrotasks();
  assert.equal(reader.getCurrentFrame(), -1);
  video.finishSeek();
  assert.equal(await retry, true);
  assert.equal(reader.getError(), null);
  assertClean(environment, video);
});

test('play rejection or timeout never commits the requested frame', async function () {
  for (const mode of ['reject', 'pending']) {
    const environment = createEnvironment();
    const video = new Video();
    video.playMode = mode;
    const reader = makeReader(environment, video, new Owner(true));
    const pending = reader.prepare(30, 1);
    await drainMicrotasks();
    if (mode === 'pending') environment.clock.advance(1000);
    assert.equal(await pending, false);
    assert.equal(reader.getCurrentFrame(), -1);
    assert.ok(reader.getError());
    if (video.playResult) video.playResult.resolve();
    await drainMicrotasks();
    assert.equal(reader.getCurrentFrame(), -1);
    assert.equal(video.paused, true);
    assertClean(environment, video);
  }
});

test('superseded prepare cannot commit or consume a stale seeked event', async function () {
  const environment = createEnvironment();
  const video = new Video();
  const reader = makeReader(environment, video, new Owner(true));
  const first = reader.prepare(60, 1);
  await drainMicrotasks();
  const second = reader.prepare(90, 2);
  await drainMicrotasks();
  assert.equal(await first, false);
  video.emit('seeked');
  await drainMicrotasks();
  assert.equal(reader.getCurrentFrame(), -1);
  video.finishSeek();
  assert.equal(await second, true);
  assert.equal(reader.getCurrentFrame(), 90);
  assert.equal(video.playCalls, 1);
  assertClean(environment, video);
});

test('Safari first-frame warmup preserves the latest playing intent', async function () {
  for (const playing of [false, true]) {
    const environment = createEnvironment({ SAFARI_OR_IOS_WEBVIEW: true });
    const video = new Video();
    video.currentTimeValue = 0;
    const reader = makeReader(environment, video);
    const pending = reader.prepare(0, 1);
    await drainMicrotasks();
    assert.equal(video.playCalls, 1);
    assert.equal(environment.clock.frames.size, 1);
    reader.isPlaying = playing;
    environment.clock.renderFrame();
    assert.equal(await pending, true);
    assert.equal(video.paused, !playing);
    assert.equal(reader.isPlaying, playing);
    assertClean(environment, video);
  }
});

test('visibility and Wechat authorization waits cannot restart playback after stop', async function () {
  for (const wait of ['visibility', 'wechat']) {
    const environment = createEnvironment({ WECHAT: wait === 'wechat' });
    const video = new Video();
    if (wait === 'visibility') environment.document.visibilityState = 'hidden';
    const reader = makeReader(environment, video, new Owner(true));
    const pending = reader.prepare(30, 1);
    await drainMicrotasks();
    reader.stop();
    assert.equal(await pending, false);
    environment.document.visibilityState = 'visible';
    environment.document.emit('visibilitychange');
    for (const callback of environment.bridgeCallbacks) callback();
    await drainMicrotasks();
    assert.equal(video.playCalls, 0);
    assert.equal(reader.getCurrentFrame(), -1);
    assertClean(environment, video);
  }
});

test('Wechat authorization, hidden document and animation frame waits time out', async function () {
  for (const wait of ['wechat', 'visibility', 'frame']) {
    const environment = createEnvironment({ WECHAT: wait === 'wechat', SAFARI_OR_IOS_WEBVIEW: wait === 'frame' });
    const video = new Video();
    if (wait === 'frame') video.currentTimeValue = 0;
    if (wait === 'visibility') environment.document.visibilityState = 'hidden';
    const reader = makeReader(environment, video, new Owner(true));
    const pending = reader.prepare(wait === 'frame' ? 0 : 30, 1);
    await drainMicrotasks();
    environment.clock.advance(1000);
    assert.equal(await pending, false);
    assert.equal(reader.getCurrentFrame(), -1);
    assert.ok(reader.getError());
    assertClean(environment, video);
  }
});

test('destroy settles each outstanding wait and late callbacks cannot use the video', async function () {
  for (const wait of ['canplay', 'seek', 'frame', 'play', 'wechat', 'visibility']) {
    const environment = createEnvironment({ WECHAT: wait === 'wechat', SAFARI_OR_IOS_WEBVIEW: wait === 'frame' });
    const video = new Video();
    if (wait === 'canplay') video.readyState = 0;
    if (wait === 'canplay' || wait === 'frame') video.currentTimeValue = 0;
    if (wait === 'play') video.playMode = 'pending';
    if (wait === 'visibility') environment.document.visibilityState = 'hidden';
    const owner = new Owner(true);
    const reader = makeReader(environment, video, owner);
    let targetFrame = 30;
    if (wait === 'canplay' || wait === 'frame') targetFrame = 0;
    if (wait === 'seek') targetFrame = 60;
    const pending = reader.prepare(targetFrame, 1);
    await drainMicrotasks();
    reader.onDestroy();
    assert.equal(await pending, false);
    assert.equal(reader.getVideo(), null);
    assert.equal(reader.getCurrentFrame(), -1);
    assert.equal(owner.readers.size, 0);
    assertClean(environment, video);
    const playCalls = video.playCalls;
    const pauseCalls = video.pauseCalls;
    if (video.playResult) video.playResult.resolve();
    video.emit('canplay');
    video.finishSeek();
    for (const callback of environment.bridgeCallbacks) callback();
    environment.document.visibilityState = 'visible';
    environment.document.emit('visibilitychange');
    environment.clock.renderFrame();
    await drainMicrotasks();
    assert.equal(video.playCalls, playCalls);
    assert.equal(video.pauseCalls, pauseCalls);
    assert.equal(await reader.prepare(targetFrame, 1), false);
    assertClean(environment, video);
  }
});

test('reader ownership is explicit and external media resources are not revoked', async function () {
  const environment = createEnvironment();
  const firstOwner = new Owner(true);
  const secondOwner = new Owner(false);
  const external = new Video();
  const first = await environment.VideoReader.create(external, 100, 100, 30, [], firstOwner);
  const second = await environment.VideoReader.create(new Uint8Array([1, 2]), 100, 100, 30, [], secondOwner);
  assert.equal(first.isPlaying, true);
  assert.equal(second.isPlaying, false);
  assert.equal(firstOwner.readers.has(first), true);
  assert.equal(secondOwner.readers.has(second), true);
  assert.equal(environment.wrongOwner.readers.size, 0);
  first.onDestroy();
  assert.equal(external.src, 'https://example.test/external.mp4');
  assert.equal(environment.revokedURLs.length, 0);
  assert.equal(secondOwner.readers.has(second), true);
  second.onDestroy();
  assert.deepEqual(environment.revokedURLs, ['blob:owned-1']);
  assert.equal(environment.videos[0].src, '');
  second.onDestroy();
  assert.equal(environment.revokedURLs.length, 1);
});

test('partial video initialization rolls back the owned URL and reader registration', async function () {
  for (const stage of ['load', 'link']) {
    const environment = createEnvironment({ IPHONE: true });
    const owner = new Owner();
    const createElement = environment.document.createElement;
    environment.document.createElement = function create(name) {
      const video = createElement(name);
      if (stage === 'load') video.load = function failedLoad() { throw new Error('load failed'); };
      return video;
    };
    if (stage === 'link') {
      owner.linkVideoReader = function failedLink(reader) {
        this.readers.add(reader);
        throw new Error('link failed');
      };
    }
    await assert.rejects(environment.VideoReader.create(new Uint8Array([1]), 100, 100, 30, [], owner), /failed/);
    assert.equal(owner.readers.size, 0);
    assert.deepEqual(environment.revokedURLs, ['blob:owned-1']);
    assert.equal(environment.videos[0].src, '');
    assertClean(environment, environment.videos[0]);
  }
});

test('a destroyed owner is rejected before allocating media resources', async function () {
  const environment = createEnvironment();
  const owner = new Owner();
  owner.isDestroyed = true;
  await assert.rejects(environment.VideoReader.create(new Uint8Array([1]), 100, 100, 30, [], owner), /destroyed owner/);
  assert.equal(environment.videos.length, 0);
  assert.equal(environment.revokedURLs.length, 0);
});
