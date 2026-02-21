---
name: fix
description: Multi-round automated code review, verification, and fix workflow.
disable-model-invocation: true
---

# Fix — Automated Code Review & Fix Workflow

Multi-round team-based workflow: review -> verify -> filter -> fix -> validate -> loop.
User interaction is limited to Phase 0 (scope confirmation) and Phase 7 (pending items
confirmation, if any). Everything in between runs fully automatically.

## Reference Map

| Need | Read |
|------|------|
| Review checklist (all levels) | `review-checklist.md` |

---

## Team-Lead Responsibilities

The team-lead (you) orchestrates the entire workflow. Critical constraints:

1. **Never modify code files directly.** All code changes are delegated to agents
   (reviewers, fixers, fixer-cross, fixer-rescue). This prevents context bloat from
   embedding large code read/write operations, which causes information loss after
   context compression.
2. **Read code only for arbitration.** When arbitrating disputes or diagnosing test
   failures, read the relevant code to make a judgment — but never edit.
3. **Prompt iteration optimization.** After each round, analyze review quality (false
   positive rate, missed patterns, repeated issue types) and adjust the next round's
   reviewer prompts accordingly — tighten constraints in high-false-positive areas,
   add missed check dimensions, remove checks that are no longer needed.

---

## Phase 0: Environment Check & Scope Confirmation

### 0.1 Environment Pre-check

Run these checks sequentially. Abort if any fails.

1. `git status` — if there are uncommitted changes, ask the user whether to commit first.
   If user declines, abort.
2. Build verification — use the build command defined in the project's rules. Abort on
   failure.
3. Run tests — use the test command defined in the project's rules. Abort on failure.

### 0.2 Scope Confirmation (AskUserQuestion — single interaction)

Ask all questions at once:

**Question 1 — Review scope:**
- A: Current branch vs main diff (default)
- B: Diff from a specified base commit
- C: Specified folder path

**Question 2 — Fix level:**
- A: Logic bugs and security issues only (correctness)
- B: Bugs + convention violations (naming, initialization, etc.)
- C: Bugs + conventions + performance optimization + code simplification
- D: All (including documentation consistency, architecture suggestions)

**Question 3 — Documentation review** (only when scope includes doc directories):
- Whether to include documentation-code consistency checks

After confirmation, no further user interaction until Phase 7.

### 0.3 Module Partitioning

- List all files in scope, partition into sub-modules by directory/functionality with
  **strictly non-overlapping file sets**
- Target 5-20 files per sub-module
- Shared headers belong to the module of their primary implementation file
- Files referenced by 3+ modules belong to their primary caller's module

---

## Phase 1: Review

- `TeamCreate` to create the team
- Create one reviewer agent (`reviewer-N`) per sub-module, all `general-purpose` type
- Reviewer prompt must include:
  - Module file list + check items for the selected fix level (reference
    `review-checklist.md`)
  - **Evidence requirement**: Every reported issue must include a code citation
    (filename:line + code snippet) as evidence. Issues without evidence must not be
    reported.
  - **Exclusion list**: Explicitly list issue types that should not be reported (see
    the exclusion list in `review-checklist.md`). When the project's own rules loaded
    in context have explicit requirements for a particular issue type, those rules take
    priority over the exclusion list.
  - **Public API protection**: Do not suggest changes to public API function signatures
    or class interface definitions, unless it is an obvious bug or a comment issue.
    Focus on internal refactoring.
  - Structured output format

### Stuck Detection

When all other reviewers in the same batch have completed but one has not responded:
1. Send a reminder message
2. Still no response -> check if the module has any output
3. No output -> `shutdown_request`, immediately create a replacement agent
4. Partial output -> `shutdown_request`, create a new agent to continue remaining work

---

## Phase 2: Verification

- After reviewers complete, **do not close them** (preserve context for Phase 4 reuse
  as fixers)
- For each sub-module with reported issues, create an independent verifier (`verifier-N`)
- Verifier prompt:
  - List of issues reported by the reviewer
  - Requirement: read the actual code to verify each issue exists
  - For non-existent issues, explain why

### Result Alignment

- After verifier completes, send verification results back to the corresponding reviewer
- Reviewer performs second confirmation after seeing the verifier's judgment:
  - Agrees with verifier -> no dispute, accept that conclusion
  - Disagrees -> reviewer provides additional evidence to rebut, marks as disputed
- After reviewer confirmation, close the verifier (`shutdown_request`, continue
  immediately)

### Dispute Resolution (Team-Lead Arbitration)

- No disputed items -> accept the agreed conclusions directly
- Disputed items -> team-lead reads the code to judge (read only, no edits)
  - Disputed issues are typically 20-30% of total, manageable in volume
  - Team-lead has global perspective, higher judgment accuracy than individual agents

Same stuck detection mechanism applies.

---

## Phase 3: Filter — Team-Lead Only

Team-lead decides whether each confirmed issue is worth fixing, based on the selected
fix level and the judgment matrix below.

### Core Principles

- **Public API protection**: Do not casually modify public API function signatures or
  class definitions. Focus on internal refactoring.
- Issues involving public API signature changes: fix obvious bugs directly; record
  others to `pending-api-changes.md` for final confirmation.

### Judgment Matrix

| Type | Min Level | Criteria |
|------|-----------|----------|
| Logic bug | A | Affects runtime correctness -> must fix |
| Security (div-by-zero / OOB / uninitialized read) | A | Must fix |
| Resource leak (handle / lock not released) | A | Must fix |
| Project convention violation | B | Fix only when inconsistent with project rules loaded in context |
| Missing variable initialization | B | Fix when declared without initial value (per project rules) |
| API comment missing | B | Fix when public API lacks param/return description (comments only, no signature changes) |
| Function implementation order | B | Fix only when clearly inconsistent with header declaration order |
| Code simplification | C | Fix when logic can be clearly simplified (early return, branch merge, etc.) |
| Duplicate code extraction | C | Fix when >= 3 identical patterns |
| Container pre-allocation | C | Fix when size is predictable **and** on a hot path |
| Unnecessary copy / perf optimization | C | **Fix only when 100% certain of semantic equivalence; skip if uncertain** |
| Type safety | C | Only change local variables, never change function signatures |
| Interface design improvement | D | **Comments and documentation level only**, no signature changes |
| Documentation consistency | D | Fix when code and documentation disagree |
| Architecture suggestion | D | Fix only when dependency direction or responsibility division is clearly wrong |
| Public API signature change (obvious bug) | A | Must fix |
| Public API signature change (not a bug) | -- | **Do not fix directly**, record to `pending-api-changes.md` for final confirmation |
| Style preference | -- | **Always skip** |

### Special Rules

- Optimizations that change output semantics (may invalidate test baselines) -> **skip
  by default**
- Type narrowing fixes: only change local variables, never function signatures
- Cross-module renames: assign as an atomic task to a single fixer
- Issues rolled back in a previous round: **do not attempt again this round**

---

## Phase 4: Fix

### Task Assignment

- Group by original module, send back to the module's reviewer (already has context,
  reuse directly as fixer)
- Before assigning, check file ownership:
  - All files in one module -> assign to that module's reviewer
  - Files span multiple modules -> create a dedicated `fixer-cross` agent with all
    involved files and fix requirements in its prompt
  - Rename refactoring spanning multiple files -> assign to a single reviewer; other
    reviewers pause work on related files

### Fixer Instructions

Include these rules in every fixer's prompt:

```
Fix rules:
1. After fixing each issue, immediately: git commit --only <files> -m "message"
2. Only commit files in your own module. Never use git add .
3. Commit message: English, under 120 characters, ending with a period.
4. When in doubt, skip the fix rather than risk a wrong change.
5. Do not run build or tests.
6. Do not modify public API function signatures or class definitions (comments are OK).
7. When done, report the commit hash for each fix.
```

### Stuck Detection

Periodically check `git log` for new commits -> no output means stuck -> remind ->
close + replace.

---

## Phase 5: Validate

- **Do not close reviewers yet** (may need retry)
- Build + test using the commands defined in the project's rules

### All Pass

Close all reviewers/fixers -> Phase 6.

### Failures

1. Record the names of failing test cases
2. Use bisection to locate the commit that introduced the failure (checkout middle
   commit -> build -> run only failing tests)
3. `git revert <bad-commit> --no-edit`
4. **Retry mechanism**: Send failure info and test error output back to the original
   reviewer to re-fix
   - Re-validate -> pass means continue
   - Still fails -> give the reviewer one more chance (max **2 retries**)
5. **After 2 failed retries** -> team-lead reads code to diagnose (read only, no edits):
   - **Fix approach was wrong** (fix bug) -> create `fixer-rescue-N` agent with full
     error context
   - **Fix was correct but changed expected behavior** (behavior change) -> revert,
     record to `pending-test-updates.md`
6. After all issues are resolved and validation passes -> close all reviewers/fixers

### pending-test-updates.md Format

```markdown
# Pending Test-Impact Fixes

The following fixes are confirmed correct in code logic but change the expected output
of test cases. User confirmation is needed before applying.

## Issue 1: [description]
- **Original commit**: [hash] (reverted)
- **Affected tests**: [test case names]
- **Behavior change**: [why the test fails, how expected output changes]
- **Code change**: [brief description of the modification]
```

---

## Phase 6: Loop

- `TeamDelete` to close the team
- **Termination condition**: the number of valid issues confirmed by team-lead in
  Phase 3 is 0
  - Even if reviewers reported many issues, if team-lead judges none are worth fixing
    -> terminate
  - Valid issues exist -> continue fixing -> validate -> create new team, back to
    Phase 1
  - No round limit — driven entirely by team-lead's valid issue judgment
- When termination condition is met -> Phase 7

### Next Round Prompt Includes

- Rollback blacklist
- Summary of issues fixed in previous rounds (to avoid re-reporting)
- Issues in `pending-test-updates.md` (to avoid re-reporting)
- Issues in `pending-api-changes.md` (to avoid re-reporting)
- **Prompt iteration optimization**: team-lead analyzes this round's review quality
  (false positive rate, missed patterns, repeated issue types) and adjusts the next
  round's reviewer prompts — tighten constraints in high-false-positive areas, add
  missed check dimensions, remove checks no longer needed

---

## Phase 7: Final Confirmation

### 7.1 Check Pending Files

- `pending-test-updates.md`: fixes that are correct but affect test baselines
- `pending-api-changes.md`: fixes involving public API signature changes
- Both files absent or empty -> output summary report, done
- Either has content -> present to user

### 7.2 User Confirmation (the only mid-workflow user interaction)

- Present both pending files to the user
- AskUserQuestion: confirm item by item which fixes should be applied
- User-approved fixes -> create agent to re-apply changes (API changes need to update
  callers and tests in sync)
- User-rejected fixes -> discard

### pending-api-changes.md Format

```markdown
# Pending Public API Changes

The following issues involve public API signature changes and require user confirmation.

## Issue 1: [description]
- **File**: [file path:line number]
- **Current signature**: [existing function signature]
- **Suggested change**: [proposed new signature]
- **Reason**: [why the change is needed]
- **Impact scope**: [which callers need to be updated]
```

### 7.3 Final Validation

- Build + test all pass -> output summary report, delete pending files

### Summary Report

- Total number of rounds
- Per-round fix count and type statistics
- Rolled-back issues and reasons
- Pending test-impact fixes and their resolution
- Pending API changes and their resolution
- Final test result (all pass)

---

## Key Principles

### Accuracy Guarantee

Five-layer filtering: reviewer self-check (code evidence) + independent verifier +
reviewer second confirmation + team-lead dispute arbitration + team-lead filtering.
Exclusion list explicitly defines issue types that should not be reported. Rolled-back
issues are remembered across rounds.

### Non-Interruption Guarantee

User interaction at only two points: Phase 0 scope confirmation + Phase 7 pending
items (test impact + API changes, if any). Agent closures always use
`shutdown_request` then continue immediately. Stuck detection: when all other agents
in the batch are done, proactively check and replace. Test failures do not terminate
the workflow: retry -> fixer-rescue takes over -> record to pending.

### Commit Discipline

- Each fix is committed immediately with `git commit --only <files>`
- Never use `git add .` / `git commit -a` / `--amend`

### Prohibited Actions

- Team-lead must not modify any code files
- Reviewers/fixers must not run builds or tests
- Must not work on the main branch
- Must not directly modify public API signatures (except obvious bugs / comments);
  non-bug changes go through the pending confirmation flow
