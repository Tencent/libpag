---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix.
---

# Review — Automated Code Review & Fix

Use `/cr` to start. Accepts a PR number/URL, commit (range), file/directory paths,
or no argument (current branch vs upstream). See Phase 0.1 for full argument parsing.

Reviews code and documents using Agent Teams. Each issue gets a risk level — low-risk
issues are auto-fixed, higher-risk issues are presented for user confirmation. In PR
mode, issues are submitted as line-level PR review comments instead of direct commits.
Runs multi-round iterations until no valid issues remain.

## Instructions

- All user-facing interactions must use the language the user has been using in the
  conversation. Do not default to English.
- When presenting choices with predefined options, use interactive dialogs with
  selectable options rather than plain text.
- **You (the team-lead) never modify files directly.** Delegate all changes to agents.
  Read code only for arbitration and diagnosis.
- After each round, analyze review quality and adjust the next round's reviewer prompts.
- **Autonomous operation**: between Phase 0 (setup) and Phase 7 (final report), the
  entire review-fix loop runs without user interaction. Deferred issues accumulate
  across rounds and are only presented to the user in Phase 7. The team-lead must
  handle all unexpected situations autonomously — e.g., an agent stuck or unresponsive,
  a fix that breaks the build, a git operation that fails, an agent producing invalid
  output. Use your judgment: terminate and replace, revert and retry, skip and move on,
  etc. Anything that cannot be resolved automatically should be recorded to
  `pending-issues.md` for user review in Phase 7.

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

## Phase 0: Scope Confirmation & Environment Check

### 0.1 Argument Parsing & Mode Detection

Parse `$ARGUMENTS` to determine the review mode:

| `$ARGUMENTS` | Detection | Mode | Scope |
|--------------|-----------|------|-------|
| (empty) | — | **Local** | Current branch vs upstream (diff) |
| PR number (e.g., `123`) | `gh pr view` succeeds | **PR** | PR diff vs base branch |
| PR URL | Extract owner/repo/number, verify | **PR** | PR diff vs base branch |
| Single commit (e.g., `abc123`) | `git rev-parse --verify` succeeds | **Local** | That commit's changes (`git show`) |
| Commit range (e.g., `abc..def`) | Contains `..`, both endpoints valid | **Local** | Diff between two commits (excluding first) |
| File/directory paths (space-separated) | All paths exist on disk | **Local** | Full content review (no diff) |

**Parse order**: contains `..` -> commit range; looks like a URL (contains `/`) ->
PR URL; single arg + purely numeric -> PR number (`gh pr view`); all args exist on
disk -> file/directory paths; single arg + `git rev-parse` succeeds -> single commit;
otherwise -> error.

**PR URL mismatch**: if the URL's owner/repo does not match the current repository
(`gh repo view --json nameWithOwner -q '.nameWithOwner'`), abort and ask the user to
run `/cr` from the correct repository.

**Empty arguments (default)**: use the current branch's upstream tracking branch as the
base. If no upstream is configured, fall back to `main` (or `master`).

**PR mode — lightweight metadata** (do not create worktree or fetch diff yet):

1. **Verify `gh` is available**: run `gh --version`. If not installed, inform the user
   (macOS: `brew install gh`, others: https://cli.github.com) and abort.

2. **Fetch PR metadata** (single API call):
   ```
   gh pr view {number} --json headRefName,baseRefName,headRefOid,state
   ```
   Extract: `PR_BRANCH`, `BASE_BRANCH`, `HEAD_SHA`, `STATE`.
   If `STATE` is not `OPEN`, inform the user that the PR is already closed/merged and
   exit.

**Local mode**: no exploration needed at this point. Proceed directly to 0.2.

### 0.2 User Questions

Ask all user-facing questions **immediately after mode detection**, before any heavy
operations (worktree, diff, build, codebase exploration). Present all applicable
questions together in one interaction. Do not read code or explore the codebase
before asking.

#### Question 1 — Review priority

Always show this question. When the scope turns out to be doc-only (determined later
in 0.5 module partitioning), all priority levels (A+B+C) are used regardless of the
user's choice here.

(code/mixed modules):
- Option 1 — "Full review (A + B + C)": correctness, refactoring, and conventions.
  e.g., null checks, duplicate code extraction, naming style, comment quality.
- Option 2 — "Correctness + optimization (A + B)": skip conventions and documentation.
  e.g., null checks, resource leaks, duplicate code, unnecessary copies.
- Option 3 — "Correctness only (A)": only safety and correctness issues.
  e.g., null dereference, out-of-bounds, resource leaks, race conditions.

#### Question 2 — Auto-fix threshold (Local mode only)

Skip in PR mode (PR mode always uses all-confirm with comment output). Inform the
user: "PR mode: issues will be presented for you to select which ones to submit as
PR review comments."

- Option 1 — "Low + Medium risk": auto-fix clear fixes including multi-location changes
  (extract shared logic, remove unused internals). Only confirm high-risk decisions
  (API changes, architecture, algorithm trade-offs).
- Option 2 — "Low risk only": auto-fix only unambiguous local fixes (null checks,
  comment typos, naming, `reserve`). Confirm everything else.
- Option 3 — "All confirm": no auto-fix, review every issue before fixing.

After all questions are answered, no further user interaction until Phase 7.

### 0.3 Pre-flight Checks

Automated checks only — no user interaction.

**Agent Teams availability**:
- Check if Agent Teams is enabled. If not available, warn the user in the conversation
  that review will run in single-agent serial mode (one module at a time) instead of
  parallel. Continue without aborting.

**Test environment**:
- Skip if PR mode or doc-only scope. If no build/test commands can be determined
  from context or the codebase, warn the user in the conversation that fix validation
  will be skipped (fixes are still applied but not automatically verified). Continue
  without aborting.

**Clean branch check**:
- **PR mode (will use worktree)**: skip — the worktree created in 0.4 will be clean.
- **PR mode (current branch is PR branch)**: skip — PR mode is read-only (comment
  output only), no local modifications will be made.
- **Local mode**: verify not on the main/master branch (abort if so). If there are
  uncommitted changes, abort and ask the user to commit or stash first (fixes may be
  committed even in all-confirm mode after user approval in Phase 7).

### 0.4 Scope Preparation

Perform the heavy operations that were deferred from 0.1.

**PR mode — working directory & diff**:

1. **Prepare working directory**:
   - If current branch equals `PR_BRANCH` -> use current directory directly.
   - Otherwise -> create a worktree:
     ```
     git fetch origin pull/{number}/head:pr-{number}
     git worktree add /tmp/pr-review-{number} pr-{number}
     ```
     All subsequent operations use the worktree directory. Record `WORKTREE_DIR` for
     cleanup in Phase 7.

2. **Set review scope**: diff against the PR's base branch (`BASE_BRANCH` from 0.1).
   ```
   git fetch origin {BASE_BRANCH}
   git diff $(git merge-base origin/{BASE_BRANCH} HEAD)
   ```

3. **Fetch existing PR review comments** for de-duplication in Phase 3:
   ```
   gh api repos/{owner}/{repo}/pulls/{number}/comments
   ```
   Store as `EXISTING_PR_COMMENTS` for later comparison.

**Local mode**: fetch the full diff content for the scope determined in 0.1 (branch
diff, `git show`, commit range diff, or read file contents for full-content review).

**Local mode — associated PR comments** (optional, best-effort):
If `gh` is available, check whether the current branch has an open PR:
```
gh pr view --json number,headRefOid,comments 2>/dev/null
```
If an open PR exists, store its review comments. These are used in Phase 3 as
reference information — each comment is verified against the actual code to confirm
the issue still exists before being treated as a known issue to fix.

**Empty scope check**: if the diff is empty or the PR has no file changes, inform the
user that there is nothing to review and exit.

**Build + test baseline** (skip for PR mode, doc-only scope, or no build/test commands):
Run build and test in the working directory to establish a passing baseline. Fail = abort.

### 0.5 Module Partitioning

Partition files in scope into **review modules** for parallel review.

- Each module is a self-contained logical unit. Split large files by section/function
  group; group related small files together. Balance workload across reviewers.
- Classify each module as `code`, `doc`, or `mixed` to determine which checklist to use.

**Fix modules** are determined in Phase 3: group confirmed issues **by file** (one
fixer per file to prevent concurrent edit conflicts). Cross-file issues go to a
dedicated `fixer-cross` agent.

### Mid-Review Supplements

The user may send additional material at any time. If reviewers are still active
(Phase 1-2), forward it directly; otherwise include the material in the next round.

---

## Phase 1: Review

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
  - Structured output format

---

## Phase 2: Verification

- **Reviewer self-check + independent verification** (run in parallel):
  - Each reviewer re-reads the relevant code and self-verifies every issue they
    reported, marking each as confirmed or withdrawn.
  - Simultaneously, create a verifier (`verifier-N`) per review module. The verifier
    receives the reviewer's issue list and reads actual code to independently confirm
    each issue.
- **Alignment**: send verifier results back to the reviewer for comparison with their
  self-check. If they disagree on any issue, team-lead reads code to judge.
- After alignment is complete, close all reviewers and verifiers.

---

## Phase 3: Filter & Risk Assessment — Team-Lead Only

For each confirmed issue, decide whether to fix and assign a risk level. Consult
`references/judgment-matrix.md` for the full matrix, risk level definitions, and
special rules.

### Risk level assignment

Assign each fixable issue a risk level (low / medium / high) based on the judgment
matrix. The user's chosen auto-fix threshold determines handling:
- Issues at or below the threshold -> queued for auto-fix (Phase 4)
- Issues above the threshold -> added to the **deferred issue list** (accumulated
  across all rounds, presented to the user only in Phase 7)
- **PR mode**: all issues are deferred (all-confirm). Skip Phase 4-6 entirely after
  Phase 3 and go directly to Phase 7.

### Key points

- **De-duplication**: skip any issue that matches one already fixed, rolled back, recorded
  to pending, or deferred in a previous round.
- **PR comment de-duplication** (PR mode): compare each confirmed issue against
  `EXISTING_PR_COMMENTS`. If an issue matches an existing comment (same file, same
  general location, same topic), exclude it from the deferred list — do not present
  it to the user.
- **Associated PR comment verification** (Local mode with associated PR): for each
  PR review comment retrieved in 0.4, verify against the current code whether the
  issue still exists. Verified issues are added to the fix queue as additional known
  issues (using the same risk assessment flow).
- For each fixable issue, check whether the change also requires updates to comments
  or documentation outside the fixer's module — if so, create a follow-up for
  `fixer-cross`.
- Previously rolled-back issues -> do not attempt again this round
- If no confirmed issues remain after filtering (all withdrawn, filtered, or
  de-duplicated), skip Phase 4-6 and proceed directly to Phase 7.

---

## Phase 4: Fix

If the auto-fix queue is empty (all issues were deferred), skip Phase 4-5 and go
directly to Phase 6.

- Group confirmed issues into **fix modules by file** (see 0.5).
- Cross-file issues -> `fixer-cross` agent; multi-file renames -> single atomic task

### Fixer Instructions (include in every fixer prompt)

Include `references/fixer-instructions.md` verbatim in every fixer agent's prompt.
Team-lead collects skipped issues and includes them in the next round's context.

---

## Phase 5: Validate

- For code/mixed modules: build + test using the project's commands. If no build/test
  commands are available (warned in 0.3), skip validation entirely.
- For doc-only modules: skip build/test; validation is done through review phases

**All pass** (or no code modules to validate) -> close all fixers -> Phase 6.

**Failures**: bisect to locate the bad commit, revert it, and send failure info back
to the original fixer for retry (max 2 retries). If still failing, revert and record
to `pending-issues.md`. Close all fixers when resolved.

---

## Phase 6: Loop

- Close the team to release all agents.

### Termination check

- If no auto-fix issues were found this round, or all auto-fix issues were skipped
  by fixers (no actual commits produced) -> Phase 7
- Otherwise -> create new team, back to Phase 1

### Next round context

Next round prompt includes: rollback blacklist, previous fix summary,
pending file contents, and prompt adjustments based on review quality analysis.
Reviewers must skip all issues already reported in previous rounds — whether fixed,
rolled back, recorded to pending files, or deferred for user confirmation.

---

## Phase 7: Final Report & Cleanup

### Step 1: Deferred issue confirmation

Present all accumulated deferred issues across all rounds. If the list is empty, skip
to step 2.

1. Present issues in a compact numbered list. Each entry should fit on one line:
   `[number] [file:line] [risk] — [description]`

2. Interaction depends on issue count:

   **5 or fewer issues**: present each issue individually with selectable options.
   First option = fix (or submit as PR comment), second option = skip.

   **More than 5 issues**: ask the user for a bulk action:
   - Option 1 — "Fix all": act on every deferred issue
   - Option 2 — "Skip all": discard all deferred issues
   - Option 3 — "Select individually": present each issue one by one (same as above)

3. **Action depends on mode**:
   - **Local mode**: create a temporary team with fixer agents for selected issues ->
     fix -> validate (same as Phase 4-5). Record any failures to `pending-issues.md`.
   - **PR mode**: submit selected issues as PR review comments using the format in
     `references/pr-comment-format.md`. Comment body should be concise, written in the
     user's conversation language, with a specific fix suggestion.

### Step 2: Pending item confirmation

1. Check `pending-issues.md` (issues that failed auto-fix or deferred-fix)
2. Empty -> proceed to step 3
3. Has content -> present to user for item-by-item confirmation
4. Approved fixes -> create a temporary team with fixer agents to re-apply (same
   fix-validate cycle as Phase 4-5, including retry on failure); rejected -> discard
5. Final build + test (skip for doc-only modules)

### Step 3: PR mode — worktree cleanup

If `WORKTREE_DIR` was created, clean up:
```
cd {original_directory}
git worktree remove {WORKTREE_DIR}
git branch -D pr-{number}
```

### Step 4: Summary Report

- Total rounds and per-round fix count/type statistics
- Issues found vs issues fixed (also show issues the user declined)
- Rolled-back issues and reasons
- Pending items and their resolution
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
