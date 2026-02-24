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

---

## Step 1: Scope

Determine the diff to review based on how this file was entered:

- **From fast path** (no `$ARGUMENTS`, uncommitted changes exist): scope is
  uncommitted changes only. Fetch with `git diff HEAD` (staged + unstaged
  tracked files). Also check for untracked files with `git status --porcelain`
  (`??` lines) and read their contents for review.
- **Empty `$ARGUMENTS`** (no uncommitted changes): determine the base branch
  from the current branch's upstream tracking branch. If no upstream, fall back
  to `main` (or `master`). Fetch the branch diff:
  ```
  git merge-base origin/{base_branch} HEAD
  git diff <merge-base-sha>
  ```
  Also check for untracked files with `git status --porcelain` (`??` lines).
- **Commit hash** (e.g., `abc123`): validate with `git rev-parse --verify`,
  then `git show`.
- **Commit range** (e.g., `abc123..def456` or `abc123...def456`): validate both
  endpoints. Fetch the diff including both endpoints:
  ```
  git diff A~1..B
  ```
- **File/directory paths**: verify all paths exist on disk, then read file
  contents.

If diff is empty → exit.

### Build baseline

Skip if doc-only. If no build/test commands can be determined, warn that fix
validation will be skipped. Otherwise run build + test. Fail → abort.

Read git log since upstream for recent-fix context (avoid re-flagging issues
a previous `/cr` session already fixed).

---

## Step 2: Review

Review the diff. Apply `code-checklist.md` to code files,
`doc-checklist.md` to documentation files. When changed lines depend on
surrounding context, read the relevant sections or related definitions as
needed. Untracked files have no diff — review their full contents as new code.

For each issue found:
- Provide a code citation (file:line + snippet) from the current tree.
- Self-verify by re-reading the code — confirm or withdraw.
- If a cited path/line no longer exists, locate the correct file/path via `git diff --name-only` or file search before reporting.

**Output rule**: only present the final confirmed issues to the user. Do not
output analysis process, exclusion reasoning, or issues that were considered
but ruled out.

---

## Step 3: Filter

Consult `judgment-matrix.md` for risk level assessment, worth-fixing criteria,
and special rules. Discard issues that are not worth reporting.

**Fix approach** (Medium/High only): decide the specific fix approach and
reasoning before applying. Low risk: single obvious fix, no planning needed.

If `FIX_MODE` = none → Step 6 (Report).

Route remaining issues:

| Risk vs `FIX_MODE` | → |
|---------------------|---|
| At or below threshold | auto-fix |
| Above threshold | `pending` — confirm with user |

**IMPORTANT**: do NOT ask the user about `pending` issues here. Auto-fix
eligible issues first; user confirmation happens only in Step 5.5, after all
auto-fixable issues have been processed and validated.

---

## Step 4: Fix

For each issue in the auto-fix queue:

- Read the relevant code and apply the fix directly.
- Do **not** run `git add` or `git commit` — leave all changes unstaged so the
  user can review and decide what to commit.
- When in doubt, skip the fix rather than risk a wrong change.
- Do not modify public API function signatures or class definitions (comments
  are OK), unless the issue description explicitly requires it.
- After each fix, check whether the change affects related comments or
  documentation within the same files. If so, update them together.

---

## Step 5: Validate

Run build + test (skip if no build/test commands available or doc-only scope).

- **Pass** → mark issues as fixed.
- **Fail** → use the build error message to identify which fix caused the
  failure, revert that fix. Retry the fix once with failure details. If still
  failing, revert and mark as failed.

### Continue?

After validation, re-review the changed code for new issues introduced by
fixes. If new issues are found → go back to Step 3 (Filter). If no new
issues:

- `pending` or `failed` issues exist → Step 5.5 (Confirm).
- Otherwise → Step 6 (Report).

### Step 5.5: Confirm

Present `pending` + `failed` issues, then ask the user to select which issues
to fix using **a single multi-select question** where each option's label is
the issue summary (e.g., `[risk] file:line — description`). User checks
multiple options in one prompt. Unchecked → skipped.

- **All skipped** → Step 6 (Report).
- **Any checked** → apply fix (same rules as Step 4) → re-validate (Step 5).
  After validation, re-review for new issues. If new issues found → back to
  Step 3 (Filter). If no new issues but more `pending`/`failed` remain →
  return here. If nothing remains → Step 6 (Report).

---

## Step 6: Report

- Summary: one paragraph describing what was reviewed and key findings.
- Issues found / fixed / skipped / failed
- Failed/rolled-back issues with reasons
- Final test result
