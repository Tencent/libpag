# Icon Examples

Icon-specific examples and pitfalls. For universal methodology (generation steps,
verification loop), follow `generate-guide.md` first.

**Note**: Colors and sizes in these examples are placeholders to illustrate structural patterns.
Always match the actual design requirements.

---

## Structure Examples

### 1 Layer — Flat Symbol

No background, just the symbol itself:

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <Path data="M -40,-50 L 0,50 L 40,-50 M 0,50 L 0,-30"/>
    <Stroke color="#3B82F6" width="8" cap="round" join="round"/>
  </Layer>
</pagx>
```

**Pattern**: Single Layer centered on canvas. Stroke-based — few path points, tolerant of
imprecision. Layer `x`/`y` at canvas center, path coordinates symmetric around origin.

### 2 Layers — Background + Foreground (Stroke)

The most common icon structure:

```xml
<pagx version="1.0" width="200" height="200">
  <!-- Background -->
  <Layer x="100" y="100">
    <Ellipse size="180,180"/>
    <Fill color="#EFF6FF"/>
  </Layer>
  <!-- Foreground symbol -->
  <Layer x="100" y="100">
    <Path data="M -30,-30 L 30,30 M 30,-30 L -30,30"/>
    <Stroke color="#3B82F6" width="7" cap="round"/>
  </Layer>
</pagx>
```

**Pattern**: Background and foreground share the same `x`/`y` (canvas center). Foreground
path is symmetric around origin. Verify with `pagx bounds` that foreground fits within
background with adequate padding.

### 2 Layers — Background + Foreground (Fill)

Fill-based variant for solid shapes:

```xml
<pagx version="1.0" width="200" height="200">
  <!-- Background -->
  <Layer x="100" y="100">
    <Rectangle size="160,160" roundness="32"/>
    <Fill color="#ECFDF5"/>
  </Layer>
  <!-- Foreground symbol -->
  <Layer x="100" y="100">
    <Group>
      <Path data="M -20,10 L -5,-20 L 25,-20"/>
      <Stroke color="#10B981" width="6" cap="round" join="round"/>
    </Group>
    <Group>
      <Ellipse center="0,15" size="40,25"/>
      <Fill color="#10B981"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Mixed technique — Stroke for line details, Fill for solid areas. Groups isolate
different painters within the foreground Layer.

### 3 Layers — Background + Accent + Foreground

For icons with visible depth:

```xml
<pagx version="1.0" width="200" height="200">
  <!-- Background -->
  <Layer x="100" y="100">
    <Ellipse size="180,180"/>
    <Fill color="#FEF3C7"/>
  </Layer>
  <!-- Accent ring -->
  <Layer x="100" y="100">
    <Ellipse size="140,140"/>
    <Fill color="#FDE68A"/>
  </Layer>
  <!-- Foreground symbol -->
  <Layer x="100" y="100">
    <Path data="M 0,-30 L 0,20 M -15,5 L 0,20 L 15,5"/>
    <Stroke color="#D97706" width="6" cap="round" join="round"/>
  </Layer>
</pagx>
```

**Pattern**: Three concentric layers, all sharing `x`/`y`. Middle layer creates depth without
a third rendering technique — just a second fill color.

---

## Technique Preference

When in doubt, **prefer Stroke** for icons. At icon scale, Stroke produces acceptable results
with fewer iterations — 2-3px coordinate imprecision is barely visible at wider stroke widths.

---

## Icon-Specific Checks

In addition to the standard verification loop in `generate-guide.md`:

- **Foreground containment**: Verify via bounds that the foreground fits within the
  background with adequate padding on all sides.
- **Batch consistency**: When generating a set, all icons should have similar overall bounds
  dimensions. An outlier that's much larger or smaller breaks visual consistency.

---

## Common Pitfalls

- **Over-detailing**: The most common failure. Small dots, thin trend lines, tiny arrows,
  text labels inside icons — all become noise at icon scale. Icons communicate through shape
  and silhouette, not fine detail. When in doubt, remove it.
- **Path complexity mismatch**: Representing a complex real-world object (gear, camera) with
  a filled Path leads to fragile geometry. Solutions: simplify to essential silhouette, switch
  from Fill to Stroke, or compose from simpler primitives.
