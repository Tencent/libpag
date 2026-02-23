---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# /cr — Code Review

You are the **coordinator**. You review code by reading files, applying
checklists, and reporting issues. For auto-fix mode, you create an Agent Team
to dispatch reviewer, verifier, and fixer agents — see
`references/auto-fix-teams.md`.

All user-facing text uses the language the user has been using. Use interactive
dialogs with selectable options for predefined choices.

## References

| File | Used by |
|------|---------|
| `references/code-checklist.md` | Code review checklist |
| `references/doc-checklist.md` | Document review checklist |
| `references/scope-preparation.md` | git/gh commands for scope setup |
| `references/auto-fix-teams.md` | Auto-fix mode with Agent Teams |

---

## Step 1: Ask — zero tool calls, present questions immediately

### Mode detection

| `$ARGUMENTS` pattern | Mode |
|----------------------|------|
| Purely numeric (`123`) | PR |
| URL containing `/pull/` | PR |
| Everything else (empty, commit, range, path) | Local |

### Pre-check (local mode only)

1. `git branch --show-current` → record whether on main/master.
2. `git status --porcelain` → record whether uncommitted changes exist.
3. If on main/master, no uncommitted changes, and `$ARGUMENTS` is empty → abort
   (nothing to review).

### Questions — single interactive prompt

**Q1 — Review priority** (always):

Priority levels apply to both code and document review checklists.

- "Full review (A + B + C)": correctness, optimization, and conventions.
  Code: null checks, duplicate code, naming. Docs: factual errors, clarity, formatting.
- "Correctness + optimization (A + B)": skip conventions and style.
  Code: null checks, resource leaks, simplification. Docs: factual errors, clarity.
- "Correctness only (A)": only safety and correctness issues.
  Code: null dereference, out-of-bounds, race conditions. Docs: factual errors,
  contradictions.

**Q2 — Auto-fix threshold** (local mode only):

PR mode skips Q2 — inform user that issues will be submitted as line-level PR
comments after confirmation.

If on main/master or uncommitted changes exist: inform user that auto-fix is
unavailable (uncommitted changes or protected branch), skip Q2 and enter
review-only mode.

Otherwise:

- "Review only": report issues without fixing. Continue below.
- "Low risk only": auto-fix only the most straightforward issues (e.g.,
  null checks, typos, naming). Confirm everything else.
  → `references/auto-fix-teams.md`.
- "Low + Medium risk": auto-fix most issues, only confirm high-risk ones
  (e.g., API changes, architecture decisions).
  → `references/auto-fix-teams.md`.
- "Full auto": auto-fix everything. Only issues affecting test baselines are
  deferred. → `references/auto-fix-teams.md`.

---

## Step 2: Scope

Follow `references/scope-preparation.md` for all git/gh commands, argument
handling, and PR comment retrieval.

**Uncommitted changes (review-only)**: scope is `git diff HEAD` only, ignoring
branch commits and `$ARGUMENTS`.

**Main/master (no uncommitted changes)**: scope follows normal argument handling.

**PR mode**: fetch PR ref into a worktree, determine `BASE_BRANCH`, fetch diff,
fetch existing PR review comments for de-duplication. Record `WORKTREE_DIR`.

If diff is empty → exit.

---

## Step 3: Review

Read all files in scope. Apply `references/code-checklist.md` to code files,
`references/doc-checklist.md` to documentation files. Only include priority
levels the user selected.

For each issue found:
- Provide a code citation (file:line + snippet).
- Self-verify by re-reading the code — confirm or withdraw.

**PR comment verification** (local mode with associated PR): verify each PR
review comment against current code. Add verified issues to the results.

---

## Step 4: Report

### Review-only mode

List all confirmed issues.

### PR mode

Present confirmed issues to user. User selects which to submit as PR comments,
declines are marked `skipped`.

Submit as a **single** GitHub PR review with line-level comments via `gh api`.
Do NOT use `gh pr comment` or `gh pr review`.

```bash
gh api repos/{owner}/{repo}/pulls/{number}/reviews --input - <<'EOF'
{
  "commit_id": "{HEAD_SHA}",
  "event": "COMMENT",
  "comments": [
    {
      "path": "relative/file/path",
      "line": 42,
      "side": "RIGHT",
      "body": "Description of the issue and suggested fix"
    }
  ]
}
EOF
```

- `commit_id`: HEAD SHA of the PR branch
- `path`: relative to repository root
- `line`: line number in the **new** file (right side of diff)
- `side`: always `"RIGHT"`
- `body`: concise, in the user's conversation language, with a specific fix
  suggestion when possible

Summary of issues found / submitted / skipped. Remove worktree and temp branch.
