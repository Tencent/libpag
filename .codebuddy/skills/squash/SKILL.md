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
- All user-facing text must use the language the user has been using in the
  conversation. Do not default to English.
- **Do NOT pause or ask for confirmation during Steps 1, 2, and Phase A of
  Step 3.** Execute them in a single uninterrupted flow. **However, you MUST
  stop and ask for user confirmation in Phase B** — see Phase B for details.
  Phase D may present an optional push prompt — see Phase D for details.
---

## Step 1: Determine Range

Clean up any leftover temporary worktree or branch from a previously
interrupted run (matching the `squash-tmp-{branch_name}` pattern).

Resolve `{squash_base}` and `{squash_end}` from `$ARGUMENTS` by matching the
**first** applicable rule. Use `git rev-parse` to determine whether the
argument is a branch name, a commit hash, or a range.

| Pattern | `{squash_base}` | `{squash_end}` |
|---------|-----------------|----------------|
| *(empty — no argument)* | Remote tracking point (fall back to merge-base with default branch if never pushed) | HEAD |
| `branch` *(literal keyword)* | Merge-base of current branch with default branch — analyzes the **entire branch** | HEAD |
| *argument is a branch name* (e.g., `feature/foo`) | Merge-base of that branch with the default branch | HEAD of that branch |
| *argument is a single commit* (e.g., `abc1234`) | `<commit>~1` (parent, so the commit itself is included in the range) | HEAD |
| *argument is a range* (e.g., `abc1234...def5678`) | `<older>~1` (parent, so `<older>` is included in the range) | `<newer>` — use `git merge-base --is-ancestor` to detect order; swap if the user wrote them in reverse |

For the default (empty) case: if HEAD equals the tracking point (nothing
unpushed), inform the user and stop.

### Collect commits

Run a **single** command to collect the commit list:

```
git log --first-parent --stat=200 --format="---commit%nH: %H%nP: %P%nS: %s" {squash_base}..{squash_end}
```

This produces structured blocks separated by `---commit`, with labeled lines
for hash (`H:`), parents (`P:`), and subject (`S:`). The `--stat` file list
follows each block. `--stat=200` ensures file lists are not truncated.
`--first-parent` ensures only the current branch's own commits are listed —
merge points appear as single commits (identifiable by multiple hashes on the
`P:` line). If the range is empty, inform the user and stop.

### Build the commit table

After collecting the log, **immediately** produce a numbered summary table in
**chronological order** (oldest = #1). This table is the **index** for all
subsequent steps.

```
 #  | Hash    | Subject
  1 | abc1234 | Add foo module.
  2 | def5678 | Fix bar crash on empty input.
  3 | 1112222 | Add unit tests for foo.
...
```

Each row: sequence number, short hash (7 chars), and the full subject line.
Mark merge commits with `[merge]` after the hash. File lists are already in
the raw log output above — refer back to them during grouping as needed.
Do not duplicate file lists into the table.

---

## Step 2: Analyze & Propose

Work entirely from the commit table built in Step 1.

### Grouping

Walk through the table and identify groups of commits that belong to the same
logical change. Two commits belong together when **either** condition is met:

1. **File overlap ≥ 30 %** — overlap ratio = (shared paths) / (smaller file
   list size). Commits touching largely the same files likely belong to the
   same unit of work.
2. **Message similarity** — their subjects reference the same module, feature
   keyword, or concern (e.g., both mention "TextBox", "excludeChildEffects",
   "playground CMake"), or the later commit is a follow-up ("fix …", "wip",
   "update …", "add tests for …", "update baseline for …").

Once a group forms, compare the next commit against the **union** of all files
in the group and the group's overall topic. Extend greedily.

Do **NOT** merge commits separated by a **[merge]** commit. Never reorder
commits — only adjacent commits can be grouped.

### Draft messages

For each squashed group, draft a new commit message by synthesizing the
original subjects in the group. **Never** run `git diff` or read file contents
during Step 2 — all grouping and message drafting must use only the commit
table (file lists and subjects).

### Commit message quality check

Review every non-grouped commit message against the project's commit
conventions. Mark commits whose messages need rewriting (e.g., vague messages
like "wip", "fix", "update", or messages violating the project's format).

### Output

If nothing to merge or rewrite, inform the user the history is already clean
and **stop — do not show a report or ask any questions**.

Otherwise, produce the **execution plan**: an ordered list (chronological) of
actions for Phase A, showing exactly which commits are [keep], [reword],
[squash] (with group members), and [merge]. Also prepare the squash report
(do not present it yet — it will be shown in Phase B).

### Report format

```
Squash report (7 commits → 4 commits):

1. ccc3333 Add pagination support for list endpoints.

2. Update API error response format.
   - bbb2222 wip

3. Add input validation with unit tests.
   - def5678 Add input validation for login form.
   - 789abcd Add unit tests for input validation.
   - aaa1111 Fix test assertion in validation test.

4. abc1234 Add user authentication module.
```

List in **reverse chronological order** (newest first, oldest last) — the same
order as `git log`. Unchanged commits show their original hash and message.
Squashed or reworded entries show the proposed new message with the original
commits listed below. The report covers **only** the analyzed range.

---

## Step 3: Execute & Replace

### Phase A — Build the new history in a temporary worktree

Before creating the worktree, record the original branch's upstream tracking
branch (e.g. via `git config branch.<name>.remote` and
`git config branch.<name>.merge`) so it can be restored in Phase C.

Use `git worktree add` to create a temporary worktree with a temporary branch
at `{squash_base}`. Name the branch and worktree directory using the fixed
pattern `squash-tmp-{branch_name}`. All operations below run **inside this
worktree** — the user's working tree, staging area, and branch remain
completely untouched until Phase C.

Process commits according to the execution plan from Step 2, in
**chronological order** (oldest first):

- **[keep]**: `git cherry-pick --allow-empty`. Batch consecutive [keep]
  commits into a single `git cherry-pick A B C ...` call.
- **[reword]**: `git cherry-pick --allow-empty`, then amend the message.
- **[squash] group**: record the current HEAD hash as `<saved_head>`, then
  cherry-pick each commit in the group **one by one** with
  `git cherry-pick --allow-empty <hash>`. After all commits are cherry-picked,
  `git reset --soft <saved_head>` to collapse them into the index, then
  `git commit -m "new message"` to create the single squashed commit.
- **[merge]**: must use `git commit-tree` (cherry-pick cannot create merge
  commits). Pass the original merge commit's tree, worktree HEAD as first
  parent (`-p`), and the original merge commit's second parent as second
  parent (`-p`). Then `git update-ref HEAD <new_hash>` and
  `git reset --hard HEAD` to sync the working tree.

If any step fails, remove the temporary worktree and branch, inform the user,
and stop. If a cherry-pick produces a conflict, resolve it automatically by
running `git checkout --theirs <conflicted_file>` for each conflicted file,
then `git add <conflicted_files>` and `git cherry-pick --continue --no-edit`.
The integrity check at the end of Phase A will verify the final tree matches
the original branch exactly.

**Integrity check**: diff `{squash_end}` against the temporary branch HEAD —
their trees must be identical. If different, abort (remove worktree and branch).

Next, record the original branch's current HEAD as `{current_head}`. If
`{current_head}` ≠ `{squash_end}`, there are commits after the analyzed range.
Cherry-pick them (`{squash_end}..{current_head}`) onto the temporary branch
**inside the worktree** as-is.

---

### Phase B — STOP and get user confirmation

**This is a mandatory user interaction point. Do NOT skip this phase.**

Validate: `git diff {current_head} <tmp_branch_head>` must be empty — the
final tree must be identical to `{current_head}`. If not, abort (remove
worktree and branch).

Then you **MUST** do the following before touching the original branch:

1. Present the squash report (using the format from Step 2) along with the
   before and after commit counts.
2. Call `AskUserQuestion` with **Confirm** / **Discard** options.
3. **Wait for the user's response.** Do NOT call any other tool until the
   user responds.

If the user chooses **Discard**, remove the temporary worktree and branch,
then stop. Only proceed to Phase C after the user selects **Confirm**.

After the user confirms, re-check for new commits: compare the original
branch's current HEAD against `{current_head}`. If they differ, cherry-pick
the new commits onto the temporary branch inside the worktree, update
`{current_head}`, and re-validate the tree diff. Loop until `{current_head}`
is stable and validation passes.

### Phase C — Replace the branch (only after user confirms in Phase B)

1. Remove the temporary worktree (the temporary **branch** still exists).
2. Move the original branch ref to the temporary branch's HEAD using
   `git update-ref` (not `git branch -f` which rejects the current branch).
   This only updates the ref — the working tree, staging area, and any
   uncommitted changes remain untouched.
3. Delete the temporary branch.
4. If the original branch had an upstream tracking branch, restore it with
   `git branch --set-upstream-to`.

Output: `Squash complete: {original_count} commits → {new_count} commits`

---

### Phase D — Offer to push

After Phase C, check whether the squashed branch can be pushed **without**
`--force`. The branch can be pushed normally when **both** conditions are true:

1. The branch has an upstream tracking branch.
2. The remote tracking point recorded in Step 1 is an ancestor of (or equal
   to) the new branch HEAD — i.e., `git merge-base --is-ancestor
   <remote_tracking_point> HEAD` succeeds.

If the branch **cannot** be pushed normally (no upstream, or the remote
tracking point is not an ancestor — meaning `--force` would be required),
**do not offer to push**. End silently after the Phase C output.

If the branch **can** be pushed normally, call `AskUserQuestion` with
**Push** / **Skip** options asking whether to push now. If the user selects
**Push**, run `git push`. If the user selects **Skip**, do nothing.
