# Icon Generation Workflow

Back to main: [SKILL.md](../SKILL.md)

Supplements `generation-guide.md` with icon-specific decisions. Follow the general generation
steps (analyze → decompose → build incrementally → verify) from that document; this file
covers the icon-specific choices within that framework.

---

## Structure Decision

Based on the reference analysis (Step 1 in `generation-guide.md`), choose Layer architecture:

- **1 Layer** — standalone flat symbol, no background/foreground separation.
- **2 Layers** — background shape + foreground symbol. The most common pattern.
- **3 Layers** — background + highlight overlay + foreground. Use when the reference shows
  clear depth layering (e.g., a two-tone background).

Minimize layer count — each additional Layer adds alignment complexity. Use Groups (not
child Layers) for internal structure within a layer.

---

## Rendering Technique

The most impactful decision for generation success.

**Stroke (line art)**: Define path skeletons, apply `Stroke` with a width. Few path points
needed, high tolerance for coordinate imprecision (2-3px off is barely visible at
`width="5"`). Use `cap="round"` and `join="round"` for a polished look.

**Fill (solid shapes)**: Define closed paths, apply `Fill`. Requires precise control points —
complex paths (>15 curve segments) are fragile. Prefer Rectangle/Ellipse for standard shapes.

**Mixed**: Fill background + Stroke foreground. A common and effective combination.

**When in doubt, prefer Stroke.** It produces acceptable results with fewer iterations.

---

## Icon-Specific Verification

In addition to the standard verification checklist in `generation-guide.md`:

- **Asymmetric centering**: Shapes that extend more in one direction (cloud, rocket) have
  visual centers that differ from Layer `x`/`y`. Always use `pagx bounds` to confirm.
- **Foreground containment**: Verify via bounds that the foreground fits within the
  background with adequate padding on all sides.
- **Batch consistency**: When generating a set, all icons should have similar overall bounds
  dimensions. An outlier that's much larger or smaller breaks visual consistency.

---

## Icon-Specific Pitfalls

- **Over-detailing**: The most common failure. Small dots, thin trend lines, tiny arrows,
  text labels inside icons — all become noise at icon scale. Icons communicate through shape
  and silhouette, not fine detail. When in doubt, remove it.
- **Path complexity mismatch**: Representing a complex real-world object (gear, camera) with
  a filled Path leads to fragile geometry. Solutions: simplify to essential silhouette, switch
  from Fill to Stroke, or compose from simpler primitives.
