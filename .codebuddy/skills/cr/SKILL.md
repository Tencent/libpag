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
| `references/code-checklist.md` | Reviewer (code modules) |
| `references/doc-checklist.md` | Reviewer (doc modules) |
| `references/scope-preparation.md` | Coordinator (git/gh commands) |
| `references/pr-comment-format.md` | Coordinator (PR comment format) |
| `references/auto-fix-teams.md` | Coordinator (auto-fix with Agent Teams) |

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

Partition files in scope into **review modules**. Each module is a
self-contained logical unit. Split large files by section/function group; group
related small files together. Classify each module as `code`, `doc`, or `mixed`.

---

## Step 3: Review & Filter

Read each module's files and apply the corresponding checklist:
`references/code-checklist.md` for code, `references/doc-checklist.md` for doc,
both for mixed. Only include priority levels the user selected.

For each issue found:
- Provide a code citation (file:line + snippet).
- Self-verify by re-reading the code — confirm or withdraw.

De-dup: remove cross-module duplicates (same location, same topic). PR mode:
also remove matches to `EXISTING_PR_COMMENTS`.

**PR comment verification** (local mode with associated PR): verify each PR
review comment against current code. Add verified issues to the results.

Assign each confirmed issue a risk level:

| Only one reasonable fix? | Design decision / external contract? | Risk |
|--------------------------|--------------------------------------|------|
| Yes | — | Low |
| No | Yes | High |
| No | No | Medium |

**Fix approach** (Medium/High only): specify the chosen approach and reasoning.

---

## Step 4: Report

### Review-only mode

List all confirmed issues with risk levels and fix approaches.

### PR mode

Present confirmed issues to user. User selects which to submit as PR comments,
declines are marked `skipped`. Submit via `gh api` using
`references/pr-comment-format.md`. Do NOT use `gh pr comment` or `gh pr review`.

Summary of issues found / submitted / skipped. Remove worktree and temp branch.
