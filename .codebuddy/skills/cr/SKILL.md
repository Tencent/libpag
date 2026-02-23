---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# /cr — Code Review

You are the **coordinator**. You review code by reading files, applying
checklists, and reporting issues. For auto-fix mode, you create an Agent Team
to dispatch reviewer, verifier, and fixer agents — see
`references/auto-fix-teams.md`.

## Operating rules

- **Immediate interaction**: Mode detection (Phase 0.1) is pure string parsing —
  zero tool calls. Present Phase 0 questions immediately. Do NOT read any files,
  references, or run any commands before the user answers.
- **Autonomy**: After Ask (Phase 0), zero user interaction until Report
  (Phase 4). Pre-flight failures abort with a clear message.
- **Error handling**: Handle unexpected situations autonomously. Record anything
  unresolvable to `CR_STATE_FILE` for inclusion in the report.
- **User language**: All user-facing text uses the language the user has been
  using. Use interactive dialogs with selectable options for predefined choices.

## References

| File | Used by |
|------|---------|
| `references/code-checklist.md` | Reviewer (code modules) |
| `references/doc-checklist.md` | Reviewer (doc modules) |
| `references/judgment-matrix.md` | Coordinator (risk & worth-fixing) |
| `references/pending-templates.md` | Coordinator (CR_STATE_FILE format) |
| `references/scope-preparation.md` | Coordinator (git/gh commands) |
| `references/pr-comment-format.md` | Coordinator (PR comment format) |
| `references/auto-fix-teams.md` | Coordinator (auto-fix mode with Agent Teams) |

---

## Flow

```
Ask → Scope → Review → Filter → Report
```

This is the default flow — the coordinator reviews directly without a team.
Auto-fix mode (`references/auto-fix-teams.md`) extends this with teams,
fixing, and multi-round iteration.

---

## Phase 0: Ask

### 0.1 Mode detection — string parsing only, zero tool calls

| `$ARGUMENTS` pattern | Mode |
|----------------------|------|
| Purely numeric (`123`) | PR |
| URL containing `/pull/` | PR |
| Everything else (empty, commit, range, path) | Local |

### 0.2 Pre-check — local mode only, before questions

1. `git branch --show-current` → record whether on main/master.
2. `git status --porcelain` → record whether uncommitted changes exist.
3. If on main/master, no uncommitted changes, and `$ARGUMENTS` is empty → abort
   (nothing to review).

### 0.3 Questions — single interactive prompt

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
- "Full auto (risky)": auto-fix everything. Only issues affecting test
  baselines are deferred. → `references/auto-fix-teams.md`.

**No further user interaction until Phase 4 Report (or Phase 7 Confirm in
auto-fix / PR mode).**

---

## Phase 1: Scope

### 1.1 Init

1. Create `.cr-cache/` if needed. CR_STATE_FILE path:
   - PR mode: `.cr-cache/pr-{number}.md`
   - Local mode: `.cr-cache/{branch}.md` (sanitize `/` to `-`)
   If exists, append a suffix to avoid conflict (e.g., `feature-foo-2.md`).

### 1.2 Prepare scope

Follow `references/scope-preparation.md` for all git/gh commands, argument
handling, and PR comment retrieval.

**Uncommitted changes (review-only)**: scope is `git diff HEAD` only, ignoring
branch commits and `$ARGUMENTS`.

**Main/master (no uncommitted changes)**: scope follows normal argument handling.

**PR mode**: fetch PR ref into a worktree, determine `BASE_BRANCH`, fetch diff,
fetch existing PR review comments for de-duplication. Record `WORKTREE_DIR`.

If diff is empty → exit.

### 1.3 Module partitioning

Partition files in scope into **review modules**.

- Each module is a self-contained logical unit. Split large files by
  section/function group; group related small files together. Balance workload.
- Classify each module as `code`, `doc`, or `mixed` for checklist selection.

### 1.4 Persist state

Write CR_STATE_FILE. See `references/pending-templates.md` for format.

**`# Session`**: mode, user choices (priority), file list with module
assignments and types, changed line ranges per file.

**`# Issues`**: updated incrementally during review.

→ Phase 2

---

## Phase 2: Review — coordinator direct

Read each module's files and apply the corresponding checklist:
`references/code-checklist.md` for code, `references/doc-checklist.md` for doc,
both for mixed. Only include priority levels the user selected.

For each issue found:
- Provide a code citation (file:line + snippet).
- Self-verify by re-reading the code — confirm or withdraw.
- Output: `[file:line] [A/B/C] — [description] — [key lines]`

**PR comment verification** (local mode with associated PR): verify each PR
review comment against current code. Add verified issues to the results.

→ Phase 3

---

## Phase 3: Filter — coordinator

### 3.0 De-dup

- Remove cross-module duplicates (same location, same topic).
- PR mode: remove matches to existing PR comments.

### 3.1 Risk level — per `references/judgment-matrix.md`

| Only one reasonable fix? | Design decision / external contract? | Risk |
|--------------------------|--------------------------------------|------|
| Yes | — | Low |
| No | Yes | High |
| No | No | Medium |

**Fix approach** (Medium/High only): specify the chosen approach and reasoning.
Record in the issue's `Proposed` field. Low risk: single obvious fix, no guidance.

Consult `references/judgment-matrix.md` for worth-fixing criteria and special rules.

### 3.2 Record to CR_STATE_FILE

All confirmed issues are recorded with risk level and fix approach.

→ Phase 4 (review-only) or Phase 7 Confirm (PR mode).

---

## Phase 4: Report

### Review-only mode

List all confirmed issues with risk levels and fix approaches. No fix/skip/fail
stats. Delete CR_STATE_FILE.

### PR mode — Phase 7: Confirm

Present confirmed issues to user. User selects which to submit as PR comments,
declines are marked `skipped`. Submit via `gh api` using
`references/pr-comment-format.md`. Do NOT use `gh pr comment` or `gh pr review`.

### PR mode — Phase 8: Report

Summary of issues found / submitted / skipped. Remove worktree and temp branch.
Delete CR_STATE_FILE.

