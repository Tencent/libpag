---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix.
---

# /cr — Code Review

Automated code review for local branches, PRs, commits, and files. Detects
review mode from arguments and routes to the appropriate review flow — either
quick single-agent review with interactive fix selection, or multi-agent
deep review with risk-based auto-fix.

All user-facing text matches the user's language; use interactive dialogs with
selectable options for predefined choices.

## Route

Run pre-checks, then match the **first** applicable rule top-to-bottom:

1. `git branch --show-current` → record whether on main/master.
2. `git status --porcelain` → record whether uncommitted changes exist.

| # | Condition | Action |
|---|-----------|--------|
| 1 | `$ARGUMENTS` is a PR number or URL containing `/pull/` | → `references/pr-review.md` |
| 2 | `$ARGUMENTS` is empty and uncommitted changes exist | → `references/local-review.md` |
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

## Question (rule 4 only)

Check whether the current environment supports agent teams (multiple agents
working in parallel and communicating with each other).

- If **not supported**, skip the question and route to `references/local-review.md`.
- If **on main/master**, skip the question, inform user that auto-fix is
  unavailable (protected branch), and route to `references/local-review.md`.
- Otherwise, ask a **single question**. The question title should inform the
  user that the current environment supports Agent Teams, and ask whether to
  enable deep adversarial review with multi-round auto-fix. Provide 4 options:

| Option | Description |
|--------|-------------|
| Quick review | Single-agent review; interactively choose which issues to fix afterward. |
| Auto-fix: low risk | Multi-agent review; auto-fix only the safest issues (e.g., null checks, typos, naming). Confirm everything else. |
| Auto-fix: low + medium risk (recommended) | Multi-agent review; auto-fix most issues, only confirm high-risk ones (e.g., API changes, architecture). |
| Auto-fix: full | Multi-agent review; auto-fix everything. Only issues affecting test baselines are deferred. |

### Hand off

| Option | → | FIX_MODE |
|--------|---|----------|
| Quick review / skipped | `references/local-review.md` | — |
| Auto-fix: low risk | `references/teams-review.md` | low |
| Auto-fix: low + medium risk | `references/teams-review.md` | low_medium |
| Auto-fix: full | `references/teams-review.md` | full |

Pass `$ARGUMENTS` and `FIX_MODE` (low / low_medium / full) to the target file.
