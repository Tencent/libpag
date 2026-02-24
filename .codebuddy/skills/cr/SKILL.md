---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# /cr — Code Review

All user-facing text matches the user's language; use interactive dialogs with
selectable options for predefined choices.

## Route

Determine mode from `$ARGUMENTS`, then follow the matching section below:

| `$ARGUMENTS` pattern | Mode |
|----------------------|------|
| Purely numeric (`123`) or URL containing `/pull/` | **PR mode** |
| Everything else (empty, commit, range, path) | **Local mode** |

---

## PR Mode

No questions — `Read` `references/pr-review.md` and follow every step. Two
constraints that differ from a typical PR review:

1. **Worktree mode**: fetch the PR branch locally via `git worktree add` and
   review code in the worktree. NEVER use `gh pr diff` or any GitHub API to
   obtain the diff.
2. **Line-level comments only**: submit results via `gh api` as line-level PR
   comments. NEVER use `gh pr comment` or `gh pr review`.

---

## Local Mode

### Pre-check

1. `git branch --show-current` → record whether on main/master.
2. `git status --porcelain` → record whether uncommitted changes exist.
3. If on main/master, no uncommitted changes, and `$ARGUMENTS` is empty → abort
   (nothing to review).

### Questions

**Q1 — Teams**:

- "No": quick single-agent review.
- "Yes": multi-agent deep review with reviewer–verifier adversarial mechanism.

**Q2 — Auto-fix**:

If on main/master or uncommitted changes exist: inform user that auto-fix is
unavailable (uncommitted changes or protected branch), skip Q2.

Otherwise:

- "Review only" → `FIX_MODE=none`: report issues without fixing.
- "Low risk only" → `FIX_MODE=low`: auto-fix only the most straightforward
  issues (e.g., null checks, typos, naming). Confirm everything else.
- "Low + Medium risk (recommended)" → `FIX_MODE=low_medium`: auto-fix most
  issues, only confirm high-risk ones (e.g., API changes, architecture
  decisions).
- "Full auto" → `FIX_MODE=full`: auto-fix everything. Only issues affecting
  test baselines are deferred.

### Hand off

| Q1 Teams | → |
|----------|---|
| No | `references/local-review.md` |
| Yes | `references/teams-review.md` |

Pass `$ARGUMENTS` and `FIX_MODE` (none / low / low_medium / full) to the
target file. Hand off entirely and stop processing this file.
