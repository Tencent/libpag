---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# /cr — Code Review

You are the **coordinator**. You orchestrate the flow below and make filtering
judgments. You MUST launch separate **sub-agents** for reviewer, verifier, and
fixer roles — each in its own isolated context. You never perform review,
verification, or file modification work directly.

## References

| File | Used by |
|------|---------|
| `references/code-checklist.md` | Reviewer (code modules) |
| `references/doc-checklist.md` | Reviewer (doc modules) |
| `references/verifier-prompt.md` | Verifier — include verbatim |
| `references/fixer-instructions.md` | Fixer — include verbatim |
| `references/judgment-matrix.md` | Coordinator (risk & worth-fixing) |
| `references/pending-templates.md` | Coordinator (CR_STATE_FILE format) |
| `references/scope-preparation.md` | Coordinator (git/gh commands) |
| `references/pr-comment-format.md` | Coordinator (gh api format) |

---

## Flow

```
Ask → Scope → [ Review → Filter → Fix → Validate → Continue? ⇄ Confirm ] → Report
```

---

## Phase 0: Ask

### 0.1 Mode detection — string parsing only, zero tool calls

| `$ARGUMENTS` pattern | Mode |
|----------------------|------|
| Purely numeric (`123`) | PR |
| URL containing `/pull/` | PR |
| Everything else | Local |

### 0.2 Pre-check — local mode only, before questions

1. `git branch --show-current` → if main/master, abort.
2. `git status --porcelain` → record whether uncommitted changes exist.

### 0.3 Present questions — single interactive prompt

Mode detection and pre-checks require zero file reads. Present questions
**immediately** — do not read any files, references, or run git diff first.

**Q1 — Review priority** (always):
- Full review (A + B + C)
- Correctness + optimization (A + B)
- Correctness only (A)

**Q2 — Auto-fix threshold** (skip if PR mode; add note: "PR mode — issues
submitted as line-level PR comments after confirmation"):
- Low + Medium risk ← pre-select as default
- Low risk only
- All confirm
- Full auto

**Q3 — Uncommitted changes** (local mode, only if 0.2 found changes):
- Commit and continue
- Abort

If Q3 = Abort → stop.
**No further user interaction until Phase 7 Confirm.**

---

## Phase 1: Scope

### 1.1 Init

1. CR_STATE_FILE = `.cr-cache/pr-{N}.md` (PR) or `.cr-cache/{branch}.md` (local).
   Overwrite if exists.
2. If Q3 = Commit: `git add -A && git commit -m "WIP: save changes before /cr session."`

### 1.2 Prepare scope

Follow `references/scope-preparation.md`. Key outputs:
- **PR mode**: PR metadata, worktree (if needed), diff, existing PR comments
- **Local mode**: diff against upstream, optionally fetch associated PR comments
- **Local mode**: read git log since upstream for recent-fix context (coordinator only)

If diff is empty → exit.

### 1.3 Build baseline — skip if PR mode or doc-only

Run build + test. Fail → abort.

### 1.4 Partition & persist

Split files into modules (code / doc / mixed). Write CR_STATE_FILE:
- **`# Session`**: mode, user choices, PR metadata (if PR), file list with module
  assignments, changed line ranges per file, build/test commands
- **`# Issues`**: (empty)

→ Phase 2

---

## Phase 2: Review

### 2.1 Reviewers — sub-agents, parallel

Launch one reviewer sub-agent per module. Each receives:
- File list + changed line ranges (reviewer reads files itself)
- Checklist: `references/code-checklist.md` or `references/doc-checklist.md`
  (both for mixed), filtered to user's priority level
- Known-issue exclusion list (round 2+): file path + line + one-line summary only
- Output format: `[file:line] [A/B/C] — [description] — [key lines]`

**PR comment reviewer** (local mode with associated PR, round 1 only):
one additional sub-agent to verify PR comments against current code, same format.

### 2.2 Verifiers — sub-agents, pipeline

As each reviewer returns, immediately launch one verifier sub-agent for that
batch. Do not wait for all reviewers. Include `references/verifier-prompt.md`
verbatim.

Collect all verifier outputs → Phase 3.

---

## Phase 3: Filter — coordinator only

### 3.0 De-dup

- Remove issues already in CR_STATE_FILE
- Remove cross-reviewer duplicates (same location, same topic)
- Remove user-rejected issues from prior Confirm
- PR mode: remove matches to existing PR comments

Previously fixed issues are NOT excluded — new problems in fixed code are valid.

### 3.1 Existence check

| Verifier verdict | Action |
|-----------------|--------|
| CONFIRM | Accept if plausible. Read code only if something looks off. |
| REJECT | Read code. Drop only if counter-argument is sound. |

### 3.2 Risk level — per `references/judgment-matrix.md`

| Only one fix approach? | Design decision / external contract? | Risk |
|------------------------|--------------------------------------|------|
| Yes | — | Low |
| No | Yes | High |
| No | No | Medium |

Medium/High: write chosen fix approach + reasoning to the issue's `Proposed`
field. Low: single obvious fix, no guidance needed.

### 3.3 Route

| Risk vs user threshold | → |
|------------------------|---|
| At or below threshold | auto-fix queue |
| Above threshold | CR_STATE_FILE as `pending` |

- PR mode: record all to CR_STATE_FILE, skip to Phase 6.
- Local, fix queue empty → Phase 6.
- Local, fix queue non-empty → Phase 4.

---

## Phase 4: Fix — sub-agents, parallel on different files

Launch one fixer sub-agent per issue or file group. Each receives:
- Issue description + file path + line range
- Fix approach (Medium/High risk, from `Proposed` field)
- `references/fixer-instructions.md` verbatim

Each fixer commits per issue (one commit per fix).

→ Phase 5

---

## Phase 5: Validate

Wait for all fixers. Run build + test.

- **Pass** → mark issues `fixed` in CR_STATE_FILE → Phase 6.
- **Fail** → bisect to find failing commit. Revert it, re-validate remaining
  (a single bad commit may cause cascading failures). Per failing issue:
  retry via new fixer sub-agent (max 2 retries), or mark `failed`.

→ Phase 6

---

## Phase 6: Continue?

| Condition | → |
|-----------|---|
| Arriving from Confirm → Fix → Validate | Phase 2 (new round) |
| New issues found this round | Phase 2 |
| CR_STATE_FILE has `pending` or `failed` entries | Phase 7 |
| Otherwise | Phase 8 |

---

## Phase 7: Confirm

Present `pending` + `failed` issues grouped by risk (high → low).

- ≤5 issues: individual fix/skip choices
- \>5 issues: Fix all / Skip all / By risk group / Individual

**PR mode**: mark selected `fixed`, declined `skipped`. Submit via `gh api`
(see `references/pr-comment-format.md`). → Phase 8.

**Local mode**: mark selected `approved`, declined `skipped`. Send approved to
Phase 4 → 5 → 6 (which routes back to Phase 2 for regression check).
All skipped → Phase 8.

---

## Phase 8: Report

Output summary: rounds, issues found/fixed/skipped/failed, final test result.
Delete CR_STATE_FILE. Clean worktree if PR mode.
