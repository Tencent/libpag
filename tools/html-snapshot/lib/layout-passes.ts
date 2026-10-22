// Copyright (C) 2026 Tencent. All rights reserved.
// @ts-nocheck

// ===== Pre-snapshot pass: prune the collapsed content of <details> =====

// A closed `<details>` paints only its first `<summary>` child. Everything
// else is hidden by the UA `::details-content` pseudo-element, and that
// hiding is invisible to the snapshot: measured inside a closed `<details>`,
// the content still reports `display: block`, `visibility: visible`,
// `opacity: 1` and `content-visibility: visible`, and
// `getBoundingClientRect()` hands back its full expanded-size rect (a
// 742x67 answer box, in the accordion that motivated this pass).
//
// The content is unpainted only because the collapsed `::details-content`
// box contributes zero height and the element's own `overflow: hidden`
// clips whatever spills past it. Nothing in the walker can test for that,
// and when the author leaves the `<details>` unclipped (`overflow: visible`,
// so a box-shadow can escape) every collapsed answer leaked into the
// snapshot as a full-size box painted across the summary rows below it.
//
// So don't test — remove. Detaching the non-summary children up front makes
// every downstream traversal skip them for free: the block walker
// (`renderChildrenInto`), the flex walker (`flexItemChildren`), the
// inline-run emitters (`emitInlineRunMarkup` and its six `emitTextSpans`
// call sites), bare text nodes, and any traversal added later. Guarding each
// traversal separately was the alternative; there are five of them today and
// a sixth would silently reintroduce the leak.
//
// Geometrically the removal is a no-op — the hidden content already
// contributes zero height, so the `<details>` box, its siblings and the
// document scroll size all measure identically before and after.
//
// Two spec details the loop encodes, both verified against Chromium:
//   - Only the *first* `summary` child is the disclosure control; a second
//     `summary` is content and is hidden along with everything else.
//   - Content *before* the summary is hidden too, so "everything except the
//     first summary" is the right cut, not "everything after it".
// A `<details>` with no summary child therefore keeps nothing. Chromium
// paints a UA-supplied "Details" label there; that string is not in the DOM
// and is not reproduced.
function dropCollapsedDetailsContent() {
  const detailsList = Array.from(document.querySelectorAll('details'));
  let detailsPruned = 0;
  let nodesRemoved = 0;
  for (const element of detailsList) {
    if (element.open) continue;
    let summary = null;
    for (const child of element.children) {
      if (child.tagName === 'SUMMARY') {
        summary = child;
        break;
      }
    }
    for (const child of Array.from(element.childNodes)) {
      if (child === summary) continue;
      element.removeChild(child);
      nodesRemoved++;
    }
    detailsPruned++;
  }
  return { details: detailsPruned, nodes: nodesRemoved };
}

// ===== Pre-snapshot pass: hoist table-row backgrounds beneath cell content =====

// In CSS a table row's painted background sits under every cell of the table:
// the paint order is all row backgrounds, then all cell backgrounds and
// content (CSS 2.1 Appendix E). The snapshot's flattened box tree has no such
// layer model — it wraps each row's background and its cells in one box
// emitted in DOM order — so a `rowspan` cell that overflows its row (the
// '华南' cell spanning three rows of the finance table in the forms sample)
// ends up beneath the *following* rows' backgrounds: '华南' vanished entirely
// and '华北' lost its lower half, while '华东' survived only because the row
// below it carried no background. Hoisting each painted row background into a
// standalone full-size box inserted ahead of every row in the same row group
// replays the browser's order — all row backgrounds first, then all cells, so
// a later row's background can no longer cover an earlier row's overflow.
//
// Only the plain `background-color` case is hoisted. A row whose sheet
// carries a background image, a border, a shadow or a radius keeps its box
// untouched: splitting such a sheet between two boxes would drop the other
// visuals. Best-effort by design — the caller treats a failure as "keep the
// old order", matching the pre-fix output.
function hoistTableRowBackgrounds() {
  // Alpha of the computed background-color, 0 when fully transparent.
  function backgroundColorAlpha(value) {
    const match = /^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([\d.]+))?\s*\)$/i.exec(
      String(value || '').trim()
    );
    if (!match) return 0;
    return match[4] === undefined ? 1 : parseFloat(match[4]);
  }

  // True when any border side paints. border-collapse tables report the
  // collapsed edge width on `tr` as well, so this doubles as the guard for
  // authored row borders.
  function hasVisibleBorder(computed) {
    for (const side of ['top', 'right', 'bottom', 'left']) {
      const style = computed.getPropertyValue('border-' + side + '-style');
      if (style === 'none' || style === 'hidden' || !style) continue;
      if (parseFloat(computed.getPropertyValue('border-' + side + '-width')) > 0) return true;
    }
    return false;
  }

  const rows = Array.from(document.querySelectorAll('tr'));
  let hoisted = 0;
  for (const row of rows) {
    const computed = getComputedStyle(row);
    if (!computed) continue;
    if ((computed.getPropertyValue('background-image') || 'none').trim() !== 'none') continue;
    const background = (computed.getPropertyValue('background-color') || '').trim();
    if (backgroundColorAlpha(background) <= 0) continue;
    if ((computed.getPropertyValue('box-shadow') || 'none').trim() !== 'none') continue;
    const hasRadius = ['border-top-left-radius', 'border-top-right-radius',
                       'border-bottom-right-radius', 'border-bottom-left-radius']
                        .some((name) => parseFloat(computed.getPropertyValue(name)) > 0);
    if (hasRadius || hasVisibleBorder(computed)) continue;
    const parent = row.parentElement;
    if (!parent) continue;
    const rowRect = row.getBoundingClientRect();
    const parentRect = parent.getBoundingClientRect();
    if (!(rowRect.width > 0) || !(rowRect.height > 0)) continue;
    // The hoisted box is absolutely positioned against the row group, which is
    // usually static in the author's CSS; `relative` costs nothing for a table
    // part and gives the box the intended containing block.
    if (getComputedStyle(parent).position === 'static') {
      parent.style.position = 'relative';
    }
    const hoist = document.createElement('div');
    hoist.setAttribute('data-pagx-row-bg', '');
    hoist.setAttribute(
      'style',
      // `z-index: -1` drops the box into the walker's negative paint band. Without it the
      // absolutely-positioned box sorts *after* the static row boxes (positioned content
      // paints in the top band), covering exactly the row content the hoist was meant to sit
      // beneath.
      'position:absolute;z-index:-1;left:' + (rowRect.left - parentRect.left) + 'px;top:' +
        (rowRect.top - parentRect.top) + 'px;width:' + rowRect.width + 'px;height:' +
        rowRect.height + 'px;background-color:' + background + ';'
    );
    parent.insertBefore(hoist, parent.firstChild);
    row.style.backgroundColor = 'transparent';
    hoisted++;
  }
  return { hoisted };
}

/* eslint-enable no-undef, no-inner-declarations */

// ===== Pre-snapshot pass: expand sticky scrollytelling blocks =====

// "Scrollytelling" pages pin a viewport-sized `position: sticky` panel inside
// a much taller scroll track and cross-fade between N stacked step layers as
// `scrollY` advances (Flect's `.method-scroll` / `.method-sticky` is the
// canonical case). A static snapshot freezes the page at scroll 0, so:
//
//   - the track keeps its full height (4545px) but only the top panel
//     (766px) has content — the rest renders as blank space, and
//   - the step layers the page's JS left at `opacity: 0` are dropped by
//     `isVisible` (or worse, half-captured by the animation sampler whose
//     state depends on the lazy-load warm-up sweep timing), losing steps 2..N
//     entirely or leaving them as invisible alpha-0 layers.
//
// This pass rewrites the live DOM before the walker runs: the sticky panel is
// cloned N times and the clones are tiled vertically down the track, one per
// step, each showing exactly one layer of every stacked group. The exported
// PAGX then reads top-to-bottom as "page 1, page 2, page 3" instead of "page
// 1, blank, blank".
//
// Detection is heuristic and deliberately conservative — no class names, no
// site-specific selectors:
//
//   1. a sticky element S whose parent track P is at least 2x S's height
//      (the scroll-into interval must exist for scrollytelling to make
//      sense), and
//   2. inside S, a group of >= 2 sibling `position: absolute` layers that
//      stack on top of each other (pairwise vertical overlap, similar size)
//      with mutually exclusive opacities (one >= 0.9, one <= 0.1) — the
//      cross-faded steps. Sticky headers, tab panels without a scroll track,
//      carousels and parallax decorations all fail one of these checks.
//
// Layer handling per segment keeps the page's own geometry: the surviving
// layer's computed `transform` is frozen inline (removing it could snap the
// layer back to an entrance-state CSS rule such as `scale(.965)`), while
// `opacity`/`filter`/`visibility` are forced visible. The track's height is
// pinned inline in case it was content-driven and would collapse once the
// sticky panel leaves the flow.
//
// Idempotent: expanded tracks carry `data-snapshot-sticky-expanded` and both
// the detection loop and this guard skip anything inside such a track (which
// also shields nested stickies inside the cloned panels).
function expandStickyScrollytelling() {
  // Self-contained on purpose: this function is shipped through
  // `page.evaluate`, so only its own body crosses the boundary (same rule as
  // materializeDecorativePseudoElements above).
  function overlapRatio(a, b) {
    const top = Math.max(a.top, b.top);
    const bottom = Math.min(a.bottom, b.bottom);
    if (bottom <= top) return 0;
    const shared = bottom - top;
    return Math.min(shared / Math.max(1, a.height), shared / Math.max(1, b.height));
  }

  function sizeSimilar(a, b) {
    const w = Math.abs(a.width - b.width) / Math.max(1, Math.max(a.width, b.width));
    const h = Math.abs(a.height - b.height) / Math.max(1, Math.max(a.height, b.height));
    return w <= 0.25 && h <= 0.25;
  }

  // Group absolutely-positioned candidates by parent, then keep the buckets
  // that look like a cross-fade step stack: >= 2 layers, pairwise stacked,
  // similar size, mutually exclusive opacities.
  function findLayerGroups(root) {
    const buckets = new Map();
    const all = root.querySelectorAll('*');
    for (const el of all) {
      const cs = getComputedStyle(el);
      if (cs.position !== 'absolute') continue;
      if (cs.display === 'none') continue;
      const rect = el.getBoundingClientRect();
      if (rect.width <= 0 || rect.height <= 0) continue;
      const parent = el.parentElement;
      if (!parent) continue;
      if (!buckets.has(parent)) buckets.set(parent, []);
      buckets.get(parent).push({ el, cs, rect });
    }
    const groups = [];
    for (const layers of buckets.values()) {
      if (layers.length < 2) continue;
      let stacked = true;
      for (let i = 0; i < layers.length && stacked; i++) {
        for (let j = i + 1; j < layers.length; j++) {
          if (overlapRatio(layers[i].rect, layers[j].rect) < 0.6 ||
              !sizeSimilar(layers[i].rect, layers[j].rect)) {
            stacked = false;
            break;
          }
        }
      }
      if (!stacked) continue;
      const ops = layers.map((l) => parseFloat(l.cs.opacity) || 0);
      if (Math.max.apply(null, ops) < 0.9) continue;
      if (Math.min.apply(null, ops) > 0.1) continue;
      groups.push(layers.map((l) => l.el));
    }
    return groups;
  }

  // Freeze one surviving layer into its visible resting state. `transform`
  // keeps the *computed* value: the layer's CSS base rule may hold an
  // entrance transform (e.g. `translateY(-50%)` centering or `scale(.965)`)
  // that inline JS overrode at runtime, and removing the inline value would
  // snap the geometry back. opacity/filter/visibility are forced so the
  // walker's `isVisible` check never prunes the layer.
  function keepLayer(el) {
    const cs = getComputedStyle(el);
    el.style.opacity = '1';
    el.style.filter = 'none';
    el.style.visibility = 'visible';
    el.style.transform = cs.transform;
    el.style.removeProperty('transition');
    el.style.removeProperty('animation');
  }

  const results = [];
  const candidates = Array.from(document.body ? document.body.querySelectorAll('*') : []);
  for (const el of candidates) {
    if (!el.parentElement) continue;
    if (el.closest('[data-snapshot-sticky-expanded]')) continue;
    let cs;
    try {
      cs = getComputedStyle(el);
    } catch (_) {
      continue;
    }
    if (cs.position !== 'sticky') continue;
    if (cs.display === 'none') continue;
    const track = el.parentElement;
    const elRect = el.getBoundingClientRect();
    const trackRect = track.getBoundingClientRect();
    if (elRect.width <= 0 || elRect.height <= 0) continue;
    if (trackRect.height < 2 * elRect.height) continue;
    // The sticky panel must be the track's only in-flow visible child:
    // switching it to absolute releases its layout slot, and any in-flow
    // sibling — including another sticky one, which keeps its own flow slot —
    // would shift up and corrupt the geometry of everything the walker
    // measures afterwards. Scrollytelling tracks are dedicated
    // height-creating wrappers, so this holds for the intended targets and
    // rejects sticky headers living inside a normal content flow.
    let hasFlowSibling = false;
    for (const sibling of Array.from(track.children)) {
      if (sibling === el) continue;
      const sc = getComputedStyle(sibling);
      if (sc.display === 'none') continue;
      if (sc.position === 'absolute' || sc.position === 'fixed') {
        continue;
      }
      if (sibling.getBoundingClientRect().height > 0) {
        hasFlowSibling = true;
        break;
      }
    }
    if (hasFlowSibling) continue;
    const groups = findLayerGroups(el);
    if (groups.length === 0) continue;

    const segments = Math.max.apply(null, groups.map((g) => g.length));
    const segmentHeight = trackRect.height / segments;
    const panelLeft = elRect.left - trackRect.left;
    const panelWidth = elRect.width;

    // Tag every layer with its group + index so the clones (fresh nodes the
    // original references don't reach) can be re-queried per segment.
    groups.forEach((layers, groupIndex) => {
      layers.forEach((layer, layerIndex) => {
        layer.setAttribute('data-snapshot-layer-group', String(groupIndex));
        layer.setAttribute('data-snapshot-layer-index', String(layerIndex));
      });
    });

    // The track must become the containing block for the absolutely
    // positioned panels.
    if (getComputedStyle(track).position === 'static') {
      track.style.position = 'relative';
    }

    const panels = [el];
    for (let i = 1; i < segments; i++) {
      panels.push(el.cloneNode(true));
    }
    panels.forEach((panel, i) => {
      if (i > 0) track.appendChild(panel);
      panel.style.position = 'absolute';
      panel.style.top = (i * segmentHeight + (segmentHeight - elRect.height) / 2) + 'px';
      panel.style.left = panelLeft + 'px';
      // `panelWidth` is a measured border-box width, so pin the sizing model before writing it:
      // a content-box panel would otherwise grow by its own padding and border, shifting every
      // measured child (the same hazard the track-height pin below handles).
      panel.style.boxSizing = 'border-box';
      panel.style.width = panelWidth + 'px';
      panel.style.removeProperty('right');
      panel.style.removeProperty('bottom');
      panel.style.removeProperty('margin-top');
      panel.style.removeProperty('margin-bottom');
      panel.style.removeProperty('margin-left');
      panel.style.removeProperty('margin-right');
      groups.forEach((layers, groupIndex) => {
        const inPanel = panel.querySelectorAll(
          '[data-snapshot-layer-group="' + groupIndex + '"]');
        if (inPanel.length === 0) return;
        const keepIndex = Math.min(i, inPanel.length - 1);
        for (let li = 0; li < inPanel.length; li++) {
          if (li === keepIndex) keepLayer(inPanel[li]);
          else inPanel[li].remove();
        }
      });
    });

    // Only pin the height when the panel leaving the flow actually collapsed
    // the track (a content-driven height). Writing getBoundingClientRect's
    // border-box height into `style.height` unconditionally would inflate a
    // `box-sizing: content-box` track by its padding and border.
    if (Math.abs(track.getBoundingClientRect().height - trackRect.height) > 1) {
      track.style.boxSizing = 'border-box';
      track.style.height = trackRect.height + 'px';
    }

    track.setAttribute('data-snapshot-sticky-expanded', String(segments));
    results.push({ segments });
  }
  return { blocks: results.length, expanded: results };
}

export { dropCollapsedDetailsContent, hoistTableRowBackgrounds, expandStickyScrollytelling };
