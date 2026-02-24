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
| `judgment-matrix.md` | Risk levels, worth-fixing criteria, special rules |

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

Consult `judgment-matrix.md` for risk level assessment, worth-fixing criteria,
handling by risk level, and special rules.

### 4.2 Route issues

| Risk vs `FIX_MODE` | → |
|---------------------|---|
| At or below threshold | auto-fix |
| Above threshold | report only |

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
