import assert from 'node:assert/strict';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { buildSync } from 'esbuild';

globalThis.wx = {
  getAppBaseInfo: () => ({ SDKVersion: '2.19.0' }),
  getSystemInfoSync: () => ({
    SDKVersion: '2.19.0',
    platform: 'android',
    benchmarkLevel: 30,
    brand: 'test',
    model: 'test',
  }),
};

const tempDir = await mkdtemp(join(tmpdir(), 'pagx-check-glass-risk-'));
try {
  const bundlePath = join(tempDir, 'pagx-check.mjs');
  buildSync({
    bundle: true,
    entryPoints: ['ts/pagx-check.ts'],
    format: 'esm',
    outfile: bundlePath,
    platform: 'node',
  });

  const { CheckPagx } = await import(pathToFileURL(bundlePath).href);
  const encoder = new TextEncoder();
  const check = (style) => CheckPagx(encoder.encode(`
    <pagx width="1000" height="1000">
      <Layer>
        <Rectangle size="1000,1000"/>
        <Fill color="#FFFFFF"/>
        ${style}
      </Layer>
    </pagx>
  `));

  const backgroundBlur = await check('<BackgroundBlurStyle blurX="50" blurY="50"/>');
  const glassFrost100 = await check('<GlassStyle frost="100"/>');
  const glassFrost50 = await check('<GlassStyle frost="50"/>');
  const glassDefaultFrost = await check('<GlassStyle/>');
  const glassFrost5 = await check('<GlassStyle frost="5"/>');

  assert.equal(glassFrost100.score, backgroundBlur.score);
  assert.ok(glassFrost50.score > glassFrost100.score);
  assert.equal(glassDefaultFrost.score, glassFrost5.score);
} finally {
  await rm(tempDir, { force: true, recursive: true });
}
