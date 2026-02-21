---
name: pr
description: Commit and push changes, auto-detecting whether to create a new PR or append to an existing one. Use when the user wants to submit, push, or create a pull request.
---

# PR — Commit & Push

Use `/pr` to start, or triggered automatically when the user asks to submit a PR,
push changes, or create a pull request.

Commits staged/unstaged changes, runs code formatting, pushes to remote, and either
creates a new PR or appends to an existing one — automatically detected.

## Instructions

- **NEVER** force push.
- **NEVER** push directly to main/master — always go through PR workflow.
- All user-facing interactions must use the language the user has been using in the
  conversation. Do not default to English.

---

## Step 0: Pre-flight

Gather current state in a single step:

1. Current branch name (`git branch --show-current`)
2. Open PRs on this branch (`gh pr list --head {branch} --state open --json number,url`)
3. GitHub username (`gh api user -q '.login'`)

**Mode selection**:

| Current branch | Open PR exists | Mode |
|----------------|----------------|------|
| main/master | — | **Create** |
| other | yes | **Append** |
| other | no | **Create** |

---

## Step 1: Determine Commit Scope

Run `git status --porcelain` and inspect the output:

- **No output**: no local changes — skip Step 2-3.
- **Both staged and unstaged changes exist**: ask the user whether to commit only
  staged files (partial) or everything (full).
- **Otherwise**: commit everything (full).

Detection: first column non-space = staged content; second column non-space or `??`
prefix = unstaged/untracked content.

For **partial** commits, record the staged file list (`git diff --cached --name-only`)
for use in Step 2.

---

## Step 2: Code Formatting

If `./codeformat.sh` exists, run it (ignore its stderr output).

After formatting, re-stage files according to Step 1:

| Scope | Action |
|-------|--------|
| Full | `git add .` |
| Partial | `git add` only the files recorded in Step 1 |

---

## Step 3: Generate Commit Message

Read the staged diff (`git diff --cached`) and generate a commit message: English,
under 120 characters, ending with a period, focusing on user-visible changes.

---

## Step 4: Commit & Push

### Append mode

Check for unpushed commits: `git log origin/{branch}..HEAD --oneline`

| Staged content | Unpushed commits | Action |
|----------------|------------------|--------|
| yes | — | commit + push |
| no | yes | push only |
| no | no | inform user "no changes to submit", stop |

Output the commit message (if committed) and the existing PR URL.

### Create mode

#### 1. Analyze full changeset

Gather all changes that will be part of the PR:
- Existing commits: `git log origin/main..HEAD --oneline`
- Current staged diff (reuse from Step 3 if available)

If both are empty, inform user there are no changes and stop.

#### 2. Generate PR metadata

Based on the full changeset, generate:

- **Branch name**: `feature/{username}_module` or `bugfix/{username}_module`
  (`{username}` = GitHub login from Step 0, lowercase; module name in snake_case,
  max two words)
- **PR title**: English, under 120 characters, ending with a period, focusing on
  user-visible changes
- **PR description**: plain text in the user's conversation language, briefly
  describing what changed and why

#### 3. Create branch, commit, and push

Switch to the new branch:

| Current branch | Action |
|----------------|--------|
| main/master | `git checkout -b {branch_name}` |
| other | `git branch -m {branch_name}` |

Then commit (if staged content exists) and push:
```
git commit -m "{commit_message}"
git push -u origin {branch_name}
gh pr create --title "{title}" --body "{description}"
```

Output the PR title, description, and the new PR URL.
