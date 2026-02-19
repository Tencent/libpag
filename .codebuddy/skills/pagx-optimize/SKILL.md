---
name: pagx-optimize
description: Optimize PAGX file structure by removing redundancy, merging shared painters, extracting reusable resources, and improving readability. Use this skill when asked to optimize, simplify, or clean up a PAGX file.
argument-hint: "[file-path]"
---

# PAGX File Structure Optimization

This skill provides a systematic checklist for optimizing PAGX file structure. The goals are
to simplify file structure, reduce file size, and improve rendering performance.

## Fundamental Constraint

**All optimizations must preserve the original design appearance.** This is a hard requirement
that overrides any individual optimization direction below.

- **Allowed**: Structural transformations that produce identical or near-identical rendering
  (minor pixel-level differences from node reordering or painter merging are acceptable).
- **Allowed**: Removing provably invisible content — elements entirely outside the canvas
  bounds, unused resources, zero-width strokes, fully transparent elements, and other content
  that contributes nothing to the final rendered image.
- **Forbidden**: Modifying any parameter that affects design intent — blur radius, spacing,
  density, colors, alpha, gradient stops, font sizes, shadow offsets, stroke widths, or any
  other visual attribute — unless the user explicitly approves the change.
- **Forbidden**: Reducing Repeater density (increasing spacing), lowering blur values, changing
  opacity, or simplifying geometry in ways that alter the visual result, even if the change
  seems minor. These are design decisions, not optimization decisions.
- **When in doubt**: If an optimization might change the rendered appearance, do not apply it.
  Instead, describe the potential optimization and its visual impact to the user, and ask for
  explicit approval before proceeding.

---

## Core Concepts (Quick Reference)

**Painter Scope**: Fill/Stroke paints all geometry accumulated in the current scope. Group creates
an isolated scope — geometry inside only affects painters inside that Group.

**Layer vs Group**: Layer creates an independent rendering surface (heavier, supports styles/filters/mask).
Group is a lightweight scope boundary (no extra surface, propagates geometry to parent).

**Layer = Logical Block**: Each Layer should represent one visually and logically independent
block (a card, a button, a bar). Multiple Layers stacked to express the same block should be
merged. Multiple distinct blocks crammed into one Layer should be split.

**Critical Constraint**: `<pagx>` and `<Composition>` direct children **MUST** be Layer.
Groups at root level are silently ignored.

---

## Optimization Checklist

### Structure Cleanup

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 1 | Move Resources to End | `<Resources>` appears before layer tree |
| 2 | Remove Empty Elements | Empty `<Layer/>`, empty `<Resources>`, `width="0"` strokes |
| 3 | Omit Default Values | Attributes explicitly set to spec default |
| 4 | Simplify Transforms | Translation-only matrix, identity matrix, cascaded translations |
| 5 | Normalize Numerics | Scientific notation near zero, trailing decimals, short hex |
| 6 | Remove Unused Resources | Resource `id` has no `@id` reference |
| 7 | Remove Redundant Wrappers | Group/Layer with no attributes wrapping single element |
| 8 | Downgrade Layer to Group | Layer uses none of: styles, filters, mask, blendMode, composition, scrollRect; AND is not a `<pagx>`/`<Composition>` direct child; AND has no child Layers; AND is stacked with siblings to express the same logical block. **High value**: removes extra rendering surface |
| 9 | Merge Overlapping Layers | Multiple adjacent Layers at same position forming one logical unit (e.g., button bg + label). Wrap in one Layer, downgrade non-exclusive children to Group |
| 10 | Split Over-packed Layer | One Layer contains multiple distinct logical blocks (e.g., "Box A" + "Box B" test areas). Each block should be its own Layer |
| 11 | Localize Internal Coordinates | Layer x/y carries block offset; internal elements use origin-relative coordinates instead of absolute canvas coordinates |

> For detailed examples and default value tables, read `references/structure-cleanup.md`.
> For Layer vs Group comparison, downgrade rules, merge/split guidance, and coordinate
> localization, read `references/layer-vs-group.md`.

> **Caveat**: Some attributes look optional but are required. `ColorStop.offset` has no default —
> omitting it causes parsing errors. See `references/structure-cleanup.md` for the full list.

> **Layer→Group is high-impact but has strict prerequisites.** Always verify the downgrade
> checklist in `references/layer-vs-group.md` before applying. Incorrect downgrade can cause
> content to disappear (at root level), change visual stacking order, or produce parsing
> errors (styles/filters are invalid inside Group).
>
> **Key principle**: Only downgrade Layers that are stacked to express the **same** logical
> block. If a Layer represents a distinct independent block (visually and logically separate),
> it should remain a Layer — do not over-optimize by merging unrelated blocks.

### Painter Merging

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 12 | Merge Geometry Sharing Identical Painters | Multiple geometry elements use same Fill/Stroke |
| 13 | Merge Painters on Identical Geometry | Same geometry appears twice with different painters |

**Critical caveat**: Different geometry needing different painters must be isolated with Groups.
This is the most common source of errors.

> For detailed examples, scope isolation patterns, and painter scope concepts, read
> `references/painter-merging.md`.

### Text Layout

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 14 | Use TextBox for Multi-Text Layout | Multiple Text elements positioned manually for sequential display |
| 15 | Merge Text Layers into Groups | Each text segment in its own Layer when Group suffices |

Multiple Text elements that form a sequential block (e.g., title + subtitle, paragraph lines)
should share a single TextBox for automatic layout instead of using absolute position
coordinates on each Text. Each text segment should be in a Group (for style isolation), not
a separate Layer.

> **TextBox ignores** Text's `position` and `textAnchor` attributes. When using TextBox, do not
> set these on child Text elements.

**Before** (absolute positioning):
```xml
<Layer>
  <Group position="200,60">
    <Text text="Title" fontFamily="Arial" fontSize="36" textAnchor="center"/>
    <Fill color="#000"/>
  </Group>
  <Group position="200,100">
    <Text text="Subtitle" fontFamily="Arial" fontSize="24" textAnchor="center"/>
    <Fill color="#666"/>
  </Group>
</Layer>
```

**After** (TextBox auto-layout):
```xml
<Layer>
  <Group>
    <Text text="Title&#10;" fontFamily="Arial" fontSize="36"/>
    <Fill color="#000"/>
  </Group>
  <Group>
    <Text text="Subtitle" fontFamily="Arial" fontSize="24"/>
    <Fill color="#666"/>
  </Group>
  <TextBox position="200,60" textAlign="center"/>
</Layer>
```

### Resource Reuse

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 16 | Composition Reuse | 2+ identical layer subtrees differing only in position |
| 17 | PathData Reuse | Same path data string appears 2+ times |
| 18 | Color Source Sharing | Identical gradient definitions inline in multiple places |
| 19 | Replace Path with Primitive | Path describes a Rectangle or Ellipse |
| 20 | Remove Full-Canvas Clips | Clip mask covers entire canvas |

> For detailed examples and coordinate conversion formulas, read `references/resource-reuse.md`.

### Performance Optimization

**Rendering Cost Model**:
- **Repeater**: N copies = N× full rendering cost (no GPU instancing)
- **Nested Repeaters**: Multiplicative (A×B elements)
- **BlurFilter / DropShadowStyle**: Cost proportional to blur radius
- **Dashed Stroke under Repeater**: Dash computation per copy
- **Layer vs Group**: Layer creates extra surface; Group is lighter

**Two categories**:
1. **Equivalent (auto-apply)**: Downgrade Layer to Group, clip Repeater to canvas bounds
2. **Suggestions (never auto-apply)**: Reduce density, lower blur, simplify geometry

> For detailed optimization techniques, read `references/performance.md`.

---

## PAGX Specification Quick Reference

For complete attribute defaults, required attributes, and enumeration values extracted from
the PAGX spec, see `references/pagx-quick-reference.md`.
