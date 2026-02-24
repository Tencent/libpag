# Teams Review

You are the **coordinator**. Delegate all review and fix work to team agents.
Never modify files directly. Read code only for arbitration and diagnosis.

The reviewer–verifier adversarial pair is the core quality mechanism: reviewers
find issues, verifiers challenge them. This two-party check significantly reduces
false positives. Reviewers and verifiers MUST NOT see each other's output or
share conversation history.

## Input from SKILL.md

- `REVIEW_PRIORITY`: A | A+B | A+B+C
- `FIX_MODE`: none | low | low_medium | full

## References

| File | Purpose |
|------|---------|
| `code-checklist.md` | Code review checklist |
| `doc-checklist.md` | Document review checklist |
| `judgment-matrix.md` | Risk levels, worth-fixing criteria, special rules |
| `scope-detection.md` | Shared scope detection logic |

## Additional operating rules

- **Agent lifecycle**: When closing agents, send the shutdown message and continue
  immediately without waiting for acknowledgment. Do not block the workflow on
  agent responses. When closing the team, force-terminate (TaskStop) any agents
  that are still running.
- **Autonomy**: zero user interaction until Confirm (Phase 7) or Report
  (Phase 8). Record anything unresolvable to `CR_STATE_FILE` for user review.
- **Error handling**: Handle unexpected situations autonomously (agent stuck or
  unresponsive, fix breaks the build, git operation fails, agent produces invalid
  output). Use your judgment: terminate and replace, revert and retry, skip and
  move on. Record anything unresolvable to `CR_STATE_FILE` for user review in
  Confirm.

## Flow

```
FIX_MODE=none:   Scope → Review → Report
FIX_MODE≠none:   Scope → [ Review → Filter → Continue? ] → Confirm → Report
                            ↑          Fix ← Validate ←┘
```

The review loop repeats as long as new issues are found. Each round is a fresh
review. After the loop: if `pending` or `failed` entries exist → Confirm → Fix
→ Validate → ... repeat until done → Report.

---

## Phase 1: Scope

Follow `scope-detection.md` to determine the review scope and fetch the diff.

### Build baseline

Skip if doc-only. If no build/test commands can be determined, warn that fix
validation will be skipped. Otherwise run build + test. Fail → abort.

Read git log since upstream for recent-fix context (avoid re-flagging issues
a previous `/cr` session already fixed).

### Module partition

Partition files in scope into **review modules** for parallel review. Each
module is a self-contained logical unit. Split large files by section/function
group; group related small files together. Classify each module as `code`,
`doc`, or `mixed`.

### Persist state

Write CR_STATE_FILE (see CR_STATE_FILE format appendix) with session info
(mode, priority, threshold, file list, module assignments, changed line ranges,
build/test commands) and an issues section updated incrementally. CR_STATE_FILE
is owned by the coordinator — team agents never read or write it.

---

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
- **Checklist**: `code-checklist.md` for code, `doc-checklist.md` for doc, both
  for mixed. Only include priority levels the user selected.
- **Evidence requirement**: every issue must have a code citation (file:line + snippet).
- **Known-issue exclusion** (round 2+): skip issues matching the coordinator's
  exclusion list. Focus on finding new issues.
- **Checklist exclusion**: see the exclusion section in the corresponding
  checklist. Project rules loaded in context take priority.
- **Self-check**: before submitting, re-read the relevant code and verify each
  issue. Mark as confirmed or withdrawn. Only submit confirmed issues.
- **Output format**: `[file:line] [A/B/C] — [description] — [key lines]`

**PR comment reviewer** (when `PR_COMMENTS` exist, round 1 only): one additional
agent to verify PR review comments against current code. Same output format,
same verification pipeline. Skip in subsequent rounds.

### Verification (pipeline)

Stance: **adversarial** — default to doubting the reviewer, actively look for
reasons each issue is wrong. Reject with real evidence, confirm if it holds up.
This step is mandatory — the coordinator MUST NOT skip it or perform
verification itself.

The verifier runs as a **pipeline** — it does not wait for all reviewers to
finish. As each reviewer reports issues, the coordinator forwards them to the
verifier immediately. Include the following verbatim in every verifier's prompt:

```
You are a code review verifier. Your stance is adversarial — default to doubting the
reviewer's conclusion and actively look for reasons why the issue might be wrong. Your
job is to stress-test each issue so that only real problems survive.

For each issue you receive:

1. Read the cited code (file:line) and sufficient surrounding context.
2. Actively try to disprove the issue: Is the reviewer's reasoning flawed? Is there
   context that makes this a non-issue (e.g., invariants guaranteed by callers, platform
   constraints, intentional design)? Does the code actually behave as the reviewer
   claims? Look for the strongest counter-argument you can find.
3. Output for each issue:
   - Verdict: REJECT or CONFIRM
   - Reasoning: for REJECT, state the concrete counter-argument. For CONFIRM, briefly
     note what you checked and why no valid counter-argument exists.

Important constraints:
- Your counter-arguments must be grounded in real evidence from the code. Do not
  fabricate hypothetical defenses or invent caller guarantees that are not visible in
  the codebase.
- A CONFIRM verdict is not a failure — it means the reviewer found a real issue and
  your challenge validated it.
```

### After review

- `FIX_MODE` = none → close all agents, close the team → Phase 8 (Report).
- `FIX_MODE` ≠ none → **keep all reviewers alive** (reused as fixers in Phase 4),
  close the verifier → Phase 3.

---

## Phase 3: Filter — coordinator only

*Skipped when `FIX_MODE` = none.*

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

### 3.2 Risk level

Consult `judgment-matrix.md` for risk level assessment, worth-fixing criteria,
handling by risk level, and special rules.

**Fix approach** (Medium/High only): specify the chosen approach and reasoning.
Record in the issue's `Proposed` field. Low risk: single obvious fix, no guidance.

### 3.3 Route — record to CR_STATE_FILE

All confirmed issues are recorded with risk level.

| Risk vs `FIX_MODE` | → |
|---------------------|---|
| At or below threshold | auto-fix queue |
| Above threshold | CR_STATE_FILE as `pending` |

- Cross-module impact: if a fix requires updates outside the fixer's module,
  create a follow-up fix task.
- Previously rolled-back issues: do not attempt again this round.

→ Phase 4 if auto-fix queue is non-empty; otherwise → Phase 6.

---

## Phase 4: Fix

*Skipped when `FIX_MODE` = none.*

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
- Fixer rules (include verbatim in every fixer prompt):

```
Fix rules:
1. After fixing each issue, immediately: git commit --only <files> -m "message"
2. Only commit files in your own module. Never use git add .
3. Commit message: English, under 120 characters, ending with a period.
4. When in doubt, skip the fix rather than risk a wrong change.
5. Do not run build or tests.
6. Do not modify public API function signatures or class definitions (comments are OK),
   unless the coordinator's issue description explicitly requires an API signature fix.
7. After each fix, check whether the change affects related comments or documentation
   within your assigned files (function/class doc-comments, inline comments describing
   the changed logic). If so, update them in the same commit as the fix.
   Cross-module documentation updates (README, spec files, other modules) are handled
   separately by the coordinator.
8. When done, report the commit hash for each fix and list any skipped issues with
   the reason for skipping.
```

Each fixer commits per issue (one commit per fix).

---

## Phase 5: Validate

*Skipped when `FIX_MODE` = none.*

Wait for all fixers. Run build + test.

- Skip if no build/test commands available or doc-only modules.
- **Pass** → mark issues `fixed` in CR_STATE_FILE → Phase 6.
- **Fail** → bisect to find the failing commit, revert it, re-validate
  remaining. Per failing issue: retry via the original fixer agent with failure
  details (max 2 retries), or revert and mark `failed`.

---

## Phase 6: Continue?

*Skipped when `FIX_MODE` = none.*

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

*When `FIX_MODE` = none, this phase is skipped — go directly to Phase 8.*

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

Force-terminate any agents still running. Close the team. Delete CR_STATE_FILE.

Summary:
- Rounds and per-round statistics
- Issues found / fixed / skipped / failed
- Rolled-back issues and reasons
- Final test result
- Issues from PR comments (when `PR_COMMENTS` existed)

---

## Appendix: CR_STATE_FILE Format

Use this format to record issues in the `# Issues` section of `CR_STATE_FILE`.

**Status values**:
- `pending` — recorded, awaiting user decision in Phase 7
- `approved` — user approved fix in Phase 7, sent to Phase 4
- `fixed` — fix applied and passed validation
- `failed` — fix attempted but failed validation after retries
- `skipped` — user declined or issue rejected (do not re-report)

```markdown
# Issues

## 1. [brief description]
- **Status**: pending | approved | fixed | failed | skipped
- **Reason**: [why not auto-fixed, e.g., "above auto-fix threshold (high risk)",
  "fix failed: build error after 2 retries", "rolled back: introduced regression"]
- **Risk**: low | medium | high
- **File**: [file path:line number]
- **Current**: [what the code does now]
- **Proposed**: [what the fix would change — for medium/high risk, include the
  specific approach chosen by the coordinator and the reasoning]
- **Impact**: [what else would be affected]
```
