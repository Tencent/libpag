---
name: fix
description: Multi-round automated code review and fix using Agent Teams.
---

# Fix — Automated Code Review & Fix

**Activation**: Only execute this workflow when the user explicitly invokes `/fix`.
Do NOT auto-trigger for general bug fix requests, code questions, or error discussions.

When the user invokes `/fix`, immediately start executing from Phase 0 below. Do NOT
ask the user what they want to fix — this is a complete automated workflow that reviews
the entire branch, not a one-off bug fix tool.

## Argument Parsing

The user's input after `/fix` is available as `$ARGUMENTS`. Parse it to determine the
review scope:

| `$ARGUMENTS` | Scope |
|--------------|-------|
| (empty) | Current branch diff vs main (default) |
| A valid git commit/ref | Current branch diff vs the specified base commit |
| A file or directory path | All files under the specified folder or the single file |

How to distinguish a commit from a path: run `git rev-parse --verify $ARGUMENTS` — if
it succeeds, treat it as a commit; otherwise treat it as a file/directory path (verify
it exists). If neither resolves, report the error and abort.

When scope is resolved from `$ARGUMENTS`, skip Question 1 in Phase 0.2 and only ask
Question 2 (fix level). Report the resolved scope to the user for confirmation in the
same interaction.

Automatically review, verify, and fix issues in code and documents across your branch.
Runs multi-round team-based iterations until no valid issues remain.

## Prerequisites

This skill requires the **Agent Teams** experimental feature. If not enabled, prompt
the user to enable it before proceeding:

- Option A: Run `/config`, find `[Experimental] Agent Teams`, toggle to `true`.
- Option B: Set environment variable `CODEBUDDY_CODE_EXPERIMENTAL_AGENT_TEAMS=1`.

## References

| Topic | File |
|-------|------|
| Code review checklist (Level A-C + exclusions) | `references/code-checklist.md` |
| Document review checklist (Level A-C + exclusions) | `references/doc-checklist.md` |
| Issue filtering judgment matrix | `references/judgment-matrix.md` |
| Pending file templates | `references/pending-templates.md` |

---

## Interaction Language

All user-facing interactions (AskUserQuestion, progress reports, summary reports,
warnings, and confirmations) must use the language the user has been using in the
conversation. Do not default to English.

## User Interaction

When presenting choices with predefined options (e.g., scope confirmation, fix level
selection), use interactive dialogs with selectable options rather than plain text.

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

1. **Clean branch check** — verify the working tree is clean and not on the main branch:
   - If on the main/master branch, or there are uncommitted changes (staged, unstaged,
     or untracked), inform the user: "Automated fix requires a clean, non-main branch.
     Each fix is committed individually for easy rollback and issue tracing."
   - On main -> abort, ask user to create a feature branch first.
   - Uncommitted changes -> ask user whether to commit or stash first. Decline = abort.
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

### 0.2 Scope & Fix Level Selection

If `$ARGUMENTS` is empty, ask both questions. If scope was resolved from the argument,
report it and only ask the fix level question.

**Question 1 — Review scope** (skip if resolved from argument):
- Option 1 — "Current branch vs main": use the diff between current branch and main
- The "Other" free-text option allows the user to type a base commit (hash or ref) or
  a folder/file path directly. Validate the input the same way as `$ARGUMENTS` parsing.

**Question 2 — Fix level** (option labels should be concise, use descriptions to explain
the incremental scope of each level):
- Option 1 — "Correctness & Safety only": logic bugs, security vulnerabilities,
  resource leaks, memory safety, thread safety
- Option 2 — "Correctness & Safety + Refactoring": adds performance optimization,
  code simplification, architecture improvement, interface usage, test coverage,
  regression risk
- Option 3 — "All": adds coding conventions, documentation consistency, accessibility

### 0.3 Reference Material (doc/mixed modules only)

If the scope contains doc or mixed modules, gather reference material for reviewers:

1. **Auto-discover**: search the project and web for relevant best-practice guides,
   official documentation, or specification standards that apply to the document type
   being reviewed (e.g., skill authoring guides, API design guidelines, RFC standards).
2. **Ask the user**: present what you found and ask if they have additional reference
   material (file paths, URLs, or inline instructions). The user may also choose to
   skip this step if no extra references are needed.
3. Include all gathered references in the reviewer prompts for doc/mixed modules.

After confirmation, no further user interaction until Phase 7 (but see Mid-Review
Supplements below).

### 0.4 Module Partitioning

Partition files in scope into modules for parallel review. Each module should
be a self-contained logical unit (a class and its implementation, a feature directory,
a subsystem, or a documentation set) that a reviewer can understand with minimal
reference to other modules.

- **Module type**: classify each module as `code`, `doc`, or `mixed` based on its file
  types. This determines which checklist the reviewer uses.
- **Non-overlapping**: every file belongs to exactly one module.
- **Split when too large**: if a logical module has too many files for one agent, split
  along internal boundaries. Keep splits roughly balanced.
- **Shared headers**: assign to the module of their primary implementation file.
- **Widely referenced files** (3+ modules): assign to the primary caller's module.

### Mid-Review Supplements

The user may send additional reference material or instructions at any time during the
review process. When this happens:

1. Acknowledge receipt and assess which modules are affected.
2. Forward the material to the relevant active reviewer/verifier agents.
3. If reviews for affected modules are already complete, note the supplements for the
   next round (Phase 6 loop) rather than restarting the current round.

---

## Phase 1: Review

- `TeamCreate` to create the team
- One `general-purpose` reviewer agent (`reviewer-N`) per module
- Reviewer prompt includes:
  - Module file list + check items for the selected fix level, using the checklist
    matching the module type: `references/code-checklist.md` for code modules,
    `references/doc-checklist.md` for doc modules, both for mixed modules
  - **Review scope rule**: reviewers must read entire files for full context, but apply
    different reporting thresholds based on whether code is within the branch diff:
    - **Code within diff**: report all issues matching the selected fix level (A/B/C)
    - **Code outside diff**: report only Level A (correctness/safety) and Level B
      (refactoring/optimization) issues. Skip Level C (conventions/documentation) for
      unchanged code.
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
- Create independent verifier (`verifier-N`) per module with issues
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
- **Cross-module doc impact**: for each fix, identify whether the change affects comments
  or documentation files outside the fixer's own module. If so, create a follow-up task
  for `fixer-cross` to update those files after the original fix is committed.
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
7. After each fix, check whether the change affects related comments or documentation
   (function/class doc-comments, inline comments describing the changed logic, README
   or spec files that reference the changed behavior). If so, update them in the same
   commit as the fix.
8. When done, report the commit hash for each fix.
```

### Stuck Detection

Periodically check `git log` for new commits -> no output = stuck -> remind -> replace.

---

## Phase 5: Validate

- **Do not close reviewers yet** (may need retry)
- For code/mixed modules: build + test using the project's commands
- For doc-only modules: skip build/test (no compilation needed); validation is
  completed through the review and verification phases

**All pass** (or no code modules to validate) -> close all reviewers/fixers -> Phase 6.

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
Solution: Re-run with a higher fix level. Team-lead will also auto-adjust prompts in
subsequent rounds.

### Fixer commits break tests repeatedly
Cause: Complex semantic changes or insufficient context.
Solution: After 2 retries, `fixer-rescue-N` takes over with full error context. If the
fix is correct but changes behavior, it goes to `pending-test-updates.md` for user
confirmation.
