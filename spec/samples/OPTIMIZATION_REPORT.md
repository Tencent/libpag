# PAGX Sample Files Structural Optimization Report

Total files analyzed: 41  
Files with optimization opportunities: 6  
Total lines reduced: ~115 lines (net)

---

## 1. `5.6_repeater.pagx` (−2 lines)

### 1.1 Remove unnecessary Group wrapper when Layer has single content group

- **Category**: Redundant Group wrapper
- **Before**: Layer → Group → (Rectangle + Fill + Repeater)
- **After**: Layer → (Rectangle + Fill + Repeater)
- **Rationale**: When a Layer contains only one Group with no transform or alpha, the Group is redundant — the Layer itself already provides an isolated geometry accumulation scope. Removing it does not change rendering behavior.

---

## 2. `B.1_complete_example.pagx` (−15 lines)

### 2.1 Remove unused resource: `bgGradient` (LinearGradient)

- **Category**: Unused resource
- **Removed**: `<LinearGradient id="bgGradient" ...>` (6 lines) — defined in `<Resources>` but never referenced via `@bgGradient` anywhere in the file.

### 2.2 Remove unused resource: `teal` (SolidColor)

- **Category**: Unused resource
- **Removed**: `<SolidColor id="teal" color="#14B8A6"/>` (1 line) — defined but never referenced via `@teal` anywhere in the file.

### 2.3 Remove unused resource: `badgeComp` (Composition)

- **Category**: Unused resource
- **Removed**: `<Composition id="badgeComp" ...>` (8 lines) — defined but never referenced via `@badgeComp` anywhere in the file. The original comment even noted "kept for reference but no longer used in main layout".

---

## 3. `B.2_rpg_character_panel.pagx` (−39 lines)

### 3.1 Fix bug: reference to non-existent resource `@glowBlue`

- **Category**: Bug fix (broken resource reference)
- **Changed**: `<Fill color="@glowBlue"/>` → `<Fill color="@glowPurple"/>`
- **Rationale**: No resource with `id="glowBlue"` exists in the file. The only glow gradient resource is `glowPurple`. This is a latent bug that would cause a rendering error.

### 3.2 Merge symmetric path pairs using multi-M path data (×6)

- **Category**: Merge paths with identical style
- **Applies to**: Armor plate lines, Legs, Boots, Boot cuffs, Arms — all representing symmetric left/right body parts with identical Fill/Stroke.
- **Technique**: Combine two `<Path data="..."/>` from separate Groups into a single Path using multiple `M` (moveto) commands: `<Path data="M ... Z M ... Z"/>`.
- **Example** (Legs):
  ```xml
  <!-- Before: two Groups -->
  <Group><Path data="M -15 12 L -18 55 L -8 55 L -5 12 Z"/><Fill color="#2E2545"/></Group>
  <Group><Path data="M 5 12 L 8 55 L 18 55 L 15 12 Z"/><Fill color="#2E2545"/></Group>

  <!-- After: one Group -->
  <Group>
    <Path data="M -15 12 L -18 55 L -8 55 L -5 12 Z M 5 12 L 8 55 L 18 55 L 15 12 Z"/>
    <Fill color="#2E2545"/>
  </Group>
  ```

### 3.3 Merge symmetric ellipse pairs with identical style (×2)

- **Category**: Merge shapes with identical style
- **Applies to**: Knee guards, Shoulders — symmetric left/right parts with identical Fill and Stroke.
- **Technique**: Place both Ellipses in the same Group, sharing one Fill and one Stroke.

### 3.4 Merge identical-geometry Groups with different painters

- **Category**: Merge painters on same geometry
- **Applies to**: Sword blade + glow — both Groups used the exact same Path geometry (`M 46 -30 L 48 -78 L 44 -78 Z`), one applied Fill and the other applied Stroke. Merged into a single Group with both painters.
- **Example**:
  ```xml
  <!-- Before: duplicate geometry -->
  <Group><Path data="M 46 -30 L 48 -78 L 44 -78 Z"/><Fill color="#E2E8F0"/></Group>
  <Group><Path data="M 46 -30 L 48 -78 L 44 -78 Z"/><Stroke color="#FFFFFF40" width="1"/></Group>

  <!-- After: one Group, two painters -->
  <Group>
    <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
    <Fill color="#E2E8F0"/>
    <Stroke color="#FFFFFF40" width="1"/>
  </Group>
  ```

### 3.5 Merge adjacent rectangles in same Group

- **Category**: Merge shapes with identical style
- **Applies to**: Belt buckle — two Rectangles at the same center with same Fill, placed in one Group.

---

## 4. `B.3_nebula_cadet.pagx` (−1 line)

### 4.1 Remove unused resource: `IconBolt` (PathData)

- **Category**: Unused resource
- **Removed**: `<PathData id="IconBolt" data="..."/>` (1 line) — defined in `<Resources>` but never referenced via `@IconBolt` anywhere in the file.

---

## 5. `B.4_game_hud.pagx` (−31 lines)

### 5.1 Merge 4 corner bracket paths into 1 multi-M path

- **Category**: Merge paths with identical style
- **Before**: 4 separate Layers each with a single Path and the same Stroke.
- **After**: 1 Layer with a single Path using 4 M subcommands and one shared Stroke.
- **Lines saved**: 14

### 5.2 Merge enemy blips with identical Fill + DropShadowStyle

- **Category**: Merge shapes with identical style
- **Before**: 2 nested Layers each with Ellipse + Fill + DropShadowStyle (same values).
- **After**: 1 Layer with 2 Ellipses sharing one Fill and one DropShadowStyle.
- **Note**: The player blip (different color, no shadow) was correctly separated into its own Layer. DropShadowStyle is a layer-level style, so when multiple Layers share the same style, the nested Layers can be collapsed if they have identical DropShadowStyle.

### 5.3 Merge radar grid circles and crosshairs

- **Category**: Merge shapes with identical style
- **Before**: 3 separate Layers — large circle, small circle, crosshair lines — each with the same Stroke.
- **After**: 1 Layer with 2 Ellipses + 1 Path sharing one Stroke.
- **Lines saved**: 7

### 5.4 Merge tech bit rectangles with identical Fill

- **Category**: Merge shapes with identical style
- **Before**: 2 Layers each with a Rectangle and the same Fill.
- **After**: 1 Layer with 2 Rectangles sharing one Fill.

### 5.5 Remove redundant inner Layer wrapper in Objectives

- **Category**: Redundant Layer wrapper
- **Before**: Outer Layer (with position) → inner Layer → (Text + Fill + TextLayout + DropShadowStyle)
- **After**: Outer Layer → (Text + Fill + TextLayout + DropShadowStyle) directly.
- **Rationale**: The inner Layer had no transform, alpha, or name. It served no purpose since the outer Layer already provides the scope.

---

## 6. `B.5_pagx_features.pagx` (−79 lines → −77 lines after fix)

### 6.1 Merge ConnectorDots — 5 Groups with identical Fill into 1 scope

- **Category**: Merge shapes with identical style
- **Before**: 5 Groups, each containing one Ellipse + `<Fill color="#06B6D4"/>`.
- **After**: 5 Ellipses + 1 shared Fill directly in the parent Layer scope.
- **Lines saved**: 15

### 6.2 Merge Connectors — 5 Groups with identical Stroke into 1 multi-M path

- **Category**: Merge paths with identical style
- **Before**: 5 Groups, each containing one Path + identical dashed Stroke.
- **After**: 1 Path with 5 M subcommands + 1 shared Stroke.
- **Lines saved**: 20

### 6.3 Merge `</>` icon paths in CardReadable

- **Category**: Merge paths with identical style
- **Before**: 2 Groups for `<` and `>` brackets, each with Path + same Stroke.
- **After**: 1 Group with merged multi-M Path + 1 shared Stroke.

### 6.4 Merge chain-link Ellipses in CardInteroperable

- **Category**: Merge shapes with identical style
- **Before**: 2 Groups, each with one Ellipse + same Stroke.
- **After**: 1 Group with 2 Ellipses + 1 shared Stroke.

### 6.5 Merge rocket fins in CardDeployable

- **Category**: Merge paths with identical style
- **Before**: 2 Groups (left fin, right fin), each with Path + same Stroke + same Fill.
- **After**: 1 Group with merged multi-M Path + 1 shared Stroke + 1 shared Fill.

### 6.6 Merge pipeline arrow shaft + head into single Layer (×2)

- **Category**: Merge paths with identical style across Layers
- **Before**: Each arrow used 2 Layers at the same position — one for the shaft (Path + Stroke) and one for the arrowhead (Path + Stroke with same color/width).
- **After**: 1 Layer with merged multi-M Path + 1 shared Stroke. Note: the merged Stroke uses `join="round"` from the arrowhead variant (superset of the shaft-only `cap="round"`).

### 6.7 Merge 4 corner decoration Layers into 1

- **Category**: Merge layers with identical style
- **Before**: 4 Layers (CornerTopLeft, CornerTopRight, CornerBottomLeft, CornerBottomRight), each with same alpha, containing Path + Stroke + Group(Ellipse + Fill).
- **After**: 1 Layer with 2 Groups — one for all Paths + Stroke, one for all Ellipses + Fill.
- **Important lesson**: When merging, different geometry types that need different painters (Stroke vs Fill) must stay in separate Groups to prevent painter leakage. The initial merge without Groups caused the Fill to apply to the Path as well, creating unintended triangular fills. Fixed by wrapping each painter group in its own `<Group>`.

---

## Optimization Categories Summary

| Category | Occurrences | Description |
|---|---|---|
| **Merge paths with identical style** | 10 | Multiple Paths/Groups with the same Fill/Stroke can be merged into a single Path using multiple `M` (moveto) subcommands |
| **Merge shapes with identical style** | 7 | Multiple shape elements (Ellipse, Rectangle) with the same painters can share one Group |
| **Unused resource** | 4 | Resources defined in `<Resources>` with an `id` but never referenced via `@id` |
| **Redundant Group/Layer wrapper** | 3 | A Group or Layer that has no transform, alpha, name, or blendMode and wraps a single content group |
| **Merge painters on same geometry** | 1 | Two Groups using identical geometry but different painters (Fill vs Stroke) can be merged into one Group |
| **Bug fix** | 1 | Reference to non-existent resource |

## Key Principles

1. **Multi-M Path merging**: When multiple Paths share the exact same painters (Fill, Stroke, or both), they can be combined into a single `<Path data="M ... M ... M ..."/>`. This is the most common and highest-yield optimization.

2. **Painter scope awareness**: In PAGX, painters (Fill/Stroke) apply to ALL accumulated geometry in the current scope. When merging, ensure that different geometry requiring different painters stays in separate Groups. Failure to do so causes painter "leakage".

3. **Unused resource detection**: Search for each `id="xxx"` in `<Resources>` and verify that `@xxx` appears elsewhere in the file. Remove any unreferenced resources.

4. **Redundant wrapper removal**: A Group or Layer that has no attributes (no transform, alpha, blendMode, name, mask, etc.) and contains a single child group is redundant and can be flattened.

5. **Symmetric pattern recognition**: In character designs or UI layouts, left/right or corner patterns are often duplicated with the same style — these are prime merge candidates.
