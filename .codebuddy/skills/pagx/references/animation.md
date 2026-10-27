# Animation: making the PAGX move

The converter can capture supported page motion as a PAGX animation timeline. Animation capture
is **off by default** in both repository and npm workflows. Enable it explicitly:

```bash
pagx import motion.html -o motion.pagx --capture-animations --render
```

Use `--overwrite` when intentionally regenerating existing outputs. `--scenario` captures static
interaction states and cannot be combined with `--capture-animations`; see
[Interaction capture](interaction.md). For prepared subset input, enable capture in the preceding
snapshot step and import with `--html-source snapshot` without the animation-capture flag.

## Table of contents

- [What can be captured](#what-can-be-captured)
- [What actually plays back](#what-actually-plays-back)
- [Animating inline SVG](#animating-inline-svg)
- [Authoring rules for faithful motion](#authoring-rules-for-faithful-motion)
- [Previewing the motion](#previewing-the-motion)
- [Timing, looping, and fill mode](#timing-looping-and-fill-mode)
- [Warnings and troubleshooting](#warnings-and-troubleshooting)

---

## What can be captured

The snapshot reads animations from every common source and normalizes them into one canonical form
(CSS `@keyframes` + an `animation` shorthand) that the importer maps onto the PAGX animation model.
Sources recognized when capture is enabled:

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

The PAGX runtime animates a fixed set of channels. The converter keeps every property below and
**drops the rest** (with a `subset:animation-unsupported-property` warning):

| CSS property you animate | PAGX channel | Notes |
|--------------------------|--------------|-------|
| `opacity` | layer `alpha` | fades |
| `transform: translateX/Y/translate(...)` | layer `x` / `y` | slides / position moves (stacks on top of layout) |
| `transform: scale / rotate / skew / matrix(...)` | layer `matrix` | zoom, spin, shear — pivots around `transform-origin` |
| `color` | fill `color` | needs a solid text `color` fill present |
| `background-color` | fill `color` | needs a solid `background-color` present |
| `filter: drop-shadow(...)` | drop-shadow filter | animated glow / shadow; each shadow in the chain is kept |
| `filter: blur(...)` | blur filter | animated blur radius |
| `clip-path: inset/circle/ellipse/polygon/path(...)` | contour-mask morph | wipes / reveals / iris — all keyframes must share the same shape |

A `box-shadow` that only appears in an animated state (e.g. a `.on` class toggled over the
timeline) is folded into the drop-shadow filter channel automatically, so a pulsing glow animates
even when authored as `box-shadow`.

**Not playable** (retained only as static appearance or dropped with a warning): **layout** properties (`width`,
`height`, `margin`, `padding`, `gap`, `flex`, `top`/`left`, …) and **3D transforms**
(`matrix3d`, `rotate3d`, `perspective`). Everything in the table above — including scale, rotate,
and skew — now plays back; earlier versions dropped non-translation transforms, so do not assume a
"zoom" or "spin" will go static.

> A `transform` chain that mixes translate with scale/rotate/skew is kept whole and routed through
> the `matrix` channel (not split). The matrix channel pivots around the element's
> `transform-origin` exactly like a static transform, so animated and static transforms agree.
> One caveat: matrix interpolation cannot recover a full turn or a rotation crossing ±180° between
> two adjacent keyframes (it takes the short way around). Author intermediate `@keyframes` stops
> (e.g. `0% / 50% / 100%`) for large spins — the usual case already does this.

Color pulses work only when the element actually paints a solid `color` / `background-color`; with
no solid fill there is no `color` channel to drive.

## Animating inline SVG

Inline `<svg>` shapes get their own painter channels, which unlock icon and line-art motion:

| SVG property you animate | Effect |
|--------------------------|--------|
| `opacity` | shape fades |
| `transform` (**translate only**) | shape slides — scale/rotate on an SVG shape is dropped |
| `fill` / `fill-opacity` | fill color / fill alpha |
| `stroke` / `stroke-opacity` | stroke color / stroke alpha |
| `stroke-dashoffset` | the path-trace **line-draw** idiom (`stroke-dasharray:1; stroke-dashoffset:1→0` with `pathLength="1"`) |

A single SVG shape may carry a comma-separated `animation` list (e.g. draw the outline, then fade
the fill in): `animation: draw 1s linear forwards, fill 0.4s ease 1s forwards`. Each entry becomes
its own PAGX animation. (On regular HTML elements only the first entry of a comma-separated list is
kept.)

## Authoring rules for faithful motion

Add these on top of the static rules in `authoring-html.md`:

- **Author with `@keyframes` + `animation`** for the most predictable result. GSAP / anime.js /
  WAAPI also work, but the declarative CSS form captures its easing exactly.
- **Keep animated properties inside the playable set above.** Layout properties and 3D transforms
  become static. Re-express a size change as `scale`, a 3D flip as a 2D `rotate`/`scaleX`.
- **Make the motion available at capture time.** Ordinary animation capture does not execute a
  scenario's clicks or hovers. Use motion attached on load, or `--scroll-reveal` for scroll-triggered
  content. If the request is for the static appearance after an interaction, use a scenario instead.
- **Make looping motion `infinite`** so it is captured reliably and so the PAGX loops (a finite
  count > 1 is downgraded to play once — see below).
- **To animate color, give the element a solid `background-color` (or text `color`).** Without a
  solid fill there is no `color` channel to drive.
- **Set `transform-origin`** on anything you scale/rotate/skew so the pivot matches your intent
  (defaults to the box center).

Example of a clean, fully-captured animation using several channels:

```html
<head>
  <style>
    @keyframes floatIn  { 0% { opacity:0; transform:translateY(24px);} 100% { opacity:1; transform:translateY(0);} }
    @keyframes spin      { 0% { transform: rotate(0deg); } 50% { transform: rotate(180deg); } 100% { transform: rotate(360deg); } }
    @keyframes pulse     { 0%,100% { background-color:#6366F1;} 50% { background-color:#8B5CF6;} }
    @keyframes glow      { 0%,100% { filter: drop-shadow(0 0 4px #6366F1);} 50% { filter: drop-shadow(0 0 24px #8B5CF6);} }
    @keyframes iris      { 0% { clip-path: circle(0%);} 100% { clip-path: circle(75%);} }
  </style>
</head>
<body style="margin:0; width:640px; height:400px;">
  <h1  style="animation: floatIn 1.2s ease-out infinite alternate;">Hello</h1>
  <div style="transform-origin:center; animation: spin 4s linear infinite;">◆</div>
  <div style="background-color:#6366F1; animation: pulse 2s linear infinite;">Badge</div>
  <div style="background:#111; animation: glow 1.5s ease-in-out infinite;">Glow</div>
  <div style="background:#0af; animation: iris 1s ease-out infinite;">Reveal</div>
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

Pick times that cover each animation's delay, key transitions and loop boundary. Animations with
different durations do not necessarily realign after the longest duration. `--time` cannot be
combined with `--crop`, `--id`, or `--xpath`.

Show the user the frame sequence (or assemble them into a GIF/contact sheet) so they can confirm the
motion, not just the still design.

## Timing, looping, and fill mode

How CSS animation timing maps onto PAGX (full detail in `spec/html_subset.md` §13):

- **Frame rate** is fixed at 60 fps; `@keyframes` percentages convert to frame times.
- **`animation-timing-function`**: `linear` stays linear; `ease` / `ease-in` / `ease-out` /
  `ease-in-out` / `cubic-bezier(...)` become bezier easing; `steps(n[, jump])` / `step-start` /
  `step-end` expand into stepped (hold) keyframes that reproduce the staircase.
- **`animation-iteration-count`**: `infinite` → the PAGX animation loops; **any finite count plays
  once** (PAGX has no finite repeat count — a finite count > 1 warns with
  `subset:animation-finite-count`). Use `infinite` for repeating motion. `0` suppresses playback.
- **`animation-direction`**: `alternate` (infinite only) → ping-pong; `reverse` /
  `alternate-reverse` reverse the keyframe order and the easing; `normal` keeps source order.
  Finite `alternate` is downgraded to play once.
- **`animation-delay`**: positive delays shift keyframes forward; negative delays clamp to 0.
- **`animation-fill-mode`**: `none` (default) reverts to the element's non-animated value before
  the delay ends and after a once-through animation finishes; `forwards` holds the last keyframe;
  `backwards` shows the first keyframe during the delay; `both` does both.
- **One animation per element** — comma-separated `animation` lists on regular HTML elements keep
  only the first (`subset:animation-multiple`). Inline SVG shapes are the exception (see above).

## Warnings and troubleshooting

Use `pagx import -v` to inspect `subset:animation-*` warnings. They can indicate lost motion even
when conversion succeeds; compare the sampled frames with the intended behavior.

| Symptom / warning | Cause / fix |
|-------------------|-------------|
| `subset:animation-unsupported-property` | An animated property has no runtime channel — a layout property (`width`, `height`, `margin`, …), a 3D transform (`matrix3d`/`rotate3d`/`perspective`), a `color` animation with no solid fill, or an inline-SVG `transform` that is not a pure translate. Re-express it or accept it as static. |
| `subset:animation-multiple` | A regular element had several comma-separated animations (or timing functions); only the first was kept. Split into separate elements, or use an inline SVG shape (which supports a list). |
| `subset:animation-finite-count` | A finite repeat count > 1 (or a finite `alternate`, or count `0`) was downgraded / suppressed. Use `infinite` to loop. |
| `subset:animation-unknown-keyframes` | The `animation` referenced a `@keyframes` name that no longer exists; the animation was dropped. |
| The element does not move in the preview | Check `--capture-animations` and render at distinct `--time` values. Then check whether the animation was attached at capture time, needed a trigger, or used an unsupported property. |
| A rotate/scale "takes the short way" or snaps | Matrix interpolation cannot cross ±180° or a full turn in one segment; add intermediate `@keyframes` stops (`0% / 50% / 100%`). |
| Color animation does nothing | The element paints no solid `color` / `background-color`; add one. |
| A `clip-path` reveal is dropped | The keyframes animate between different shape functions (e.g. `circle()` → `polygon()`); CSS only interpolates matching shapes. Keep every stop the same function with the same point count. |
| `paint:animated-stacking` | The animation changes stacking topology (for example `z-index` or `display`); exported paint order remains static. Use a fixed stacking structure or capture the desired static states separately. |
| Want a purely static `.pagx` | Import the original HTML without `--capture-animations`. Use a scenario for a particular interaction state. |
