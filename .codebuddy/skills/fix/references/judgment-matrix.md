# Judgment Matrix

Team-lead uses this matrix to decide whether each confirmed issue is worth fixing,
based on the selected fix level and the module type (code or document).

## Core Principles

- **Public API protection**: Do not casually modify public API function signatures or
  class definitions. Focus on internal refactoring.
- Issues involving public API signature changes: fix obvious bugs directly; record
  others to `pending-api-changes.md` for final confirmation.

## Code Modules

| Type | Min Level | Criteria |
|------|-----------|----------|
| Logic bug | A | Affects runtime correctness -> must fix |
| Security (div-by-zero / OOB / uninitialized read) | A | Must fix |
| Resource leak (handle / lock not released) | A | Must fix |
| Memory safety (use-after-move / dangling ref) | A | Must fix |
| Thread safety violation | A | Must fix |
| Public API signature change (obvious bug) | A | Must fix |
| Performance optimization | B | **Must be 100% certain of semantic equivalence; skip if uncertain** |
| Code simplification | B | Fix when logic can be clearly simplified (early return, branch merge, etc.) |
| Duplicate code extraction | B | Fix when >= 3 identical patterns |
| Container pre-allocation | B | Fix when size is predictable **and** on a hot path |
| Architecture improvement | B | Fix only when dependency direction or responsibility division is clearly wrong |
| Interface usage | B | Fix when API is used against its design intent |
| Test coverage gap | B | Flag when changed logic lacks test coverage |
| Regression risk | B | Flag when modification may affect other callers |
| Naming convention violation | C | Fix only when inconsistent with project rules loaded in context |
| Missing variable initialization | C | Fix when declared without initial value (per project rules) |
| Public API comment inaccuracy | A | Must fix — comments that mislead API consumers are correctness issues |
| Comment / documentation issue | C | Fix when internal docs are missing or style is inconsistent |
| Function implementation order | C | Fix only when clearly inconsistent with header declaration order |
| Type safety (narrowing / magic numbers) | C | Only change local variables, never change function signatures |
| Const correctness | C | Fix when clearly applicable and low risk |
| File organization | C | Fix only when clearly inconsistent with project conventions |
| Public API signature change (not a bug) | -- | **Do not fix directly**, record to `pending-api-changes.md` for final confirmation |
| Style preference | -- | **Always skip** |

## Document Modules

| Type | Min Level | Criteria |
|------|-----------|----------|
| Contradicts code implementation | A | Must fix — verify against actual code |
| Incorrect values / constants / ranges | A | Must fix |
| Internal contradiction between sections | A | Must fix |
| Missing documentation for existing features | A | Fix when public API or feature is undocumented |
| Incomplete conditional branches or steps | A | Fix when undefined behavior or missing steps found |
| Broken internal/external references | A | Must fix |
| Ambiguous description with multiple interpretations | B | Fix when it could lead to incorrect understanding |
| Verbose or redundant description | B | Fix when clearly simplifiable without losing meaning |
| Poor logical flow or organization | B | Fix when information order is confusing |
| Missing examples for complex concepts | B | Fix when concept is hard to understand without example |
| Inconsistent terminology with codebase | B | Fix when terms differ from code identifiers |
| Formatting inconsistency | C | Fix only when it affects readability |
| Grammar or wording issues | C | Fix only when meaning is unclear |
| Stylistic preference | -- | **Always skip** |

## Special Rules

- Type narrowing fixes: only change local variables, never function signatures
- Cross-module renames: assign as an atomic task to a single fixer
- Issues rolled back in a previous round: **do not attempt again this round**
