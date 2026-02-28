# PAGX Optimization Guide

Complete workflow for optimizing existing PAGX files — structure, performance, and correctness.
Also used as the final step after generating a new PAGX file.

## References

Read these as needed during optimization:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `structure-optimization.md` | Layer/Group semantics, coordinate localization, redundant nesting |
| `painter-merging.md` | Duplicate painter elimination, cross-layer merging |
| `resource-reuse.md` | Composition extraction, PathData sharing, color source dedup |
| `performance.md` | Rendering cost model, Repeater limits, mask optimization, blur cost |
| `pagx-quick-reference.md` | Attribute defaults, enumerations, required attributes |
| `cli-reference.md` | CLI tool usage — `optimize`, `render`, `validate`, `bounds` commands |

---

## Fundamental Constraint

All optimizations must preserve the original design appearance. Never modify visual attributes
(colors, blur, spacing, opacity, font sizes, etc.) without explicit user approval.

---

## Optimization Workflow

### Step 1: Automated Optimization

Run `pagx optimize` to apply safe, mechanical transformations in one step — empty element
removal, resource deduplication, Path→primitive replacement, coordinate localization, and
consistent formatting. It also validates the file and aborts with errors if the input is
invalid:

```bash
pagx optimize -o output.pagx input.pagx
```

### Step 2: Structural Review

Check for issues that automated optimization cannot fix. See `structure-optimization.md` and
`painter-merging.md` for detailed patterns and examples:

- **Redundant Layer nesting** — child Layers that should be Groups (no styles, filters, or
  mask needed).
- **Duplicate painters** — identical Fill/Stroke across Groups or Layers that could share
  scope.
- **Scattered blocks** — one logical visual unit split across multiple sibling Layers that
  should be merged.
- **Over-packed Layers** — multiple independent visual units crammed into one Layer that
  should be separated.
- **Manual text positioning** — hand-positioned Text elements that should use TextBox for
  automatic layout.

### Step 3: Performance Review

Check for rendering performance issues. See `performance.md` for the full cost model and
optimization patterns:

- **Unnecessary Layers** — child Layers not using Layer-exclusive features (styles, filters,
  mask, blendMode) should be downgraded to Groups to eliminate extra surface allocation.
- **Repeater count** — single Repeater should stay under ~200 copies; nested Repeater product
  under ~500.
- **Blur cost** — BlurFilter / DropShadowStyle cost is proportional to blur radius. Use the
  smallest radius that achieves the visual effect.
- **Stroke alignment** — `align="center"` (default) is GPU-accelerated; `"inside"` or
  `"outside"` require CPU operations.
- **Mask type** — opaque solid-color fills → fast clip path; gradients with transparency or
  images → slower texture mask. Prefer `scrollRect` over mask for rectangular clipping.
- **Path complexity** — a single Path with >15 curve segments is fragile and slow. Consider
  decomposing into simpler primitives (Rectangle, Ellipse). See `resource-reuse.md` for
  Path→primitive replacement patterns.

### Step 4: Final Verification Checklist

After all optimizations, verify the following. Each item links to where the rule is defined:

- [ ] All `<pagx>`/`<Composition>` direct children are `<Layer>` — not Group
  (`spec-essentials.md` §1)
- [ ] All required attributes present; no redundant default-value attributes
  (`pagx-quick-reference.md`)
- [ ] Painter scope isolation correct — different painters in Groups, same painters shared
  (`spec-essentials.md` §4, `painter-merging.md`)
- [ ] Text `position`/`textAnchor` not set when TextBox is present
  (`spec-essentials.md` §7 TextBox)
- [ ] Internal coordinates relative to Layer origin, not canvas-absolute
  (`structure-optimization.md`)
- [ ] `<Resources>` placed after all Layers; all `@id` references resolve
  (`spec-essentials.md` §10)
- [ ] Repeater copies reasonable (~200 single, ~500 nested product)
  (`performance.md`)
- [ ] Visual stacking order preserved
  (`structure-optimization.md` — all-or-nothing rule)
- [ ] Rendered screenshot matches expected design (layout, alignment, consistent spacing)

> **Stacking order caveat**: When downgrading child Layers to Groups, Layer contents (Groups,
> geometry) always render below child Layers regardless of XML order. Partial downgrade can
> break stacking — either downgrade all qualifying siblings or none. See
> `structure-optimization.md` for the all-or-nothing rule.
