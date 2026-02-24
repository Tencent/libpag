# Local Review

Single-agent review for local changes with optional auto-fix.

## Input from SKILL.md

- `REVIEW_PRIORITY`: A | A+B | A+B+C
- `FIX_MODE`: none | low | low_medium | full

## References

| File | Purpose |
|------|---------|
| `code-checklist.md` | Code review checklist |
| `doc-checklist.md` | Document review checklist |

---

## Step 1: Scope

### Uncommitted changes

If uncommitted changes exist (detected in SKILL.md pre-check), scope is
uncommitted changes only:
```
git diff HEAD
```
Skip argument-based scope below.

### Normal scope — validate arguments and fetch diff

- Empty arguments: determine the base branch from the current branch's upstream
  tracking branch. If no upstream, fall back to `main` (or `master`). Fetch the
  branch diff.
- Commit hash: validate with `git rev-parse --verify`, then `git show`.
- Commit range: validate both endpoints, then fetch the range diff.
- File/directory paths: verify all paths exist on disk, then read file contents.

### Associated PR comments (optional, best-effort)

If `gh` is available, check whether the current branch has an open PR:
```
gh pr view --json number,state --jq 'select(.state == "OPEN") | .number' 2>/dev/null
```
If an open PR exists, fetch its line-level review comments:
```
gh api repos/{owner}/{repo}/pulls/{number}/comments
```
Store as `PR_COMMENTS` for verification in Step 2.

If diff is empty → exit.

---

## Step 2: Review

Read all files in scope. Apply `code-checklist.md` to code files,
`doc-checklist.md` to documentation files. Only include priority levels the user
selected.

For each issue found:
- Provide a code citation (file:line + snippet).
- Self-verify by re-reading the code — confirm or withdraw.

**PR comment verification** (when `PR_COMMENTS` exist): verify each PR review
comment against current code. Add verified issues to the results.

If `FIX_MODE` = none → Step 3. Otherwise → Step 4.

---

## Step 3: Report

List all confirmed issues. End.

---

## Step 4: Auto-fix

### 4.1 Risk assessment

For each confirmed issue:

| Only one reasonable fix? | Design decision / external contract? | Risk |
|--------------------------|--------------------------------------|------|
| Yes | — | Low |
| No | Yes | High |
| No | No | Medium |

Consult the Judgment Matrix appendix for worth-fixing criteria and special rules.

### 4.2 Route issues

| Risk vs `FIX_MODE` | → |
|---------------------|---|
| At or below threshold | auto-fix |
| Above threshold | report only |

**Special rule for "full" mode**: issues that would change test baselines
(screenshot comparisons, golden files) are always deferred, regardless of risk.

### 4.3 Apply fixes

For each issue in the auto-fix queue:

- Read the relevant code and apply the fix directly.
- Commit each fix individually:
  ```
  git commit --only <files> -m "message"
  ```
- Commit message: English, under 120 characters, ending with a period.
- When in doubt, skip the fix rather than risk a wrong change.
- Do not modify public API function signatures or class definitions (comments
  are OK), unless the issue description explicitly requires it.
- After each fix, check whether the change affects related comments or
  documentation within the same files. If so, update them in the same commit.

### 4.4 Validate

Run build + test (skip if no build/test commands available or doc-only scope).

- **Pass** → mark issues as fixed.
- **Fail** → bisect to find the failing commit, revert it. Retry the fix once
  with failure details. If still failing, mark as failed and keep reverted.

### 4.5 Final report

- Fixed issues (with commit hashes)
- Above-threshold issues (need manual attention)
- Failed/rolled-back issues with reasons
- Final test result

---

## Appendix: Judgment Matrix

### Risk Level Examples

- **Low**: null check, fix incorrect comment, rename to match convention, remove
  redundant duplicate code, add `reserve`, fix obvious off-by-one error
- **Medium**: extracting shared logic across functions, removing unused internal
  methods, simplifying cross-function control flow, adjusting internal module boundaries
- **High**: public API change (signature, behavior, deprecation), test baseline change,
  architecture restructuring, algorithm replacement with multiple viable approaches,
  introducing a new dependency, changing data persistence/serialization format,
  changing threading model, performance optimization involving space-time trade-offs

### Handling by Risk Level

| `FIX_MODE` | Low risk | Medium risk | High risk |
|------------|----------|-------------|----------|
| full       | Auto-fix | Auto-fix    | Auto-fix |
| low_medium | Auto-fix | Auto-fix    | Report   |
| low        | Auto-fix | Report      | Report   |

### Code Modules — Worth Fixing?

Risk level is per-issue, not per-type (a "Logic bug" can be low or high risk).

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
| Container pre-allocation | Fix when size is predictable (e.g., loop count known, input size available) |
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

### Document Modules

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

### Special Rules

- Type narrowing fixes: only change local variables, never function signatures
- Cross-module renames: assign as an atomic task to a single fixer
- Issues rolled back in a previous round: **do not attempt again this round**
