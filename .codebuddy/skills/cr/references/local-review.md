# Local Review

Single-agent review for local changes with optional auto-fix.

## Input from SKILL.md

- `FIX_MODE`: none | low | low_medium | full

## References

| File | Purpose |
|------|---------|
| `code-checklist.md` | Code review checklist |
| `doc-checklist.md` | Document review checklist |
| `judgment-matrix.md` | Risk levels, worth-fixing criteria, special rules |
| `scope-detection.md` | Shared scope detection logic |

---

## Step 1: Scope

Follow `scope-detection.md` to determine the review scope and fetch the diff.

---

## Step 2: Review

Review the diff. Apply `code-checklist.md` to code files,
`doc-checklist.md` to documentation files. When changed lines depend on
surrounding context, read the relevant sections or related definitions as
needed. Untracked files have no diff — review their full contents as new code.

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
| Above threshold | `pending` — confirm with user |

### 4.3 Apply fixes

For each issue in the auto-fix queue:

- Read the relevant code and apply the fix directly.
- Do **not** run `git add` or `git commit` — leave all changes unstaged so the
  user can review and decide what to commit.
- When in doubt, skip the fix rather than risk a wrong change.
- Do not modify public API function signatures or class definitions (comments
  are OK), unless the issue description explicitly requires it.
- After each fix, check whether the change affects related comments or
  documentation within the same files. If so, update them together.

### 4.4 Validate

Run build + test (skip if no build/test commands available or doc-only scope).

- **Pass** → mark issues as fixed.
- **Fail** → use the build error message to identify which fix caused the
  failure, revert that fix. Retry the fix once with failure details. If still
  failing, revert and mark as failed.

### 4.5 Confirm

If `pending` or `failed` issues exist, present them and offer a multi-select
prompt where each option's label is the issue summary (e.g.,
`[risk] file:line — description`). User checks the ones to fix. Checked →
apply fix (same rules as 4.3) and re-validate. Unchecked → skipped.

If no `pending`/`failed` issues → Step 4.6.

### 4.6 Final report

- Fixed issues
- Skipped issues
- Failed/rolled-back issues with reasons
- Final test result
