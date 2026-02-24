---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix.
---

# /cr — Code Review

Automated code review for local branches, PRs, commits, and files. Detects
review mode from arguments, asks the user about auto-fix preferences, then
routes to the appropriate review flow. Supports multi-round iteration — each
round discovers issues, applies risk-based auto-fixes, and loops until no new
issues are found.

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

No questions — `Read` `references/pr-review.md` and follow it exactly. Do NOT
review the PR from memory or habit — the flow requires worktree-based local
checkout and line-level comments, with specific constraints on how to obtain the
diff and submit results.

---

## Local Mode

### Pre-check

1. `git branch --show-current` → record whether on main/master.
2. `git status --porcelain` → record whether uncommitted changes exist.
3. If on main/master, no uncommitted changes, and `$ARGUMENTS` is empty → abort
   with usage examples: `/cr` (uncommitted changes or current branch),
   `/cr a1b2c3d`, `/cr a1b2c3d..e4f5g6h`,
   `/cr src/foo.cpp`, `/cr 123`, `/cr https://github.com/.../pull/123`.

### Questions

**Q1 — Teams** (only when no uncommitted changes exist):

- "No": quick single-agent review.
- "Yes": multi-agent deep review with reviewer–verifier adversarial mechanism.

Skip this question if uncommitted changes exist — use single-agent mode.

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
