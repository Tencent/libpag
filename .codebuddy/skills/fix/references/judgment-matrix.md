# Judgment Matrix

Team-lead uses this matrix to decide whether each confirmed issue is worth fixing,
based on the selected fix level.

## Core Principles

- **Public API protection**: Do not casually modify public API function signatures or
  class definitions. Focus on internal refactoring.
- Issues involving public API signature changes: fix obvious bugs directly; record
  others to `pending-api-changes.md` for final confirmation.

## Matrix

| Type | Min Level | Criteria |
|------|-----------|----------|
| Logic bug | A | Affects runtime correctness -> must fix |
| Security (div-by-zero / OOB / uninitialized read) | A | Must fix |
| Resource leak (handle / lock not released) | A | Must fix |
| Project convention violation | B | Fix only when inconsistent with project rules loaded in context |
| Missing variable initialization | B | Fix when declared without initial value (per project rules) |
| API comment missing | B | Fix when public API lacks param/return description (comments only, no signature changes) |
| Function implementation order | B | Fix only when clearly inconsistent with header declaration order |
| Code simplification | C | Fix when logic can be clearly simplified (early return, branch merge, etc.) |
| Duplicate code extraction | C | Fix when >= 3 identical patterns |
| Container pre-allocation | C | Fix when size is predictable **and** on a hot path |
| Unnecessary copy / perf optimization | C | **Fix only when 100% certain of semantic equivalence; skip if uncertain** |
| Type safety | C | Only change local variables, never change function signatures |
| Interface design improvement | D | **Comments and documentation level only**, no signature changes |
| Documentation consistency | D | Fix when code and documentation disagree |
| Architecture suggestion | D | Fix only when dependency direction or responsibility division is clearly wrong |
| Public API signature change (obvious bug) | A | Must fix |
| Public API signature change (not a bug) | -- | **Do not fix directly**, record to `pending-api-changes.md` for final confirmation |
| Style preference | -- | **Always skip** |

## Special Rules

- Optimizations that change output semantics (may invalidate test baselines) -> **skip
  by default**
- Type narrowing fixes: only change local variables, never function signatures
- Cross-module renames: assign as an atomic task to a single fixer
- Issues rolled back in a previous round: **do not attempt again this round**
