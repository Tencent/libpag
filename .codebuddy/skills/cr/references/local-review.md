# Local Review

Single-agent review for local changes. Reviews the diff, presents confirmed
issues, and lets the user interactively choose which ones to fix.

## References

| File | Purpose |
|------|---------|
| `code-checklist.md` | Code review checklist |
| `doc-checklist.md` | Document review checklist |
| `judgment-matrix.md` | Risk levels, worth-fixing criteria, special rules |

---

## Step 1: Scope

Determine the diff to review based on `$ARGUMENTS` and working tree state:

- **Empty `$ARGUMENTS`**, **uncommitted changes exist**: scope is
  uncommitted changes only. Fetch with `git diff HEAD` (staged + unstaged
  tracked files). Also check for untracked files with `git status --porcelain`
  (`??` lines) and read their contents for review.
- **Empty `$ARGUMENTS`**, **no uncommitted changes**: determine the base branch
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

If diff is empty → show usage examples and exit:
`/cr` (uncommitted changes or current branch),
`/cr a1b2c3d`, `/cr a1b2c3d..e4f5g6h`,
`/cr src/foo.cpp`, `/cr 123`, `/cr https://github.com/.../pull/123`.

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

If no issues remain after filtering → report "no issues found" and exit.

---

## Step 4: Report and fix

Present a summary of what was reviewed, then list all confirmed issues. Ask
which ones to fix using **a single multi-select question** where each option's
label is the issue summary (e.g., `[risk] file:line — description`). User
checks multiple options in one prompt.

If the user selects any issues, apply the fixes.
