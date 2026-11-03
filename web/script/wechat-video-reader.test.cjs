// Copyright (C) 2026 Tencent. All rights reserved.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('typescript');
const { Deferred: CreateDeferred, DrainMicrotasks, CreateTimers } = require('./test-support/harness.cjs');

const SourcePath = path.join(__dirname, '../src/wechat/video-reader.ts');
const Source = ts.transpileModule(fs.readFileSync(SourcePath, 'utf8'), {
  compilerOptions: { target: ts.ScriptTarget.ES2020, module: ts.ModuleKind.CommonJS },
}).outputText;

function CreateHarness(context, options = {}) {
  const clock = CreateTimers();
  const timers = clock.timers;
  const files = new Set();
  const decoders = [];
  const readers = [];
  const owner = {
    isDestroyed: false,
    playing: options.playing ?? true,
    linked: new Set(),
    linkCalls: 0,
    unlinkCalls: 0,
    linkVideoReader(reader) {
      this.linkCalls += 1;
      this.linked.add(reader);
    },
    unlinkVideoReader(reader) {
      this.unlinkCalls += 1;
      this.linked.delete(reader);
    },
    isVideoPlaying() {
      return this.playing;
    },
  };

  class ArrayBufferImage {
    constructor() {
      this.frames = [];
    }
    setFrameData(frame) {
      this.frames.push(frame);
    }
  }

  function CreateDecoder() {
    if (options.createError) throw new Error('Decoder creation failed.');
    const decoder = {
      startTask: CreateDeferred(),
      seekTasks: [],
      frames: [],
      handlers: new Map(),
      removeCalls: 0,
      activeSeeks: 0,
      maxConcurrentSeeks: 0,
      readError: null,
      start(startOptions) {
        assert.equal(startOptions.mode, 1);
        assert.ok(files.has(startOptions.source));
        if (options.startError) throw new Error('Decoder start failed.');
        return this.startTask.promise;
      },
      seek(position) {
        this.activeSeeks += 1;
        this.maxConcurrentSeeks = Math.max(this.maxConcurrentSeeks, this.activeSeeks);
        const task = CreateDeferred();
        this.seekTasks.push({ position, task });
        return task.promise.then(
          function complete() {
            decoder.activeSeeks -= 1;
          },
          function fail(error) {
            decoder.activeSeeks -= 1;
            throw error;
          },
        );
      },
      getFrameData() {
        if (this.readError) throw this.readError;
        return this.frames.shift() || null;
      },
      remove() {
        this.removeCalls += 1;
        if (options.removeError) throw new Error('Decoder removal failed.');
        return Promise.resolve();
      },
      on(name, callback) {
        this.handlers.set(name, callback);
      },
      off(name, callback) {
        assert.equal(this.handlers.get(name), callback);
        this.handlers.delete(name);
        if (options.offError) throw new Error('Decoder listener cleanup failed.');
      },
    };
    decoders.push(decoder);
    return decoder;
  }

  function MockRequire(name) {
    if (name === './constant') return { MP4_CACHE_PATH: '/mock/' };
    if (name === '@tgfx/wechat/array-buffer-image') return { ArrayBufferImage };
    if (name === './file-utils') {
      return {
        touchDirectory() {},
        writeFile(filePath) {
          files.add(filePath);
          if (options.writeError) throw new Error('MP4 write failed.');
        },
        removeFile(filePath) {
          files.delete(filePath);
        },
      };
    }
    throw new Error(`Unexpected dependency: ${name}`);
  }

  const exports = {};
  vm.runInNewContext(
    Source,
    {
      exports,
      require: MockRequire,
      ArrayBuffer,
      Uint8Array,
      Date,
      Error,
      Promise,
      wx: { createVideoDecoder: CreateDecoder },
      setInterval: clock.setInterval,
      clearInterval: clock.clearInterval,
      setTimeout: clock.setTimeout,
      clearTimeout: clock.clearTimeout,
    },
    { filename: SourcePath },
  );

  context.after(function cleanup() {
    for (const reader of readers) reader.onDestroy();
    assert.equal(timers.size, 0);
    assert.equal(files.size, 0);
    assert.equal(owner.linked.size, 0);
  });

  return {
    Reader: exports.VideoReader,
    owner,
    timers,
    files,
    decoders,
    readers,
    createReader() {
      const reader = new exports.VideoReader(new Uint8Array([1]), 1, 1, 30, [], owner);
      readers.push(reader);
      return reader;
    },
    tick(kind) {
      for (const [id, timer] of [...timers]) {
        if (timer.kind !== kind || !timers.has(id)) continue;
        if (kind === 'timeout') {
          assert.equal(timer.delay, 10000);
          timers.delete(id);
        } else {
          assert.equal(timer.delay, 2);
        }
        timer.callback();
      }
    },
    frame(decoder = decoders.at(-1)) {
      const frame = { data: new Uint8Array([1, 2, 3, 4]).buffer, width: 1, height: 1, pkDts: 0, pkPts: 0 };
      decoder.frames.push(frame);
      return frame;
    },
  };
}

async function StartDecoder(harness) {
  harness.decoders.at(-1).startTask.resolve();
  await DrainMicrotasks();
}

async function PrepareSuccessfully(harness, reader, targetFrame) {
  const preparation = reader.prepare(targetFrame, 1);
  await DrainMicrotasks();
  const decoder = harness.decoders.at(-1);
  if (!decoder.startTask.settled) await StartDecoder(harness);
  if (decoder.activeSeeks) {
    decoder.seekTasks.at(-1).task.resolve();
    await DrainMicrotasks();
  }
  harness.frame(decoder);
  harness.tick('interval');
  assert.equal(await preparation, true);
  assert.equal(reader.getCurrentFrame(), targetFrame);
}

async function BeginWait(harness, stage) {
  const reader = harness.createReader();
  if (stage !== 'start') await StartDecoder(harness);
  const targetFrame = stage === 'seek' ? 63 : 0;
  const preparation = reader.prepare(targetFrame, 1);
  await DrainMicrotasks();
  return { reader, preparation, targetFrame, decoder: harness.decoders.at(-1) };
}

async function FinishNativeWait(decoder, stage) {
  if (stage === 'start') decoder.startTask.resolve();
  if (stage === 'seek') decoder.seekTasks.at(-1).task.resolve();
  await DrainMicrotasks();
}

test('WeChat factory links the explicit owner and prepares an arbitrary first target', async function (context) {
  const harness = CreateHarness(context, { playing: false });
  const reader = await harness.Reader.create(new Uint8Array([1]), 1, 1, 30, [], harness.owner);
  harness.readers.push(reader);
  assert.equal(reader.isPlaying, false);
  assert.equal(harness.owner.linkCalls, 1);
  assert.ok(harness.owner.linked.has(reader));
  harness.owner.playing = true;
  await PrepareSuccessfully(harness, reader, 63);
  assert.equal(reader.isPlaying, true);
  assert.equal(harness.decoders[0].seekTasks[0].position, 2100);
  assert.deepEqual(new Uint8Array(reader.getVideo().frames[0].data), new Uint8Array([1, 2, 3, 4]));
  reader.onDestroy();
  reader.onDestroy();
  assert.equal(harness.owner.unlinkCalls, 1);
  assert.equal(harness.decoders[0].removeCalls, 1);
  assert.equal(await reader.prepare(0, 1), false);
});

for (const stage of ['start', 'seek', 'getFrameData']) {
  test(`WeChat cancelPrepare settles ${stage} and allows retry`, async function (context) {
    const harness = CreateHarness(context);
    const { reader, preparation, targetFrame, decoder } = await BeginWait(harness, stage);
    reader.cancelPrepare();
    assert.equal(await preparation, false);
    assert.equal(harness.timers.size, 0);
    await FinishNativeWait(decoder, stage);
    assert.equal(harness.timers.size, 0);
    assert.equal(reader.getCurrentFrame(), -1);
    assert.equal(reader.getVideo().frames.length, 0);
    await PrepareSuccessfully(harness, reader, targetFrame);
  });

  test(`WeChat ${stage} timeout is bounded and allows retry after native completion`, async function (context) {
    const harness = CreateHarness(context);
    const { reader, preparation, targetFrame, decoder } = await BeginWait(harness, stage);
    harness.tick('timeout');
    assert.equal(await preparation, false);
    assert.match(String(reader.getError()), /timed out/);
    assert.equal(harness.timers.size, 0);
    await FinishNativeWait(decoder, stage);
    assert.equal(reader.getCurrentFrame(), -1);
    await PrepareSuccessfully(harness, reader, targetFrame);
    assert.equal(reader.getError(), null);
  });

  test(`WeChat destroy settles ${stage} exactly once and ignores late completion`, async function (context) {
    const harness = CreateHarness(context);
    const { reader, preparation, decoder } = await BeginWait(harness, stage);
    reader.onDestroy();
    reader.onDestroy();
    assert.equal(await preparation, false);
    await FinishNativeWait(decoder, stage);
    assert.equal(reader.getCurrentFrame(), -1);
    assert.equal(reader.getVideo().frames.length, 0);
    assert.equal(harness.timers.size, 0);
    assert.equal(harness.files.size, 0);
    assert.equal(harness.owner.unlinkCalls, 1);
    assert.equal(decoder.removeCalls, 1);
    assert.equal(decoder.handlers.size, 0);
  });
}

test('WeChat replacement prepare and ended wait for the original native seek', async function (context) {
  const harness = CreateHarness(context);
  const { reader, preparation, decoder } = await BeginWait(harness, 'seek');
  const replacement = reader.prepare(90, 1);
  assert.equal(await preparation, false);
  await DrainMicrotasks();
  decoder.handlers.get('ended')();
  assert.equal(decoder.seekTasks.length, 1);
  decoder.seekTasks[0].task.resolve();
  await DrainMicrotasks();
  assert.equal(decoder.seekTasks.length, 2);
  assert.equal(decoder.seekTasks[1].position, 3000);
  assert.equal(reader.getCurrentFrame(), -1);
  decoder.seekTasks[1].task.resolve();
  await DrainMicrotasks();
  harness.frame();
  harness.tick('interval');
  assert.equal(await replacement, true);
  assert.equal(reader.getCurrentFrame(), 90);
  assert.equal(decoder.maxConcurrentSeeks, 1);
});

test('WeChat repeated timeouts cannot bypass an unresolved native seek', async function (context) {
  const harness = CreateHarness(context);
  const { reader, preparation, decoder } = await BeginWait(harness, 'seek');
  harness.tick('timeout');
  assert.equal(await preparation, false);
  const replacement = reader.prepare(90, 1);
  await DrainMicrotasks();
  assert.equal(decoder.seekTasks.length, 1);
  harness.tick('timeout');
  assert.equal(await replacement, false);
  assert.equal(decoder.maxConcurrentSeeks, 1);
  decoder.seekTasks[0].task.resolve();
  await DrainMicrotasks();
  await PrepareSuccessfully(harness, reader, 90);
  assert.equal(decoder.maxConcurrentSeeks, 1);
});

test('WeChat cancellation after frame delivery cannot commit the frame', async function (context) {
  const harness = CreateHarness(context);
  const { reader, preparation } = await BeginWait(harness, 'getFrameData');
  harness.frame();
  harness.tick('interval');
  reader.cancelPrepare();
  assert.equal(await preparation, false);
  assert.equal(reader.getCurrentFrame(), -1);
  assert.equal(reader.getVideo().frames.length, 0);
});

test('WeChat start rejection removes the failed decoder and retries with a new decoder', async function (context) {
  const harness = CreateHarness(context);
  const { reader, preparation, decoder } = await BeginWait(harness, 'start');
  decoder.startTask.reject(new Error('Native start failed.'));
  assert.equal(await preparation, false);
  assert.equal(decoder.removeCalls, 1);
  assert.equal(reader.getCurrentFrame(), -1);
  await PrepareSuccessfully(harness, reader, 0);
  assert.equal(harness.decoders.length, 2);
  assert.equal(reader.getError(), null);
});

test('WeChat seek rejection is not ready and can be retried', async function (context) {
  const harness = CreateHarness(context);
  const { reader, preparation, decoder, targetFrame } = await BeginWait(harness, 'seek');
  decoder.seekTasks[0].task.reject(new Error('Native seek failed.'));
  assert.equal(await preparation, false);
  assert.equal(reader.getCurrentFrame(), -1);
  assert.ok(reader.getError());
  await PrepareSuccessfully(harness, reader, targetFrame);
  assert.equal(decoder.seekTasks.length, 2);
  assert.equal(decoder.maxConcurrentSeeks, 1);
});

test('WeChat frame read exceptions settle preparation and allow retry', async function (context) {
  const harness = CreateHarness(context);
  const { reader, preparation, decoder } = await BeginWait(harness, 'getFrameData');
  decoder.readError = new Error('Frame read failed.');
  harness.tick('interval');
  assert.equal(await preparation, false);
  assert.equal(reader.getCurrentFrame(), -1);
  assert.equal(harness.timers.size, 0);
  decoder.readError = null;
  await PrepareSuccessfully(harness, reader, 0);
});

test('WeChat partial initialization failures clean up owned resources', function (context) {
  for (const options of [
    { writeError: true },
    { createError: true },
    { startError: true },
    { startError: true, offError: true, removeError: true },
  ]) {
    const harness = CreateHarness(context, options);
    assert.throws(function construct() {
      harness.createReader();
    });
    assert.equal(harness.files.size, 0);
    assert.equal(harness.owner.linked.size, 0);
    if (harness.decoders.length) assert.equal(harness.decoders[0].removeCalls, 1);
  }
});

test('WeChat rejects a destroyed owner without allocating resources', function (context) {
  const harness = CreateHarness(context);
  harness.owner.isDestroyed = true;
  assert.throws(function construct() {
    harness.createReader();
  }, /owner is destroyed/);
  assert.equal(harness.decoders.length, 0);
  assert.equal(harness.owner.linkCalls, 0);
});

test('WeChat preserves prefetch across prepares and the six frame cache ceiling', async function (context) {
  const harness = CreateHarness(context);
  const reader = harness.createReader();
  await PrepareSuccessfully(harness, reader, 0);
  const loop = reader.getFrameDataLoopTimer;
  assert.ok(loop);
  assert.equal(await reader.prepare(0, 1), true);
  assert.equal(reader.getFrameDataLoopTimer, loop);
  harness.frame();
  harness.tick('interval');
  assert.equal(await reader.prepare(1, 1), true);
  assert.equal(reader.getFrameDataLoopTimer, loop);
  for (let index = 0; index < 8; index++) harness.frame();
  for (let index = 0; index < 9; index++) harness.tick('interval');
  assert.equal(reader.frameDataBuffers.length, 6);
  assert.equal(harness.timers.size, 0);
  assert.equal(await reader.prepare(4, 1), true);
  assert.equal(harness.decoders[0].seekTasks.length, 0);
});
