---
name: pr
description: Commit and push changes, then create a new PR or append to an existing one. Use when user says "submit PR", "push changes", "create pull request", or "open a PR".
---

# PR — Commit & Push

Commits local changes, pushes to remote, and creates a new PR or appends to an
existing one — automatically detected based on branch state.

## Instructions

- **NEVER** force push.
- **NEVER** push directly to main/master — always go through PR workflow.
- All user-facing text must use the language the user has been using in the
  conversation. Do not default to English.

---

## Step 1: Pre-flight

1. `git branch --show-current` — current branch name.
   If empty (detached HEAD), ask the user to switch to a branch first and stop.
2. Then run in parallel:
   - `gh pr list --head {branch} --state open --json number,url` — open PRs
   - `gh api user -q '.login'` — GitHub username

Determine the remote's default branch and store as `{default_branch}`.

**Mode selection**:

| Current branch | Open PR on this branch | Mode |
|----------------|------------------------|------|
| {default_branch} | — | **Create** |
| other | yes | **Append** |
| other | no | **Create** |

---

## Step 2: Stage & Generate Commit Message

Run `git status --porcelain` and inspect the output:

- **No output** → no local changes. Skip to Step 3.
- **Both staged and unstaged changes** → ask the user: commit only the staged
  files (**partial**), or stage everything (**full**)?
- **Otherwise** → **full** (stage everything).

If **full**: run `git add -A`. If **partial**: skip (files are already staged).

Read the staged diff (`git diff --cached`) and generate a commit message
following the project's commit conventions. If no convention is found, default
to a concise English message under 120 characters describing the change.

---

## Step 3: Commit & Push

### Append mode

If there is staged content, commit first. Then push and output the existing
PR URL.

If there is nothing to commit and nothing to push, inform the user and stop.

### Create mode

If there is staged content, commit first.

#### Generate PR metadata

Based on all commits since {default_branch}, generate:

- **Branch name** (only when on {default_branch}): follow the project's branch
  naming convention if one exists; otherwise use `feature/{username}_topic` or
  `bugfix/{username}_topic` (`{username}` = GitHub login from Step 1, lowercase).
  When on a non-default branch, use the current branch name.
- **PR title**: concise summary following project conventions, or a short
  English sentence if none found. May reuse the commit message when there is
  only one commit.
- **PR description**: plain text (no Markdown formatting) in the user's
  conversation language, briefly describing what changed and why.

#### Create branch and push

If on {default_branch}, create a new branch: `git checkout -b {branch_name}`.

Push and create the PR:

```
git push -u origin {branch_name}
gh pr create --title "{title}" --body "$(cat <<'EOF'
{description}
EOF
)"
```

Output the PR URL to the user.
