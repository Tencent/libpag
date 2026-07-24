# Coverage matrix

Maps each CSS property in the importer allow-list
(`src/pagx/html/importer/HTMLSubsetPropertyTable.cpp`) to at least one case, so
that both the *supported* mapping and the *degraded* path are exercised.

Legend: [x] covered in this phase (P1) · [ ] reserved for a later phase.

## Layout / box model (P1)

| Property | Behavior | Case |
|----------|----------|------|
| `display: block/flex` | supported | `04-display__display-block`, `04-display__display-flex` |
| `display: inline/inline-block` | downgraded -> block | `04-display__display-inline-downgraded`, `...-inline-block-downgraded` |
| `display: grid/table` | dropped (snapshot) | `04-display__display-grid`, `04-display__display-table` |
| `float` | dropped (snapshot) | `04-display__float-left-right` |
| `flex-direction` | supported + reverse downgrade | `05-flexbox__flex-direction-*` |
| `justify-content` | supported (6 values) | `05-flexbox__justify-content-*` |
| `align-items` | supported (5 values) | `05-flexbox__align-items-*` |
| `gap` | supported | `05-flexbox__gap-single` |
| `flex` | grow only; none/auto mapping | `05-flexbox__flex-grow`, `...flex-none` |
| `flex-shrink` | 0 dropped silently | `05-flexbox__flex-shrink-zero` |
| `flex-wrap/grow/basis`, `order`, `align-self` | dropped (snapshot) | `05-flexbox__flex-wrap`, `...flex-basis`, `...align-self`, `...order` |
| `padding` / `padding-*` | supported (1-4 + longhand) | `02-box-model__padding-*` |
| `margin` / `margin-*` | folded / promoted | `02-box-model__margin-*` |
| `box-sizing` | border-box only | `02-box-model__box-sizing-border-box` |
| `width`/`height` px & % | supported | `03-sizing-units__length-px`, `...size-percent` |
| `min/max-*`, `aspect-ratio` | ignored | `03-sizing-units__min-max-width-ignored`, `...aspect-ratio-ignored` |
| units `em/rem/pt/vw/vh` | coerced to px | `03-sizing-units__unit-*-coerced` |
| `calc/var/min/max/clamp` | rejected | `03-sizing-units__calc-rejected` |

## Positioning (P1)

| Property | Behavior | Case |
|----------|----------|------|
| `position: absolute` + explicit sides | supported | `07-position__absolute-four-sides`, `...absolute-negative` |
| `inset` shorthand | dropped (use explicit sides) | `07-position__absolute-inset` |
| `position: relative` | silent no-op | `07-position__relative-noop` |
| `position: fixed/sticky` | downgraded -> absolute | `07-position__fixed-downgraded`, `...sticky-downgraded` |
| `z-index` | dropped (snapshot) | `07-position__z-index` |

## Typography (P1)

| Property | Behavior | Case |
|----------|----------|------|
| `font-family` | fallback chain | `15-typography__font-family-fallback` |
| `font-size` | px | `15-typography__font-size-px` |
| `font-weight` | bold / numeric>=600 | `15-typography__font-weight-bold`, `...font-weight-numeric` |
| `font-style` | italic | `15-typography__font-style-italic` |
| `line-height` | unitless / px / % | `15-typography__line-height-*` |
| `letter-spacing` | px | `15-typography__letter-spacing` |
| `text-align` | start/center/end/justify | `15-typography__text-align-*` |
| `text-decoration` | underline / line-through | `15-typography__text-decoration-underline`, `...line-through` |
| `text-decoration` overline | unsupported | `15-typography__text-decoration-overline` |
| `white-space` | nowrap / pre | `15-typography__white-space-nowrap`, `...white-space-pre` |
| `writing-mode` | vertical-rl | `15-typography__writing-mode-vertical` |
| `-webkit-text-stroke` | glyph stroke | `15-typography__webkit-text-stroke` |
| `text-overflow: ellipsis` | warned | `15-typography__text-overflow-ellipsis` |
| `text-transform` | ignored | `15-typography__text-transform-ignored` |
| `<br/>` | hard break | `15-typography__br-line-break` |

## Paint / color / effects (P2 — covered)

| Property | Behavior | Cases |
|----------|----------|-------|
| colors | hex 3/4/6/8, rgb(a), hsl(a), named, currentColor, transparent, display-p3 | `08-color__*` |
| `background-color` | solid fill | `09-background__background-color-solid` |
| gradients | linear (dir/angle/multistop), radial (circle/ellipse), conic | `09-background__linear-*`, `...radial-*`, `...conic-gradient` |
| `background-clip: text` | gradient routed to text | `09-background__background-clip-text-gradient`, `16-text-tricks__gradient-text` |
| `background-image: url` + size/pos/repeat | ImagePattern | `09-background__background-image-url`, `...background-size-cover` |
| multiple backgrounds | first-resolvable layer | `09-background__multiple-backgrounds` |
| `border` | solid/dashed/dotted | `10-border-radius__border-solid`, `...dashed`, `...dotted` |
| `border-radius` | uniform / 50% / 4-value | `10-border-radius__border-radius-uniform`, `...circle-50`, `...four-corners` |
| `border-radius` elliptical / per-corner, `border-*`, `outline` | dropped | `10-border-radius__border-radius-elliptical`, `...per-corner`, `...border-per-side`, `...outline` |
| `box-shadow` | single/multiple/inset; spread ignored | `11-shadow-filter__box-shadow-*` |
| `filter` | blur / drop-shadow | `11-shadow-filter__filter-blur`, `...filter-drop-shadow` |
| `backdrop-filter` | blur | `11-shadow-filter__backdrop-filter-blur` |
| `filter` other fns | needs snapshot | `11-shadow-filter__filter-brightness` |
| `opacity`, `mix-blend-mode` | supported | `12-opacity-blend__opacity`, `...mix-blend-mode-*` |
| `background-blend-mode` | dropped | `12-opacity-blend__background-blend-mode` |
| `transform` single fn + origin | supported | `13-transform__rotate/scale/scale-xy/skew-x/translate/matrix/transform-origin-center` |
| `transform` chained / 3d / perspective | dropped | `13-transform__rotate-chained`, `...rotate3d`, `...perspective` |
| `overflow: hidden` | clipToBounds | `14-overflow-clip__overflow-hidden` |
| `clip-path: url(#id)` | contour mask (XFAIL) | `14-overflow-clip__clip-path-url` |
| `clip-path: polygon()` | dropped | `14-overflow-clip__clip-path-polygon` |
| `mask-image` svg | mask layer (XFAIL) | `14-overflow-clip__mask-image-svg` |

## Grid / text-tricks / css-delivery (P3 — covered)

| Area | Behavior | Cases |
|------|----------|-------|
| CSS grid | dropped, needs snapshot | `06-grid__*` |
| gradient/outline text | supported | `16-text-tricks__gradient-text`, `...text-stroke-outline` |
| `-webkit-line-clamp` | needs snapshot | `16-text-tricks__line-clamp` |
| single-line ellipsis | warns, not implemented | `16-text-tricks__ellipsis-single-line` |
| inline/style/class/element/comma/multi-class | supported | `19-css-delivery__inline-style`, `...style-block-class`, `...element-selector`, `...comma-selectors`, `...multi-class` |
| id / descendant / pseudo selectors | dropped (unsupported-selector) | `19-css-delivery__id-selector`, `...descendant-selector`, `...pseudo-before` |
| `@media` / `@font-face` | dropped (unsupported-at-rule) | `19-css-delivery__media-query`, `...font-face` |
| `var()` / custom property | dropped | `19-css-delivery__css-variable` |

## Images / inline SVG (P4 — covered)

| Area | Behavior | Cases |
|------|----------|-------|
| `<img>` + object-fit fill/contain/cover | supported | `17-image__img-basic`, `...img-object-fit-cover`, `...img-object-fit-contain` |
| object-fit none / aspect-ratio | downgraded / ignored | `17-image__img-object-fit-none`, `...img-aspect-ratio` |
| rounded avatar fold, srcset | supported / ignored | `17-image__img-rounded-avatar`, `...img-srcset` |
| inline svg shapes/path/gradient/currentColor/stroke/clipPath/filter/use | supported | `18-svg-inline__svg-*` |
| inline svg mask | resolves (XFAIL) | `18-svg-inline__svg-mask` |

## Components (P5 — covered)

`20-components__card / button / badge / navbar / list / stat-tile / gallery-grid` —
small multi-feature integrations, all verify clean.

## Known verify diagnostics (XFAIL)

Subset-legal inputs whose importer / SVG-resolver output currently fails
`pagx verify`. Surfaced (not hidden) via the `XFAIL` bucket in `validate.sh` and
flagged with `known-verify-diagnostic` in the manifest notes:

| Case | Diagnostic |
|------|-----------|
| `15-typography__text-decoration-underline` / `...line-through` | painter leaks geometry (decoration rect shares `<Group>` with text) |
| `14-overflow-clip__clip-path-url` / `...mask-image-svg` | mask sublayer `includeInLayout="false"` in a non-layout parent |
| `18-svg-inline__svg-mask` | painter leaks geometry after mask resolve |
