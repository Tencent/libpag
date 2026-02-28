# Icon Examples

Back to main: [SKILL.md](../SKILL.md)

Icon-specific examples and pitfalls. For universal methodology (rendering technique, layer
decisions, verification loop), follow `generation-guide.md` first.

---

## Structure Examples

- **1 Layer** — standalone flat symbol, no background/foreground separation.
- **2 Layers** — background shape + foreground symbol. The most common icon pattern.
- **3 Layers** — background + highlight overlay + foreground. Use when the reference shows
  clear depth layering (e.g., a two-tone background).

## Technique Preference

When in doubt, **prefer Stroke** for icons. At icon scale, Stroke produces acceptable results
with fewer iterations — 2-3px coordinate imprecision is barely visible at wider stroke widths.

## Icon-Specific Checks

In addition to the standard verification loop in `generation-guide.md`:

- **Foreground containment**: Verify via bounds that the foreground fits within the
  background with adequate padding on all sides.
- **Batch consistency**: When generating a set, all icons should have similar overall bounds
  dimensions. An outlier that's much larger or smaller breaks visual consistency.

## Common Pitfalls

- **Over-detailing**: The most common failure. Small dots, thin trend lines, tiny arrows,
  text labels inside icons — all become noise at icon scale. Icons communicate through shape
  and silhouette, not fine detail. When in doubt, remove it.
- **Path complexity mismatch**: Representing a complex real-world object (gear, camera) with
  a filled Path leads to fragile geometry. Solutions: simplify to essential silhouette, switch
  from Fill to Stroke, or compose from simpler primitives.
