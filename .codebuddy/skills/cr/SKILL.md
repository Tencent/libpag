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
3. Check whether the current environment supports agent teams (multiple agents
   working in parallel and communicating with each other).

| # | Condition | Action |
|---|-----------|--------|
| 1 | `$ARGUMENTS` is a PR number or URL containing `/pull/` | → `references/pr-review.md` |
| 2 | Not on main/master, and agent teams supported | → Question below |
| 3 | Everything else | → `references/local-review.md` |

Each `→` means: `Read` the target file and follow it as the sole remaining
instruction. Ignore all sections below. Do NOT review from memory or habit —
each target file defines specific constraints on how to obtain diffs, apply
fixes, and submit results.

---

## Question (rule 2 only)

Ask a **single question**. The question title should inform the user that the
current environment supports Agent Teams, and ask whether to enable multi-agent
review with reviewer–verifier adversarial mechanism and auto-fix. Provide 4
options:

| Option | Description |
|--------|-------------|
| Quick review | Single-agent review; interactively choose which issues to fix afterward. |
| Auto-fix: low risk | Multi-agent review; auto-fix only the safest issues (e.g., null checks, typos, naming). Confirm everything else. |
| Auto-fix: low + medium risk (recommended) | Multi-agent review; auto-fix most issues, only confirm high-risk ones (e.g., API changes, architecture). |
| Auto-fix: full | Multi-agent review; auto-fix everything. Only issues affecting test baselines are deferred. |

### Hand off

| Option | → | FIX_MODE |
|--------|---|----------|
| Quick review | `references/local-review.md` | — |
| Auto-fix: low risk | `references/teams-review.md` | low |
| Auto-fix: low + medium risk | `references/teams-review.md` | low_medium |
| Auto-fix: full | `references/teams-review.md` | full |

Pass `$ARGUMENTS` to the target file. For teams-review, also pass `FIX_MODE`
(low / low_medium / full).
