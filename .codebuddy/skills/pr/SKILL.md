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

Run these three commands **in parallel** (they are independent):

1. `git branch --show-current` — current branch name
2. `gh pr list --head {branch} --state open --json number,url` — open PRs
3. `gh api user -q '.login'` — GitHub username

Detect the **default branch** of the remote:

```
git symbolic-ref refs/remotes/origin/HEAD | sed 's@^refs/remotes/origin/@@'
```

If this fails (e.g., `origin/HEAD` not set), fall back to `main`; if `main`
does not exist, try `master`.

Store this as `{default_branch}` for use throughout.

**Mode selection**:

| Current branch | Open PR on this branch | Mode |
|----------------|------------------------|------|
| {default_branch} | — | **Create** |
| other | yes | **Append** |
| other | no | **Create** |

---

## Step 2: Stage & Generate Commit Message

Run `git status --porcelain` and inspect the output:

- **No output** → no local changes. Skip to Step 3 (push-only path).
- **Both staged and unstaged changes** → ask the user: commit only the staged
  files (**partial**), or stage everything (**full**)?
- **Otherwise** → **full** (stage everything).

Detection: first column non-space = staged content; second column non-space or
`??` prefix = unstaged/untracked content.

If **full**: run `git add -A`. If **partial**: skip (files are already staged).

Read the staged diff (`git diff --cached`) and generate a commit message
following the project's commit conventions. If no convention is found, default
to a concise English message under 120 characters describing the change.

---

## Step 3: Commit & Push

### Append mode

Check for unpushed commits: `git log origin/{branch}..HEAD --oneline`

| Staged content | Unpushed commits | Action |
|----------------|------------------|--------|
| yes | — | commit → push |
| no | yes | push only |
| no | no | inform user "nothing to push" and stop |

Push and output the commit message (if committed) and the existing PR URL.

### Create mode

Gather all changes that will be part of the PR:

- Existing commits: `git log origin/{default_branch}..HEAD --oneline`
- Current staged diff (from Step 2)

If both are empty, inform user there are no changes and stop.

#### Generate PR metadata

Based on the full changeset, generate:

- **Branch name**: follow the project's branch naming convention if one exists;
  otherwise use `feature/{username}_topic` or `bugfix/{username}_topic`
  (`{username}` = GitHub login from Step 1, lowercase).
- **PR title**: concise summary following project conventions, or a short
  English sentence if none found.
- **PR description**: plain text (no Markdown formatting) in the user's
  conversation language, briefly describing what changed and why.

When there is only one commit, the PR title may reuse the commit message.

#### Create branch, commit, and push

Create or rename the branch:

| Current branch | Action |
|----------------|--------|
| {default_branch} | `git checkout -b {branch_name}` |
| other | `git branch -m {branch_name}` |

If the branch name already exists locally, append a numeric suffix (e.g.,
`feature/user_topic_2`).

Commit (if there is staged content), push, and create the PR:

```
git commit -m "{commit_message}"
git push -u origin {branch_name}
gh pr create --title "{title}" --body "$(cat <<'EOF'
{description}
EOF
)"
```

Output the PR URL to the user.

---

## Error Handling

- **Push rejected** (remote has new commits): run `git pull --rebase origin
  {branch}`, then retry push once. If still failing, inform the user.
- **`gh` not installed or not authenticated**: inform the user to install
  (`brew install gh` on macOS, or https://cli.github.com) and run
  `gh auth login`.
- **PR creation fails**: output the error. The commit and push are already done,
  so instruct the user to create the PR manually or retry `/pr`.
