# Auto-Fix Mode (Teams)

When the user selects an auto-fix threshold in Q2, the flow extends with Agent
Teams, multi-round review, and automatic fixing. This replaces Phase 2–4 of
the main flow in SKILL.md.

## Additional operating rules

- **Delegation**: You (the coordinator) **never modify files directly**. Delegate
  all changes to fixer agents. Read code only for arbitration and diagnosis.
- **Agent lifecycle**: When closing agents, send the shutdown message and continue
  immediately without waiting for acknowledgment. Do not block the workflow on
  agent responses. When closing the team, force-terminate (TaskStop) any agents
  that are still running.
- **Autonomy**: zero user interaction until Confirm (Phase 7) or Report
  (Phase 8). Record anything unresolvable to `CR_STATE_FILE` for user review.

## Additional references

| File | Used by |
|------|---------|
| `references/verifier-prompt.md` | Verifier — include verbatim |
| `references/fixer-instructions.md` | Fixer — include verbatim |

## Flow

```
Scope → [ Review → Filter → Continue? ] → Confirm → Report
            ↑          Fix ← Validate ←┘
```

The review loop repeats as long as new issues are found. Each round is a fresh
review. After the loop: if `pending` or `failed` entries exist → Confirm → Fix
→ Validate → ... repeat until done → Report.

## Phase 1: Scope (additions)

Same as main flow, plus:
- Build baseline (skip if doc-only). If no build/test commands, warn that fix
  validation will be skipped. Fail → abort.
- Read git log since upstream for recent-fix context (avoid re-flagging issues
  a previous `/cr` session already fixed).
- Persist additional state: threshold, build/test commands.

## Phase 2: Review (teams)

### Round invariants

- **Restore state**: coordinator reads CR_STATE_FILE at each round start.
  Reviewers do NOT read this file.
- **Known-issue exclusion list** (round 2+): coordinator extracts from
  CR_STATE_FILE — each entry contains only file path, line range, and a one-line
  description. No risk levels, no verifier verdicts, no fix outcomes.
- **Cross-round de-dup** (coordinator, Phase 3): de-duplicate against
  CR_STATE_FILE as a safety net. User-rejected issues from prior Confirm are
  also excluded. Previously fixed issues are **not excluded** — new problems in
  fixed code are valid new issues.

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

## Phase 3: Filter — coordinator only

Your stance here is **neutral** — trust no single party. Treat reviewer reports
and verifier rebuttals as equally weighted inputs.

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

Same as main flow Phase 3.1.

### 3.3 Route — record to CR_STATE_FILE

All confirmed issues are recorded with risk level.

| Risk vs user threshold | → |
|------------------------|---|
| At or below threshold | auto-fix queue |
| Above threshold | CR_STATE_FILE as `pending` |

- Cross-module impact: if a fix requires updates outside the fixer's module,
  create a follow-up fix task.
- Previously rolled-back issues: do not attempt again this round.

→ Phase 4 if auto-fix queue is non-empty; otherwise → Phase 6.

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

## Phase 5: Validate

Wait for all fixers. Run build + test.

- Skip if no build/test commands available or doc-only modules.
- **Pass** → mark issues `fixed` in CR_STATE_FILE → Phase 6.
- **Fail** → bisect to find the failing commit, revert it, re-validate
  remaining. Per failing issue: retry via the original fixer agent with failure
  details (max 2 retries), or revert and mark `failed`.

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

## Phase 7: Confirm

Present `pending` + `failed` issues grouped by risk (high → low), sorted by
file path within each group:
`[number] [file:line] [risk] [reason] — [description]`

- **≤5 issues**: individual fix/skip choices per issue.
- **>5 issues**: Fix all / Skip all / By risk group / Individual

Mark selected `approved`, declined `skipped`. Send approved to Phase 4
(Fix → Validate → Continue?, which may route to a new Review round for
regression check). All skipped → Phase 8.

## Phase 8: Report

Force-terminate any agents still running. Close the team. Delete CR_STATE_FILE.

Summary:
- Rounds and per-round statistics
- Issues found / fixed / skipped / failed
- Rolled-back issues and reasons
- Final test result
- Local mode with associated PR: note issues from PR comments
