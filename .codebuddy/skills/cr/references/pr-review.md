# PR Review

Review for pull requests. Issues are submitted as line-level PR comments.

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

1. **Validate PR target and fetch metadata**:
   If `$ARGUMENTS` is a URL, extract the PR number from it for use in
   subsequent steps.
   ```
   gh repo view --json nameWithOwner --jq .nameWithOwner
   gh pr view {number} --json headRefName,baseRefName,headRefOid,state,body
   ```
   Record `OWNER_REPO` from the first command (used in later steps).
   If either command fails (not a git repo, gh not installed, not
   authenticated, or PR not found), inform the user and abort.
   If `$ARGUMENTS` is a URL containing `{owner}/{repo}`, verify it matches
   `OWNER_REPO`. If not, inform the user that cross-repo PR review is not
   supported and suggest switching to the target repository first, then abort.
   Extract: `PR_BRANCH`, `BASE_BRANCH`, `HEAD_SHA`, `STATE`, `PR_BODY`.
   If `STATE` is not `OPEN`, inform the user and exit.

2. **Prepare working directory**:
   - If current branch equals `PR_BRANCH` and HEAD equals `HEAD_SHA` → use
     current directory directly.
   - Otherwise:
     - Clean up any existing worktree for this PR number:
       ```
       git worktree remove /tmp/pr-review-{number} 2>/dev/null
       git branch -D pr-{number} 2>/dev/null
       ```
     - Create a fresh worktree:
       ```
       git fetch origin pull/{number}/head:pr-{number}
       git worktree add --no-track /tmp/pr-review-{number} pr-{number}
       ```
       If worktree creation fails, inform the user and abort.
       All subsequent operations use the worktree directory. Record `WORKTREE_DIR`.

   **Do NOT use `gh pr diff` or any remote API to substitute for a local
   worktree — all file reads and diffs must be performed against local files.**

3. **Set review scope** (run inside `WORKTREE_DIR` if created):
   ```
   git fetch origin {BASE_BRANCH}
   git diff $(git merge-base origin/{BASE_BRANCH} HEAD)
   ```
   If the diff exceeds 200 lines, first run `git diff --stat` to get an
   overview, then read the diff per file using `git diff -- {file}` to avoid
   output truncation.

4. **Fetch existing PR review comments** for de-duplication in Step 2:
   ```
   gh api repos/{OWNER_REPO}/pulls/{number}/comments
   ```
   Store as `EXISTING_PR_COMMENTS`.

If diff is empty → exit.

---

## Step 2: Review

1. **Read changed files**: Read the full content of every file that appears in
   the diff (not just the changed lines — full file context is needed for
   accurate review).
2. **Read referenced context**: For each changed file, identify symbols (types,
   base classes, called functions) that are defined elsewhere and are relevant
   to understanding the change's correctness. Read those definitions. Stop
   expanding when the change's behavior can be fully evaluated.
3. **Understand author intent**: Read `PR_BODY` (fetched in Step 1) to
   understand the stated motivation and approach. Verify the implementation
   actually achieves what the author describes.
4. **Apply checklists**: Apply `code-checklist.md` to code files,
   `doc-checklist.md` to documentation files.

For each issue found:
- Provide a code citation (file:line + snippet).
- Self-verify before confirming — re-read the surrounding code and check:
  - Is there a guard, null check, or early return elsewhere that already
    handles this case?
  - Does the call chain guarantee preconditions that make this issue
    impossible?
  - Am I misunderstanding the variable's lifetime or ownership?
  If any of these apply, withdraw the issue.
- De-duplicate against `EXISTING_PR_COMMENTS` — skip issues already covered.

---

## Step 2.5: Worktree cleanup

After review is complete, immediately clean up the worktree before reporting
results. All necessary information (issue list, file paths, line numbers, code
snippets) has already been collected — local files are no longer needed.

If `WORKTREE_DIR` was created:
```
cd {original_directory}
git worktree remove {WORKTREE_DIR}
git branch -D pr-{number}
```

---

## Step 3: Report

If no issues found → inform the user that the review is clean, summarize the
key areas checked, and ask whether to submit an approval review on the PR:
```bash
gh api repos/{OWNER_REPO}/pulls/{number}/reviews --input - <<'EOF'
{
  "commit_id": "{HEAD_SHA}",
  "event": "APPROVE"
}
EOF
```
Skip the comment submission below.

If issues found → present confirmed issues to user in the following format:

```
{N}. [{priority}] {file}:{line} — {description of the problem and suggested fix}
```

Where `{priority}` is the checklist item ID (e.g., A2, B1, C7). User selects
which to submit as PR comments, declines are marked `skipped`.

Submit as a **single** GitHub PR review with line-level comments via `gh api`.
Do NOT use `gh pr comment` or `gh pr review`.

```bash
gh api repos/{OWNER_REPO}/pulls/{number}/reviews --input - <<'EOF'
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
- `line`: line number in the **new** file (right side of diff). Must be
  determined during Step 2 by reading the actual file in the worktree — do
  not derive from diff hunk offsets.
- `side`: always `"RIGHT"`
- `body`: concise, in the user's conversation language, with a specific fix
  suggestion when possible

Summary of issues found / submitted / skipped.
