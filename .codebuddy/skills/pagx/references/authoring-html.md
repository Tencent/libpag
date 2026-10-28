# Authoring HTML that converts cleanly to PAGX

Read this before designing HTML for PAGX. The browser executes the source page before the
native importer consumes its prepared subset. Modern layout and scripting can work through
that preprocessing, but arbitrary CSS, browser-native widgets and resources are not guaranteed
to convert faithfully. The native input contract is [HTML subset](../spec/html_subset.md).

## Authoring checklist

- [ ] Choose static, playable animation, or interaction-state output before conversion.
- [ ] Give a new fixed-size design explicit `body` dimensions and `margin:0`; match the viewport.
- [ ] Keep resources available to the browser and choose how required fonts/images will be delivered.
- [ ] Use real text for copy and SVG/icon assets where vector icons are needed.
- [ ] For motion, use supported channels and explicitly enable `--capture-animations`.
- [ ] For a state reached through interaction, write a scenario with explicit readiness checks.

---

## 1. Canvas size

For a new fixed-size design, declare the `body` dimensions that determine the PAGX canvas.
For an existing application, select the intended viewport/state without unnecessarily changing
its layout:

```html
<body style="margin: 0; width: 1080px; height: 1440px;">
```

- Match `pagx capture --viewport-width/--viewport-height` to those dimensions. In scenario
  mode set `viewport` inside the scenario instead; those CLI viewport options are rejected.
- Design at 1× (real pixels). For crisp previews, render the PNG at a higher scale instead of
  inflating the canvas.

## 2. Resources

A self-contained HTML file is convenient for a new design. Existing local HTML is loaded through
a `file:` URL, so relative assets can resolve against that file; file-origin/CORS restrictions
and missing resources can still break loading. Use a local HTTP URL for apps that require an
HTTP origin. Preserve working source assets rather than rewriting them without need.

- **Images**: inline as data URIs or use reachable files/URLs. Check downloaded or external
  resources when delivering the PAGX; see [Pipeline resources](pipeline.md#resources).
- **CSS**: inline styles or a `<style>` block make a new design easier to reproduce. Existing
  linked stylesheets and runtime CSS may also work through the browser pass.
- **Fonts**: load the intended fonts before capture. A browser font is not automatically available
  to native rendering. `pagx capture --embed-fonts` downloads and embeds the captured fonts when
  possible; inspect typography and warnings. For existing PAGX font handling use `pagx embed`
  with the correct document fonts/fallbacks, not the removed `pagx font embed` command.

## 3. Layout

Use ordinary CSS. The browser measures layout; ordinary snapshots can recover flex from absolute
boxes. Interaction-state snapshots retain their measured positions without flex inference:

- Prefer **flexbox** (`display:flex`, `flex-direction`, `gap`, `align-items`, `justify-content`,
  `flex: N`) and `padding` for spacing. These map most directly to PAGX containers.
- `position: absolute` with `left/top/right/bottom` is fine for overlays and precise placement.
- Rounded corners (`border-radius`), shadows (`box-shadow`), opacity, gradients
  (`linear-/radial-/conic-gradient`), and supported transforms can convert. Inspect complex
  effects for approximations or rasterization.
- Avoid CSS Grid where flexbox suffices; grid is not modeled and is approximated from the rendered
  positions only.

## 4. Text

- Write real text in `<h1>`–`<h6>`, `<p>`, `<span>` so the converter can retain text structure.
  Check font handling and any embedding/rasterization before promising further text editing.
- Set `font-family`, `font-size`, `font-weight`, `color`, `line-height`, `text-align`,
  `letter-spacing` as normal.
- For gradient-filled text use `background: linear-gradient(...)` + `-webkit-background-clip: text`
  + `color: transparent`; this converts to a gradient text fill.
- Do not render text as part of an image — it loses selectability and crispness.

## 5. Icons and illustrations

- **Inline `<svg>`** is preferred for vector icons: supported geometry resolves into PAGX nodes.
  Inspect complex SVG effects for approximations or rasterization.
- **Icon webfonts** (Phosphor, Material Icons, Font Awesome, Lucide, Remix, Tabler) used via
  `::before { content: "\eXXX" }` are automatically converted to inline `<svg>` during conversion,
  so the result does not depend on the icon font being installed.
- Use SVG/icon assets when consistent icon geometry matters. Characters such as `+`, `×` or `→`
  remain font-dependent text; retain them when they are intentionally part of the typography.

## 6. Images and photos

- `<img>` with `width`/`height` becomes an image fill. Set `object-fit` to control scaling:
  `cover` → zoom-to-fill, `contain` → letterbox, `fill` → stretch.
- The rounded-avatar pattern (an `<img>` inside a `border-radius` + `overflow:hidden` wrapper)
  converts to a clipped rounded image.

## 7. Charts

- Chart libraries that draw on `<canvas>` (ECharts, Chart.js, D3-canvas) are captured as a static
  bitmap, so charts survive as a picture. If the chart animates in, give the page enough settle time
  with ordinary capture's `--wait-ms`/`--selector`, or a scenario `waitFor` condition.
- SVG-based charts (many D3 setups) convert as vectors, which is sharper — prefer them when
  available.

## 8. Animation and interaction states

Ordinary capture is static unless `--capture-animations` is supplied. For a playable timeline,
use supported CSS `@keyframes`, WAAPI, GSAP or anime.js motion that is still attached at capture
time. [Animation](animation.md) defines supported channels and playback checks; read it before
writing motion. Do not assume all CSS properties, canvas redraws or app events become animation.

For a dropdown, selected tab, filled form, hover state or scrolled view, use
[Interaction capture](interaction.md). Write actual controls and the needed source behavior,
then drive them with a scenario. Checkpoints record their visible state; they do not preserve
JavaScript handlers in the PAGX. Keep essential content visible at the selected checkpoint.

## Conversion boundaries

- **Forms and controls**: form containers and supported text-bearing controls retain their painted
  content; labels, current values, placeholders and selected options can become static text.
  Browser-native non-text inputs have additional limitations. Do not replace working controls
  solely to avoid interaction capture.
- **Scrolling**: ordinary capture does not replay arbitrary actions. A scenario can explicitly
  scroll the page or a container before capture. Document extent captures materialized content,
  not content that has never been loaded.
- **Hidden content**: a state capture records what is currently visible. Reveal content through
  the intended source action when it belongs in the output; this is distinct from whether PAGX
  itself can represent visibility or opacity.
- **Unsupported browser surfaces**: ordinary snapshots may omit native media, dialogs or frames.
  Structured interaction capture reports unsupported surfaces; an explicit viewport raster
  fallback is available when a bitmap meets the task. See the interaction reference for limits.
- **Canvas and complex effects**: unreadable/tainted canvas, WebGL surfaces and unsupported CSS
  may be missing or approximated. Inspect the result; use a suitable image only when that matches
  the requested editability and appearance.

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

Use the selected CLI build and a viewport matching the design:

```bash
pagx capture page.html -o out --viewport-width 640 --viewport-height 400 --embed-fonts
```

A simple `pagx import --input page.html --output page.pagx` is sufficient when default viewport
and locally available fonts meet the task. See [Pipeline](pipeline.md) for resource portability
and manual subset debugging.
