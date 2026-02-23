---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# /cr — Code Review

You are the **coordinator** (team-lead). You create an Agent Team and dispatch
reviewer, verifier, and fixer agents as team members — this keeps your context
focused on orchestration and issue judgment while agents handle the heavy lifting
of reading and modifying files. This is critical for multi-round iteration:
agents are disposable, so your context grows only by structured issue lists, not
raw file contents, avoiding context compression that would lose track of earlier
rounds.

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
| `references/pr-comment-format.md` | Coordinator (PR comment format) |

---

## Flow

```
Ask → Scope → [ Review → Filter → Continue? ] → Confirm → Report
                  ↑          Fix ← Validate ←┘
```

Phase 0 determines the mode. PR mode and review-only mode follow a simplified
flow described in the appendix. The main flow below covers **auto-fix local
mode** (teams + automatic fixing).

---

## Phase 0: Ask

### 0.1 Mode detection — string parsing only, zero tool calls

| `$ARGUMENTS` pattern | Mode |
|----------------------|------|
| Purely numeric (`123`) | PR → see Appendix A |
| URL containing `/pull/` | PR → see Appendix A |
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

**Q2 — Auto-fix threshold**:

If on main/master or uncommitted changes exist: inform user that auto-fix is
unavailable (uncommitted changes or protected branch), enter review-only mode
automatically (see Appendix B), skip Q2.

Otherwise:

- "Low + Medium risk (recommended)": auto-fix most issues, only confirm
  high-risk ones (e.g., API changes, architecture decisions).
- "Low risk only": auto-fix only the most straightforward issues (e.g.,
  null checks, typos, naming). Confirm everything else.
- "Full auto (risky)": auto-fix everything. Only issues affecting test
  baselines are deferred for confirmation.
- "Review only": report issues without fixing. See Appendix B.

**No further user interaction until Phase 7 Confirm.**

---

## Phase 1: Scope

### 1.1 Init

1. Create `.cr-cache/` if needed. CR_STATE_FILE path:
   `.cr-cache/{branch}.md` (sanitize `/` to `-`).
   If exists, append a suffix to avoid conflict (e.g., `feature-foo-2.md`).

### 1.2 Prepare scope

Follow `references/scope-preparation.md` for all git/gh commands and argument
handling.

Also read git log since upstream for recent-fix context (coordinator only —
avoid re-flagging issues a previous `/cr` session already fixed).

If diff is empty → exit.

### 1.3 Build baseline — skip if doc-only

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
- Mode, user choices (priority, threshold)
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

### Team setup

Create a new team for this round. Each round gets a fresh team — do not reuse
agents from prior rounds (they lose context after team close).

- One `general-purpose` reviewer agent (`reviewer-N`) per module.
- One `general-purpose` **verifier** agent (`verifier`), shared across all
  modules.

### Reviewer prompt

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
- **Self-check**: before submitting, re-read the relevant code and verify each
  issue. Mark as confirmed or withdrawn. Only submit confirmed issues.
- **Output format**: `[file:line] [A/B/C] — [description] — [key lines]`

**PR comment reviewer** (local mode with associated PR, round 1 only):
one additional agent to verify PR review comments against current code.
Same output format, same verification pipeline. Skip in subsequent rounds.

### Verification (pipeline)

Stance: **adversarial** — default to doubting the reviewer, actively look for
reasons each issue is wrong. Reject with real evidence, confirm if it holds up.
This step is mandatory — the coordinator MUST NOT skip it or perform
verification itself.

The verifier runs as a **pipeline** — it does not wait for all reviewers to
finish. As each reviewer reports issues, the coordinator forwards them to the
verifier immediately. Include `references/verifier-prompt.md` verbatim in the
verifier's prompt.

### After review

- **Keep all reviewers alive** — they will be reused as fixers in Phase 4
  (they already have full context on the code).
- Close the verifier after all verification is complete.

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

| Risk vs user threshold | → |
|------------------------|---|
| At or below threshold | auto-fix queue |
| Above threshold | CR_STATE_FILE as `pending` |

- Cross-module impact: if a fix requires updates to comments or documentation
  outside the fixer's module, create a follow-up fix task.
- Previously rolled-back issues: do not attempt again this round.

→ Phase 4 if auto-fix queue is non-empty; otherwise → Phase 6.

---

## Phase 4: Fix

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
agents.

### Step 2: Route

**Evaluate conditions top-to-bottom; take the first match.**

| Condition | → |
|-----------|---|
| New confirmed issues this round | Phase 2 (new review round) |
| Arriving from Validate (Phase 5) | Phase 2 (regression review round) |
| `pending` or `failed` in CR_STATE_FILE | Phase 7 |
| Otherwise | Phase 8 |

---

## Phase 7: Confirm

Present `pending` + `failed` issues grouped by risk (high → low), sorted by
file path within each group:
`[number] [file:line] [risk] [reason] — [description]`

- **≤5 issues**: individual fix/skip choices per issue.
- **>5 issues**: Fix all / Skip all / By risk group / Individual

Mark selected `approved`, declined `skipped`. Send approved to Phase 4
(Fix → Validate → Continue?, which may route to a new Review round for
regression check). All skipped → Phase 8.

---

## Phase 8: Report

### Cleanup

Force-terminate any agents still running. Close the team. Delete CR_STATE_FILE.

### Summary

- Rounds and per-round statistics
- Issues found / fixed / skipped / failed
- Rolled-back issues and reasons
- Final test result
- Local mode with associated PR: note issues from PR comments

---

## Appendix A: PR Mode

PR mode reviews a pull request and submits issues as line-level PR comments.
The coordinator handles everything directly — no Agent Team, no auto-fix.

### Phase 0

Mode detection identifies PR from numeric or `/pull/` URL argument. Ask Q1
(Review priority) only. Skip Q2 — inform user that issues will be submitted as
line-level PR comments.

### Phase 1: Scope

1. Init CR_STATE_FILE at `.cr-cache/pr-{number}.md`.
2. Follow `references/scope-preparation.md` for PR-specific setup: fetch PR
   ref into a worktree, determine `BASE_BRANCH`, fetch diff, fetch existing
   PR review comments for de-duplication. Record `WORKTREE_DIR` for cleanup.
3. Module partitioning (same as main flow).
4. Persist state (include PR metadata: number, `HEAD_SHA`, `BASE_BRANCH`,
   `PR_BRANCH`, `WORKTREE_DIR`, `EXISTING_PR_COMMENTS`).

### Phase 2–3: Review & Filter

Coordinator reviews and filters directly (same as Appendix B review-only).
Additional de-dup: remove matches to `EXISTING_PR_COMMENTS`.

### Phase 7: Confirm

Present confirmed issues to user. User selects which to submit as PR comments,
declines are marked `skipped`. Submit via `gh api` using
`references/pr-comment-format.md`. Do NOT use `gh pr comment` or `gh pr review`.

### Phase 8: Report

Summary of issues found / submitted / skipped. Remove worktree and temp branch.
Delete CR_STATE_FILE.

---

## Appendix B: Review-Only Mode

Review-only mode reports issues without fixing. The coordinator handles
everything directly — no Agent Team, no auto-fix, no commits.

Entered when:
- User selects "Review only" in Q2.
- On main/master or uncommitted changes exist (automatic, Q2 skipped).

### Phase 1: Scope

Same as main flow Phase 1, with differences:
- Skip build baseline.
- **Uncommitted changes**: scope is `git diff HEAD` only, ignoring branch
  commits and `$ARGUMENTS`.
- **Main/master (no uncommitted changes)**: scope follows normal argument
  handling.

### Phase 2–3: Review & Filter

Coordinator reviews and filters directly without creating a team:
- Read each module's files and apply the checklist.
- For each issue found, self-verify by re-reading the code.
- Output issues in the same format as reviewer agents.
- Apply the same Filter logic (risk level, fix approach, de-dup). Record all
  confirmed issues to CR_STATE_FILE.

### Phase 8: Report

List all confirmed issues with risk levels and fix approaches. No fix/skip/fail
stats. Delete CR_STATE_FILE.
