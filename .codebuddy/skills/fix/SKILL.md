---
name: fix
description: Multi-round automated code review and fix using Agent Teams.
disable-model-invocation: true
compatibility: Requires CodeBuddy Code with Agent Teams experimental feature enabled.
---

# Fix — Automated Code Review & Fix

Automatically review, verify, and fix code issues across your branch. Runs multi-round
team-based iterations until no valid issues remain. You only interact twice: choosing
the scope at the start, and confirming pending items at the end.

## Prerequisites

This skill requires the **Agent Teams** experimental feature. If not enabled, prompt
the user to enable it before proceeding:

- Option A: Run `/config`, find `[Experimental] Agent Teams`, toggle to `true`.
- Option B: Set environment variable `CODEBUDDY_CODE_EXPERIMENTAL_AGENT_TEAMS=1`.

## References

| Topic | File |
|-------|------|
| Review checklist (Level A-D + exclusions) | `references/review-checklist.md` |
| Issue filtering judgment matrix | `references/judgment-matrix.md` |
| Pending file templates | `references/pending-templates.md` |

---

## Team-Lead Responsibilities

You (the team-lead) orchestrate the entire workflow. Critical constraints:

1. **Never modify code files directly.** Delegate all code changes to agents. This
   prevents context bloat that causes information loss after compression.
2. **Read code only for arbitration and diagnosis** — never edit.
3. **Prompt iteration optimization.** After each round, analyze review quality and
   adjust the next round's reviewer prompts accordingly.

---

## Phase 0: Environment Check & Scope Confirmation

### 0.1 Environment Pre-check

Run sequentially. Abort if any fails.

1. `git status` — uncommitted changes? Ask user whether to commit first. Decline = abort.
2. **Automated test detection** — check whether the project has a usable test suite:
   - If the project's rules loaded in context already describe build/test commands, use
     those directly and skip exploration.
   - Otherwise, explore the codebase to identify the test framework and run commands
     (look for test directories, CMakeLists.txt test targets, package.json test scripts,
     Makefile test targets, etc.).
   - If no automated tests are found, warn the user: without tests, Phase 5 validation
     cannot verify fix correctness, and results may be unreliable. Ask the user whether
     to continue (review + fix without validation) or abort.
3. Build verification — use the project's build command. Fail = abort.
4. Run tests — use the project's test command. Fail = abort.

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

- Partition files into sub-modules with **strictly non-overlapping file sets**
- Target 5-20 files per sub-module
- Shared headers belong to the module of their primary implementation file
- Files referenced by 3+ modules belong to their primary caller's module

---

## Phase 1: Review

- `TeamCreate` to create the team
- One `general-purpose` reviewer agent (`reviewer-N`) per sub-module
- Reviewer prompt includes:
  - Module file list + check items for the selected fix level (see
    `references/review-checklist.md`)
  - **Evidence requirement**: every issue must have a code citation (file:line + snippet)
  - **Exclusion list**: see `references/review-checklist.md` exclusion section. Project
    rules loaded in context take priority over the exclusion list.
  - **Public API protection**: no signature/interface changes unless obvious bug or
    comment issue
  - Structured output format

### Stuck Detection

When all other agents in the batch are done but one has not responded:
1. Send a reminder
2. No response -> check for output
3. No output -> `shutdown_request` + create replacement
4. Partial output -> `shutdown_request` + create agent for remaining work

---

## Phase 2: Verification

- **Do not close reviewers** (reuse as fixers in Phase 4)
- Create independent verifier (`verifier-N`) per sub-module with issues
- Verifier reads actual code to confirm each issue exists

### Result Alignment

1. Send verification results back to the corresponding reviewer
2. Reviewer second confirmation:
   - Agrees -> no dispute, accept conclusion
   - Disagrees -> provides rebuttal evidence, marks as disputed
3. Close verifier after reviewer confirms

### Dispute Resolution

- No disputes -> accept agreed conclusions
- Disputes -> team-lead reads code to judge (read only, no edits)

---

## Phase 3: Filter — Team-Lead Only

Decide which confirmed issues to fix. Consult `references/judgment-matrix.md` for the
full matrix and special rules.

Key points:
- **Public API protection**: obvious bugs = fix; non-bug signature changes = record to
  `pending-api-changes.md` (see `references/pending-templates.md`)
- Output semantics changes -> skip by default
- Previously rolled-back issues -> do not attempt again

---

## Phase 4: Fix

- Reuse reviewers as fixers (they already have module context)
- Cross-module files -> dedicated `fixer-cross` agent
- Multi-file renames -> single fixer as atomic task

### Fixer Instructions (include in every fixer prompt)

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

Periodically check `git log` for new commits -> no output = stuck -> remind -> replace.

---

## Phase 5: Validate

- **Do not close reviewers yet** (may need retry)
- Build + test using the project's commands

**All pass** -> close all reviewers/fixers -> Phase 6.

**Failures**:
1. Record failing test case names
2. Bisect to locate the bad commit -> `git revert <bad-commit> --no-edit`
3. Send failure info back to the original reviewer to re-fix (max **2 retries**)
4. After 2 failed retries -> team-lead reads code to diagnose (read only):
   - Fix approach was wrong -> create `fixer-rescue-N` with full error context
   - Fix was correct but changed behavior -> revert, record to
     `pending-test-updates.md` (see `references/pending-templates.md`)
5. All resolved -> close all reviewers/fixers

---

## Phase 6: Loop

- `TeamDelete` to close the team
- **Termination**: valid issues from Phase 3 = 0 -> Phase 7
- Otherwise -> create new team, back to Phase 1
- No round limit — driven by valid issue count

Next round prompt includes: rollback blacklist, previous fix summary,
pending file contents (to avoid re-reporting), and prompt adjustments based on
review quality analysis.

---

## Phase 7: Final Confirmation

1. Check `pending-test-updates.md` and `pending-api-changes.md`
2. Both empty -> output summary report, done
3. Either has content -> present to user via AskUserQuestion, confirm item by item
4. Approved fixes -> create agent to re-apply; rejected -> discard
5. Final build + test -> output summary report, delete pending files

### Summary Report

- Total rounds and per-round fix count/type statistics
- Rolled-back issues and reasons
- Pending items and their resolution
- Final test result

---

## Troubleshooting

### Agent Teams not available
Cause: Experimental feature not enabled.
Solution: Run `/config` -> `[Experimental] Agent Teams` -> `true`.

### Build or test fails in Phase 0
Cause: Pre-existing issues in the codebase.
Solution: Fix build/test failures before running `/fix`.

### Reviewer reports no issues but code has problems
Cause: Fix level too restrictive or reviewer prompt insufficient.
Solution: Re-run with a higher fix level (C or D). Team-lead will also auto-adjust
prompts in subsequent rounds.

### Fixer commits break tests repeatedly
Cause: Complex semantic changes or insufficient context.
Solution: After 2 retries, `fixer-rescue-N` takes over with full error context. If the
fix is correct but changes behavior, it goes to `pending-test-updates.md` for user
confirmation.
