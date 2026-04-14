Check **only** the items listed below — do not inspect anything outside this
checklist. Think through each item, but only output the problems found — do not
output passing items or analysis. Reference the `line` attribute from layout XML
when reporting issues. If no issues are found, output only "No issues found."

Layout XML key attributes:
- `bounds="x,y,width,height"` — position and size in canvas coordinates.

### Layout

Apply these checks when the design intent is a structured layout (UI screens, cards,
dashboards, tables, etc.). Skip for freeform artwork or illustrations.

1. **Alignment and spacing**: Within each container, siblings should be aligned
   (sharing the same edge or center line) with consistent spacing between them.
   Spacing should reflect hierarchy — gaps between sections should not be smaller
   than gaps between elements within a section. When a sibling has a visible
   background, measure the gap from that background's edge — flag if smaller than
   the element's own internal padding.
2. **Inset symmetry**: For every container with a visible background (card, button,
   badge, icon container), compute insets from background bounds to content bounds
   in the layout XML. Left/right should be equal; top/bottom should be equal.
   Flag any pair differing by more than 2px. Cross-check the screenshot for
   visually off-center content.
3. **Cross-element alignment**: Elements at the same logical level across different
   containers should be aligned on the same edge or center line — e.g., headings
   in adjacent columns start at the same y, or centered elements share the same
   center x.
4. **Repetition consistency**: Repeating structures (list items, cards, tabs) should
   have identical internal layout across every instance — same height, margins,
   padding, and spacing. Width may vary when sized by content (e.g., text-length
   buttons). Flag any instance whose internal structure deviates from the pattern.

### Content

5. **Completeness**: Check the screenshot and layout XML against the design intent.
   Flag any described feature missing entirely, or any element that has bounds in
   layout XML but is not visible in the screenshot.
6. **Text truncation and clipping**: In the screenshot, check whether any text
   appears truncated, clipped, or overflowing its container. Also check for any
   child content being unexpectedly clipped by its parent.
7. **Text spacing**: In the screenshot, verify lines are not overlapping or
   excessively spaced, and letter spacing looks natural.
8. **Text legibility**: Is all text readable? Check for insufficient contrast
   or unexpected weight.
9. **Icon rendering**: In the screenshot, check each icon or small graphic for
    path corruption (broken lines, missing segments, unexpected fills), distortion,
    or poor clarity at the rendered size.
10. **Icon semantics**: Infer each icon's intended meaning from its Layer id/name
    and adjacent text. Then critically examine the screenshot — could the shape be
    mistaken for a different common icon? Flag any icon that is ambiguous,
    unrecognizable, or more likely to be read as something else.

### Visual fidelity

11. **Canvas sizing**: Content should fill the canvas without being clipped at the
    bottom or leaving excessive blank space. Check the screenshot for large empty
    areas that suggest the canvas is oversized.
12. **Unexpected visuals**: In the screenshot, look for shapes with unexpected
    fills or strokes, elements accidentally covering others, or any visual
    artifact that doesn't match the design intent.
13. **Overall impression**: Compare the screenshot holistically to the design
    intent. Check visual hierarchy (clear dominant region), visual balance,
    content density, and rhythm in multi-section designs. Pay special attention
    to cross-section consistency — colors, font sizes, spacing, and component
    styles should be uniform throughout. Flag anything that feels "off".
