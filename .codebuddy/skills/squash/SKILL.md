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
  stop and ask for user confirmation in Phase B** — this is the one and only
  mandatory interaction point in the entire skill. See Phase B for details.
- **NEVER use `$()`, backtick substitution, or shell variable assignment in
  Bash tool calls** — the sandbox rejects all of these. Each Bash call must be
  a plain, self-contained command with no dynamic expansion. When one command's
  output is needed by another, run the first command in a separate Bash call,
  read its output, then construct the next command as a literal string yourself
  (i.e. paste the value directly into the command text).

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

Collect the commit list in `{squash_base}..{squash_end}` using a single
`git log --first-parent --stat --format="%H %P %s"` command (newest first).
The format includes commit hash, parent hashes (space-separated), and subject
— parent hashes with two or more entries identify merge commits.
`--first-parent` ensures that only the current branch's own commits are listed
— commits brought in by merge points (e.g. merging main into the branch) are
**not expanded** and do not appear individually. Each merge point itself
appears as a single merge commit in the list and will be preserved as-is in
Step 3. If the range is empty, inform the user and stop.

---

## Step 2: Analyze & Propose

Using the `git log` output from Step 1:

### Analysis passes

**`git diff` must NEVER be used to decide grouping, and NEVER for individual
commits.** The only permitted use is in Pass 2: one `git diff` per **group**
to read the combined diff for drafting the new commit message. No other Bash
commands may be run during Step 2. All information needed for grouping is
already in the Step 1 `git log` output (commit messages and `--stat` file
lists).

**Pre-pass — Mark merge commits**: Identify all merge commits (commits with
two or more parents). Mark each as **[merge]** immediately — they are
preserved as-is in Step 3 and take no further part in analysis. They act as
segment boundaries that split the remaining commits into independent segments.

**Pass 1 — Scan and group**: Within each segment (non-merge commits between
two adjacent merge commits, or between the range boundary and a merge commit),
apply the **Grouping criteria** below to identify which adjacent commits
should be merged.

**Pass 2 — Draft messages**: For each squashed or reworded group, read the
**single combined diff** spanning the group (`git diff <oldest>~1 <newest>` —
using `<oldest>~1` so the oldest commit's changes are included) and use it to
draft an accurate commit message. One diff per group, not per commit.

### Grouping criteria

The task is simple: **walk through commits in order and check whether adjacent
commits can be merged.** Never reorder commits. For example, if commit A and
commit C are a revert pair but commit B sits between them, do NOT try to
eliminate A and C — just evaluate A↔B and B↔C adjacency as usual.

For each pair of adjacent non-merge commits, collect their `--stat` file lists
and compute the **overlap ratio** = (number of shared paths) / (size of the
smaller list). Merge the pair when **either** condition is met:

1. **File overlap ≥ 50 %** — the commits modify largely the same files,
   indicating work on the same functional module.
2. **Message similarity** — no file overlap (or below 50 %), but their commit
   messages clearly reference the same module or feature keyword (e.g., both
   mention "TextBox", "squash skill", "CMake option"), or the later commit
   carries a continuation signal ("fix …", "wip", "update", "clarify").

Once two commits form a group, compare the next commit against the **union** of
all files in the group using the same overlap ratio, and also check message
similarity against the group's overall topic. Extend greedily.

Do **NOT** merge when:

- Neither condition is met — files are disjoint **and** messages point to
  different topics.
- Commits are separated by a merge commit.

### Commit message quality check

Review every non-grouped commit message against the project's commit
conventions. Mark commits whose messages need rewriting (e.g., vague messages
like "wip", "fix", "update", or messages violating the project's format).

### Result

If nothing to merge or rewrite, inform the user the history is already clean
and **stop — do not show a report or ask any questions**.

Otherwise, mark each commit as **[keep]**, **[squash]**, **[reword]**, or
**[merge]** and prepare the squash report (do not present it yet).

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
order as the `git log` output and the analysis in Pass 1. Unchanged commits
show their original hash and message. Squashed or reworded entries show the
proposed new message with the original commits listed below. The report covers
**only** the analyzed range.

Do **not** present the report here — it will be shown in Phase B for user
confirmation.

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

Process the commits from **Step 1's list** (not a raw git range) in
**chronological order** (oldest first) — reverse of Step 1's listing order:

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
and stop.

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
