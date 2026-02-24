---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# /cr — Code Review

Automated code review for local branches, PRs, commits, and files. Detects
review mode from arguments, asks the user about review priority and auto-fix
preferences, then routes to the appropriate review flow. Supports multi-round
iteration — each round discovers issues, applies risk-based auto-fixes, and
loops until no new issues are found.

All user-facing text matches the user's language; use interactive dialogs with
selectable options for predefined choices.

## References

| File | Purpose |
|------|---------|
| `references/local-review.md` | Local review flow (single agent) |
| `references/pr-review.md` | PR review flow |
| `references/teams-review.md` | Teams review flow (multi-agent) |
| `references/code-checklist.md` | Code review checklist |
| `references/doc-checklist.md` | Document review checklist |
| `references/judgment-matrix.md` | Risk levels, worth-fixing criteria, special rules |
| `references/scope-detection.md` | Shared scope detection logic (local & teams) |

---

## Detect and Ask

### Mode detection

| `$ARGUMENTS` pattern | Mode |
|----------------------|------|
| Purely numeric (`123`) | PR |
| URL containing `/pull/` | PR |
| Everything else (empty, commit, range, path) | Local |

**PR mode**: skip Q2 and Q3 — ask only Q1, then hand off to
`references/pr-review.md`.

### Pre-check (local mode only)

1. `git branch --show-current` → record whether on main/master.
2. `git status --porcelain` → record whether uncommitted changes exist.
3. If on main/master, no uncommitted changes, and `$ARGUMENTS` is empty → abort
   (nothing to review).

### Questions — single interactive prompt

**Q1 — Review priority**:

Priority levels apply to both code and document review checklists.

- "Full review (A + B + C)": correctness, optimization, and conventions.
  Code: null checks, duplicate code, naming. Docs: factual errors, clarity, formatting.
- "Correctness + optimization (A + B)": skip conventions and style.
  Code: null checks, resource leaks, simplification. Docs: factual errors, clarity.
- "Correctness only (A)": only safety and correctness issues.
  Code: null dereference, out-of-bounds, race conditions. Docs: factual errors,
  contradictions.

**Q2 — Teams**:

- "No": quick single-agent review.
- "Yes": multi-agent deep review with reviewer–verifier adversarial mechanism.

**Q3 — Auto-fix**:

If on main/master or uncommitted changes exist: inform user that auto-fix is
unavailable (uncommitted changes or protected branch), skip Q3.

Otherwise:

- "Review only" → `FIX_MODE=none`: report issues without fixing.
- "Low risk only" → `FIX_MODE=low`: auto-fix only the most straightforward issues (e.g.,
  null checks, typos, naming). Confirm everything else.
- "Low + Medium risk (recommended)" → `FIX_MODE=low_medium`: auto-fix most issues, only confirm high-risk ones
  (e.g., API changes, architecture decisions).
- "Full auto" → `FIX_MODE=full`: auto-fix everything. Only issues affecting test baselines are
  deferred.

### Route

| Q2 Teams | → |
|----------|---|
| No | `references/local-review.md` |
| Yes | `references/teams-review.md` |

Pass to the target file: `$ARGUMENTS`, `REVIEW_PRIORITY`, `FIX_MODE` (none /
low / low_medium / full). Hand off entirely and stop here.
