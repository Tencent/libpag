# Teams Review

You are the **coordinator**. Create an Agent Team and dispatch reviewer,
verifier, and fixer agents. This is critical for multi-round iteration: agents
are disposable, so your context grows only by structured issue lists, not raw
file contents — avoiding context compression that would lose track of earlier
rounds. Never modify files directly. Read code only for arbitration and
diagnosis.

The review loop is designed for **uninterrupted multi-round iteration**.
Always process all auto-fixable issues before involving the user. Do NOT pause
to ask the user anything until Confirm (Phase 6) or Report (Phase 7). The only
post-report interaction is the optional checklist evolution prompt.

The reviewer–verifier adversarial pair is the core quality mechanism: reviewers
find issues, verifiers challenge them. This two-party check significantly reduces
false positives. Reviewers and verifiers MUST NOT see each other's output or
share conversation history.

## Input from SKILL.md

- `FIX_MODE`: low | low_medium | full

## References

| File | Purpose |
|------|---------|
| `code-checklist.md` | Code review checklist |
| `doc-checklist.md` | Document review checklist |
| `judgment-matrix.md` | Risk levels, worth-fixing criteria, special rules |
| `checklist-evolution.md` | Checklist update flow and rules |

## Flow

```
Scope
  │
  ↓
┌─────────── Review Loop ───────────┐
│                                   │
│  Review → Filter → Fix/Validate   │
│    ↑                     │        │
│    └── new issues ───────
│                                   │
└───────────────────────────────────┘
  │ no new issues
  ↓
pending/failed? ──no──→ Report + Checklist Evolution
  │ yes
  ↓
Confirm ──all skipped──→ Report + Checklist Evolution
  │ approved
  ↓
Fix/Validate ──→ Review Loop ↑
```

The Review Loop repeats as long as new issues are found. Each round is a fresh
full review — not a targeted re-check of previous fixes.

**After Confirm**: approved fixes go through Fix/Validate, then **always
re-enter the Review Loop** (Phase 2) to catch any issues introduced by the
fixes. The loop only exits to Confirm (if pending/failed remain) or Report
(if nothing remains) when a round produces no new issues.

---

## Phase 1: Scope

Determine the diff to review based on `$ARGUMENTS`:

- **Empty arguments**: find the base branch by checking common base branches
  in order: `main`, `master`. Use the first one that exists. Fetch the branch
  diff:
  ```
  git merge-base origin/{base_branch} HEAD
  git diff <merge-base-sha>
  ```
- **Commit hash** (e.g., `abc123`): validate with `git rev-parse --verify`,
  then `git show`.
- **Commit range** (e.g., `abc123..def456` or `abc123...def456`): validate both
  endpoints. Fetch the diff including both endpoints:
  ```
  git diff A~1..B
  ```
- **File/directory paths**: verify all paths exist on disk, then read file
  contents.

If diff is empty → show usage examples and exit:
`/cr` (uncommitted changes or current branch),
`/cr a1b2c3d`, `/cr a1b2c3d..e4f5g6h`,
`/cr src/foo.cpp`, `/cr 123`, `/cr https://github.com/.../pull/123`.

### Associated PR comments (optional, best-effort)

If `gh` is available, check whether the current branch has an open PR:
```
gh pr view --json number,state --jq 'select(.state == "OPEN") | .number' 2>/dev/null
```
If an open PR exists, fetch its line-level review comments:
```
gh api repos/{owner}/{repo}/pulls/{number}/comments
```
Store as `PR_COMMENTS` for verification in the review step.

### Build baseline

Skip if doc-only. If no build/test commands can be determined, warn that fix
validation will be skipped. Otherwise run build + test. Fail → abort.

Read git log since the base branch for recent-fix context (avoid re-flagging issues
a previous `/cr` session already fixed).

### Module partition

Partition files in scope into **review modules** for parallel review. Each
module is a self-contained logical unit. Split large files by section/function
group; group related small files together. Classify each module as `code`,
`doc`, or `mixed`.

### Persist state

Initialize `CR_STATE_FILE`: create `.cr-cache/` if it does not exist. Derive the
filename from the review scope: `.cr-cache/{branch}.md` (sanitize `/` to `-`,
e.g., `feature/dom_text_box` → `feature-dom_text_box.md`).

Each `/cr` invocation is a **fresh session**. Never read or resume from an
existing file — each session starts from scratch with a new full review.
If the derived filename already exists (leftover from a previous or concurrent
session), find the lowest unused numeric suffix (`-2`, `-3`, …) and use that.
Record the chosen path as `CR_STATE_FILE`.

Write CR_STATE_FILE with session info (mode, threshold, file list, module
assignments, changed line ranges, build/test commands) and an issues section
updated incrementally. CR_STATE_FILE is owned by the coordinator — team agents
never read or write it. Record anything unresolvable to CR_STATE_FILE for user
review.

**Issue format** in CR_STATE_FILE:

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

Status values:
- `pending` — recorded, awaiting user decision in Phase 6
- `approved` — user approved fix in Phase 6, sent to Phase 4
- `fixed` — fix applied and passed validation
- `failed` — fix attempted but failed validation after retries
- `skipped` — user declined or issue rejected (do not re-report)

---

## Phase 2: Review

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

- One reviewer agent (`reviewer-N`) per module.
- One **verifier** agent (`verifier`), shared across all modules.

### Reviewer prompt

Stance: **thorough** — discover as many real issues as possible, self-verify
before submitting.

Each reviewer receives:
- **Scope**: file list + changed line ranges for its module. Reviewers fetch
  diffs and read additional context themselves as needed — coordinator does NOT
  pass raw diff or file contents.
- **Checklist**: `code-checklist.md` for code, `doc-checklist.md` for doc, both
  for mixed.
- **Evidence requirement**: every issue must have a code citation (file:line + snippet) from the current tree.
- **Known-issue exclusion** (round 2+): skip issues matching the coordinator's
  exclusion list. Focus on finding new issues.
- **Checklist exclusion**: see the exclusion section in the corresponding
  checklist. Project rules loaded in context take priority.
- **Self-check**: before submitting, re-read the relevant code and verify each
  issue. Mark as confirmed or withdrawn. Only submit confirmed issues. If a cited
  path/line no longer exists, locate the correct file/path via `git diff --name-only`
  or file search before reporting.
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
finish. As each reviewer sends a report via SendMessage, the coordinator MUST
forward it to the verifier immediately. Forwarding rules:

- **Quote verbatim**: wrap the reviewer's original SendMessage content in a
  quote block and send it as-is.
- **No rewriting**: do not summarize, reorganize, merge multiple reviewer
  reports into one message, or add coordinator commentary.
- **One forward per reviewer**: each reviewer report is a separate message to
  the verifier.

Include the following verbatim in every verifier's prompt:

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

Keep all reviewers alive (reused as fixers in Phase 4), close the verifier
→ Phase 3.

---

## Phase 3: Filter — coordinator only

Before entering this phase, confirm: (1) all reviewers have submitted their
final reports; (2) the verifier has given a CONFIRM/REJECT verdict for every
forwarded finding. Do NOT proceed until both conditions are met.

Your stance here is **neutral** — trust no single party. Treat reviewer reports
and verifier rebuttals as equally weighted inputs. Use your project-wide view to
consider cross-module impact, conventions, and architectural intent that local
reviewers may miss.

### 3.1 De-dup

- Remove issues already in CR_STATE_FILE
- Remove cross-reviewer duplicates (same location, same topic)
- Remove user-rejected issues from prior Confirm

Previously fixed issues are NOT excluded — new problems in fixed code are valid.

### 3.2 Existence check

| Verifier verdict | Action |
|-----------------|--------|
| CONFIRM | Plausibility check — verify description matches cited code. Read code if anything looks off. |
| REJECT | Read code. Evaluate both arguments. Drop only if counter-argument is sound. |

### 3.3 Risk level

Consult `judgment-matrix.md` for risk level assessment, worth-fixing criteria,
handling by risk level, and special rules.

**Fix approach** (Medium/High only): specify the chosen approach and reasoning.
Record in the issue's `Proposed` field. Low risk: single obvious fix, no guidance.

### 3.4 Route — record to CR_STATE_FILE

All confirmed issues are recorded with risk level.

| Risk vs `FIX_MODE` | → |
|---------------------|---|
| At or below threshold | auto-fix queue |
| Above threshold | CR_STATE_FILE as `pending` |

- Cross-module impact: if a fix requires updates outside the fixer's module,
  create a follow-up fix task.
- Previously rolled-back issues: do not attempt again this round.

Phase 4 if auto-fix queue is non-empty; Phase 5 if empty.
Always auto-fix eligible issues first — do NOT present `pending` issues to the
user before all auto-fixable issues have been processed and validated.

---

## Phase 4: Fix/Validate

This phase is an atomic unit reused by the Review Loop and post-Confirm flow.

### Fix

Stance: **precise** — apply each fix completely and correctly, never expand
scope. The coordinator MUST NOT apply fixes directly.

**Agent assignment**: reuse surviving reviewers as fixers when available — each
reviewer already has context on the files it reviewed. When no reviewers survive
(e.g., after Phase 5 closes the team), create new fixer agents. The coordinator
MUST assign by explicit file list and ensure a file is owned by only one fixer
at a time:

- Issue in a file that a surviving reviewer already read → assign to that
  reviewer.
- Otherwise → create a fixer agent (or `fixer-cross` for cross-module issues).
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
2. Only modify files explicitly assigned by the coordinator. Never use git add .
3. If a fix requires changes to unassigned files, stop and report to the coordinator
   for re-assignment.
4. Commit message: English, under 120 characters, ending with a period.
5. When in doubt, skip the fix rather than risk a wrong change.
6. Do not run build or tests.
7. Do not modify public API function signatures or class definitions (comments are OK),
   unless the coordinator's issue description explicitly requires an API signature fix.
8. After each fix, check whether the change affects related comments or documentation
   within your assigned files (function/class doc-comments, inline comments describing
   the changed logic). If so, update them in the same commit as the fix.
   Cross-module documentation updates (README, spec files, other modules) are handled
   separately by the coordinator.
9. When done, report the commit hash for each fix and list any skipped issues with
   the reason for skipping.
```

Each fixer commits per issue (one commit per fix).

### Validate

Wait for all fixers. Run build + test.

- Skip if no build/test commands available or doc-only modules.
- **Pass** → mark issues `fixed` in CR_STATE_FILE → Phase 5.
- **Fail** → bisect to find the failing commit, revert it, re-validate
  remaining before blaming others (one bad commit may cause cascading failures).
  Per failing issue: retry via the original fixer agent with failure
  details (max 2 retries), or revert and mark `failed`.

---

## Phase 5: Continue?

### Step 1: Close the current round's team

Close the team to release reviewers and fixers. Force-terminate any unresponsive
agents.

### Step 2: Route

**Evaluate conditions top-to-bottom; take the first match.**

| Condition | → |
|-----------|---|
| Any issues were confirmed and fixed this round | Phase 2 (new review round) |
| `pending` or `failed` in CR_STATE_FILE | Phase 6 |
| Otherwise | Phase 7 |

The first condition means: if this round's review found real issues and fixes
were applied, always do a fresh full review to catch issues the previous round
may have missed. This is NOT a targeted re-check of the fixes — it is a
complete new review of the entire scope with a fresh team.

---

## Phase 6: Confirm

Present `pending` + `failed` issues grouped by risk (high → low), sorted by
file path within each group:
`[number] [file:line] [risk] [reason] — [description]`

Then ask the user to select which issues to fix using **a single multi-select
question** where each option's label is the issue summary (e.g.,
`[risk] file:line — description`). User checks multiple options in one prompt.
Checked → `approved`, unchecked → `skipped`.

If the user replies with a bulk instruction (e.g., "fix all", "skip the rest"),
apply it only to issues **at or below** the current `FIX_MODE` threshold.
Issues above the threshold still require individual confirmation.

- **All skipped** → Phase 7.
- **Any approved** → Phase 4 (Fix/Validate). After Validate, go to Phase 5
  (Continue?). If new issues are found, re-enter the full Review Loop (Phase 2).
  If no new issues but more `pending`/`failed` remain, return here (Phase 6).
  If nothing remains, proceed to Phase 7.

---

## Phase 7: Report

Force-terminate any agents still running. Close the team.

Summary:
- Rounds and per-round statistics
- Issues found / fixed / skipped / failed
- Rolled-back issues and reasons
- Final test result
- Issues from PR comments (when `PR_COMMENTS` existed)

### Checklist evolution

Review all confirmed issues from this session. If any represent a recurring
pattern not covered by the current checklist, read `checklist-evolution.md` and
follow its steps.

### Clean up

Delete CR_STATE_FILE.
