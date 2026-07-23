// Shared "resize the viewport to the body canvas" step used by both the subset
// capture (lib/snapshot-runner.ts) and the eval ground-truth capture
// (eval/baseline.js). Keeping the decision in one place is what keeps the two
// renders in lock-step: they must resize (or decline to resize) identically, or
// the eval would diff a subset measured one way against a baseline captured the
// other way.
//
// Why resize at all: `pagx render` roots the subset at <body> (0,0) and the
// baseline is clipped to the body canvas, so any element whose containing block
// is the initial containing block — a `position: absolute` box anchored by
// `right`/`bottom` with no positioned ancestor, `position: fixed`, or a
// `vw`/`vh` length — must be measured/painted against a viewport equal to that
// canvas. At the default 1400x900 viewport such an element resolves to
// viewport-relative coordinates the body-rooted render can't reproduce.
//
// Why it is guarded: the resize is only safe when the page's rendered content
// does NOT depend on the viewport size. Fixed-size mocks
// (`body{width:1080px;height:1440px}`, the common case) are unaffected and
// benefit from it. Fluid / viewport-driven live pages misbehave:
//   - `vh` sections balloon (a 5200px-tall viewport multiplies every `100vh`
//     band, inflating the canvas), and
//   - scroll-driven reveal animations fire their `resize`/`scroll` recompute at
//     scrollY 0 and snap every below-the-fold section back to its hidden initial
//     state (`opacity:0`), leaving the body height unchanged but the capture
//     mostly blank.
// Body height alone can't tell the second case apart, so we compare a "visible
// content" fingerprint (count of painted, non-transparent boxes) plus the canvas
// height, captured before and after the resize, and revert to the settle
// viewport when the resize destroyed content or ballooned the layout. The ICB
// geometry fix is forfeited for those pages, but their content is preserved
// (the whole point of the capture), and subset + baseline stay aligned because
// both revert together.

import { setViewport, type EngineName, type Page } from './browser-engine';
import { MAX_CAPTURE_HEIGHT_PX } from './common';

export interface CanvasSize {
  width: number;
  height: number;
}

export interface CanvasViewportDecision {
  // true when the resize was reverted because the page depends on the viewport
  // size. Callers use it to keep subset + baseline symmetric: the subset falls
  // back to re-measuring the canvas at the settle viewport, and the baseline
  // clips the (unchanged) canvas rect with `captureBeyondViewport`.
  reverted: boolean;
}

// In-page fingerprint of the visible layout: the number of painted,
// non-transparent boxes bigger than 64px², plus the (uncapped) body height.
// Serialised into the page via `page.evaluate`, so it must stay a
// self-contained expression string (both engines accept that form).
const CONTENT_FINGERPRINT_EXPR = `(() => {
  var b = document.body; if (!b) return { score: 0, height: 0 };
  void b.offsetHeight;
  var els = b.getElementsByTagName('*'), n = 0;
  for (var i = 0; i < els.length; i++) {
    var el = els[i], cs = window.getComputedStyle(el);
    if (cs.visibility === 'hidden' || cs.display === 'none') continue;
    if (parseFloat(cs.opacity) === 0) continue;
    var r = el.getBoundingClientRect();
    if (r.width * r.height < 64) continue;
    n++;
  }
  return { score: n, height: Math.max(b.scrollHeight, Math.round(b.getBoundingClientRect().height)) };
})()`;

interface Fingerprint {
  score: number;
  height: number;
}

// Resize the page's viewport to `canvas`, then decide whether that resize was
// safe. If it hid content (visible box count dropped >15%) or ballooned the
// layout (body height grew >2%), revert to `settleViewport` and report it.
// Both thresholds are slack enough to ignore sub-pixel reflow jitter while
// catching real viewport-dependence failures. Best-effort: the caller wraps
// this in try/catch and degrades to the un-resized capture on error.
export async function applyCanvasViewport(
  page: Page,
  engine: EngineName,
  canvas: CanvasSize,
  settleViewport: CanvasSize,
  log?: ((msg: string) => void) | null,
): Promise<CanvasViewportDecision> {
  const before = (await page.evaluate(CONTENT_FINGERPRINT_EXPR)) as Fingerprint;
  await setViewport(page, engine, {
    width: canvas.width,
    height: canvas.height,
    deviceScaleFactor: 1,
  });
  // Let the resize's reflow settle before the geometry is read.
  await new Promise((r) => setTimeout(r, 50));
  const after = (await page.evaluate(CONTENT_FINGERPRINT_EXPR)) as Fingerprint;

  const beforeScore = before && typeof before.score === 'number' ? before.score : 0;
  const afterScore = after && typeof after.score === 'number' ? after.score : 0;
  // Clamp the post-resize height to the same cap `measureCanvas` /
  // `captureBodyRect` apply to `canvas.height`, so a page taller than
  // MAX_CAPTURE_HEIGHT_PX (whose `canvas.height` is already clamped) does not
  // read as "ballooned" purely because `scrollHeight` reports the un-clamped
  // height.
  const afterHeight = Math.min(
    MAX_CAPTURE_HEIGHT_PX,
    after && typeof after.height === 'number' ? after.height : canvas.height,
  );

  const lostContent = beforeScore > 0 && afterScore < beforeScore * 0.85;
  // Distinguish a viewport-proportional (`vh`/`dvh`) balloon from the benign
  // reflow a fixed-size stage produces when a `fit()`-style resize handler
  // rescales it to the new viewport. Resizing from the settle viewport up to the
  // canvas enlarges it by `viewportGrew` px vertically. A `vh` layout re-inflates
  // by MORE than that — each stacked viewport-height band adds ~`viewportGrew`,
  // so N bands grow the body by ~N×`viewportGrew` and the canvas runs away. A
  // fixed stage (a 1920x1080 mock scaled by `transform: scale(min(w/W, h/H))` on
  // `resize`) reflows by a bounded amount that does NOT track the viewport, so
  // its growth stays at or below the delta. Allow growth up to the viewport
  // increase (with a 2% slack floor for sub-pixel reflow jitter) before calling
  // it a balloon, so fit-to-window stages keep the resize and fill the canvas
  // instead of freezing at the smaller settle-viewport scale. This only ever
  // relaxes the old `canvas.height * 1.02 + 2` bound — a real `vh` balloon whose
  // growth outpaces the viewport still reverts.
  const viewportGrew = Math.max(0, canvas.height - settleViewport.height);
  const balloonAllowance = Math.max(viewportGrew, canvas.height * 0.02) + 2;
  const ballooned = afterHeight > canvas.height + balloonAllowance;
  if (lostContent || ballooned) {
    if (log) {
      const why = lostContent
        ? `hid content (visible boxes ${beforeScore}\u2192${afterScore})`
        : `ballooned layout (height ${canvas.height}\u2192${afterHeight}px)`;
      log(
        `viewport-relative resize skipped: canvas viewport ${why} — page depends on viewport size; capturing at ${settleViewport.width}x${settleViewport.height}`,
      );
    }
    await setViewport(page, engine, {
      width: settleViewport.width,
      height: settleViewport.height,
      deviceScaleFactor: 1,
    });
    await new Promise((r) => setTimeout(r, 50));
    return { reverted: true };
  }
  return { reverted: false };
}
