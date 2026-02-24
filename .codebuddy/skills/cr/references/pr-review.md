# PR Review

Review for pull requests. Issues are submitted as line-level PR comments.

## Input from SKILL.md

- `REVIEW_PRIORITY`: A | A+B | A+B+C

Auto-fix is not available in PR mode.

## References

| File | Purpose |
|------|---------|
| `code-checklist.md` | Code review checklist |
| `doc-checklist.md` | Document review checklist |
| `judgment-matrix.md` | Worth-fixing criteria and special rules |

---

## Step 1: Scope

### Clean up leftover worktrees

Check for and remove leftover worktree directories from previous sessions:
```
ls -d /tmp/pr-review-* 2>/dev/null
```
If any exist, clean up each one:
```
git worktree remove /tmp/pr-review-{N} 2>/dev/null
git branch -D pr-{N} 2>/dev/null
```

### Fetch PR metadata

1. **Fetch PR metadata**:
   ```
   gh pr view {number} --json headRefName,baseRefName,headRefOid,state
   ```
   Extract: `PR_BRANCH`, `BASE_BRANCH`, `HEAD_SHA`, `STATE`.
   If the command fails (gh not installed, not authenticated, PR not found, or URL
   repo mismatch), inform the user and abort.
   If `STATE` is not `OPEN`, inform the user and exit.

2. **Prepare working directory**:
   - If current branch equals `PR_BRANCH` → use current directory directly.
   - Otherwise → create a worktree:
     ```
     git fetch origin pull/{number}/head:pr-{number}
     git worktree add --no-track /tmp/pr-review-{number} pr-{number}
     ```
     If worktree creation fails (e.g., directory still exists), inform the user
     and abort.
     All subsequent operations use the worktree directory. Record `WORKTREE_DIR`.

3. **Set review scope**: diff against `BASE_BRANCH`.
   ```
   git fetch origin {BASE_BRANCH}
   git diff $(git merge-base origin/{BASE_BRANCH} HEAD)
   ```

4. **Fetch existing PR review comments** for de-duplication in Step 2:
   ```
   gh api repos/{owner}/{repo}/pulls/{number}/comments
   ```
   Store as `EXISTING_PR_COMMENTS`.

If diff is empty → exit.

---

## Step 2: Review

Read all files in scope. Apply `code-checklist.md` to code files,
`doc-checklist.md` to documentation files. Only include priority levels the user
selected.

For each issue found:
- Provide a code citation (file:line + snippet).
- Self-verify by re-reading the code — confirm or withdraw.
- De-duplicate against `EXISTING_PR_COMMENTS` — skip issues already covered.

---

## Step 3: Report

Present confirmed issues to user. User selects which to submit as PR comments,
declines are marked `skipped`.

Submit as a **single** GitHub PR review with line-level comments via `gh api`.
Do NOT use `gh pr comment` or `gh pr review`.

```bash
gh api repos/{owner}/{repo}/pulls/{number}/reviews --input - <<'EOF'
{
  "commit_id": "{HEAD_SHA}",
  "event": "COMMENT",
  "comments": [
    {
      "path": "relative/file/path",
      "line": 42,
      "side": "RIGHT",
      "body": "Description of the issue and suggested fix"
    }
  ]
}
EOF
```

- `commit_id`: HEAD SHA of the PR branch
- `path`: relative to repository root
- `line`: line number in the **new** file (right side of diff)
- `side`: always `"RIGHT"`
- `body`: concise, in the user's conversation language, with a specific fix
  suggestion when possible

Summary of issues found / submitted / skipped.

### Worktree cleanup

If `WORKTREE_DIR` was created, clean up:
```
cd {original_directory}
git worktree remove {WORKTREE_DIR}
git branch -D pr-{number}
```
