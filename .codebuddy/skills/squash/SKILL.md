---
name: squash
description: Analyze branch commits and consolidate iterative modifications.
disable-model-invocation: true
---

# Squash — Consolidate Branch History

Analyzes commits on the current branch and consolidates them into clean,
logical units. Consecutive commits that serve the same functional intent are
merged; commit messages that violate project conventions are rewritten. The
original branch is untouched until the user explicitly confirms the result.

## Instructions

- **NEVER** operate on the default branch (main/master).
- **NEVER** push to remote without the user's explicit consent in Step 2.
- All user-facing text must use the language the user has been using in the
  conversation. Do not default to English.
- **Do NOT pause or ask for confirmation between Steps 1 and 2.** Execute
  them in a single uninterrupted flow. The only user interaction point is the
  confirmation in Step 2 before execution begins.

---

## Step 1: Determine Range

Clean up any leftover temporary worktree or branch from a previously
interrupted run (matching the `squash-tmp-{branch_name}` pattern).

Resolve `{squash_base}` and `{squash_end}` from `$ARGUMENTS` by matching the
**first** applicable rule:

| Pattern | `{squash_base}` | `{squash_end}` |
|---------|-----------------|----------------|
| *(empty)* | Remote tracking point (fall back to merge-base if never pushed) | HEAD |
| `branch` | Merge-base with default branch | HEAD |
| `<commit>` (single ref) | `<commit>~1` (parent, so the commit itself is included in the range) | HEAD |
| `<commit1>...<commit2>` (range) | `<commit1>~1` (parent, so `<commit1>` is included in the range) | `<commit2>` |

For the default (empty) case: if HEAD equals the tracking point (nothing
unpushed), inform the user and stop.

Collect the commit list in `{squash_base}..{squash_end}` using a single
`git log --reverse --stat` command (chronological order, includes file stats
for Step 2 analysis). Include parent hashes to identify merge commits. Record
`{original_head}` = current HEAD. If the range is empty, inform the user and
stop.

---

## Step 2: Analyze & Propose

Using the `git log` output from Step 1:

**Pass 1 — Coarse scan**: Identify candidate groups from the commit messages
and file stats (consecutive commits touching the same files or with related
messages). Merge commits are **segment boundaries** — only non-merge commits
within each segment are candidates.

**Pass 2 — Detailed inspection**: For candidate groups only, read the full
diff to confirm whether they truly share the same functional intent. Use the
diff content to draft new commit messages for squashed groups and reworded
commits.

### Grouping criteria

The core question: **would the merged result be a better revert unit than the
individual commits?**

Group consecutive non-merge commits when:

- They serve the **same functional intent** — the group can be described with a
  single sentence explaining one coherent change.
- The later commits in the group **lack independent value** — reverting them
  individually would leave the codebase in a broken or nonsensical state.

Do **NOT** group commits when:

- Each commit is a **self-contained, independently revertible** change.
- Commits are separated by an unrelated commit or a merge commit — **never
  reorder commits**.

### Commit message quality check

Review every non-grouped commit message against the project's commit
conventions. Mark commits whose messages need rewriting (e.g., vague messages
like "wip", "fix", "update", or messages violating the project's format).

### Result

If nothing to merge or rewrite, inform the user the history is already clean
and stop.

Otherwise, mark each commit as **[keep]**, **[squash]**, **[reword]**, or
**[merge]** and present the squash report.

### Report format

```
Squash report (7 commits → 4 commits):

1. ccc3333 Add pagination support for list endpoints.

2. 9a8b7c6 Update API error response format.
   - bbb2222 wip

3. fe12a34 Add input validation with unit tests.
   - def5678 Add input validation for login form.
   - 789abcd Add unit tests for input validation.
   - aaa1111 Fix test assertion in validation test.

4. abc1234 Add user authentication module.
```

List in **reverse chronological order** (newest first, matching `git log`).
The report covers **only** the analyzed range.

Ask the user to choose using your interactive dialog tool (e.g.
AskUserQuestion) — never output options as plain text:

- **Confirm & push** (or **Confirm & force push** if the range includes already-pushed commits)
- **Confirm** — replace branch, do not push
- **Discard** — no changes made

If the user chooses **Discard**, stop. Otherwise proceed to Step 3.

---

## Step 3: Execute & Replace

Only reached after user confirmation in Step 2.

Use `git worktree add` to create a temporary worktree with a temporary branch
at `{squash_base}`. Name the branch and worktree directory using a fixed
pattern like `squash-tmp-{branch_name}` (no shell substitution — avoid `$()`
syntax). Run all cherry-pick operations inside this worktree so the user's
working tree, staging area, and branch are completely untouched.

Only process commits in the analyzed range `{squash_base}..{squash_end}`:

- **[keep]**: cherry-pick as-is (`--allow-empty`). Batch consecutive [keep]
  commits into a single `git cherry-pick A B C ...` call.
- **[reword]**: cherry-pick (`--allow-empty`), then amend the message.
- **[squash] group**: cherry-pick each with `--no-commit`, commit once after
  the last with the new message.
- **[merge]**: recreate using `git commit-tree` with the original tree and
  both parents to preserve merge semantics.

If any step fails, remove the temporary worktree and branch, inform the user,
and stop.

**Integrity check**: diff `{squash_end}` against the temporary branch HEAD —
their trees must be identical (squash only rewrites history, never content). If
different, remove the temporary worktree and branch, inform the user, and stop.

After the integrity check passes, apply the replacement:

1. Record the original branch's current HEAD as `{current_head}`.
2. If `{current_head}` ≠ `{squash_end}`, cherry-pick all commits after the
   analyzed range (`{squash_end}..{current_head}`) onto the temporary branch
   as-is in the worktree.
3. Remove the temporary worktree.
4. Move the original branch ref to the temporary branch's HEAD using
   `git update-ref` (not `git branch -f` which rejects the current branch).
   This only updates the ref — the working tree, staging area, and any
   uncommitted changes remain untouched.
5. Verify that staged (`git diff --cached`) and unstaged (`git diff`) changes
   are identical to before the replacement. If not, warn the user.
6. Delete the temporary branch.

For **Push** / **Force push**, additionally push to remote (use
`--force-with-lease` if the squash range included already-pushed commits).
New commits and uncommitted changes are carried over silently — do not
mention them to the user.

Output: `Squash complete: {original_count} commits → {new_count} commits`
