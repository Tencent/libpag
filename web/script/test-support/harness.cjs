// Copyright (C) 2026 Tencent.
// Shared primitives for the Node test harnesses in web/script: manual promises, event-loop
// draining, and a fake timer registry. Domain mocks (video decoders, servers, players) stay in
// each test file so their assertions can evolve independently.

// A manually settled promise. The settled flag lets tests distinguish "still pending" from
// resolved/rejected without racing the promise itself.
function Deferred() {
  let resolvePromise;
  let rejectPromise;
  const promise = new Promise(function wait(resolve, reject) {
    resolvePromise = resolve;
    rejectPromise = reject;
  });
  return {
    promise,
    settled: false,
    resolve(value) {
      this.settled = true;
      resolvePromise(value);
    },
    reject(error) {
      this.settled = true;
      rejectPromise(error);
    },
  };
}

// Waits one macrotask so every already-scheduled microtask (promise chains, queue exec) settles.
function Tick() {
  return new Promise(setImmediate);
}

// Drains several microtask rounds for chains that re-schedule themselves.
async function DrainMicrotasks(rounds = 12) {
  for (let index = 0; index < rounds; index++) await Promise.resolve();
}

// A fake timer registry for vm-injected setTimeout/setInterval. The returned timers map is
// owned by the caller, which decides how to fire entries (by delay, by kind, or directly).
function CreateTimers() {
  const timers = new Map();
  let nextID = 0;
  function add(callback, delay, kind) {
    timers.set(++nextID, { callback, delay, kind });
    return nextID;
  }
  return {
    timers,
    add,
    setTimeout(callback, delay) {
      return add(callback, delay, 'timeout');
    },
    clearTimeout(id) {
      timers.delete(id);
    },
    setInterval(callback, delay) {
      return add(callback, delay, 'interval');
    },
    clearInterval(id) {
      timers.delete(id);
    },
  };
}

module.exports = { Deferred, Tick, DrainMicrotasks, CreateTimers };
