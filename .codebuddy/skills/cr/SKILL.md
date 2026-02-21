---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix.
---

# Review — Automated Code Review & Fix

Use `/cr` to start. Accepts a PR number/URL, commit (range), file/directory paths,
or no argument (current branch vs upstream). See Phase 0.1 for full argument parsing.

Reviews code and documents using Agent Teams. Each issue gets a risk level — low-risk
issues are auto-fixed, higher-risk issues are presented for user confirmation. For
other people's PRs, issues are submitted as line-level PR review comments instead of
direct commits. Runs multi-round iterations until no valid issues remain.

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

**Parse order**: contains `..` -> commit range; single arg + `gh pr view` succeeds ->
PR; all args exist on disk -> file/directory paths; single arg + `git rev-parse`
succeeds -> single commit; otherwise -> error.

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
   gh pr view {number} --json headRefName,baseRefName,author,headRefOid,files
   ```
   Extract: `PR_BRANCH`, `BASE_BRANCH`, `PR_AUTHOR`, `HEAD_SHA`, file list.

3. **Determine code ownership**: compare `PR_AUTHOR` with `gh api user -q '.login'`.
   Store result as `IS_OWN_PR` (true/false).

**Local mode — lightweight file list**: for the purpose of 0.2, quickly obtain the
file list without full diff content (e.g., `git diff --name-only` for branch diff,
`git show --name-only` for single commit, or the paths themselves for file/directory).

### 0.2 User Questions

Ask all user-facing questions **immediately after mode detection**, before any heavy
operations (worktree, diff, build). Use the lightweight file list from 0.1 to determine
whether to show Q1.

#### Question 1 — Review priority

Scan the file list from 0.1 to determine if the scope contains **only** document files.
If so, skip this question (doc modules use their full checklist automatically). When
skipped, all priority levels (A+B+C) are checked.

(code/mixed modules):
- Option 1 — "Full review (A + B + C)": correctness, refactoring, and conventions.
  e.g., null checks, duplicate code extraction, naming style, comment quality.
- Option 2 — "Correctness + optimization (A + B)": skip conventions and documentation.
  e.g., null checks, resource leaks, duplicate code, unnecessary copies.
- Option 3 — "Correctness only (A)": only safety and correctness issues.
  e.g., null dereference, out-of-bounds, resource leaks, race conditions.

#### Question 2 — Auto-fix threshold

**Own code** (local mode, or PR mode with `IS_OWN_PR = true`):
- Option 1 — "Low + Medium risk": auto-fix clear fixes including multi-location changes
  (extract shared logic, remove unused internals). Only confirm high-risk decisions
  (API changes, architecture, algorithm trade-offs).
- Option 2 — "Low risk only": auto-fix only unambiguous local fixes (null checks,
  comment typos, naming, `reserve`). Confirm everything else.
- Option 3 — "All confirm": no auto-fix, review every issue before fixing.

**Other's PR** (`IS_OWN_PR = false`):
Threshold is automatically set to **all confirm** with PR comment output. Inform the
user: "This is someone else's PR. Issues will be presented for you to select which ones
to submit as PR review comments."

### 0.3 Pre-flight Checks

All checks that might require user action are performed here, before the automated
process begins. Complete all checks before proceeding to module partitioning.

**Agent Teams availability**:
- Verify Agent Teams is enabled. If not available, prompt the user to enable it and
  abort.

**Clean branch check**:
- **PR mode (will use worktree)**: skip — the worktree created in 0.4 will be clean.
- **PR mode (current branch is PR branch)**: verify no uncommitted changes. If dirty,
  abort and ask the user to resolve first.
- **Local mode**: verify not on the main/master branch (abort if so). If there are
  uncommitted changes, abort and ask the user to commit or stash first (fixes may be
  committed even in all-confirm mode after user approval in Phase 7).

**Environment verification** (skip for other's PR or doc-only scope):
- Detect build and test commands from project rules or by exploring the codebase.
- If no automated tests are found, warn the user: without tests, fix validation cannot
  run. Abort unless the user confirms to continue.
- Actual build + test baseline runs after 0.4 (scope preparation), in the correct
  working directory.

**Reference material** (doc/mixed modules only):
- Auto-discover relevant best-practice guides, official documentation, or specification
  standards for the document type being reviewed.
- Ask the user if they have additional reference material (file paths, URLs, or inline
  instructions). The user may skip.

After all checks pass, no further user interaction until Phase 7 (final report).

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

2. **Fetch existing PR comments** for reviewer context:
   ```
   gh pr view {number} --comments
   ```
   Include in reviewer prompts so they avoid re-reporting already-discussed issues.

3. **Set review scope**: diff against the PR's base branch (`BASE_BRANCH` from 0.1).
   ```
   git fetch origin {BASE_BRANCH}
   git diff $(git merge-base origin/{BASE_BRANCH} HEAD)
   ```

**Local mode**: fetch the full diff content for the scope determined in 0.1 (branch
diff, `git show`, commit range diff, or read file contents for full-content review).

**Build + test baseline** (skip for other's PR or doc-only scope):
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

- Create the team.
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
  - **PR context** (PR mode only): include existing PR comments so reviewers avoid
    re-reporting already-discussed issues
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
- **Other's PR**: since all issues are deferred, skip Phase 4-6 entirely after Phase 3
  and go directly to Phase 7.

### Key points

- For each fixable issue, check whether the change also requires updates to comments
  or documentation outside the fixer's module — if so, create a follow-up for
  `fixer-cross`.
- Previously rolled-back issues -> do not attempt again this round

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

- For code/mixed modules: build + test using the project's commands
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
   - **Local mode / own PR**: create fixer agents for selected issues -> fix -> validate
     (same as Phase 4-5). Record any failures to `pending-issues.md`.
   - **Other's PR**: user selects which to submit as PR review comments using the
     format in `references/pr-comment-format.md`. Comment body should be concise,
     written in the user's conversation language, with a specific fix suggestion.

### Step 2: Pending item confirmation

1. Check `pending-issues.md` (issues that failed auto-fix or deferred-fix)
2. Empty -> proceed to step 3
3. Has content -> present to user for item-by-item confirmation
4. Approved fixes -> create agent to re-apply; rejected -> discard
5. Final build + test (skip for doc-only modules)

### Step 3: PR mode — push fixes (own PR only)

If fixes were made in PR mode and `IS_OWN_PR = true`, push all commits to the PR
branch. If push fails, inform the user — commits remain local for manual push.

### Step 4: PR mode — worktree cleanup

If `WORKTREE_DIR` was created, clean up after push is verified:
```
cd {original_directory}
git worktree remove {WORKTREE_DIR}
git branch -D pr-{number}
```

### Step 5: Summary Report

- Total rounds and per-round fix count/type statistics
- Issues found vs issues fixed (also show issues the user declined)
- Rolled-back issues and reasons
- Pending items and their resolution
- Final test result
- **PR mode**: list commits pushed (own PR) or review comments submitted (other's PR)

Delete pending files after report.

---

## Troubleshooting

### PR URL does not match current repository
The PR belongs to a different repository. Run `/cr` from the correct repo.

### gh CLI not installed or not authenticated
PR mode requires `gh`. Install with `brew install gh` (macOS) or see
https://cli.github.com, then run `gh auth login`.
