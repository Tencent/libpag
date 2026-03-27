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

Whenever stopping due to failure or discard after Step 2, run:

```
git worktree remove --force {worktree_path}
git branch -D {tmp_branch}
rm -rf {session_dir}
```

Ignore errors from the first two commands (resources may not exist yet).

---

## Step 1: Resolve Scope

```
python3 scripts/resolve_scope.py {repo_dir}
```

The script detects branch state and cleans up stale sessions from previous
runs. If it fails, inform the user and stop.

Based on the `scope` field:

- **`"unpushed_only"`** — ask: *{pushed_count} pushed + {unpushed_count}
  unpushed commits. Which range?*
  - **Unpushed only** (default) → proceed with scope `unpushed`.
  - **Entire branch** → proceed with scope `branch`.

- **`"entire_branch"`** — ask: *All {pushed_count} commits are already pushed.
  Squash entire branch? (requires `--force` push)*
  - **Entire branch** → proceed with scope `branch`.
  - **Cancel** → stop.

- **`"all_unpushed"`** — proceed with scope `branch`.

Scope mapping: `unpushed_only` → `unpushed`, others → `branch`.

---

## Step 2: Collect Commits

```
python3 scripts/collect_commits.py {repo_dir} {scope}
```

Pass the scope determined in Step 1 (`unpushed` or `branch`).

If `$ARGUMENTS` is non-empty, **skip Step 1** and pass `$ARGUMENTS` directly as
the scope (supports `unpushed`, `branch`, commit ranges, or branch names).

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

Present the report, then ask: **Confirm** / **Discard**. Wait for response.

**Discard** → run cleanup procedure and stop.

---

## Step 6: Replace Branch

Write `{session_dir}/replace.json` and run:

```
python3 scripts/replace_branch.py {session_dir}/replace.json
```

```json
{
  "repo_dir": "...", "branch": "...", "tmp_branch": "...",
  "worktree_path": "...", "current_head": "<full_hash>",
  "upstream_remote": null, "upstream_merge": null
}
```

The script automatically appends any new commits added since `{current_head}`,
verifies tree equality, and atomically updates the branch ref. On success it
removes the worktree and tmp branch.

On failure → run cleanup procedure and stop.

On success:

```
rm -rf {session_dir}
```

Report using `original_commit_count` and `new_commit_count` from Step 4 output:
`Squash complete: {original_commit_count} commits → {new_commit_count} commits`

---

## Step 7: Offer to Push

If the branch has upstream tracking **and** the remote tracking point is an
ancestor of the new HEAD, ask: **Push** / **Skip**.

Otherwise, end silently.
