---
name: commit
description: Commit local changes without pushing. Use when user says "commit", "save changes", "commit my work", or "commit code".
---

# Commit — Local Commit Only

Commits local changes to the current branch. Does not push or create a PR.
If on the default branch, creates a new branch first.

## Instructions

- **NEVER** commit directly on the default branch (main/master) — always
  create a new branch first.
- **NEVER** push. This skill only creates local commits.
- All user-facing text must use the language the user has been using in the
  conversation. Do not default to English.
- When presenting choices, use interactive dialogs with selectable options
  rather than plain text.

---

## Step 1: Pre-flight

`git branch --show-current` — current branch name.
If empty (detached HEAD), ask the user to switch to a branch first and stop.

Determine the remote's default branch and store as `{default_branch}`.

If on {default_branch}, create a new branch before committing:

- Generate a branch name following the project's branch naming convention if
  one exists; otherwise use `feature/{username}_topic` or
  `bugfix/{username}_topic` (`{username}` = GitHub username, lowercase; obtain via `gh api user -q .login`).
- `git checkout -b {branch_name}`

---

## Step 2: Stage & Commit

Run `git status --porcelain` and inspect the output:

- **No output** → no local changes. Inform the user and stop.
- **Both staged and unstaged changes** → ask the user: commit only the staged
  files (**partial**), or stage everything (**full**)?
- **Otherwise** → **full** (stage everything).

If **full**: run `git add -A`. If **partial**: skip (files are already staged).

Read the staged diff (`git diff --cached`) and generate a commit message
following the project's commit conventions. If no convention is found, default
to a concise English message under 120 characters describing the change.

Commit and output the commit message to the user.
