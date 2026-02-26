# Document Review Checklist

Review in priority order: A (highest impact) → B → C. The reviewer prompt specifies
which levels to check. Project rules loaded in context override this checklist.

---

## A. Accuracy

Issues where the document contains incorrect, contradictory, or incomplete information.

### A1. Code-Document Consistency
- Described behaviors match actual code implementation
- Parameter names, types, default values consistent with code
- Return values and error conditions accurately documented
- Described algorithms / processing steps consistent with implementation

### A2. Factual Correctness
- Version numbers, format identifiers, constants correct
- Value ranges and constraints accurate
- Enum values and meanings consistent with code definitions
- File format structures (byte offsets, field sizes, etc.) correct

### A3. Internal Consistency
- Different sections describing the same concept agree with each other
- Constraints and rules consistent across the document (no contradictions like
  "must be >= 0" in one section with a negative default in another)
- Same rule appearing in multiple places identical in meaning

### A4. Completeness
- All public APIs / features documented
- Newly added features or parameters reflected in the document
- Removed or deprecated features marked accordingly
- Edge cases and limitations documented

### A5. Logical Completeness
- All conditional branches exhaustively covered (no undocumented "else" cases)
- Sequential steps complete with no missing intermediate steps
- Undefined behaviors identified (input combinations with no documented result)

### A6. Reference Validity
> Do not attempt to verify URL reachability.
- Internal cross-references point to existing sections or files
- External links / URLs well-formed and not obviously outdated
- Referenced file paths, tool names, command examples actually exist

---

## B. Clarity & Structure

Improvements to readability, unambiguity, and organization.

### B1. Ambiguity Detection
- No descriptions interpretable in multiple ways
- Conditional statements precise ("should" vs "must", "may" vs "will")
- Boundary conditions clearly stated (inclusive vs exclusive, "at least" vs "exactly")

### B2. Simplification
- Verbose descriptions made concise without losing meaning
- No redundant paragraphs or sections repeating the same information
- Complex explanations replaced with tables, lists, or examples where possible

### B3. Logical Flow
- Information presented in logical order
- Related topics grouped together
- Forward references minimized

### B4. Examples & Illustrations
- Complex concepts have illustrative examples
- Examples correct and consistent with described behavior
- Edge case examples provided where helpful

### B5. Terminology Consistency
- Terms used consistently throughout the document
- Terms match codebase usage (class names, function names, enum values)
- Abbreviations defined on first use

---

## C. Formatting & Style

Polish and stylistic consistency.

### C1. Formatting Consistency
- Headings, lists, tables formatted consistently
- Code snippets properly formatted and syntax-highlighted

### C2. Grammar & Wording
- No grammatical errors or awkward phrasing
- Writing style consistent throughout
- Tone appropriate for target audience

### C3. Section Organization
- Sections appropriately sized (not too long or too short)
- Table of contents (if present) consistent with actual sections
- Deprecated or obsolete sections cleaned up

---

## Exclusion List

> Project rules override this exclusion list.

1. Pure formatting preferences not affecting readability
2. Stylistic rewrites that don't improve clarity or accuracy
3. Suggestions based on assumed future requirements, not current content
4. Suggestions to add content beyond the document's stated scope
