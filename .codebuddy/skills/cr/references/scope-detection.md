# Scope Detection

Shared scope-detection logic for local and teams review flows.

## Uncommitted changes

If uncommitted changes exist (detected in SKILL.md pre-check), scope is
uncommitted changes only:
```
git diff HEAD
```
This covers both staged and unstaged changes to tracked files. Additionally,
check for untracked files with `git status --porcelain` (lines starting with
`??`) and read their contents for review.

Skip argument-based scope below.

## Normal scope — validate arguments and fetch diff

- Empty arguments: determine the base branch from the current branch's upstream
  tracking branch. If no upstream, fall back to `main` (or `master`). Fetch the
  branch diff:
  ```
  git diff $(git merge-base origin/{base_branch} HEAD)
  ```
- Commit hash (e.g., `abc123`): validate with `git rev-parse --verify`, then
  `git show`.
- Commit range (e.g., `abc123..def456` or `abc123...def456`): validate both
  endpoints. Fetch the diff including both endpoints:
  ```
  git diff A~1..B
  ```
- File/directory paths: verify all paths exist on disk, then read file contents.

For empty arguments and commit range: also check for untracked files with
`git status --porcelain` (`??` lines) and read their contents for review.

## Associated PR comments (optional, best-effort)

If `gh` is available, check whether the current branch has an open PR:
```
gh pr view --json number,state --jq 'select(.state == "OPEN") | .number' 2>/dev/null
```
If an open PR exists, fetch its line-level review comments:
```
gh api repos/{owner}/{repo}/pulls/{number}/comments
```
Store as `PR_COMMENTS` for verification in the review step.

If diff is empty → exit.
