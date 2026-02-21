# Document Checklist

Reviewers load the check items for the selected fix level and all levels below it.

**Project-specific rules**: Project rules already loaded in the agent's context take
priority over this generic checklist.

---

## Level A: Accuracy

Issues where the document contradicts the actual implementation. Highest impact.

**A1. Code-Document Consistency**
- Do described behaviors match the actual code implementation?
- Are parameter names, types, and default values consistent with code?
- Are return values and error conditions accurately documented?
- Are described algorithms or processing steps consistent with implementation?

**A2. Factual Correctness**
- Are version numbers, format identifiers, and constants correct?
- Are value ranges and constraints accurate?
- Are enum values and their meanings consistent with code definitions?
- Are file format structures (byte offsets, field sizes, etc.) correct?

**A3. Completeness**
- Are all public APIs / features documented?
- Are newly added features or parameters reflected in the document?
- Are removed or deprecated features marked accordingly?
- Are edge cases and limitations documented?

---

## Level B: Clarity & Structure

Improvements to readability and organization. Medium impact.

**B1. Simplification**
- Can verbose descriptions be made more concise without losing meaning?
- Are there redundant paragraphs or sections that repeat the same information?
- Can complex explanations be replaced with tables, lists, or examples?

**B2. Logical Flow**
- Is information presented in a logical order?
- Are related topics grouped together?
- Are forward references minimized (referring to something explained later)?

**B3. Examples & Illustrations**
- Do complex concepts have illustrative examples?
- Are examples correct and consistent with the described behavior?
- Are edge case examples provided where helpful?

**B4. Terminology Consistency**
- Are terms used consistently throughout the document?
- Do terms match those used in the codebase (class names, function names, enum values)?
- Are abbreviations defined on first use?

---

## Level C: Formatting & Style

Polish and stylistic consistency. Lower impact on content quality.

**C1. Formatting Consistency**
- Are headings, lists, and tables formatted consistently?
- Are code snippets properly formatted and syntax-highlighted?
- Are cross-references and links valid?

**C2. Grammar & Wording**
- Are there grammatical errors or awkward phrasing?
- Is the writing style consistent throughout?
- Is the tone appropriate for the target audience?

**C3. Section Organization**
- Are sections appropriately sized (not too long or too short)?
- Is the table of contents (if present) consistent with actual sections?
- Are deprecated or obsolete sections cleaned up?

---

## Exclusion List

> **Important**: Project rules loaded in context take priority over this exclusion list.

**Issue types excluded by default:**

1. Pure formatting preferences not affecting readability
2. Stylistic rewrites that don't improve clarity or accuracy
3. Speculative issues without evidence from the codebase
4. Suggestions to add content beyond the document's stated scope
