---
name: html-pagx-gen
description: >-
  Generate PAGX content via the HTML fast path: model produces a restricted HTML/CSS
  document, the `pagx import --format html` pipeline converts it losslessly into PAGX.
  Use when the user asks to quickly produce a visual canvas (card, panel, tab bar,
  table, dashboard tile, login screen, etc.) and prefers HTML drafting over writing
  PAGX directly. The output is the same PAGX format produced by the `pagx` skill — it
  can be polished, verified, and exported afterwards.
---

# HTML → PAGX Generation Skill

## When to use this skill

Prefer this skill over the manual `pagx` skill when **all** of the following hold:

- The design is UI-shaped: rectangular containers, flexbox layout, text + simple
  visual decoration (colors, gradients, borders, shadows, opacity, blurs).
- Throughput matters: producing 10×+ more iterations per minute is worth more than
  squeezing the last 5% of visual fidelity.
- The user has not explicitly asked for hand-authored PAGX or for shapes that
  HTML cannot describe natively (e.g., bespoke vector paths, complex masks). For
  those, fall back to the `pagx` skill.

The full PAGX skill (`.claude/skills/pagx/SKILL.md`) is still the authority for the
final review pass (`pagx verify`, polish, adversarial QA). This skill only owns the
first-pass HTML drafting and the import command.

---

## Pipeline

```
LLM draft  ─►  layout.html
                  │
                  ▼
       pagx import --format html
       --input layout.html
       --output layout.pagx
                  │
                  ▼
       pagx verify layout.pagx   ◄── pagx skill takes over from here
```

The importer is the C++ module under `src/pagx/html/` (see
`include/pagx/HTMLImporter.h`). It enforces the same subset documented in
`spec/html_subset.md`; any deviation produces a warning in `pagx-import`'s
stderr or, in strict mode, a hard failure.

---

## Subset Contract (must read)

`spec/html_subset.md` (English) and `spec/html_subset.zh_CN.md` (Chinese) are the
**source of truth**. Highlights for prompt purposes:

- **Document skeleton**: `<html><head>?<style>?</style></head><body style="width:Wpx; height:Hpx;">…</body></html>`.
  The canvas size MUST come from `<body>`'s `width`/`height` (or `--target-width/--target-height`).
- **Allowed structural tags**: `<div>`, `<section>`, `<header>`, `<footer>`, `<main>`,
  `<aside>`, `<nav>`, `<article>`, `<p>`, `<h1>`–`<h6>`, `<span>`, `<a>`, `<br/>`,
  `<img/>`, inline `<svg>`. Everything else (tables, forms, inputs, canvas, scripts,
  iframes, custom elements) is rejected with a warning.
- **Allowed CSS**:
  - **Layout**: `display: flex | block`, `flex-direction: row | column`, `flex: <N>`,
    `gap`, `padding` (1–4 values, px only), `align-items`, `justify-content`,
    `box-sizing: border-box` (default).
  - **Sizing**: `width`, `height` (px or `%`).
  - **Positioning**: `position: absolute` + `top/right/bottom/left` (px, negatives ok).
  - **Visuals**: `background-color`, `background-image: linear-gradient | radial-gradient | conic-gradient`,
    `border-radius`, `border: Wpx solid #COLOR`, `box-shadow` (single + multiple,
    incl. `inset`), `opacity`, `mix-blend-mode`, `filter: blur(...) drop-shadow(...)`,
    `backdrop-filter: blur(...)`, `overflow: hidden`.
  - **Text**: `color`, `font-family`, `font-size`, `font-weight`, `font-style`,
    `letter-spacing`, `text-align`, `line-height`, `text-decoration`,
    `white-space: nowrap`, `text-overflow: ellipsis`.
- **Explicitly forbidden** (importer warns + drops): `margin`, `flex-wrap`, `grid`,
  `position: relative`, `transform`, `em`/`rem` units, media queries, external
  stylesheets / `link rel="stylesheet"`, JavaScript.
- **Self-closing void elements**: write `<br/>` and `<img/>` (the parser is
  XHTML-strict; `<br>` / `<img>` without slash will fail).

---

## Authoring Rules of Thumb

1. **One canvas per file.** Put `width`/`height` on `<body>` to lock the canvas.
2. **Container vs background.** When a `<div>` has both a background visual
   (color/gradient/border/shadow/border-radius) **and** internal padding/flex
   layout, the importer auto-splits it into "outer Layer (background) + inner
   Layer (padding + children)". You don't need to do it manually. Just write the
   one element.
3. **Sizing**: give every direct child of a flex container either an explicit
   size, `flex: <N>`, or `width: 100%; height: 100%` so the layout collapses
   deterministically. Avoid implicit-shrink-to-fit; it causes 1px verify diffs.
4. **Text**: `<p>`/`<hN>`/`<span>` text becomes a `Text` (single-style) or
   `TextBox` (multi-style or alignment/wrap needed). For a button-style block,
   wrap label text in a `<div>` with `padding`, `border-radius`, `background-*`,
   and `text-align: center`.
5. **Rich text**: nest `<span>` inside a text container with different `color` /
   `font-*` for each run; the importer emits a `TextBox` with multi-fragment
   `Text` children.
6. **Decoration**: `text-decoration: underline` and `line-through` become 1px
   `Rectangle` overlays inside the text Layer (matches `references/patterns.md`
   §Text Decoration).
7. **Gradients**: CSS angles match the importer's conversion table — `0deg = top`,
   `90deg = right`; `linear-gradient(90deg, …)` ends up traveling along +X in
   PAGX, which is what you want.
8. **Filters / shadows**: chain CSS `filter:` to combine `blur(...)` and
   `drop-shadow(...)`. `backdrop-filter: blur(Xpx)` becomes a
   `BackgroundBlurStyle`.
9. **Images**: `<img src="path/to/file.png" alt="…"/>` registers `file.png` as
   a `<Image>` resource (id `@imgN`). Use relative paths when feasible.
10. **Inline SVG**: drop an `<svg>` block in place; the importer adds it as a
    PAGX `<svg>` import directive, which `pagx verify` resolves into geometry.
11. **Do not use** `margin`. Use `gap`, `padding`, or `flex`-based spacing
    instead. The importer will warn on `margin` but it will silently disappear,
    which produces visual drift.

---

## Generation Workflow

### Step 1 — Plan (no code)

Build a containment tree exactly like the `pagx` skill (`SKILL.md` §Step 1):
sections → rows → cells → leaves. Decide the canvas size, color palette, font
hierarchy, and spacing scale. **Do not write HTML yet.**

### Step 2 — Draft HTML

Write the HTML following the subset contract. Reuse the snippets below as a
starting point.

- Always set `<body style="width: Wpx; height: Hpx;">`.
- Prefer `<style>` class selectors over inline styles when an attribute is reused
  ≥ 3 times. Otherwise inline is fine.
- Self-close `<br/>` and `<img/>`.
- Keep nesting shallow — match the containment tree from Step 1 1:1.

### Step 3 — Convert

```bash
pagx import --format html \
  --input layout.html \
  --output layout.pagx \
  --html-strict          # optional: turn warnings into errors
```

If you saw warnings on stderr, fix the HTML (every warning maps to a dropped
visual). Re-run until the import is silent (or `--html-strict` exits 0).

### Step 4 — Hand off to the pagx skill

Open `layout.pagx` and continue with the standard PAGX skill workflow:

1. `pagx verify layout.pagx` — must exit 0 with zero diagnostics.
2. Inspect `layout.layout.xml` and `layout.png`.
3. Polish in PAGX directly if visual fidelity needs the last few percent.
4. Run the adversarial QA reviewer (`.claude/skills/pagx/SKILL.md` §Step 4).

---

## Examples

All snippets below convert directly to PAGX and are also kept as fixtures under
`resources/html/` — run `pagx import --format html --input resources/html/<file>
--output /tmp/out.pagx` to verify.

### 1. Profile card (avatar + two-line text)

```html
<!DOCTYPE html>
<html>
  <body style="width: 320px; height: 96px;">
    <div style="width: 100%; height: 100%; display: flex; flex-direction: row;
                align-items: center; gap: 12px; padding: 12px;
                background-color: #FFFFFF; border-radius: 12px;
                box-shadow: 0 2px 6px #00000026; overflow: hidden;">
      <div style="width: 48px; height: 48px; border-radius: 24px;
                  background-color: #6366F1;"></div>
      <div style="display: flex; flex-direction: column; gap: 4px; flex: 1;">
        <span style="font-size: 16px; font-weight: bold; color: #1E293B;">Alice Chen</span>
        <span style="font-size: 13px; color: #64748B;">alice@example.com</span>
      </div>
    </div>
  </body>
</html>
```

### 2. Tab bar (three pills, one selected)

```html
<!DOCTYPE html>
<html>
  <head>
    <style>
      .bar { width: 100%; height: 100%; display: flex; flex-direction: row;
             gap: 8px; padding: 8px; background-color: #F1F5F9;
             border-radius: 16px; }
      .tab { flex: 1; padding: 10px 12px; border-radius: 12px;
             text-align: center; color: #475569; font-size: 14px;
             font-weight: bold; }
      .active { background-color: #FFFFFF;
                box-shadow: 0 1px 3px #0000001F;
                color: #0F172A; }
    </style>
  </head>
  <body style="width: 320px; height: 56px;">
    <div class="bar">
      <div class="tab active">Home</div>
      <div class="tab">Search</div>
      <div class="tab">Profile</div>
    </div>
  </body>
</html>
```

### 3. Login panel (vertical flex + gradient CTA)

```html
<!DOCTYPE html>
<html>
  <head>
    <style>
      .panel { width: 100%; height: 100%; display: flex; flex-direction: column;
               gap: 12px; padding: 24px; background-color: #FFFFFF;
               border-radius: 16px; box-shadow: 0 6px 16px #00000022;
               overflow: hidden; }
      .title { font-size: 22px; font-weight: bold; color: #0F172A; }
      .subtitle { font-size: 13px; color: #64748B; }
      .input { padding: 10px 12px; border-radius: 8px;
               background-color: #F8FAFC; border: 1px solid #E2E8F0;
               color: #1E293B; font-size: 14px; }
      .cta { padding: 12px 16px; border-radius: 12px; color: #FFFFFF;
             font-size: 15px; font-weight: bold; text-align: center;
             background-image: linear-gradient(90deg, #6366F1 0%, #8B5CF6 100%); }
    </style>
  </head>
  <body style="width: 320px; height: 280px;">
    <div class="panel">
      <span class="title">Sign in</span>
      <span class="subtitle">Welcome back. Enter your credentials.</span>
      <div class="input">name@example.com</div>
      <div class="input">password</div>
      <div class="cta">Sign in</div>
    </div>
  </body>
</html>
```

### 4. Stats tile (label + KPI + delta)

```html
<!DOCTYPE html>
<html>
  <body style="width: 240px; height: 120px;">
    <div style="width: 100%; height: 100%; display: flex; flex-direction: column;
                gap: 8px; padding: 16px; background-color: #0F172A;
                border-radius: 14px; overflow: hidden;">
      <span style="font-size: 12px; color: #94A3B8;
                   letter-spacing: 1px;">REVENUE</span>
      <span style="font-size: 28px; font-weight: bold;
                   color: #FFFFFF;">$48.2k</span>
      <span style="font-size: 13px; color: #22C55E;">+12.4% this week</span>
    </div>
  </body>
</html>
```

### 5. Badge over an avatar (absolute positioning)

```html
<!DOCTYPE html>
<html>
  <body style="width: 80px; height: 80px;">
    <div style="width: 64px; height: 64px; border-radius: 32px;
                background-color: #6366F1;"></div>
    <div style="position: absolute; right: 0; top: 0; width: 18px; height: 18px;
                border-radius: 9px; background-color: #EF4444;
                border: 2px solid #FFFFFF;"></div>
  </body>
</html>
```

### 6. Inline rich text (multi-style `<span>` runs)

```html
<!DOCTYPE html>
<html>
  <body style="width: 320px; height: 60px;">
    <p style="font-size: 16px; color: #1E293B; line-height: 1.4;">
      Welcome <span style="font-weight: bold; color: #6366F1;">Alice</span>!
      You have <span style="color: #EF4444;">3 new</span> notifications.
    </p>
  </body>
</html>
```

### 7. Mini stats table (nested flex rows)

```html
<!DOCTYPE html>
<html>
  <head>
    <style>
      .row { display: flex; flex-direction: row; padding: 8px 12px;
             gap: 8px; }
      .head { background-color: #F1F5F9; }
      .cell { flex: 1; font-size: 13px; color: #1E293B; }
      .cell-right { flex: 1; font-size: 13px; color: #1E293B;
                    text-align: right; }
    </style>
  </head>
  <body style="width: 320px; height: 144px;">
    <div style="width: 100%; height: 100%; display: flex; flex-direction: column;
                background-color: #FFFFFF; border-radius: 10px;
                border: 1px solid #E2E8F0; overflow: hidden;">
      <div class="row head">
        <span class="cell">Region</span>
        <span class="cell-right">Revenue</span>
      </div>
      <div class="row">
        <span class="cell">APAC</span>
        <span class="cell-right">$12.4k</span>
      </div>
      <div class="row">
        <span class="cell">EMEA</span>
        <span class="cell-right">$18.9k</span>
      </div>
      <div class="row">
        <span class="cell">AMER</span>
        <span class="cell-right">$16.9k</span>
      </div>
    </div>
  </body>
</html>
```

### 8. Glass card (backdrop-filter + gradient bg)

```html
<!DOCTYPE html>
<html>
  <body style="width: 320px; height: 160px;
               background-image: linear-gradient(135deg, #6366F1 0%, #06B6D4 100%);">
    <div style="position: absolute; left: 24px; top: 24px;
                width: 272px; height: 112px;
                background-color: #FFFFFF55; backdrop-filter: blur(12px);
                border-radius: 16px; padding: 16px;
                display: flex; flex-direction: column; gap: 8px;">
      <span style="font-size: 14px; color: #FFFFFFCC;
                   letter-spacing: 1px;">PREMIUM</span>
      <span style="font-size: 22px; font-weight: bold;
                   color: #FFFFFF;">Cloud Storage</span>
      <span style="font-size: 13px; color: #FFFFFFB3;">10 TB · multi-region</span>
    </div>
  </body>
</html>
```

### 9. Notification banner (icon slot + dismissible text)

```html
<!DOCTYPE html>
<html>
  <body style="width: 360px; height: 64px;">
    <div style="width: 100%; height: 100%; display: flex; flex-direction: row;
                align-items: center; gap: 12px; padding: 12px 16px;
                background-color: #FEF3C7; border-radius: 12px;
                border: 1px solid #FCD34D;">
      <div style="width: 32px; height: 32px; border-radius: 16px;
                  background-color: #F59E0B;"></div>
      <div style="display: flex; flex-direction: column; gap: 2px; flex: 1;">
        <span style="font-size: 14px; font-weight: bold;
                     color: #78350F;">Storage almost full</span>
        <span style="font-size: 12px; color: #92400E;">
          92% used. Upgrade to keep syncing.
        </span>
      </div>
      <span style="font-size: 13px; font-weight: bold; color: #92400E;
                   text-decoration: underline;">Upgrade</span>
    </div>
  </body>
</html>
```

### 10. Sectioned dashboard tile (header + body split)

```html
<!DOCTYPE html>
<html>
  <body style="width: 320px; height: 200px;">
    <div style="width: 100%; height: 100%; display: flex; flex-direction: column;
                background-color: #FFFFFF; border-radius: 14px;
                box-shadow: 0 4px 12px #0000001A; overflow: hidden;">
      <div style="display: flex; flex-direction: row; align-items: center;
                  justify-content: space-between; padding: 12px 16px;
                  background-color: #F8FAFC;">
        <span style="font-size: 14px; font-weight: bold;
                     color: #0F172A;">Weekly Activity</span>
        <span style="font-size: 12px; color: #6366F1;">View all</span>
      </div>
      <div style="display: flex; flex-direction: column; gap: 6px;
                  padding: 16px; flex: 1;">
        <span style="font-size: 12px; color: #64748B;">Sessions</span>
        <span style="font-size: 24px; font-weight: bold;
                     color: #0F172A;">1,284</span>
        <span style="font-size: 12px; color: #22C55E;">+8.1% vs last week</span>
      </div>
    </div>
  </body>
</html>
```

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `failed to determine canvas size` | Add `width`/`height` to `<body>` or pass `--target-width/--target-height`. |
| Warning: `unsupported element <X>` | Remove the element or replace with allowed equivalent (`<table>` → nested `<div>` flex rows). |
| Warning: `margin not supported` | Replace with `gap`/`padding`/`flex`. |
| `pagx verify` reports `child extends beyond parent bounds` | The parent has implicit shrink-to-content sizing. Set `width: 100%; height: 100%;` (or explicit px) on the container, or add `overflow: hidden`. |
| Gradient direction looks flipped | CSS `0deg` is top; `90deg` is right. Match your CSS angle to the visual direction you want; the importer handles the PAGX axis swap. |
| Text Decoration overlay not visible | Confirm the text color is not identical to the background — the 1px overlay inherits the fill colour from the text run. |
| Inline SVG renders empty | `pagx verify` must run to resolve the `<svg>` import directive. The raw `.pagx` keeps the SVG as a directive. |

---

## Anti-patterns (do **not** generate)

- `<table>`, `<tr>`, `<td>` — use nested `<div>` flex rows. The importer rejects
  tables outright.
- `margin: …` — use `gap`/`padding`/`flex`.
- `position: relative; top: …; left: …` — only `position: absolute` is
  supported.
- `transform: …` — drop it. v1 of the importer ignores transforms.
- `em` / `rem` / `vh` / `vw` — px or `%` only.
- External CSS (`<link rel="stylesheet">`) — inline everything in `<style>` or
  `style="…"`.
- Self-defining custom elements (`<my-card>`) — rejected.

When a request demands something outside the subset, fall back to the
hand-authored `pagx` skill workflow rather than fighting the importer.
