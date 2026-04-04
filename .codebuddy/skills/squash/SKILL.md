---
name: squash
description: Analyze branch commits and consolidate iterative modifications.
disable-model-invocation: true
---

# Squash — Consolidate Branch History

Analyzes commits on the current branch and consolidates them into clean,
logical units. The original branch is untouched until the user explicitly
confirms the result. Heavy git operations are delegated to Python scripts
(`scripts/` directory) that enforce safety checks.

## Instructions

- **NEVER** operate on the default branch (main/master).
- All user-facing text must match the language the user has been using.
- When presenting choices, use interactive dialogs with selectable options
  rather than plain text.
- Scripts are at `.codebuddy/skills/squash/scripts/` relative to repo root.
  Use `python3` to run them. **NEVER** read or modify the script contents.

### Cleanup procedure

Whenever stopping due to failure or user discard, extract these values from
the latest successful step output (if available), then run:

```bash
git worktree remove --force {worktree_path}
git branch -D {tmp_branch}
rm -rf {session_dir}
```

| Field | Source |
|---|---|
| `worktree_path` | Step 4 output (on Step 4 failure or Step 5 discard) |
| `tmp_branch` | Step 4 output (on Step 4 failure or Step 5 discard) |
| `session_dir` | Step 2 output (on Step 2/3 failure) |

If these values cannot be determined (e.g., Step 2 failed before creating
session_dir), skip the cleanup that depends on them. Errors during cleanup
can be safely ignored—resources will eventually be removed by `resolve_scope.py`
on the next run.

---

## Step 1: Cleanup

If `$ARGUMENTS` is **empty**, run:

```
python3 scripts/resolve_scope.py {repo_dir}
```

The script validates that the current branch is not the default branch, and
cleans up stale sessions from previous runs. If it fails, inform the user
and stop.

If `$ARGUMENTS` is non-empty (a branch name), **skip this step** — validation
is handled by the collect script in Step 2.

---

## Step 2: Collect Commits

Two modes depending on `$ARGUMENTS`:

- **No argument** — squash unpushed commits on the current branch:
  ```
  python3 scripts/collect_commits.py {repo_dir}
  ```
- **Branch name** (e.g. `/squash feature/foo`) — squash the entire named
  branch (does not require checking it out):
  ```
  python3 scripts/collect_commits.py {repo_dir} $ARGUMENTS
  ```

If the script fails, inform the user and stop.

Record `{session_dir}` from the output — all temporary files and the worktree
will live inside this directory throughout the session.

If `commits` is empty, inform the user and stop.
Present the `commit_table` field to the user.

---

## Step 3: Analyze & Decide

Use `commits` (with `subject`, `files`, `is_merge`, `quality_flag`) and
`overlaps` (adjacent pair file overlap ratios) from Step 2.

### Grouping

Group adjacent commits belonging to the same logical change when **either**:

1. **File overlap ≥ 30 %** (from `overlaps`).
2. **Message similarity** — same module/feature keyword, or follow-up pattern.

A `"revert"` commit should be grouped with the commit it reverts. If both are
adjacent, squashing them cancels them out.

**Never** group across a merge commit. **Never** include a merge commit in a
squash group. Never reorder commits.

### Messages

Synthesize a new commit message for each squash group. Rewrite any commit with
a non-null `quality_flag`.

If nothing to merge or rewrite, inform the user the history is clean and
**stop**.

### Output — Decision Table

Write to `{session_dir}/decision.json`:

```json
{
  "session_dir": "...",
  "repo_dir": "...",
  "branch": "...",
  "squash_base": "<full_hash>",
  "squash_end": "<full_hash>",
  "upstream_remote": null,
  "upstream_merge": null,
  "actions": [
    {"type": "keep",   "commits": ["<hash>"]},
    {"type": "squash", "commits": ["<hash1>", "<hash2>"], "message": "..."},
    {"type": "reword", "commits": ["<hash>"], "message": "..."},
    {"type": "merge",  "commits": ["<hash>"]}
  ]
}
```

Copy `session_dir`, `repo_dir`, `branch`, `squash_base`, `squash_end`,
`upstream_remote`, `upstream_merge` directly from Step 2 output.

Rules: chronological order (oldest first), every commit in exactly one action,
full 40-char hashes, `squash`/`reword` require `message`, `merge` must have
exactly one commit, `squash` must never contain a merge.

---

## Step 4: Build New History

```
python3 scripts/build_history.py {session_dir}/decision.json
```

On failure → run cleanup procedure and stop (error output includes
`worktree_path` and `tmp_branch`).

On success, parse the JSON output and record `{worktree_path}`, `{tmp_branch}`,
and `{current_head}` from it. The script automatically handles any commits
added after `squash_end` — no manual git operations needed.

---

## Step 5: User Confirmation

**Mandatory interaction point. Do NOT skip.**

```
python3 scripts/format_report.py {session_dir}/decision.json
```

Present the report (showing planned changes), then ask: **Confirm** / **Discard**.
Wait for response.

**Note:** The reported "new commit count" in the format report is an estimate
based on the decision table. The actual count after execution (which accounts
for zero-diff groups being skipped) will be shown in Step 6 results.

**Discard** → run cleanup procedure and stop.

---

## Step 6: Replace Branch

Write `{session_dir}/replace.json` using values from prior steps, then run:

```
python3 scripts/replace_branch.py {session_dir}/replace.json
```

| Field | Source |
|---|---|
| `repo_dir` | Step 2 output |
| `branch` | Step 2 output (`branch` field — may be a non-current branch) |
| `tmp_branch` | Step 4 output |
| `worktree_path` | Step 4 output |
| `current_head` | Step 4 output |
| `upstream_remote` | Step 2 output |
| `upstream_merge` | Step 2 output |

The script automatically appends any new commits added since `{current_head}`,
verifies tree equality, and atomically updates the branch ref. On success it
removes the worktree and tmp branch.

On failure → run cleanup procedure and stop.

On success:

```
rm -rf {session_dir}
```

Report using `new_commit_count` from Step 4 output (which accounts for commits
skipped due to zero-diff groups):

`Squash complete: {original_commit_count} commits → {new_commit_count} commits`

---

## Step 7: Offer to Push

If the branch has upstream tracking **and** the remote tracking point is an
ancestor of the new HEAD, ask: **Push** / **Skip**.

Otherwise, end silently.
