# Authoring HTML that converts cleanly to PAGX

Read this before writing the design HTML in Step 2. The converter renders the page in a real
headless browser and converts supported layout, paint, text and images into PAGX. Review the
render and warnings for unsupported behavior. These are guidelines for the
**source design**, not the strict importer subset (the browser snapshot handles the translation);
the authoritative subset reference is `spec/html_subset.md`.

## TL;DR checklist

- [ ] Resources are available through inline data, valid local paths or reachable URLs.
- [ ] For a fixed-size design, `body` has the target pixel dimensions and `margin:0`; match the capture viewport.
- [ ] Intended font faces and weights are loaded; use `--embed-fonts` for portable typography.
- [ ] Use SVG or icon fonts when an icon needs a predictable shape.
- [ ] Real, selectable text for all copy (not text baked into images).
- [ ] Choose ordinary capture, animation capture or a scenario to match the requested result.
- [ ] Requested motion is attached at capture time, uses `--capture-animations`, and stays inside supported channels — `opacity`, `transform`
      (translate/scale/rotate/skew), `color` / `background-color`, `filter` (drop-shadow/blur),
      `clip-path` (see §8 + `references/animation.md`).

---

## 1. Canvas size

For a fixed-size poster/card, set the `body` box to the target canvas:

```html
<body style="margin: 0; width: 1080px; height: 1440px;">
```

- Match the converter's `--viewport-width` (and `--viewport-height` for tall designs) to these
  numbers so the page is not clipped or letterboxed.
- Design at 1× (real pixels). For crisp previews, render the PNG at a higher scale instead of
  inflating the canvas.
- For an existing responsive application, preserve its layout and choose the intended viewport.
  Scenario capture sets its viewport and capture extent in the scenario JSON.

## 2. Make the file self-contained

Self-contained HTML is convenient for a new design, but existing local assets are supported.
Local files are opened as `file:` URLs and ordinary relative resources resolve against the HTML
location. Use a local HTTP URL for apps that require an HTTP origin (for example module/fetch
behavior blocked on `file:`). Keep assets available until conversion finishes.

- **Images**: use data URIs, local relative paths or reachable URLs. Final `--images external`
  output has an `.assets/` directory; `--images embed` stores the bytes in the PAGX.
- **CSS**: put it in a `<style>` block or inline `style=""` attributes. A Tailwind CDN `<script>`
  works because the page runs in a browser, but plain inline CSS is the most predictable.
- **Fonts**: load the intended faces/weights with local `@font-face`, a stylesheet or an available
  system font. `pagx import --render` acquires web fonts for the preview; `--embed-fonts` embeds
  shaped glyphs so the PAGX can reproduce the typography on other hosts. `--download-fonts` also
  retains the acquired font files. See [Fonts and images](pipeline.md#fonts-and-images).

## 3. Layout

Use ordinary CSS. The browser computes positions and paint order; the importer infers flex only
where it can preserve the captured result:

- Prefer **flexbox** (`display:flex`, `flex-direction`, `gap`, `align-items`, `justify-content`,
  `flex: N`) and `padding` for spacing. These map most directly to PAGX containers.
- `position: absolute` with `left/top/right/bottom` is fine for overlays and precise placement.
- Rounded corners (`border-radius`), shadows (`box-shadow`), opacity, gradients
  (`linear-/radial-/conic-gradient`), and `transform` all convert.
- Avoid CSS Grid where flexbox suffices; grid is not modeled and is approximated from the rendered
  positions only.
- Layered backgrounds, per-side border styles, outlines, text shadows and supported backdrop
  effects are handled by the browser/import pipeline. Inspect complex combinations rather than
  replacing them preemptively. Source-browser support does not imply that the same CSS can be
  passed directly to `--html-source snapshot`.

## 4. Text

- Write real text in `<h1>`–`<h6>`, `<p>`, `<span>`. It becomes editable PAGX text.
- Set `font-family`, `font-size`, `font-weight`, `color`, `line-height`, `text-align`,
  `letter-spacing` as normal.
- For gradient-filled text use `background: linear-gradient(...)` + `-webkit-background-clip: text`
  + `color: transparent`; this converts to a gradient text fill.
- Do not render text as part of an image — it loses selectability and crispness.

## 5. Icons and illustrations

- **Inline `<svg>`** is the best icon source: it is preserved as vector PAGX. Paste the icon's SVG
  markup directly.
- **Icon webfonts** (Phosphor, Material Icons, Font Awesome, Lucide, Remix, Tabler) used via
  `::before { content: "\eXXX" }` are automatically converted to inline `<svg>` during conversion,
  so the result does not depend on the icon font being installed.
- Prefer SVG for graphical icons whose shape must stay consistent across fonts. Keep intentional
  text and symbols as text when their typography is part of the design.

## 6. Images and photos

- `<img>` with `width`/`height` becomes an image fill. Set `object-fit` to control scaling:
  `cover` → zoom-to-fill, `contain` → letterbox, `fill` → stretch, `none` → natural size,
  `scale-down` → the smaller of natural size and contain. Use `object-position` for alignment.
- The rounded-avatar pattern (an `<img>` inside a `border-radius` + `overflow:hidden` wrapper)
  converts to a clipped rounded image.

## 7. Charts

- Chart libraries that draw on `<canvas>` (ECharts, Chart.js, D3-canvas) are captured as a static
  bitmap, so charts survive as a picture. If the chart animates in, give the page enough settle time
  by raising the converter's `--wait-ms` option.
- SVG-based charts (many D3 setups) convert as vectors, which is sharper — prefer them when
  available.

## 8. Animation

With `--capture-animations`, supported CSS, GSAP, anime.js and Web Animations API motion can
become a PAGX timeline. Without that flag the output is static. Keep motion
faithful by animating the channels the runtime can play — `opacity`, `transform` (translate, scale,
rotate, skew), `color` / `background-color`, `filter: drop-shadow(...)` / `blur(...)`, and
`clip-path` reveals; inline SVG shapes also animate `fill` / `stroke` and the `stroke-dashoffset`
line-draw — and by keeping the animation attached at capture time. Use `infinite` for requested
looping motion and preserve one-shot timing when requested. Layout/size changes
(`width`, `height`, `margin`, …) and 3D transforms (`matrix3d`/`rotate3d`/`perspective`) are not
animatable and convert to a static element. Full rules, playable-channel table, preview
instructions, and warnings live in `references/animation.md` — read it before authoring any
animation.

## Capture boundaries

Choose capture behavior based on the required appearance and editability:

- **Visibility and readiness:** static capture records the chosen state. Use explicit scenario
  actions and `waitFor` conditions for content revealed by a click or completed async operation.
- **Form controls:** supported controls preserve their current appearance/value as static content.
  Keep meaningful source controls; use a scenario for filled, selected or focused states, and
  inspect native-control styling in the result. The PAGX does not retain HTML event handlers.
- **Scrolling:** use scenario `scroll` steps for a specific view. Document capture includes only
  materialized content; it does not automatically load the rest of an infinite feed.
- **Embedded/browser-only content:** media, iframes, shadow DOM and native top-layer UI have
  limits. For scenario capture, unsupported content fails by default; an explicit viewport raster
  fallback is available when a bitmap is acceptable. See [Interaction capture](interaction.md#capture-boundaries).
- **Canvas:** supported readable canvas pixels become a bitmap. Tainted or unavailable WebGL
  pixels may fail or be missing; use readable resources or a suitable image/SVG alternative.
- **Paint and motion:** 3D composition and animated changes to stacking order are limited. Review
  `paint:*` warnings; absolute children in paint-planned containers intentionally retain their order.

## Minimal template

```html
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8" />
    <link
      rel="stylesheet"
      href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap"
    />
    <style>
      * { box-sizing: border-box; }
    </style>
  </head>
  <body style="margin: 0; width: 640px; height: 400px; font-family: Inter, Arial, sans-serif;">
    <div style="width: 100%; height: 100%; display: flex; flex-direction: column; gap: 16px;
                padding: 32px; background: linear-gradient(135deg, #6366F1, #8B5CF6); color: #fff;">
      <h1 style="margin: 0; font-size: 40px; font-weight: 800;">Headline</h1>
      <p style="margin: 0; font-size: 18px; line-height: 1.5; opacity: 0.92;">
        Supporting copy that explains the offer in one or two short sentences.
      </p>
      <div style="margin-top: auto; align-self: flex-start; padding: 12px 24px; border-radius: 999px;
                  background: #fff; color: #6366F1; font-weight: 600; font-size: 16px;">
        Call to action
      </div>
    </div>
  </body>
</html>
```

Convert with the selected `pagx` binary, matching the viewport and embedding fonts for portability:

```bash
pagx import page.html -o page.pagx --viewport-width 640 --viewport-height 400 \
  --embed-fonts --render
```
