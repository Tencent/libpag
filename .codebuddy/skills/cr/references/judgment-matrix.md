# Judgment Matrix

Team-lead uses this matrix in Phase 4 to decide whether each confirmed issue should be
auto-fixed, recorded to `pending-issues.md` for user confirmation, or skipped entirely.

## Risk Level Assessment

For each confirmed issue, the team-lead assigns a risk level that determines how it is
handled. The user's chosen auto-fix threshold controls which risk levels are auto-fixed.

**Low risk**: the fix is the only correct approach — any experienced developer would
agree without discussion.
- Examples: null check, fix incorrect comment, rename non-conforming variable, remove
  redundant duplicate code, add `reserve`, fix obvious off-by-one error

**High risk**: the fix involves a design decision or changes an external contract —
the code author or owner should weigh in.
- Examples: public API change (signature, behavior, deprecation), test baseline change,
  architecture restructuring, algorithm replacement with multiple viable approaches,
  introducing a new dependency, changing data persistence format or serialization
  protocol, changing threading model or concurrency architecture, performance
  optimization involving space-time trade-offs

**Medium risk**: the fix approach is clear but the impact extends beyond the immediate
locality — the team-lead has enough context to judge, but it is not as self-evident as
low risk.
- Examples: extracting shared logic across functions, removing unused internal methods,
  simplifying cross-function control flow, adjusting internal module boundaries

## Handling by Risk Level

| User threshold | Low risk | Medium risk | High risk |
|----------------|----------|-------------|-----------|
| Full auto      | Auto-fix | Auto-fix    | Auto-fix  |
| Low + Medium   | Auto-fix | Auto-fix    | Confirm   |
| Low only       | Auto-fix | Confirm     | Confirm   |
| All confirm    | Confirm  | Confirm     | Confirm   |

**Special rule for "Full auto"**: issues that would change test baselines (screenshot
comparisons, golden files) are always deferred for user confirmation, regardless of
risk level.

## Code Modules

The "Criteria" column below determines whether an issue is worth fixing. The risk level
is **not** determined by issue type — it is assessed per-issue based on the risk level
definitions above. For example, a "Logic bug" with an obvious one-line fix is low risk,
while a logic bug requiring cross-module restructuring is high risk.

| Type | Criteria |
|------|----------|
| Logic bug | Affects runtime correctness -> must fix |
| Security (div-by-zero / OOB / uninitialized read) | Must fix |
| Injection / XSS (unsanitized user input in DOM) | Must fix `[Web]` |
| Sensitive data exposure (keys / tokens in client code) | Must fix `[Web]` |
| Resource leak (handle / lock not released) | Must fix |
| Memory safety (use-after-move / dangling ref) | Must fix |
| Event listener / timer / subscription leak on unmount | Must fix `[Web]` |
| Thread safety violation | Must fix |
| Public API signature change (obvious bug) | Must fix |
| Public API comment inaccuracy | Must fix — comments that mislead API consumers are correctness issues |
| Performance optimization | **Must be 100% certain of semantic equivalence; skip if uncertain** |
| Unnecessary re-renders / missing memoization | Fix when measurably impactful `[Web]` |
| Full-library import when partial suffices | Fix when tree-shaking is clearly ineffective `[Web]` |
| Code simplification | Fix when logic can be clearly simplified (early return, branch merge, etc.) |
| Duplicate code extraction | Fix when >= 3 identical patterns |
| Container pre-allocation | Fix when size is predictable **and** on a hot path |
| Architecture improvement | Fix only when dependency direction or responsibility division is clearly wrong |
| Interface usage | Fix when API is used against its design intent |
| Rendering correctness (stale key / stale closure / wrong deps) | Fix when it causes incorrect UI behavior `[Web]` |
| Test coverage gap | Flag when changed logic lacks test coverage |
| Regression risk | Flag when modification may affect other callers |
| Naming convention violation | Fix only when inconsistent with project rules loaded in context |
| Missing variable initialization | Fix when declared without initial value (per project rules) |
| Comment / documentation issue | Fix when internal docs are missing or style is inconsistent |
| Function implementation order | Fix only when clearly inconsistent with header declaration order |
| Type safety (narrowing / magic numbers) | Only change local variables, never change function signatures |
| Const correctness | Fix when clearly applicable and no semantic change |
| File organization | Fix only when clearly inconsistent with project conventions |
| Accessibility (missing alt / label / keyboard nav) | Fix when semantic HTML or ARIA is clearly missing `[Web]` |
| Public API signature change (not a bug) | Fix when justified by clear benefit to API consumers |
| Style preference | **Always skip** |

## Document Modules

| Type | Criteria |
|------|----------|
| Contradicts code implementation | Must fix — verify against actual code |
| Incorrect values / constants / ranges | Must fix |
| Internal contradiction between sections | Must fix |
| Missing documentation for existing features | Fix when public API or feature is undocumented |
| Incomplete conditional branches or steps | Fix when undefined behavior or missing steps found |
| Broken internal/external references | Must fix |
| Ambiguous description with multiple interpretations | Fix when it could lead to incorrect understanding |
| Verbose or redundant description | Fix when clearly simplifiable without losing meaning |
| Poor logical flow or organization | Fix when information order is confusing |
| Missing examples for complex concepts | Fix when concept is hard to understand without example |
| Inconsistent terminology with codebase | Fix when terms differ from code identifiers |
| Formatting inconsistency | Fix only when it affects readability |
| Grammar or wording issues | Fix only when meaning is unclear |
| Stylistic preference | **Always skip** |

## Special Rules

- Type narrowing fixes: only change local variables, never function signatures
- Cross-module renames: assign as an atomic task to a single fixer
- Issues rolled back in a previous round: **do not attempt again this round**
