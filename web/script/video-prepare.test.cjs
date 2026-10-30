// Copyright (C) 2026 Tencent. All rights reserved.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('typescript');

function LoadSource(file, dependencies) {
  const source = ts.transpileModule(fs.readFileSync(path.join(__dirname, '../src', file), 'utf8'), {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020, experimentalDecorators: true },
  }).outputText;
  const context = { exports: {}, console, setTimeout, clearTimeout, require(name) {
    assert.ok(Object.hasOwn(dependencies, name), `Unexpected import ${name}`);
    return dependencies[name];
  } };
  new vm.Script(source, { filename: file }).runInNewContext(context);
  return context.exports;
}

function Deferred() {
  let resolve;
  let reject;
  const promise = new Promise(function pending(done, fail) { resolve = done; reject = fail; });
  return { promise, resolve, reject };
}

function Tick() { return new Promise(setImmediate); }

function CreateHarness() {
  const readers = [];
  const managers = [];
  const calls = [];
  const module = { _useSoftwareDecoder: false, currentPlayer: null, videoReaderManager: undefined };
  const queue = LoadSource('utils/queue.ts', {});
  module.webAssemblyQueue = new queue.WebAssemblyQueue();
  const options = { block: false, failFrame: false, failCreate: false, failNative: false, createGate: null };
  module.VideoReader = { async create(data, width, height, frameRate, ranges, owner) {
    if (options.failCreate) throw new Error('reader creation failed');
    if (options.createGate) await options.createGate.promise;
    const reader = {
      owner, isPlaying: !owner.isDestroyed && owner.isVideoPlaying(), isSought: false, isDestroyed: false, frame: -1,
      attempts: [], gate: null,
      async prepare(frame, rate) {
        this.attempts.push({ frame, rate });
        calls.push(['video', owner, frame]);
        if (options.block) {
          this.gate = Deferred();
          if (!await this.gate.promise) return false;
        }
        if (this.isDestroyed || options.failFrame) return false;
        this.frame = frame;
        return true;
      },
      cancelPrepare() { this.gate?.resolve(false); },
      pause() { this.isPlaying = false; this.cancelPrepare(); },
      stop() { this.pause(); },
      async play() { this.isPlaying = true; },
      getError() { return options.failFrame ? new Error('frame failure') : null; },
      getVideo() { return { frame: this.frame }; },
      getCurrentFrame() { return this.frame; },
      onDestroy() {
        assert.equal(this.isDestroyed, false, 'reader destroyed exactly once');
        this.isDestroyed = true;
        this.cancelPrepare();
        if (!owner.isDestroyed) owner.unlinkVideoReader(this);
      },
    };
    if (!owner.isDestroyed) owner.linkVideoReader(reader);
    readers.push(reader);
    return reader;
  } };
  module._videoInfoManager = {
    _HasVideo(composition) { return composition.video; },
    _Make(composition) {
      let previous = -1;
      const manager = {
        deleted: false,
        _getVideoIDs() { return [1]; },
        _hasTimeRangeOverlap() { return false; },
        _getMp4DataByID() { return new Uint8Array([1, 2, 3]); },
        _getWidthByID() { return 65; }, _getHeightByID() { return 3; },
        _getFrameRateByID() { return 30; }, _getStaticTimeRangesByID() { return []; },
        _getPlaybackRateByID() { assert.equal(this.deleted, false); return 1; },
        _getTargetFrameByID() {
          assert.equal(this.deleted, false);
          if (previous === composition.frame) return -1;
          previous = composition.frame;
          return previous;
        },
        delete() { assert.equal(this.deleted, false); this.deleted = true; },
      };
      managers.push(manager);
      return manager;
    },
  };
  const bindings = { PAGModule: module };
  const { VideoReaderManager } = LoadSource('video-reader-manager.ts', { './pag-module': bindings });
  const decorators = LoadSource('utils/decorators.ts', { '../pag-module': bindings });
  const { PAGPlayer } = LoadSource('pag-player.ts', {
    './pag-module': bindings, './pag-file': {}, './pag-surface': {}, './core/matrix': {}, './pag-composition': {},
    './utils/decorators': decorators, './utils/type-utils': { getWasmIns(value) { return value?.wasmIns || null; } },
    './types': {}, './video-reader-manager': { VideoReaderManager }, './utils/queue': queue,
  });
  let nextID = 0;
  function Composition(video = true, frame = 0) { return { wasmIns: { video, frame, id: ++nextID } }; }
  function Player(composition = Composition()) {
    let content = null;
    let enabled = true;
    let drawn = null;
    let deleted = false;
    const native = {
      _setComposition(value) { content = value; },
      _setProgress(value) { if (content) content.frame = value; },
      _currentFrame() { return content?.frame || 0; },
      _videoEnabled() { return enabled; },
      _setVideoEnabled(value) { enabled = value; },
      _prepare() { Enter('prepare'); },
      _flush() {
        Enter('flush');
        const key = `${content?.id}-${content?.frame}`;
        const changed = drawn !== key;
        drawn = key;
        return changed;
      },
      delete() { deleted = true; },
    };
    let player;
    function Enter(operation) {
      assert.equal(deleted, false, 'native object must remain live');
      assert.equal(module.currentPlayer, player);
      if (enabled && content?.video && module._useSoftwareDecoder === false) {
        const reader = module.videoReaderManager.getVideoReaderByID(1);
        assert.equal(reader.owner, player, 'correct owner bound at every native call');
        assert.equal(reader.frame, content.frame, 'video ready before native work');
      } else {
        assert.equal(module.videoReaderManager, undefined);
      }
      calls.push([operation, player, content?.frame]);
      if (options.failNative) throw new Error('native failure');
    }
    player = new PAGPlayer(native);
    player.setComposition(composition);
    return player;
  }
  return { module, Player, Composition, readers, managers, calls, options };
}

test('prepare creates video before native preparation and preserves flush idempotence', async function () {
  const h = CreateHarness();
  const player = h.Player();
  await player.prepare();
  assert.equal(h.readers.length, 1);
  assert.deepEqual(h.calls.map(function name(call) { return call[0]; }), ['video', 'prepare']);
  assert.equal(await player.flush(), true);
  assert.equal(await player.flush(), false);
  player.destroy();
  assert.ok(h.readers.every(function destroyed(reader) { return reader.isDestroyed; }));
});

test('same player serializes requests without duplicating initialization', async function () {
  const h = CreateHarness();
  const player = h.Player();
  const results = await Promise.all([player.prepare(), player.flush(), player.flush()]);
  assert.deepEqual(results, [undefined, true, false]);
  assert.equal(h.readers.length, 1);
  player.destroy();
});

test('another player renders while the first video is waiting', async function () {
  const h = CreateHarness();
  const first = h.Player();
  const second = h.Player();
  h.options.block = true;
  const pending = first.prepare();
  await Tick();
  assert.equal(h.readers[0].attempts.length, 1);
  h.options.block = false;
  assert.equal(await second.flush(), true);
  h.readers[0].gate.resolve(true);
  await pending;
  assert.equal(await first.flush(), true);
  assert.equal(await second.flush(), false);
  assert.equal(h.module.currentPlayer, null);
  assert.equal(h.module.videoReaderManager, undefined);
  first.destroy(); second.destroy();
});

test('progress changing during video wait resamples the new frame', async function () {
  const h = CreateHarness();
  const player = h.Player();
  h.options.block = true;
  const pending = player.flush();
  await Tick();
  h.options.block = false;
  player.setProgress(9);
  assert.equal(await pending, true);
  assert.deepEqual(h.readers[0].attempts.map(function frame(attempt) { return attempt.frame; }), [0, 9]);
  assert.equal(h.calls.find(function flushed(call) { return call[0] === 'flush'; })[2], 9);
  player.destroy();
});

test('failed target remains retryable even when the native target query deduplicates it', async function () {
  const h = CreateHarness();
  const player = h.Player(h.Composition(true, 9));
  h.options.failFrame = true;
  await assert.rejects(player.prepare(), /Video frame preparation failed/);
  assert.equal(h.calls.some(function native(call) { return call[0] === 'prepare'; }), false);
  h.options.failFrame = false;
  await player.prepare();
  assert.deepEqual(h.readers[0].attempts.map(function frame(attempt) { return attempt.frame; }), [9, 9]);
  player.destroy();
});

test('destroy cancels active and queued work without accessing freed native objects', async function () {
  const h = CreateHarness();
  const player = h.Player();
  h.options.block = true;
  const active = player.flush();
  const queued = player.flush();
  await Tick();
  player.destroy();
  assert.deepEqual(await Promise.all([active, queued]), [false, false]);
  assert.equal(h.managers[0].deleted, true);
  assert.equal(h.calls.some(function native(call) { return call[0] === 'flush'; }), false);
});

test('composition replacement cancels the old request and prepares the new composition', async function () {
  const h = CreateHarness();
  const player = h.Player();
  h.options.block = true;
  const pending = player.flush();
  await Tick();
  h.options.block = false;
  player.setComposition(h.Composition(true, 4));
  assert.equal(await pending, false);
  assert.equal(h.readers[0].isDestroyed, true);
  assert.equal(await player.flush(), true);
  assert.equal(h.readers[1].frame, 4);
  player.destroy();
});

test('destroy during asynchronous reader creation releases a late reader exactly once', async function () {
  const h = CreateHarness();
  const player = h.Player();
  h.options.createGate = Deferred();
  const pending = player.flush();
  await Tick();
  player.destroy();
  assert.equal(await pending, false);
  h.options.createGate.resolve();
  await Tick();
  assert.equal(h.readers.length, 1);
  assert.equal(h.readers[0].isDestroyed, true);
});

test('initialization failure cleans up and the next request can retry', async function () {
  const h = CreateHarness();
  const player = h.Player();
  h.options.failCreate = true;
  await assert.rejects(player.prepare(), /reader creation failed/);
  assert.equal(h.managers[0].deleted, true);
  h.options.failCreate = false;
  await player.prepare();
  assert.equal(h.readers.length, 1);
  player.destroy();
});

test('native failure restores shared state and does not poison the player queue', async function () {
  const h = CreateHarness();
  const player = h.Player();
  const previous = { marker: 'previous' };
  h.module.videoReaderManager = previous;
  h.options.failNative = true;
  await assert.rejects(player.flush(), /native failure/);
  assert.equal(h.module.videoReaderManager, previous);
  assert.equal(h.module.currentPlayer, null);
  h.options.failNative = false;
  assert.equal(await player.flush(), true);
  player.destroy();
});

test('video disabled empty and software paths create no browser readers', async function () {
  for (const mode of ['disabled', 'empty', 'software', 'solid']) {
    const h = CreateHarness();
    const player = h.Player(mode === 'empty' ? null : h.Composition(mode !== 'solid'));
    if (mode === 'disabled') player.setVideoEnabled(false);
    if (mode === 'software') h.module._useSoftwareDecoder = true;
    await player.prepare();
    assert.equal(await player.flush(), true);
    assert.equal(h.readers.length, 0);
    player.destroy();
  }
});

test('new readers inherit playback intent and pausing reaches existing readers', async function () {
  const h = CreateHarness();
  const player = h.Player();
  player.setVideoPlaying(true);
  await player.prepare();
  assert.equal(h.readers[0].isPlaying, true);
  player.setVideoPlaying(false);
  assert.equal(h.readers[0].isPlaying, false);
  player.destroy();
});

test('composition and shared frame buffer callback stay together in the native queue', async function () {
  const h = CreateHarness();
  const first = h.Player();
  const second = h.Player();
  const order = [];
  function FirstComposite() {
    assert.equal(h.module.currentPlayer, first);
    order.push('first');
  }
  function SecondComposite() {
    assert.equal(h.module.currentPlayer, second);
    order.push('second');
  }
  await Promise.all([first.flushInternal(FirstComposite), second.flushInternal(SecondComposite)]);
  assert.deepEqual(order, ['first', 'second']);
  first.destroy(); second.destroy();
});

function CreateViewHarness() {
  const bindings = { PAGModule: {} };
  const decorators = LoadSource('utils/decorators.ts', { '../pag-module': bindings });
  const { PAGView } = LoadSource('pag-view.ts', {
    './utils/event-manager': LoadSource('utils/event-manager.ts', {}), './pag-surface': {},
    './utils/decorators': decorators, './core/backend-context': {}, './pag-module': bindings,
    './core/render-canvas': {}, './utils/clock': {}, './utils/type-utils': {}, '@tgfx/utils/canvas': {},
  });
  const gate = Deferred();
  const events = [];
  let playing = false;
  let progress = 0;
  const player = {
    videoReaders: [{ isSought: true, stop() {} }],
    duration() { return 1000000; },
    setVideoPlaying(value) { playing = value; },
    setProgress(value) { progress = value; },
    getProgress() { return progress; },
    destroy() {},
  };
  const view = new PAGView(player, {});
  view.frameRate = 30;
  view.getNowTime = function now() { return 100; };
  view.setTimer = function schedule() {};
  view.clearTimer = function clear() {};
  view.flush = function render() { return gate.promise; };
  for (const event of ['onAnimationPlay', 'onAnimationPause', 'onAnimationCancel', 'onAnimationEnd']) {
    view.addListener(event, function record() { events.push(event); });
  }
  return { view, gate, events, playing() { return playing; } };
}

test('pausing during an in-flight play cannot emit a late play event', async function () {
  const h = CreateViewHarness();
  const pending = h.view.play();
  h.view.pause();
  h.gate.resolve(true);
  await pending;
  assert.equal(h.view.isPlaying, false);
  assert.equal(h.playing(), false);
  assert.deepEqual(h.events, ['onAnimationPause']);
});

test('a new play supersedes an older stop continuation', async function () {
  const h = CreateViewHarness();
  h.view.isPlaying = true;
  const stopping = h.view.stop();
  assert.equal(h.playing(), false);
  const playing = h.view.play();
  h.gate.resolve(true);
  await Promise.all([stopping, playing]);
  assert.equal(h.view.isPlaying, true);
  assert.equal(h.playing(), true);
  assert.deepEqual(h.events, ['onAnimationPlay']);
});

test('late seek completion does not overwrite a newer requested playback position', async function () {
  const h = CreateViewHarness();
  h.view.isPlaying = true;
  const pending = h.view.flushNextFrame(true);
  h.view.setProgress(0.75);
  const startTime = h.view.startTime;
  const playTime = h.view.playTime;
  h.gate.resolve(true);
  assert.equal(await pending, false);
  assert.equal(h.view.startTime, startTime);
  assert.equal(h.view.playTime, playTime);
  assert.equal(playTime, 750000);
});

test('destroying during an in-flight play suppresses all late playback events', async function () {
  const h = CreateViewHarness();
  const pending = h.view.play();
  h.view.destroy();
  h.gate.resolve(true);
  await pending;
  assert.equal(h.view.isPlaying, false);
  assert.deepEqual(h.events, []);
});
