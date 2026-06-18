# Animation: making the PAGX move

The converter does not just flatten a still frame — it also captures the page's **motion** and
bakes it into the `.pagx` as a real PAGX animation timeline. Capture is **automatic** (on by
default in both the in-repo `html2pagx` path and the npm `pagx import --html-snapshot` path); no
flag is needed. This reference covers what gets captured, how to author HTML that animates
faithfully, how to preview the motion, and how to read the warnings.

## Table of contents

- [What can be captured](#what-can-be-captured)
- [What actually plays back](#what-actually-plays-back)
- [Authoring rules for faithful motion](#authoring-rules-for-faithful-motion)
- [Previewing the motion](#previewing-the-motion)
- [Timing and looping](#timing-and-looping)
- [Warnings and troubleshooting](#warnings-and-troubleshooting)

---

## What can be captured

The snapshot reads animations from every common source and normalizes them into one canonical form
(CSS `@keyframes` + an `animation` shorthand) that the importer maps onto the PAGX animation model.
Sources captured automatically:

- **CSS `@keyframes` + `animation`** — plain stylesheet animations (the most reliable source).
- **Web Animations API (WAAPI)** — `element.animate(...)`, and libraries built on it such as
  **Motion One** and `web-animations-js`.
- **GSAP** and **anime.js** — rAF-driven libraries are sampled deterministically (the timeline is
  paused, seeked at many points, and reduced to compact keyframes).

Only animations that are **running or attached at snapshot time** are seen. Looping / infinite
animations (the common case for ambient motion, loaders, marquees) are captured most reliably. A
one-shot entrance that has already finished and been released by the browser may not be captured —
prefer `iteration-count: infinite` (or `loop: true`) while designing, or an animation that is still
attached when the page settles.

## What actually plays back

The PAGX runtime can only animate a fixed set of channels. The converter keeps these and **drops
everything else** (with a `subset:animation-unsupported-property` warning):

| CSS property you animate | PAGX channel | Notes |
|--------------------------|--------------|-------|
| `opacity` | layer `alpha` | fades |
| `transform: translateX/translateY/translate(...)` | layer `x` / `y` | slides / position moves |
| `color` | fill `color` | needs a solid text `color` fill present |
| `background-color` | fill `color` | needs a solid `background-color` present |

Not playable (silently flattened to the static frame): **`scale`, `rotate`, `skew`, `matrix`**, and
any **layout** property (`width`, `height`, `margin`, `padding`, `gap`, `flex`, `top`/`left`, …).
A `transform` that combines translate with scale/rotate keeps only its translation.

> Design motion within the playable channels. A "zoom in" (`scale`) or "spin" (`rotate`) will
> convert to a static element; re-express it as a fade (`opacity`) or a slide (`translate`) to keep
> the motion. Color pulses work only when the element actually paints a solid color/background.

## Authoring rules for faithful motion

Add these on top of the static rules in `authoring-html.md`:

- **Animate with `@keyframes` + `animation`** for the most predictable result. GSAP / anime.js /
  WAAPI also work, but the declarative CSS form captures its easing exactly.
- **Restrict animated properties to `opacity`, `transform: translate`, `color`,
  `background-color`.** Other properties become static.
- **Auto-play on load.** Hover-, scroll-, or click-triggered animations have not fired when the
  snapshot is taken (no interaction occurs). Use `animation` that starts immediately, or — for
  scroll-revealed sections — convert with `--scroll-reveal` so they trigger. Do not rely on `:hover`.
- **Make looping motion `infinite`** so it is captured reliably and so the PAGX loops (a finite
  count > 1 is downgraded to play once — see below).
- **To animate color, give the element a solid `background-color` (or text `color`).** Without a
  solid fill there is no `color` channel to drive.

Example of a clean, fully-captured animation:

```html
<head>
  <style>
    @keyframes floatIn {
      0%   { opacity: 0; transform: translateY(24px); }
      100% { opacity: 1; transform: translateY(0px); }
    }
    @keyframes pulse {
      0%, 100% { background-color: #6366F1; }
      50%      { background-color: #8B5CF6; }
    }
  </style>
</head>
<body style="margin:0; width:640px; height:400px;">
  <h1 style="animation: floatIn 1.2s ease-out infinite alternate;">Hello</h1>
  <div style="background-color:#6366F1; animation: pulse 2s linear infinite;">Badge</div>
</body>
```

## Previewing the motion

A plain `pagx render` (no `--time`) renders the **static base frame** — good for checking the
design, but it does not show motion. To inspect the animation, render specific playback times with
`--time <seconds>` and compare a few frames:

```bash
pagx render <name>.pagx -o frame_00.png --time 0
pagx render <name>.pagx -o frame_05.png --time 0.5
pagx render <name>.pagx -o frame_10.png --time 1.0
```

Pick times that span one loop period (the longest `delay + duration` across the page's animations).
Looping animations realign every period, so one period is enough to see the whole motion. `--time`
cannot be combined with `--crop`, `--id`, or `--xpath`.

Show the user the frame sequence (or assemble them into a GIF/contact sheet) so they can confirm the
motion, not just the still design.

## Timing and looping

How CSS animation timing maps onto PAGX (full detail in `spec/html_subset.md` §13):

- **Frame rate** is fixed at 60 fps; `@keyframes` percentages convert to frame times.
- **`animation-timing-function`**: `linear` stays linear; `ease` / `ease-in` / `ease-out` /
  `ease-in-out` / `cubic-bezier(...)` become bezier easing; `steps(...)` / `step-start` /
  `step-end` become hold (stepped).
- **`animation-iteration-count`**: `infinite` → the PAGX animation loops; **any finite count plays
  once** (PAGX has no finite repeat count — a finite count > 1 warns with
  `subset:animation-finite-count`). Use `infinite` for repeating motion.
- **`animation-direction`**: `alternate` → ping-pong (forward then reverse); `reverse` reverses the
  keyframe order; `normal` keeps source order.
- **`animation-delay`**: positive delays shift keyframes forward; negative delays clamp to 0.
- **One animation per element** — comma-separated `animation` lists keep only the first
  (`subset:animation-multiple`).

## Warnings and troubleshooting

`pagx import` prints `subset:animation-*` diagnostics; they are informational, not failures:

| Symptom / warning | Cause / fix |
|-------------------|-------------|
| `subset:animation-unsupported-property` | An animated property has no runtime channel (`scale`, `rotate`, `width`, …) and was dropped. Re-express the motion as `opacity` / `translate` / color, or accept it as static. |
| `subset:animation-multiple` | Element had several comma-separated animations; only the first was kept. Split into separate elements if you need more than one. |
| `subset:animation-finite-count` | A finite repeat count > 1 was downgraded to play once. Use `infinite` to loop. |
| `subset:animation-unknown-keyframes` | The `animation` referenced a `@keyframes` name that no longer exists; the animation was dropped. |
| The element does not move in the preview | The animation was hover/scroll/click-triggered (never fired — make it auto-play or use `--scroll-reveal`), already finished before the snapshot (use `infinite`), or only animated a non-playable property. |
| Color animation does nothing | The element paints no solid `color` / `background-color`; add one. |
| Want a purely static `.pagx` | The capture cannot be disabled from `html2pagx` today; remove the `animation` declarations from the HTML, or run `pagx import` on the static subset HTML. |
