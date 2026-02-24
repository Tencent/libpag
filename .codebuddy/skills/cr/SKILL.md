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

Run pre-checks, then match the **first** applicable rule top-to-bottom:

1. `git branch --show-current` → record whether on main/master.
2. `git status --porcelain` → record whether uncommitted changes exist.

| # | Condition | Action |
|---|-----------|--------|
| 1 | `$ARGUMENTS` is a PR number or URL containing `/pull/` | → `references/pr-review.md` |
| 2 | `$ARGUMENTS` is empty and uncommitted changes exist | → `references/local-review.md` with `FIX_MODE=none` |
| 3 | `$ARGUMENTS` is empty, no uncommitted changes, on main/master | Abort with usage examples (see below) |
| 4 | Everything else | → Questions below |

Each `→` means: `Read` the target file and follow it as the sole remaining
instruction. Ignore all sections below. Do NOT review from memory or habit —
each target file defines specific constraints on how to obtain diffs, apply
fixes, and submit results.

**Abort message** (rule 3): show usage examples —
`/cr` (uncommitted changes or current branch),
`/cr a1b2c3d`, `/cr a1b2c3d..e4f5g6h`,
`/cr src/foo.cpp`, `/cr 123`, `/cr https://github.com/.../pull/123`.

---

## Questions (rule 4 only)

**Q1 — Teams**:

- "No": quick single-agent review.
- "Yes": multi-agent deep review with reviewer–verifier adversarial mechanism.

**Q2 — Auto-fix**:

If on main/master: inform user that auto-fix is unavailable (protected branch),
set `FIX_MODE=none`, skip Q2.

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
target file.
