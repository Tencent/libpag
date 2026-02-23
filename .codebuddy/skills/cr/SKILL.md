---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# /cr — Code Review

You are the **coordinator**. Read changed files, apply checklists, and report
issues. All user-facing text matches the user's language; use interactive
dialogs with selectable options for predefined choices.

## References

| File | Purpose |
|------|---------|
| `references/code-checklist.md` | Code review checklist |
| `references/doc-checklist.md` | Document review checklist |
| `references/scope-preparation.md` | Local mode scope setup |
| `references/pr-review.md` | PR review flow |
| `references/auto-fix-teams.md` | Auto-fix flow with Agent Teams |

---

## Step 1: Detect and Ask — zero tool calls, present questions immediately

### Mode detection

| `$ARGUMENTS` pattern | Mode |
|----------------------|------|
| Purely numeric (`123`) | PR → `references/pr-review.md` |
| URL containing `/pull/` | PR → `references/pr-review.md` |
| Everything else (empty, commit, range, path) | Local |

**PR mode**: hand off entirely to `references/pr-review.md` and stop here.

### Pre-check (local mode)

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

**Q2 — Auto-fix threshold**:

If on main/master or uncommitted changes exist: inform user that auto-fix is
unavailable (uncommitted changes or protected branch), skip Q2.

Otherwise:

- "Review only": report issues without fixing.
- "Low risk only": auto-fix only the most straightforward issues (e.g.,
  null checks, typos, naming). Confirm everything else.
  → `references/auto-fix-teams.md`.
- "Low + Medium risk": auto-fix most issues, only confirm high-risk ones
  (e.g., API changes, architecture decisions).
  → `references/auto-fix-teams.md`.
- "Full auto": auto-fix everything. Only issues affecting test baselines are
  deferred. → `references/auto-fix-teams.md`.

**Auto-fix mode**: hand off to `references/auto-fix-teams.md` and stop here.

---

## Step 2: Scope

Follow `references/scope-preparation.md` for all git/gh commands and argument
handling.

If diff is empty → exit.

---

## Step 3: Review

Read all files in scope. Apply `references/code-checklist.md` to code files,
`references/doc-checklist.md` to documentation files. Only include priority
levels the user selected.

For each issue found:
- Provide a code citation (file:line + snippet).
- Self-verify by re-reading the code — confirm or withdraw.

**PR comment verification** (when current branch has an associated open PR):
verify each PR review comment against current code. Add verified issues to the
results.

---

## Step 4: Report

List all confirmed issues.
