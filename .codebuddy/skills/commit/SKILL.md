---
name: commit
description: Commit local changes without pushing.
disable-model-invocation: true
---

# Commit — Local Commit Only

Commits local changes to the current branch. Does not push or create a PR.

## Instructions

- **NEVER** push. This skill only creates local commits.
- All user-facing text must use the language the user has been using in the
  conversation. Do not default to English.

---

## Stage & Commit

Run `git status --porcelain` and inspect the output:

- **No output** → no local changes. Inform the user and stop.
- **Staged changes exist, no unstaged changes** → proceed directly to commit.
- **Staged changes exist, unstaged changes also exist** → ask the user: commit
  only the staged files (**partial**), or stage everything (**full**)?
  If **partial**: skip staging (files are already staged).
  If **full**: run `git add -A` first.
- **No staged changes, unstaged changes exist** → run `git add -A` to stage
  everything, then commit.

Read the staged diff (`git diff --cached`) and generate a commit message
following the project's commit conventions. If no convention is found, default
to a concise English message under 120 characters ending with a period, with no other punctuation, focusing on user-perceivable changes.

Commit, then output a single line in the format:
`已提交 {short_hash}：{commit message}`

If the user chose **partial**, also inform them that unstaged changes remain
and were not included in this commit.
