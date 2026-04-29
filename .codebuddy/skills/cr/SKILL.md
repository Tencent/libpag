---
name: cr
description: >-
  Automated code review and fix for local branches, PRs, commits, and files.
  Supports single-agent interactive fix and multi-agent adversarial review with
  auto-fix.
disable-model-invocation: true
---

# /cr — Code Review

Automated code review for local branches, PRs, commits, and files. Detects
review mode from arguments and routes to the appropriate review flow — either
single-agent review on protected branches or when teams are unavailable, or
multi-agent deep review with risk-based auto-fix.

All user-facing text matches the user's language. Start the review immediately
based on the routing rules below — do NOT ask the user to choose the review
mode upfront. Interactive prompts inside each flow (e.g. final confirmation of
high-risk fixes) still use your interactive dialog tool (e.g. AskUserQuestion).

## Route

Run pre-checks, then match the **first** applicable rule top-to-bottom:

1. `git branch --show-current` → record whether on main/master.
2. `git status --porcelain` → record whether uncommitted changes exist.
3. Check whether the current environment supports agent teams (multiple agents
   working in parallel and communicating with each other).

| # | Condition | Action |
|---|-----------|--------|
| 1 | `$ARGUMENTS` is `diag` | → `references/diagnosis.md` |
| 2 | `$ARGUMENTS` is a PR number or URL containing `/pull/` | → `references/pr-review.md` |
| 3 | On main/master branch | → `references/local-review.md` |
| 4 | Agent teams NOT supported | → `references/local-review.md` |
| 5 | Everything else | → `references/teams-review.md` with `FIX_MODE=low_medium` |

Each `→` means: `Read` the target file and follow it as the sole remaining
instruction. Ignore all sections below. Do NOT review from memory or habit —
each target file defines specific constraints on how to obtain diffs, apply
fixes, and submit results.

Pass `$ARGUMENTS` to the target file. For teams-review, also pass `FIX_MODE`
(fixed to `low_medium`: multi-agent review auto-fixes low and medium risk
issues; high-risk issues such as API changes or architecture impact are
deferred to user confirmation).
