# Judgment Matrix

## Risk Level Assessment

| Risk | Rule | Examples |
|------|------|----------|
| Low | Only one reasonable fix exists | null check, fix incorrect comment, rename to match convention, remove redundant duplicate code, add `reserve`, fix obvious off-by-one error |
| Medium | Multiple fixes possible, but no design decision or external contract involved | extracting shared logic across functions, removing unused internal methods, simplifying cross-function control flow, adjusting internal module boundaries |
| High | Involves design decisions or external contracts | public API change (signature, behavior, deprecation), test baseline change, architecture restructuring, algorithm replacement with multiple viable approaches, introducing a new dependency, changing data persistence/serialization format, changing threading model, performance optimization involving space-time trade-offs |

## Handling by Risk Level

| `FIX_MODE` | Low risk | Medium risk | High risk |
|------------|----------|-------------|-----------|
| full       | Auto-fix | Auto-fix    | Auto-fix  |
| low_medium | Auto-fix | Auto-fix    | Confirm   |
| low        | Auto-fix | Confirm     | Confirm   |

**Special rule for "full" mode**: issues that would change test baselines
(screenshot comparisons, golden files) are always deferred for user confirmation,
regardless of risk level.

## Worth Fixing?

Code-checklist and doc-checklist define **what to look for**. This section
defines **whether to fix** a discovered issue. Risk level is per-issue, not
per-type (a "Logic bug" can be low or high risk).

### Decision principles

1. **Must fix** — The issue affects runtime correctness, safety, or security.
   Corresponds to checklist Priority A items.
2. **Fix when clear** — The issue improves code quality (performance,
   simplification, architecture). Fix only when the solution is unambiguous and
   does not introduce new risk. Performance changes require high confidence in
   semantic equivalence. Corresponds to checklist Priority B items.
3. **Fix when inconsistent** — The issue involves naming, initialization,
   comments, or file organization. Fix only when it violates project rules
   loaded in context or contradicts the surrounding code's established patterns.
   Corresponds to checklist Priority C items.
4. **Always skip** — Pure style preferences (not violating any consistency
   rule), suggestions based on assumed future requirements rather than current
   code, and alternative implementation rewrites for stable code that has no
   correctness issue.

### Exceptions

- Duplicate code extraction: fix when identical logic is clearly duplicated
  (not by count threshold — judge by complexity and maintenance cost).
- Public API signature changes that are not bug fixes: fix only when justified
  by clear benefit to API consumers. Always high risk.
- Test coverage gaps and regression risks are **flagged, not fixed** — report
  them for the user's awareness rather than auto-fixing.

## Special Rules

- Cross-module renames: assign as an atomic task to a single fixer
- Issues rolled back in a previous round: **do not attempt again this round**
