# Animation: HTML capture and native PAGX transforms

HTML conversion is **static by default**. Enable `--html-capture-animations` to capture supported
page motion as a PAGX animation timeline:

```bash
pagx import --input motion.html --output motion.pagx --html-capture-animations
# Also render a PNG; add resource/subset options when needed:
pagx import motion.html --output-dir out --render --html-capture-animations
```

The separate repository `snapshot.js` tool uses `--capture-animations` instead. Check the
selected executable's command help before using it. For click/hover/scroll states, see
[Interaction capture](interaction.md): scenarios export static checkpoints and cannot be combined
with `--html-capture-animations`.

The HTML sections below describe the capture pipeline, not every native PAGX animation
capability. For direct XML editing, see [Native transforms](#native-pagx-transform-animation).

## Table of contents

- [What can be captured](#what-can-be-captured)
- [What actually plays back](#what-actually-plays-back)
- [Animating inline SVG](#animating-inline-svg)
- [Authoring rules for faithful motion](#authoring-rules-for-faithful-motion)
- [Previewing the motion](#previewing-the-motion)
- [Timing, looping, and fill mode](#timing-looping-and-fill-mode)
- [Warnings and troubleshooting](#warnings-and-troubleshooting)
- [Native PAGX transform animation](#native-pagx-transform-animation)

---

## What can be captured

The snapshot reads supported animation sources and normalizes them into one canonical form
(CSS `@keyframes` + an `animation` shorthand) that the importer maps onto the PAGX animation model.
With animation capture enabled, supported sources include:

- **CSS `@keyframes` + `animation`** — plain stylesheet animations (the most reliable source).
- **Web Animations API (WAAPI)** — `element.animate(...)`, and libraries built on it such as
  **Motion One** and `web-animations-js`.
- **GSAP** and **anime.js** — rAF-driven libraries are sampled deterministically (the timeline is
  paused, seeked at many points, and reduced to compact keyframes).

Only animations that are **running or attached at snapshot time** are seen. Looping / infinite
animations (the common case for ambient motion, loaders, marquees) are captured most reliably. A
one-shot entrance that has already finished and been released by the browser may not be captured —
capture it while still attached, and use fill/settle timing appropriate to the intended motion.
Use infinite looping only when repetition is part of the requested result.

## What actually plays back

The HTML converter maps supported CSS properties to PAGX channels. Unsupported animated
properties are dropped with a `subset:animation-unsupported-property` warning:

| CSS property you animate | PAGX channel | Notes |
|--------------------------|--------------|-------|
| `opacity` | layer `alpha` | fades |
| `transform: translateX/Y/translate(...)` | layer `x` / `y`, or `matrix` | local translation on top of layout; a static transform requires full matrix samples so the animation can replace it |
| `transform: scale / rotate / skew / matrix(...)` | layer `matrix` | zoom, spin, shear — pivots around `transform-origin` |
| `color` | fill `color` | needs a solid text `color` fill present |
| `background-color` | fill `color` | needs a solid `background-color` present |
| `filter: drop-shadow(...)` | drop-shadow filter | animated glow / shadow; each shadow in the chain is kept |
| `filter: blur(...)` | blur filter | animated blur radius |
| `filter: brightness(...)` | generated layer `alpha` | approximate dimming; values above 1 are clamped, with a diagnostic |
| `clip-path: inset/circle/ellipse/polygon/path(...)` | contour-mask morph | wipes / reveals / iris — all keyframes must share the same shape |

A `box-shadow` that only appears in an animated state (e.g. a `.on` class toggled over the
timeline) is folded into the drop-shadow filter channel automatically, so a pulsing glow animates
even when authored as `box-shadow`.

**Not captured as playable HTML motion**: **layout** properties (`width`,
`height`, `margin`, `padding`, `gap`, `flex`, `top`/`left`, …) and **3D transforms**
(`matrix3d`, `rotate3d`, `perspective`). The browser capture may retain only translation from a
computed 3D matrix, and native subset import rejects 3D transform keyframes. Native PAGX supports
3D animation independently; that does not make arbitrary HTML 3D motion importable.

> A `transform` chain that mixes translate with scale/rotate/skew is kept whole and routed through
> the `matrix` channel (not split). The matrix channel pivots around the element's
> `transform-origin` exactly like a static transform, so animated and static transforms agree.
> Matrix interpolation takes the shorter rotation path and cannot recover extra turns from two
> endpoint matrices. Author intermediate `@keyframes` stops
> (e.g. quarter-turn stops for a full spin) to keep each rotation segment below 180°.

Color pulses work only when the element actually paints a solid `color` / `background-color`; with
no solid fill there is no `color` channel to drive.

## Animating inline SVG

Inline `<svg>` shapes use Layer, Group and painter channels for icon and line-art motion:

| SVG property you animate | Effect |
|--------------------------|--------|
| `opacity` | shape fades |
| `transform` (supported 2D forms) | translation uses `x`/`y` when no static transform is present; a pure rotation may use the resolved Group's `rotation`; other affine transforms use the shape Layer's `matrix` |
| `fill` / `fill-opacity` | fill color / fill alpha |
| `stroke` / `stroke-opacity` | stroke color / stroke alpha |
| `stroke-dashoffset` | the path-trace **line-draw** idiom (`stroke-dasharray:1; stroke-dashoffset:1→0` with `pathLength="1"`) |

A pre-existing static shape transform makes the animation use full matrix samples to replace
that transform; the original static transform supplies the fill-mode baseline. 3D/perspective
keyframes are still unsupported.

A single SVG shape may carry a comma-separated `animation` list (e.g. draw the outline, then fade
the fill in): `animation: draw 1s linear forwards, fill 0.4s ease 1s forwards`. Each entry becomes
its own PAGX animation. (On regular HTML elements only the first entry of a comma-separated list is
kept.)

## Authoring rules for faithful motion

Add these on top of the static rules in `authoring-html.md`:

- **Author with `@keyframes` + `animation`** for the most predictable result. GSAP / anime.js /
  WAAPI also work, but the declarative CSS form captures its easing exactly.
- **Keep HTML animation within the supported set above.** If the requested motion requires
  unsupported properties, use a suitable native PAGX implementation or explain the approximation.
  A 2D substitute for a 3D flip changes the result and should match the user's intent.
- **Auto-play on load.** Hover-, scroll-, or click-triggered animations have not fired when the
  ordinary snapshot is taken. Use motion that starts on load, or capture with `--html-scroll-reveal`
  for scroll-revealed content. A scenario can capture a triggered static state, but does not
  record its interaction as a playable animation.
- **Use `infinite` for intentionally looping motion.** Preserve one-shot motion when requested;
  finite repeat counts above 1 are downgraded to play once (see below).
- **To animate color, give the element a solid `background-color` (or text `color`).** Without a
  solid fill there is no `color` channel to drive.
- **Set `transform-origin`** on anything you scale/rotate/skew so the pivot matches your intent
  (defaults to the box center).

A minimal HTML rotation example with intermediate stops and no external assets:

```html
<head>
  <style>
    @keyframes spin {
      0%   { transform: rotate(0deg); }
      25%  { transform: rotate(90deg); }
      50%  { transform: rotate(180deg); }
      75%  { transform: rotate(270deg); }
      100% { transform: rotate(360deg); }
    }
  </style>
</head>
<body style="margin:0; width:640px; height:400px; background:#f3f4f6;">
  <div style="position:absolute; left:280px; top:180px; width:80px; height:40px;
              background:#2456bc; transform-origin:center; animation:spin 4s linear infinite;"></div>
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

Pick times covering each animation's delay, active interval and boundary behavior. Different
loop durations need not realign at the longest duration; an alternate loop also includes its
return leg. `--time` cannot be combined with `--crop`, `--id`, or `--xpath`.

Timed rendering also drives timelines mounted on Composition instances, honoring each slot's
`evaluationOffset` (in frames) and `playing` state. Paused slots do not advance with the requested
time. Compare both active and paused instances when diagnosing reused animated components.

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

`pagx import -v` exposes `subset:animation-*` conversion warnings. Review lost or approximated
motion; these warnings are distinct from command failures and `pagx verify` diagnostics:

| Symptom / warning | Cause / fix |
|-------------------|-------------|
| `subset:animation-unsupported-property` | The HTML importer cannot map the property — for example a layout property, a 3D transform, color with no solid fill, or an invalid SVG transform. Use a supported representation or report the lost motion. |
| `subset:animation-multiple` | A regular element had several comma-separated animations (or timing functions); only the first was kept. Split into separate elements, or use an inline SVG shape (which supports a list). |
| `subset:animation-finite-count` | A finite repeat count > 1 (or a finite `alternate`, or count `0`) was downgraded / suppressed. Use `infinite` to loop. |
| `subset:animation-unknown-keyframes` | The `animation` referenced a `@keyframes` name that no longer exists; the animation was dropped. |
| The element does not move in the preview | First check `--html-capture-animations` and render different `--time` values. Then check whether the animation was never triggered, already finished/detached, or used an unsupported property. |
| A rotate/scale "takes the short way" or snaps | Matrix interpolation loses full turns and chooses the shorter rotation path; add stops so each intended rotation segment is below 180°. |
| Color animation does nothing | The element paints no solid `color` / `background-color`; add one. |
| A `clip-path` reveal is dropped | The keyframes animate between different shape functions (e.g. `circle()` → `polygon()`); CSS only interpolates matching shapes. Keep every stop the same function with the same point count. |
| Want a static page capture | Omit `--html-capture-animations` on ordinary HTML capture/import. No source animation removal is needed. Direct import of an already prepared subset can still contain authored `@keyframes`. |

## Native PAGX transform animation

Use this section when authoring or editing PAGX XML directly. These native channels are broader
than the HTML capture set. Static attributes and channel names differ:

| Static Layer attributes | Animation / DataBind channel names | Value type |
|---|---|---|
| `x`, `y`, `z` | `x`, `y`, `z` | `float` |
| `rotationX`, `rotationY`, `rotationZ` | `rotation.x`, `rotation.y`, `rotation.z` | `float`, degrees |
| `scaleX`, `scaleY`, `scaleZ` | `scale.x`, `scale.y`, `scale.z` | `float` |
| `skewX`, `skewY` | `skew.x`, `skew.y` | `float`, degrees strictly between −90 and 90 |
| `perspective` | `perspective` | `float`, nonnegative distance; 0 disables perspective |
| `matrix` | `matrix` | `matrix`; animation only |
| `matrix3D` | `matrix3D` | `matrix3D`; animation only |

Scalar translation and component channels support animation and DataBind. Full `matrix` and
`matrix3D` channels cannot be data-bound. Use the channel name (for example `rotation.z`), not
the static attribute name (`rotationZ`), in `<Channel name="...">` or `<DataBind channel="...">`.
See [Layer transforms](guide.md#layer-transforms) for layout composition and static precedence.

For a layer with `id="card"`, a native full-turn animation can use an unwrapped scalar angle:

```xml
<Animations>
  <Animation id="spin" duration="120" frameRate="60" loop="loop">
    <Object target="card">
      <Channel name="rotation.z" type="float">
        <Key time="0" value="0" interpolation="linear"/>
        <Key time="120" value="360"/>
      </Channel>
    </Object>
  </Animation>
</Animations>
```

Place `<Animations>` in the document or Composition containing the target. Composition animations
must be activated on the referencing Layer through `<Timelines><Animation ref="@spin"/></Timelines>`.
Key times and duration are frames; `pagx render --time` takes seconds.

Component channels retain authored angles while driven as components, allowing full turns and
scales passing through zero. Matrix writes replace the corresponding transform, after which a
component write first needs a valid decomposition. Avoid competing matrix and scalar channels
when a single representation expresses the intended motion. A non-identity static `matrix`
suppresses `x`/`y` and component writes; a non-identity static `matrix3D` also suppresses `z` and
`matrix` writes. Remove a conflicting static matrix or animate its matching matrix channel.

`matrix3D` values contain 16 column-major numbers. Authored matrices and keyframes must be finite
and invertible; use explicit `type="matrix3D"` for clarity. Interpolation normally decomposes the
matrix and uses quaternion rotation interpolation, with element-wise interpolation as a fallback.
Matrix endpoints alone cannot preserve extra full turns. Intermediate poses can be singular:
a collapsed content plane has no drawable area, and component access requiring decomposition can
fail until a decomposable matrix is restored. Component writes with invalid skew or negative
perspective are rejected. Inspect timed renders when mixing transform representations or crossing
degenerate poses.
