# Scope Preparation — Phase 1.2

Validate arguments and fetch the actual diff/content.

## PR mode

1. **Fetch PR metadata**:
   ```
   gh pr view {number} --json headRefName,baseRefName,headRefOid,state
   ```
   Extract: `PR_BRANCH`, `BASE_BRANCH`, `HEAD_SHA`, `STATE`.
   If the command fails (gh not installed, not authenticated, PR not found, or URL
   repo mismatch), inform the user and abort.
   If `STATE` is not `OPEN`, inform the user and exit.

2. **Prepare working directory**:
   - If current branch equals `PR_BRANCH` -> use current directory directly.
   - Otherwise -> create a worktree:
     ```
     git fetch origin pull/{number}/head:pr-{number}
     git worktree add --no-track /tmp/pr-review-{number} pr-{number}
     ```
     All subsequent operations use the worktree directory. Record `WORKTREE_DIR` for
     cleanup in Phase 8.

3. **Set review scope**: diff against `BASE_BRANCH`.
   ```
   git fetch origin {BASE_BRANCH}
   git diff $(git merge-base origin/{BASE_BRANCH} HEAD)
   ```

4. **Fetch existing PR review comments** for de-duplication in Phase 3:
   ```
   gh api repos/{owner}/{repo}/pulls/{number}/comments
   ```
   Store as `EXISTING_PR_COMMENTS` for later comparison.

## Local mode

Validate arguments and fetch diff:

- Empty arguments: determine the base branch from the current branch's upstream
  tracking branch. If no upstream, fall back to `main` (or `master`). Fetch the
  branch diff.
- Commit hash: validate with `git rev-parse --verify`, then `git show`.
- Commit range: validate both endpoints, then fetch the range diff.
- File/directory paths: verify all paths exist on disk, then read file contents.

## Local mode — associated PR comments (optional, best-effort)

If `gh` is available, check whether the current branch has an open PR:
```
gh pr view --json number,state --jq 'select(.state == "OPEN") | .number' 2>/dev/null
```
If an open PR exists, fetch its line-level review comments:
```
gh api repos/{owner}/{repo}/pulls/{number}/comments
```
These are used in Phase 3 as reference information — each comment is verified against
the actual code to confirm the issue still exists before being treated as a known
issue to fix.
