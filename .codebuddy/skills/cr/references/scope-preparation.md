# Scope Preparation

Validate arguments and fetch the actual diff/content for local mode.

## Uncommitted changes

Scope is uncommitted changes only:
```
git diff HEAD
```
Skip argument-based scope below — uncommitted changes are the entire scope.

## Normal scope — validate arguments and fetch diff

- Empty arguments: determine the base branch from the current branch's upstream
  tracking branch. If no upstream, fall back to `main` (or `master`). Fetch the
  branch diff.
- Commit hash: validate with `git rev-parse --verify`, then `git show`.
- Commit range: validate both endpoints, then fetch the range diff.
- File/directory paths: verify all paths exist on disk, then read file contents.

## Associated PR comments (optional, best-effort)

If `gh` is available, check whether the current branch has an open PR:
```
gh pr view --json number,state --jq 'select(.state == "OPEN") | .number' 2>/dev/null
```
If an open PR exists, fetch its line-level review comments:
```
gh api repos/{owner}/{repo}/pulls/{number}/comments
```
These are verified against current code during review (Step 3) and added to the
results if confirmed.
