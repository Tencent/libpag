# Teams Review

You are the **coordinator**. Create an Agent Team and dispatch reviewer,
verifier, and fixer agents. Never modify files directly. Read code only for
arbitration, diagnosis, and fix verification.

Always process all auto-fixable issues before involving the user. Do NOT pause
to ask the user anything until Confirm (Phase 5) or Report (Phase 6).

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
Scope → Review → Filter → Fix/Validate → Confirm → Report
```

- **Filter** routes auto-fixable issues to Fix/Validate; remaining go to Confirm.
  If nothing to fix or confirm, skip directly to Report.
- **Confirm** ↔ **Fix/Validate** loop until no pending issues remain.

---

## Phase 1: Scope

Determine the diff to review based on `$ARGUMENTS`:

- **Empty arguments**: find the base branch by checking common base branches
  in order: `main`, `master`. Use the first one that exists. Fetch the full diff
  from merge-base to the working tree (committed + uncommitted changes):
  ```
  git merge-base origin/{base_branch} HEAD
  git diff <merge-base-sha>
  ```
  Also check for untracked files with `git status --porcelain` (`??` lines)
  and read their contents for review.
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

### Associated PR comments

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

### Module partition

Partition files in scope into **review modules** for parallel review. Each
module is a self-contained logical unit. Split large files by section/function
group; group related small files together. Classify each module as `code`,
`doc`, or `mixed`.

### Issue tracking

The coordinator tracks all issues in memory throughout the session. Each issue
has:
- Brief description
- Status: `pending` | `approved` | `fixed` | `failed` | `skipped`
- Risk: low | medium | high
- File: file path:line
- Proposed fix (medium/high risk only)

---

## Phase 2: Review

### Team setup

Create a team for the session.

- One reviewer agent (`reviewer-N`) per module.
- One **verifier** agent (`verifier`), shared across all modules.

**Module merging**: if the total diff is ≤1000 changed lines AND ≤20 files,
merge all modules into a single reviewer. The overhead of multiple agents
(startup, coordination, forwarding) outweighs the parallelism benefit at
this scale.

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
- **Checklist exclusion**: see the exclusion section in the corresponding
  checklist. Project rules loaded in context take priority.
- **Self-check**: before submitting, re-read the relevant code and verify each
  issue. Mark as confirmed or withdrawn. Only submit confirmed issues. If a cited
  path/line no longer exists, locate the correct file/path via `git diff --name-only`
  or file search before reporting.
- **Output format**: `[file:line] [A/B/C] — [description] — [key lines]`

**PR comment reviewer** (when `PR_COMMENTS` exist): one additional agent to
verify PR review comments against current code. Same output format, same
verification pipeline.

### Verification (pipeline)

Stance: **adversarial** — default to doubting the reviewer, actively look for
reasons each issue is wrong. Reject with real evidence, confirm if it holds up.
This step is mandatory — the coordinator MUST NOT skip it or perform
verification itself. **Exception**: if every reviewer explicitly reports zero
issues (LGTM / no issues found), skip verification and proceed directly to
Phase 3.

The verifier runs as a **pipeline** — it does not wait for all reviewers to
finish. As each reviewer sends a report via SendMessage, the coordinator MUST
forward it to the verifier immediately. Forwarding rules:

- **Quote verbatim**: wrap the reviewer's original SendMessage content in a
  quote block and send it as-is.
- **No rewriting**: do not summarize, reorganize, merge multiple reviewer
  reports into one message, or add coordinator commentary.
- **One forward per reviewer**: each reviewer report is a separate message to
  the verifier.
- **Completion signal**: after forwarding the last reviewer's report, send a
  separate message to the verifier stating: "All N reviewer reports have been
  forwarded. Please finalize your verdicts for all issues above." (Replace N
  with the actual reviewer count.)

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
   - For cross-module "inconsistency" claims: read the type definitions at both
     locations — different libraries may use different conventions (coordinate systems,
     sign conventions) that make surface-level differences correct. REJECT if the
     reviewer's evidence is limited to "these two snippets look different" without
     confirming both operate under the same conventions.
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
- Reviewer reports arrive incrementally via the coordinator. Do NOT produce a final
  summary until the coordinator explicitly tells you all reports have been forwarded.
  Process each report as it arrives, but wait for the completion signal before
  concluding.
- The coordinator will tell you the total number of reviewers in the completion
  signal. If you have not received that many reports, do NOT finalize — wait for
  the remaining forwards.
```

### After review

**Missing report detection**: if a reviewer's system notification shows
"completed" but the coordinator has not received a SendMessage report from
that reviewer, immediately send a follow-up message to the reviewer requesting
its report. Do not wait or assume the report was lost — the agent may have
exhausted its turn limit before sending.

Before entering Phase 3, confirm: (1) all reviewers have submitted their final
reports; (2) the verifier has given a CONFIRM/REJECT verdict for every
forwarded finding, OR all reviewers reported zero issues and verification was
skipped.

Keep all agents alive — do not close any agents mid-session. Reviewers are
reused as fixers in Phase 4. All agents are cleaned up in Phase 6 (Report).

---

## Phase 3: Filter — coordinator only

Your stance here is **neutral** — trust no single party. Treat reviewer reports
and verifier rebuttals as equally weighted inputs. Use your project-wide view to
consider cross-module impact, conventions, and architectural intent that local
reviewers may miss.

### 3.1 De-dup

Remove cross-reviewer duplicates (same location, same topic).

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

### 3.4 Route

All confirmed issues are recorded with risk level.

| Risk vs `FIX_MODE` | → |
|---------------------|---|
| At or below threshold | auto-fix queue |
| Above threshold | `pending` (for Phase 5 Confirm) |

- Cross-module impact: if a fix requires updates outside the fixer's module,
  add it to the current fix queue and assign to the appropriate fixer.

Always auto-fix eligible issues first — do NOT present `pending` issues to the
user before all auto-fixable issues have been processed and validated.
Phase 4 if auto-fix queue is non-empty. Otherwise jump to Phase 5 if pending
issues exist, or Phase 6 if none.

---

## Phase 4: Fix/Validate

### Fix

Stance: **precise** — apply each fix completely and correctly, never expand
scope. The coordinator MUST NOT apply fixes directly.

**Agent assignment**: reuse reviewers as fixers when available — each reviewer
already has context on the files it reviewed. The coordinator MUST assign by
explicit file list and ensure a file is owned by only one fixer at a time:

- Issue in a file that a reviewer already read → assign to that reviewer.
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

Each fixer commits per issue (one commit per fix — never combine multiple
issues into a single commit).

### Verify fixes (coordinator)

Wait for all fixers. Before running build + test, the coordinator reads each
fixer's commit diff and verifies:
1. The fix correctly addresses the original issue
2. No new issues introduced (naming inconsistencies, missing updates in
   surrounding code, logic errors)
3. Fix scope matches the issue — no unintended changes

If a problem is found, send the fixer a correction request with specific
details (max 1 retry). If the retry fails or the fixer is unavailable,
revert and mark `failed`.

### Build/test validate

Run build + test.

**Revert scope**: only revert commits produced by fixers in this phase. Never
revert commits unrelated to the current fixes — other users or tools may commit
concurrently during the fix phase. Identify fixer commits by the commit hashes
reported by fixers; any other commits on the branch are out of scope.

- Skip if no build/test commands available or doc-only modules.
- **Pass** → mark issues `fixed`.
- **Fail** → bisect among fixer commits only to find the failing commit, revert
  it, re-validate remaining before blaming others (one bad commit may cause
  cascading failures). Per failing issue: retry via the original fixer agent
  with failure details (max 2 retries), or revert and mark `failed`.

### After validation

| Condition | → |
|-----------|---|
| `pending` or `failed` issues exist | Phase 5 (Confirm) |
| Otherwise | Phase 6 (Report) |

If Phase 5 approves further fixes, reuse existing reviewers as fixers (or
create new fixer agents if needed) and re-enter Phase 4.

---

## Phase 5: Confirm

Present `pending` + `failed` issues grouped by risk (high → low), sorted by
file path within each group:
`[number] [file:line] [risk] [reason] — [description]`

Then present issues via multi-select. Each option label is the issue summary
(e.g., `[risk] file:line — description`).
Checked → `approved`, unchecked → `skipped`.

If the user replies with a bulk instruction (e.g., "fix all", "skip the rest"),
apply it only to issues **at or below** the current `FIX_MODE` threshold.
Issues above the threshold still require individual confirmation.

- **All skipped** → Phase 6.
- **Any approved** → Phase 4 (Fix/Validate). After validation, if more
  `pending`/`failed` remain, return here (Phase 5). If nothing remains,
  proceed to Phase 6.

---

## Phase 6: Report

Send shutdown_request to all remaining agents in parallel (single message with
multiple SendMessage calls). Then call TeamDelete immediately without waiting
for individual shutdown responses.

Summary:
- Issues found / fixed / skipped / failed
- Rolled-back issues and reasons
- Final test result
- Issues from PR comments (when `PR_COMMENTS` existed)
- Note: "To verify fix quality, run `/cr` again."

### Checklist evolution

Review all confirmed issues from this session. If any represent a recurring
pattern not covered by the current checklist, read `checklist-evolution.md` and
follow its steps.
