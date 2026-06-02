'use strict';

const fs = require('fs');
const fsp = require('fs').promises;
const os = require('os');
const path = require('path');
const { writeFileAtomic } = require('../lib/atomic-write');

let dir;
beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'html-snapshot-atomic-test-'));
});
afterEach(() => {
  fs.rmSync(dir, { recursive: true, force: true });
});

describe('writeFileAtomic', () => {
  test('writes the bytes to the target path and leaves no temp behind', async () => {
    const target = path.join(dir, 'out.bin');
    await writeFileAtomic(target, Buffer.from('payload'));
    expect(fs.readFileSync(target).toString()).toBe('payload');
    // No temp leftovers in the directory — only the final file.
    expect(fs.readdirSync(dir)).toEqual(['out.bin']);
  });

  test('overwrites an existing file (rename replaces atomically)', async () => {
    const target = path.join(dir, 'out.bin');
    fs.writeFileSync(target, 'OLD');
    await writeFileAtomic(target, Buffer.from('NEW'));
    expect(fs.readFileSync(target).toString()).toBe('NEW');
  });

  test('treats a "target already exists" race as success', async () => {
    // Simulate the content-addressed race: another worker won the rename and
    // the target file already exists when our rename runs. Force rename to
    // throw, but leave the target on disk so the catch path takes the
    // success branch.
    const target = path.join(dir, 'shared.bin');
    fs.writeFileSync(target, 'WINNER');

    const realRename = fsp.rename;
    fsp.rename = async () => { throw Object.assign(new Error('EEXIST'), { code: 'EEXIST' }); };
    try {
      await expect(writeFileAtomic(target, Buffer.from('LOSER'))).resolves.toBeUndefined();
    } finally {
      fsp.rename = realRename;
    }

    // Target still has the winner's bytes — our payload was discarded.
    expect(fs.readFileSync(target).toString()).toBe('WINNER');
    // Temp file we staged was cleaned up; no `.tmp-*` siblings remain.
    const stragglers = fs.readdirSync(dir).filter((n) => n.includes('.tmp-'));
    expect(stragglers).toEqual([]);
  });

  test('rethrows when rename fails and target is missing', async () => {
    const target = path.join(dir, 'nope.bin');
    const realRename = fsp.rename;
    fsp.rename = async () => { throw Object.assign(new Error('EACCES'), { code: 'EACCES' }); };
    try {
      await expect(writeFileAtomic(target, Buffer.from('x'))).rejects.toThrow(/EACCES/);
    } finally {
      fsp.rename = realRename;
    }
    expect(fs.existsSync(target)).toBe(false);
    // Even on the error path, the staged temp is removed.
    const stragglers = fs.readdirSync(dir).filter((n) => n.includes('.tmp-'));
    expect(stragglers).toEqual([]);
  });

  test('uses a per-call temp suffix so concurrent writers do not collide', async () => {
    const target = path.join(dir, 'concurrent.bin');
    // Race two writers at the same target. Both must eventually resolve; the
    // resulting file holds whichever payload won the rename, which by the
    // module contract is acceptable for a content-addressed write (callers
    // ensure both payloads are byte-identical). We only assert the writers
    // complete without throwing and that no temp leak survives.
    await Promise.all([
      writeFileAtomic(target, Buffer.from('A')),
      writeFileAtomic(target, Buffer.from('A')),
    ]);
    expect(fs.readFileSync(target).toString()).toBe('A');
    const stragglers = fs.readdirSync(dir).filter((n) => n.includes('.tmp-'));
    expect(stragglers).toEqual([]);
  });
});
