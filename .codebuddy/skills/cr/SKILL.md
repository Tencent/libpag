---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# Review — Automated Code Review & Fix

Use `/cr` to start. Accepts a PR number/URL, commit (range), file/directory paths,
or no argument (current branch vs upstream). See Phase 1 for full argument parsing.

Reviews code and documents using Agent Teams. Each issue gets a risk level — low-risk
issues are auto-fixed, higher-risk issues are presented for user confirmation. In PR
mode, issues are submitted as line-level PR review comments instead of direct commits.
Runs multi-round iterations until no valid issues remain.

## Instructions

- You (the team-lead) **never modify files directly**. Delegate all changes to
  agents. Read code only for arbitration and diagnosis.
- **Autonomous operation**: between Phase 0 (user confirmation) and Phase 8
  (remaining issue confirmation), the review-fix loop runs without user interaction.
  Deferred issues accumulate across rounds and are only presented to the user in
  Phase 8.
- All user-facing interactions must use the language the user has been using in the
  conversation. Do not default to English.
- When presenting choices with predefined options, use interactive dialogs with
  selectable options rather than plain text.
- After each round, analyze review quality and adjust the next round's reviewer prompts.
- The team-lead must handle all unexpected situations autonomously — e.g., an agent
  stuck or unresponsive, a fix that breaks the build, a git operation that fails, an
  agent producing invalid output. Use your judgment: terminate and replace, revert and
  retry, skip and move on, etc. Anything that cannot be resolved automatically should
  be recorded to `pending-issues.md` for user review in Phase 8.
- The user may send additional material (files, URLs, context) at any time. If
  reviewers are still active (Phase 2-3), forward it directly; otherwise include
  the material in the next round.

## References

| Topic | File |
|-------|------|
| Code review checklist (Priority A-C + exclusions) | `references/code-checklist.md` |
| Document review checklist (Priority A-C + exclusions) | `references/doc-checklist.md` |
| Issue filtering & risk level judgment matrix | `references/judgment-matrix.md` |
| Pending issue template | `references/pending-templates.md` |
| Fixer agent instructions | `references/fixer-instructions.md` |
| PR review comment format | `references/pr-comment-format.md` |

---

## Phase 0: User Confirmation

Prioritize asking questions before any time-consuming operations (git, gh, diff,
build, codebase exploration) to minimize user wait time.

### Mode detection

Parse `$ARGUMENTS` to determine the review mode:
- Purely numeric or URL (contains `/`) -> **PR mode**
- Everything else (empty, commit-like, paths, `..` range) -> **Local mode**

### Questions

Present all applicable questions in **one interaction**.

#### Question 1 — Review priority

Always show. When the scope turns out to be doc-only (determined later in Phase 1
module partitioning), all priority levels (A+B+C) are used regardless of the user's
choice.

(code/mixed modules):
- Option 1 — "Full review (A + B + C)": correctness, refactoring, and conventions.
  e.g., null checks, duplicate code extraction, naming style, comment quality.
- Option 2 — "Correctness + optimization (A + B)": skip conventions and documentation.
  e.g., null checks, resource leaks, duplicate code, unnecessary copies.
- Option 3 — "Correctness only (A)": only safety and correctness issues.
  e.g., null dereference, out-of-bounds, resource leaks, race conditions.

#### Question 2 — Auto-fix threshold (skip in PR mode)

In PR mode, add a note alongside Q1: "PR mode — issues will be submitted as PR
review comments after your review."

Option 3 should be pre-selected as the default.

- Option 1 — "All confirm": no auto-fix, confirm every issue before any change.
- Option 2 — "Low risk only": auto-fix only fixes with a single correct approach
  (e.g., null checks, comment typos, naming, `reserve`). Confirm anything whose
  impact extends beyond the immediate locality.
- Option 3 — "Low + Medium risk (recommended)": auto-fix unambiguous fixes
  and clear cross-location refactors (e.g., null checks, naming, extract shared logic,
  remove unused internals). Confirm high-risk issues.
- Option 4 — "Full auto (risky)": auto-fix all risk levels, autonomously deciding
  fix approach for high-risk issues (e.g., API changes, architecture restructuring,
  algorithm trade-offs). Only test baseline changes require confirmation.

After all questions are answered, no further user interaction until Phase 8.

---

## Phase 1: Scope Confirmation & Environment Check

### 1.1 Pre-flight Checks

Automated checks — no user interaction.
- Check if Agent Teams is enabled. If not available, warn the user in the conversation
  that review will run in single-agent serial mode (one module at a time) instead of
  parallel. Continue without aborting.

**Test environment**:
- Skip if PR mode. If the scope appears doc-only based on file extensions
  (e.g., all `.md`/`.txt`/`.rst`) or no build/test commands can be determined from
  context or the codebase, warn the user in the conversation that fix validation will
  be skipped (fixes are still applied but not automatically verified). Continue without
  aborting.

**Clean branch check**:
- **PR mode**: skip — PR mode is read-only (comment output only), no local
  modifications will be made.
- **Local mode**: verify not on the main/master branch (abort if so). If there are
  uncommitted changes, abort and ask the user to commit or stash first (fixes may be
  committed even in all-confirm mode after user approval in Phase 8).

### 1.2 Scope Preparation

Validate arguments and fetch the actual diff/content.

**PR mode**:

1. **Fetch PR metadata**:
   ```
   gh pr view {number} --json headRefName,baseRefName,headRefOid,state
   ```
   Extract: `PR_BRANCH`, `BASE_BRANCH`, `HEAD_SHA`, `STATE`.
   If the command fails (gh not installed, not authenticated, PR not found, or URL
   repo mismatch), inform the user and abort.
   If `STATE` is not `OPEN`, inform the user and exit.

2. **Prepare working directory**:
   - If current branch equals `PR_BRANCH` -> use current directory directly.
   - Otherwise -> create a worktree:
     ```
     git fetch origin pull/{number}/head:pr-{number}
     git worktree add /tmp/pr-review-{number} pr-{number}
     ```
     All subsequent operations use the worktree directory. Record `WORKTREE_DIR` for
     cleanup in Phase 9.

3. **Set review scope**: diff against `BASE_BRANCH`.
   ```
   git fetch origin {BASE_BRANCH}
   git diff $(git merge-base origin/{BASE_BRANCH} HEAD)
   ```

4. **Fetch existing PR review comments** for de-duplication in Phase 4:
   ```
   gh api repos/{owner}/{repo}/pulls/{number}/comments
   ```
   Store as `EXISTING_PR_COMMENTS` for later comparison.

**Local mode** — validate arguments and fetch diff:

- Empty arguments: determine the base branch from the current branch's upstream
  tracking branch. If no upstream, fall back to `main` (or `master`). Fetch the
  branch diff.
- Commit hash: validate with `git rev-parse --verify`, then `git show`.
- Commit range: validate both endpoints, then fetch the range diff.
- File/directory paths: verify all paths exist on disk, then read file contents.

**Local mode — associated PR comments** (optional, best-effort):
If `gh` is available, check whether the current branch has an open PR:
```
gh pr view --json number,state --jq 'select(.state == "OPEN") | .number' 2>/dev/null
```
If an open PR exists, fetch its line-level review comments:
```
gh api repos/{owner}/{repo}/pulls/{number}/comments
```
These are used in Phase 4 as reference information — each comment is verified against
the actual code to confirm the issue still exists before being treated as a known
issue to fix.

**Empty scope check**: if the diff is empty or the PR has no file changes, inform the
user that there is nothing to review and exit.

**Build + test baseline** (skip for PR mode, doc-only scope, or no build/test commands):
Run build and test in the working directory to establish a passing baseline. Fail = abort.

### 1.3 Module Partitioning

Partition files in scope into **review modules** for parallel review.

- Each module is a self-contained logical unit. Split large files by section/function
  group; group related small files together. Balance workload across reviewers.
- Classify each module as `code`, `doc`, or `mixed` to determine which checklist to use.

**Fix modules** are determined in Phase 4: group confirmed issues **by file** (one
fixer per file to prevent concurrent edit conflicts). Cross-file issues go to a
dedicated `fixer-cross` agent.

---

## Phase 2: Review

- Create the team (or run serially without a team if Agent Teams is unavailable).
- One `general-purpose` reviewer agent (`reviewer-N`) per module
- Reviewer prompt includes:
  - Module file list + checklist matching the module type:
    `references/code-checklist.md` for code modules, `references/doc-checklist.md` for
    doc modules, both for mixed modules. Only include priority levels selected by the
    user (e.g., if user chose "A + B", do not include Priority C items).
  - **Review scope rule**: reviewers must read entire files for full context, but apply
    different reporting thresholds based on scope type:
    - **Diff-based scope** (branch diff, PR, commit, commit range): report issues at
      the selected priority levels for code within the diff. For code outside the diff,
      only report Priority A issues.
    - **Full content scope** (file/directory path): report issues at the selected
      priority levels for all code in the files.
  - **Evidence requirement**: every issue must have a code citation (file:line + snippet)
  - **Exclusion list**: see the exclusion section in the corresponding checklist. Project
    rules loaded in context take priority over the exclusion list.
  - **Public API protection**: no signature/interface changes unless obvious bug or
    comment issue
  - **PR context** (PR mode only): include existing PR review comments so reviewers
    have context on already-discussed topics
  - **Output format**: for each issue, report:
    `[file:line] [priority A/B/C] — [description] — [code snippet]`

---

## Phase 3: Verification

- **Reviewer self-check + independent verification** (run in parallel):
  - Each reviewer re-reads the relevant code and self-verifies every issue they
    reported, marking each as confirmed or withdrawn.
  - Simultaneously, create a verifier (`verifier-N`) per review module. The verifier
    receives the reviewer's issue list and reads actual code to independently confirm
    each issue.
- **Alignment**: team-lead collects both the reviewer's self-check results and the
  verifier's results. If they disagree on any issue, team-lead reads code to judge.
- After alignment is complete, close all reviewers and verifiers.

---

## Phase 4: Filter & Risk Assessment — Team-Lead Only

For each confirmed issue, decide whether to fix and assign a risk level. Consult
`references/judgment-matrix.md` for the full matrix, risk level definitions, and
special rules.

### De-duplication

- Skip any issue that matches one already fixed, rolled back, recorded to pending,
  deferred, or **rejected by the user** in a previous round.
- **PR comment de-duplication** (PR mode): compare each confirmed issue against
  `EXISTING_PR_COMMENTS`. If an issue matches an existing comment (same file, same
  general location, same topic), exclude it — do not present it to the user.
- **Associated PR comment verification** (Local mode with associated PR): for each
  PR review comment retrieved in 1.2, verify against the current code whether the
  issue still exists. Verified issues are added to the fix queue as additional known
  issues (using the same risk assessment flow).

### Risk level assignment

Assign each fixable issue a risk level (low / medium / high) based on the judgment
matrix. The user's chosen auto-fix threshold determines handling:
- Issues at or below the threshold -> queued for auto-fix (Phase 5)
- Issues above the threshold -> added to the **deferred issue list** (accumulated
  across all rounds, presented to the user only in Phase 8)

### Mode-specific routing

- **PR mode**: all issues go to the deferred list. Skip Phase 5-7. If the deferred
  list is empty -> Phase 9; otherwise -> Phase 8.
- **Local mode**: if no auto-fix issues remain, proceed to Phase 5 (which will
  skip to Phase 7 due to empty queue, then Phase 7 checks for deferred/pending
  issues: if any -> Phase 8, otherwise -> Phase 9).

### Additional checks

- For each fixable issue, check whether the change also requires updates to comments
  or documentation outside the fixer's module — if so, create a follow-up for
  `fixer-cross`.
- Previously rolled-back issues -> do not attempt again this round

---

## Phase 5: Fix

If the auto-fix queue is empty (all issues were deferred), skip Phase 5-6 and go
directly to Phase 7.

- Group confirmed issues into **fix modules by file** (see 1.3).
- Cross-file issues -> `fixer-cross` agent; multi-file renames -> single atomic task

### Fixer Instructions (include in every fixer prompt)

Include `references/fixer-instructions.md` verbatim in every fixer agent's prompt.
Team-lead collects skipped issues and includes them in the next round's context.

---

## Phase 6: Validate

- For code/mixed modules: build + test using the project's commands. If no build/test
  commands are available (warned in 1.1), skip validation entirely.
- For doc-only modules: skip build/test; validation is done through review phases

**All pass** (or no code modules to validate) -> close all fixers -> Phase 7.

**Failures**: identify the failing commit (bisect if multiple commits, direct revert
if only one), revert it, and send failure info back to the original fixer for retry
(max 2 retries). If still failing, revert and record to `pending-issues.md`. Close
all fixers when resolved.

---

## Phase 7: Loop

- Close the team to release all agents (skip if running in serial mode without a team).

### Termination check

- If at least one commit was produced this round and round count < 100
  -> create new team, back to Phase 2
- Otherwise (no commits produced, or max 100 rounds reached):
  - Deferred list or `pending-issues.md` has entries -> Phase 8
  - Both empty -> Phase 9

### Next round context

Next round prompt includes: rollback blacklist, previous fix summary,
pending file contents, and prompt adjustments based on review quality analysis.
Reviewers must skip all issues already reported in previous rounds — whether fixed,
rolled back, recorded to pending files, deferred for user confirmation, or rejected
by the user.

---

## Phase 8: Remaining Issue Confirmation

Collect all issues that were not auto-fixed during the loop:
- **Deferred issues**: above the user's auto-fix threshold (accumulated in Phase 4)
- **Pending issues**: failed auto-fix or rolled back (recorded in `pending-issues.md`)

Merge both lists, deduplicate, and present to the user. If the merged list is empty,
skip to Phase 9.

1. Present issues in a compact numbered list. Each entry should fit on one line:
   `[number] [file:line] [risk] [source: deferred/failed] — [description]`

2. Interaction depends on issue count:

   **5 or fewer issues**: present each issue individually with selectable options.
   First option = fix (or submit as PR comment), second option = skip.

   **More than 5 issues**: ask the user for a bulk action:
   - Option 1 — "Fix all": act on every issue
   - Option 2 — "Skip all": discard all issues
   - Option 3 — "Select individually": present each issue one by one (same as above)

3. **Action depends on mode**:
   - **PR mode**: submit selected issues as PR review comments using the format in
     `references/pr-comment-format.md`. Comment body should be concise, written in the
     user's conversation language, with a specific fix suggestion. -> Phase 9
   - **Local mode**: jump back to Phase 5 with the user-approved issues as the fix
     queue. The full cycle runs: Phase 5 (fix) -> Phase 6 (validate) -> Phase 7 (loop)
     -> Phase 2 (review the new changes) -> ... until no new issues are found, then
     back to Phase 8. Any remaining deferred/pending issues from this cycle are
     presented again. The loop terminates when the merged list is empty or the user
     skips all remaining issues -> Phase 9.

---

## Phase 9: Cleanup & Report

### Worktree cleanup (PR mode only)

If `WORKTREE_DIR` was created, clean up:
```
cd {original_directory}
git worktree remove {WORKTREE_DIR}
git branch -D pr-{number}
```

### Summary Report

- Total rounds and per-round fix count/type statistics
- Issues found vs issues fixed (also show issues the user declined)
- Rolled-back issues and reasons
- Final test result
- **PR mode**: list review comments submitted
- **Local mode with associated PR**: note which issues originated from PR comments

Delete pending files after report.

---

## Troubleshooting

### PR URL does not match current repository
The PR belongs to a different repository. Run `/cr` from the correct repo.

### gh CLI not installed or not authenticated
PR mode requires `gh`. Install with `brew install gh` (macOS) or see
https://cli.github.com, then run `gh auth login`.

### Build baseline fails
The project does not compile or tests fail before any changes are made. Fix the
existing build issues first, then re-run `/cr`.

### No upstream branch configured
When running `/cr` without arguments, the current branch has no upstream tracking
branch. Either push the branch first (`git push -u origin branch-name`) or specify
a scope explicitly (e.g., `/cr main..HEAD`).

### Uncommitted changes detected
Local mode requires a clean working tree. Commit or stash changes before running `/cr`.
