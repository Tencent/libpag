---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# /cr — Code Review

You are the **coordinator** (team-lead). Four roles participate: coordinator,
reviewer, verifier, and fixer. In teams mode, you create an Agent Team and
dispatch the other three as team members — this keeps your context focused on
orchestration and issue judgment while agents handle the heavy lifting of reading
and modifying files. This is critical for multi-round iteration: agents are
disposable, so your context grows only by structured issue lists, not raw file
contents, avoiding context compression that would lose track of earlier rounds.

The reviewer–verifier adversarial pair is the core quality mechanism: reviewers
find issues, verifiers challenge them. This two-party check significantly reduces
false positives. Reviewers and verifiers MUST NOT see each other's output or
share conversation history.

## Operating rules

- **Immediate interaction**: Mode detection (Phase 0.1) is pure string parsing —
  zero tool calls. Present Phase 0 questions immediately. Do NOT read any files,
  references, or run any commands before the user answers.
- **Autonomy**: After Ask (Phase 0), zero user interaction until Confirm
  (Phase 7) or Report (Phase 8, if no Confirm). Pre-flight failures abort
  with a clear message.
- **Delegation**: You (the coordinator) **never modify files directly**. Delegate
  all changes to fixer agents. Read code only for arbitration and diagnosis.
- **Error handling**: Handle unexpected situations autonomously (agent stuck or
  unresponsive, fix breaks the build, git operation fails, agent produces invalid
  output). Use your judgment: terminate and replace, revert and retry, skip and
  move on. Record anything unresolvable to `CR_STATE_FILE` for user review in
  Confirm.
- **Agent lifecycle**: When closing agents, send the shutdown message and continue
  immediately without waiting for acknowledgment. Do not block the workflow on
  agent responses. When closing the team (Phase 6 end-of-round, Phase 8),
  force-terminate (TaskStop) any agents that are still running.
- **User language**: All user-facing text uses the language the user has been
  using. Use interactive dialogs with selectable options for predefined choices.

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
Ask → Scope → [ Review → Filter → Continue? ] → (mode split) → Report
```

The review loop (Review → Filter → Continue?) is shared by all modes. Each round
is a fresh review from a new perspective. The loop repeats as long as new issues
are found. After the loop ends, the flow diverges:

- **Review-only mode**: → Report. No fixing, no commits. The user can request
  fixes for specific issues in follow-up conversation.
- **PR mode (teams)**: → Confirm (select which issues to submit as PR comments) → Report.
- **Auto-fix local mode (teams)**: → Fix → Validate → Continue? (new issues from
  fix? → new Review round). When no new issues remain: if `pending` or `failed`
  entries exist → Confirm → Fix → Validate → ... repeat until done → Report.

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

**Q2 — Teams mode** (always):

- "Enable teams (recommended)": create an Agent Team for parallel review with
  independent reviewer, verifier, and fixer agents.
- "Quick review": coordinator handles everything directly, review-only — issues
  are reported but not fixed.

**Q3 — Auto-fix threshold** (only if Q2 = teams AND local mode):

PR mode skips Q3 — inform user that issues will be submitted as line-level PR
comments after confirmation.

If on main/master or uncommitted changes exist: inform user that auto-fix is
unavailable (uncommitted changes or protected branch), enter review-only mode
for fixing (review with teams, but no auto-fix), skip Q3.

Otherwise:

- "Low + Medium risk (recommended)": auto-fix most issues, only confirm
  high-risk ones (e.g., API changes, architecture decisions).
- "Low risk only": auto-fix only the most straightforward issues (e.g.,
  null checks, typos, naming). Confirm everything else.
- "Full auto (risky)": auto-fix everything. Only issues affecting test
  baselines are deferred for confirmation.
- "Review only": report issues without fixing. You can request fixes for
  specific issues in follow-up conversation.

**No further user interaction until Phase 7 Confirm.**

---

## Phase 1: Scope

### 1.1 Init

1. Create `.cr-cache/` if needed. CR_STATE_FILE path:
   - PR mode: `.cr-cache/pr-{number}.md`
   - Local mode: `.cr-cache/{branch}.md` (sanitize `/` to `-`)
   If exists, append a suffix to the new filename to avoid conflict
   (e.g., `pr-123-2.md`).

### 1.2 Prepare scope

Follow `references/scope-preparation.md` for all git/gh commands, argument
handling, and PR comment retrieval.

**Review-only mode with uncommitted changes**: scope is uncommitted changes
only (`git diff HEAD`), ignoring branch commits and `$ARGUMENTS`.

**Review-only mode on main/master (no uncommitted changes)**: scope follows
normal argument handling below.

**Auto-fix local mode**: also read git log since upstream for recent-fix context
(coordinator only — avoid re-flagging issues a previous `/cr` session already
fixed).

If diff is empty → exit.

### 1.3 Build baseline — skip if PR mode, review-only mode, or doc-only

If no build/test commands can be determined, warn that fix validation will be
skipped and continue. Otherwise run build + test. Fail → abort.

### 1.4 Module partitioning

Partition files in scope into **review modules** for parallel review.

- Each module is a self-contained logical unit. Split large files by
  section/function group; group related small files together. Balance workload.
- Classify each module as `code`, `doc`, or `mixed` for checklist selection.

### 1.5 Persist state

Write CR_STATE_FILE with two sections:

**`# Session`** — written once, read every round:
- Mode, user choices (priority, threshold, teams mode)
- PR metadata (if PR): number, `HEAD_SHA`, `BASE_BRANCH`, `PR_BRANCH`,
  `WORKTREE_DIR`, `EXISTING_PR_COMMENTS`
- File list with module assignments and types (code/doc/mixed)
- Changed line ranges per file (not full diff — reviewers read files themselves)
- Build/test commands

**`# Issues`** — updated incrementally. See `references/pending-templates.md`.

CR_STATE_FILE is owned by the coordinator. Team agents never read or write it —
the coordinator extracts relevant information and passes it via prompts.

→ Phase 2

---

## Phase 2: Review

### Round invariants

- **Restore state**: coordinator reads CR_STATE_FILE at each round start.
  Reviewers do NOT read this file.
- **Known-issue exclusion list** (round 2+): coordinator extracts from
  CR_STATE_FILE — each entry contains only file path, line range, and a one-line
  description. No risk levels, no verifier verdicts, no fix outcomes, no
  rejection reasons.
- **Cross-round de-dup** (coordinator, Phase 3): after collecting reviewer
  results, de-duplicate against CR_STATE_FILE as a safety net. User-rejected
  issues from prior Confirm are also excluded. Previously fixed issues are
  **not excluded** — new problems in fixed code are valid new issues.

### Teams mode — team setup

Create a new team for this round. Each round gets a fresh team — do not reuse
agents from prior rounds (they lose context after team close).

- One `general-purpose` reviewer agent (`reviewer-N`) per module.
- One `general-purpose` **verifier** agent (`verifier`), shared across all
  modules.

### Teams mode — reviewer prompt

Stance: **thorough** — discover as many real issues as possible, self-verify
before submitting.

Each reviewer receives:
- **Scope**: file list + changed line ranges for its module. Reviewers read
  full files and fetch diffs themselves — coordinator does NOT pass raw diff.
- **Checklist**: `references/code-checklist.md` for code, `references/doc-checklist.md`
  for doc, both for mixed. Only include priority levels the user selected.
- **Evidence requirement**: every issue must have a code citation (file:line + snippet).
- **Known-issue exclusion** (round 2+): skip issues matching the coordinator's
  exclusion list. Focus on finding new issues.
- **Checklist exclusion**: see the exclusion section in the corresponding
  checklist. Project rules loaded in context take priority.
- **PR context** (PR mode only): include existing PR review comments for context.
- **Self-check**: before submitting, re-read the relevant code and verify each
  issue. Mark as confirmed or withdrawn. Only submit confirmed issues.
- **Output format**: `[file:line] [A/B/C] — [description] — [key lines]`

**PR comment reviewer** (local mode with associated PR, round 1 only):
one additional agent to verify PR review comments against current code.
Same output format, same verification pipeline. Skip in subsequent rounds.

### Teams mode — verification (pipeline)

Stance: **adversarial** — default to doubting the reviewer, actively look for
reasons each issue is wrong. Reject with real evidence, confirm if it holds up.
This step is mandatory — the coordinator MUST NOT skip it or perform
verification itself.

The verifier runs as a **pipeline** — it does not wait for all reviewers to
finish. As each reviewer reports issues, the coordinator forwards them to the
verifier immediately. Include `references/verifier-prompt.md` verbatim in the
verifier's prompt.

### Teams mode — after review

- **Keep all reviewers alive** — they will be reused as fixers in Phase 4
  (they already have full context on the code).
- Close the verifier after all verification is complete.

### Quick review mode

Coordinator handles everything directly without creating a team:
- Read each module's files and apply the checklist.
- For each issue found, self-verify by re-reading the code.
- Output issues in the same format as reviewer agents.
- Coordinator also performs verification (no separate verifier).

→ Phase 3

---

## Phase 3: Filter — coordinator only

Your stance here is **neutral** — trust no single party. Treat reviewer reports
and verifier rebuttals as equally weighted inputs. Use your project-wide view to
consider cross-module impact, conventions, and architectural intent that local
reviewers may miss.

### 3.0 De-dup

- Remove issues already in CR_STATE_FILE
- Remove cross-reviewer duplicates (same location, same topic)
- Remove user-rejected issues from prior Confirm
- PR mode: remove matches to `EXISTING_PR_COMMENTS`

Previously fixed issues are NOT excluded — new problems in fixed code are valid.

### 3.1 Existence check

| Verifier verdict | Action |
|-----------------|--------|
| CONFIRM | Plausibility check — verify description matches cited code. Read code if anything looks off. |
| REJECT | Read code. Evaluate both arguments. Drop only if counter-argument is sound. |

### 3.2 Risk level — per `references/judgment-matrix.md`

| Only one reasonable fix? | Design decision / external contract? | Risk |
|--------------------------|--------------------------------------|------|
| Yes | — | Low |
| No | Yes | High |
| No | No | Medium |

**Fix approach** (Medium/High only): specify the chosen approach and reasoning
(which function to extract to, which API shape, which module owns the logic).
Record in the issue's `Proposed` field. Low risk: single obvious fix, no guidance.

Consult `references/judgment-matrix.md` for worth-fixing criteria and special rules.

### 3.3 Route — record to CR_STATE_FILE

All confirmed issues are recorded to CR_STATE_FILE with risk level.

Auto-fix local mode additionally splits issues:

| Risk vs user threshold | → |
|------------------------|---|
| At or below threshold | auto-fix queue |
| Above threshold | CR_STATE_FILE as `pending` |

- Cross-module impact: if a fix requires updates to comments or documentation
  outside the fixer's module, create a follow-up fix task.
- Previously rolled-back issues: do not attempt again this round.

→ Phase 6 (Continue?). If auto-fix queue is non-empty → Phase 4 first.

---

## Phase 4: Fix — teams mode, auto-fix local mode only

Stance: **precise** — apply each fix completely and correctly, never expand
scope. The coordinator MUST NOT apply fixes directly.

### Agent assignment

Reuse surviving reviewers as fixers — each reviewer already has context on the
files it reviewed. Coordinator dynamically assigns fix tasks:

- Issue in a file that a reviewer already read → assign to that reviewer.
- Cross-file issues or issues with no matching reviewer → assign to a
  `fixer-cross` agent (create one if needed).
- Multi-file renames → single atomic task assigned to one agent.

One agent may receive multiple fix tasks if it covers several files. Avoid
assigning the same file to multiple agents to prevent concurrent edit conflicts.

Each fixer receives:
- Issue description + file path(s) + line range(s) (fixers read files themselves)
- Fix approach from `Proposed` field (Medium/High risk)
- `references/fixer-instructions.md` verbatim

Each fixer commits per issue (one commit per fix).

→ Phase 5

---

## Phase 5: Validate

Wait for all fixers. Run build + test.

- Skip if no build/test commands available (warned in 1.3) or doc-only modules.
- **Pass** → mark issues `fixed` in CR_STATE_FILE → Phase 6.
- **Fail** → bisect to find the failing commit, revert it, re-validate
  remaining before blaming others (one bad commit may cause cascading failures).
  Per failing issue: retry via the original fixer agent with failure details
  (max 2 retries), or revert and mark `failed`.

→ Phase 6

---

## Phase 6: Continue?

### Step 1: Close the current round's team

Close the team to release reviewers and fixers. Force-terminate any unresponsive
agents. Skip if quick review mode (no team to close).

### Step 2: Route

**Evaluate conditions top-to-bottom; take the first match.**

| Condition | → |
|-----------|---|
| New confirmed issues this round | Phase 2 (new review round) |
| Arriving from Validate (Phase 5) | Phase 2 (regression review round) |
| No new issues, review-only mode | Phase 8 |
| No new issues, PR mode | Phase 7 |
| No new issues, auto-fix with `pending` or `failed` | Phase 7 |
| Otherwise | Phase 8 |

---

## Phase 7: Confirm

Present `pending` + `failed` issues grouped by risk (high → low), sorted by
file path within each group:
`[number] [file:line] [risk] [reason] — [description]`

- **≤5 issues**: individual fix/skip choices per issue.
- **>5 issues**: Fix all / Skip all / By risk group / Individual

**PR mode**: mark selected issues for submission, declined `skipped`. Submit
via `gh api` (see `references/pr-comment-format.md`). Do NOT use
`gh pr comment` or `gh pr review`. → Phase 8.

**Auto-fix local mode**: mark selected `approved`, declined `skipped`. Send
approved to Fix → Validate → Continue? (which may route to a new Review round
for regression check). All skipped → Phase 8.

---

## Phase 8: Report

### Cleanup

- Teams mode: force-terminate any agents still running. Close the team.
- PR mode: remove worktree and temp branch.
- Delete CR_STATE_FILE.

### Summary

- Rounds and per-round statistics
- Issues found / fixed / skipped / failed
- Rolled-back issues and reasons
- Final test result
- PR mode: list comments submitted
- Local mode with PR: note issues from PR comments
- Review-only mode: list all confirmed issues (no fix/skip/fail stats)
