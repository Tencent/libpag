---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# Review — Automated Code Review & Fix

Use `/cr` to start. Accepts a PR number/URL, commit (range), file/directory paths,
or no argument (current branch vs upstream). See Phase 1 for full argument parsing.

Automated code review with multi-round iteration. Each issue gets a risk level —
low-risk issues are auto-fixed, higher-risk issues are presented for user confirmation.
In PR mode, issues are submitted as line-level PR review comments instead of direct
commits.

## Flow

```
Main path: Phase 0 → 1 → [Round: 2 → 3 → 4 → 5 → 6] → 7 → 8 (exit)
```

**Phase 3 routing (PR mode)**:
- All issues → `PENDING_FILE`, skip Phase 4-6 → Phase 7 (if PENDING) or Phase 8

**Phase 3 routing (local mode)**:
- Auto-fix queue not empty → Phase 4
- Auto-fix queue empty → Phase 6 (which routes to Phase 7 or 8)

**Phase 6 routing (single authority for loop/exit)**:
- Commits produced, round < 100 → Phase 2 (new round)
- No commits, `PENDING_FILE` not empty → Phase 7
- No commits, `PENDING_FILE` empty → Phase 8
- Max 100 rounds reached → Phase 7 (if `PENDING_FILE` not empty) or Phase 8

**Phase 7 routing**:
- User approves fix(es) → Phase 4 (then normal 4 → 5 → 6 routing)
- User skips all → Phase 8

## Roles

Four roles participate in the review-fix loop:

| Role | Responsibility |
|------|---------------|
| **Coordinator** | You — orchestrate the flow, make value/risk decisions, route between phases |
| **Reviewer** | Find issues in the code (one per module, can run in parallel) |
| **Verifier** | Challenge each issue — is it real? (one per issue, can run in parallel) |
| **Fixer** | Apply approved fixes (one per fix or file group, can run in parallel) |

The coordinator is the only role that persists across the entire session. It never
modifies files directly — all file changes are delegated to fixers.

### Role isolation

Reviewer, verifier, and fixer roles should each operate with **isolated context** —
they should not see each other's work or share conversation history. This prevents
cross-contamination of judgments (e.g., a verifier biased by seeing the reviewer's
reasoning process, or a new round's reviewer anchored by previous findings).

**Recommended implementation** (in order of preference):
1. **Sub-agents / child tasks**: launch each role as an independent sub-agent with its
   own context. Multiple sub-agents can run in parallel for maximum throughput.
2. **Team agents**: use a team/swarm system where each role is a separate team member.
3. **Serial role-play**: if neither sub-agents nor teams are available, the coordinator
   performs each role sequentially, switching prompts between roles. This is the least
   effective option — context leakage is unavoidable — but the flow still works.

## Instructions

**Delegation**: You (the coordinator) never modify files directly. Delegate all
file changes to the fixer role. Read code only for arbitration and diagnosis.

**Autonomy**: Between Phase 0 (user confirmation) and Phase 7 (remaining issue
confirmation), the review-fix loop runs without user interaction unless a previously
failed issue fails again in Phase 5 (which prompts inline). Issues above the auto-fix
threshold and failed fixes are recorded to `PENDING_FILE` across rounds and presented
to the user in Phase 7.

**Parallelism**: Run reviewer, verifier, and fixer tasks in parallel whenever possible.
Reviewers for different modules, verifiers for different issues, and fixers for
different files are all independent and can run concurrently.

**Error handling**: Handle all unexpected situations autonomously — role execution
timeout or failure, fix breaks the build, git operation fails, invalid output. Use
your judgment: retry, skip and move on, etc. Anything that cannot be resolved
automatically should be recorded to `PENDING_FILE` for user review in Phase 7.

**User interaction**: All user-facing text must use the language the user has been
using. When presenting choices with predefined options, use interactive dialogs with
selectable options rather than plain text. The user may send additional material
(files, URLs, context) at any time; include it in the current or next round's context.

## References

| Topic | File |
|-------|------|
| Code review checklist (Priority A-C + exclusions) | `references/code-checklist.md` |
| Document review checklist (Priority A-C + exclusions) | `references/doc-checklist.md` |
| Issue filtering & risk level judgment matrix | `references/judgment-matrix.md` |
| Pending issue template | `references/pending-templates.md` |
| Fixer agent instructions | `references/fixer-instructions.md` |
| Verifier prompt | `references/verifier-prompt.md` |
| PR review comment format | `references/pr-comment-format.md` |
| User questions (Phase 0 options) | `references/user-questions.md` |
| Scope preparation commands (Phase 1.2) | `references/scope-preparation.md` |

---

## Phase 0: User Confirmation

### 0.1 Mode detection (pure string parsing, no tools)

Parse `$ARGUMENTS` to determine the review mode:
- Purely numeric (e.g., `123`) -> **PR mode** (PR number)
- URL containing a PR path (e.g., `github.com/.../pull/123`) -> **PR mode**
- Everything else (empty, commit-like, file/directory paths, `..` range) -> **Local mode**

### 0.2 Ask questions

**STOP. Do NOT call any tools (Bash, Read, Glob, Grep, Task, etc.) before receiving
user answers.** The questions below do not depend on any repository state.

Present all applicable questions in **one AskUserQuestion call**. See
`references/user-questions.md` for the full question definitions and option details.

- **Question 1 — Review priority** (always show): A+B+C / A+B / A only
- **Question 2 — Auto-fix threshold** (skip in PR mode): Low+Medium / Low only /
  All confirm / Full auto

After all questions are answered, no further user interaction until Phase 7 (except
when a previously failed issue fails again in Phase 5, which prompts the user inline).

---

## Phase 1: Scope & Environment

**Prerequisite**: Phase 0 questions have been answered. If not, STOP and return to
Phase 0.2.

### 1.1 Pre-flight Checks

Automated checks — no user interaction.

**Initialize `PENDING_FILE`**: set path to `/tmp/cr-pending-issues.md`. If the file
already exists (leftover from a concurrent or crashed session), append a numeric
suffix (`-2`, `-3`, …) until an unused name is found. Record the chosen path as
`PENDING_FILE` for all subsequent phases.

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
  committed even in all-confirm mode after user approval in Phase 7).

### 1.2 Scope Preparation

See `references/scope-preparation.md` for detailed commands and argument handling
for both PR mode and local mode.

Key steps:
- **PR mode**: fetch PR metadata, prepare worktree if needed, diff against base
  branch, fetch existing PR comments for de-duplication.
- **Local mode**: validate arguments (empty/commit/range/path), fetch diff. Optionally
  fetch associated PR comments if `gh` is available.

**Empty scope check**: if the diff is empty or the PR has no file changes, inform the
user that there is nothing to review and exit.

**Build + test baseline** (skip for PR mode, doc-only scope, or no build/test commands):
Run build and test in the working directory to establish a passing baseline. Fail = abort.

### 1.3 Module Partitioning

Partition files in scope into **review modules** for parallel review.

- Each module is a self-contained logical unit. Split large files by section/function
  group; group related small files together. Balance workload across reviewers.
- Classify each module as `code`, `doc`, or `mixed` to determine which checklist to use.

The scope (diff content, file list, module partitioning) determined here is **fixed for
all rounds**. Subsequent rounds reuse the same scope without re-running diff or
re-partitioning.

---

## Phase 2: Review

### Round invariants

These apply to every round including the first:

- **Fixed scope**: the diff content, file list, and module partitioning are determined
  once in Phase 1. Do not re-run `git diff` or re-partition modules in any round.
- **Independent review**: reviewers receive no information about previous rounds' fixes,
  issues, or outcomes. Each round is a fresh, unbiased review of the full scope.
- **Cross-round exclusion (coordinator only)**: after collecting reviewer results, the
  coordinator skips issues already recorded in `PENDING_FILE` or rejected by the user
  in a previous Phase 7. Do **not** pass this exclusion list to reviewers — they review
  with a clean slate. Previously fixed issues are **not excluded** — if a reviewer
  independently finds a new problem in code that was fixed last round, that is a valid
  new issue.

### Reviewers

Run one reviewer per module (in parallel when possible). Each reviewer receives:
- The **diff** for its module (to identify what changed) and the **file list** (to
  locate full files). Reviewers must **read entire files** themselves for full context —
  they are not limited to reviewing only the diff lines.
- Checklist matching the module type:
  `references/code-checklist.md` for code modules, `references/doc-checklist.md` for
  doc modules, both for mixed modules. Only include priority levels selected by the
  user (e.g., if user chose "A + B", do not include Priority C items). For doc-only
  modules, always include all priority levels (A+B+C) regardless of user selection.
- **Evidence requirement**: every issue must have a code citation (file:line + snippet)
- **Exclusion list**: see the exclusion section in the corresponding checklist. Project
  rules loaded in context take priority over the exclusion list.
- **Public API protection**: no signature/interface changes unless obvious bug or
  comment issue
- **PR context** (PR mode only): include existing PR review comments so reviewers
  have context on already-discussed topics
- **Self-check**: before submitting results, re-read the relevant code and verify each
  reported issue. Mark each as confirmed or withdrawn. Only submit confirmed issues.
- **Output format**: for each confirmed issue, report:
  `[file:line] [priority A/B/C] — [description] — [code snippet]`

### Verification (pipeline)

Verification runs as a **pipeline** — as soon as any reviewer finishes and submits
its issues, immediately launch verifiers for those issues. Do not wait for all
reviewers to finish. Each issue gets its own independent verifier (can run in parallel
with both other verifiers and still-running reviewers).

**Verifier failure/timeout**: if a verifier fails to return a result (timeout, error,
invalid output), the coordinator reads the cited code and makes the existence judgment
directly — treat it as if the verifier were absent for that issue.

Each verifier receives the issue description and cited code. See
`references/verifier-prompt.md` for the full verifier prompt.

---

## Phase 3: Filter & Risk Assessment — Coordinator Only

Three steps, applied to every issue in order:

### 3.0 De-duplication (before verification results)

Run de-duplication **before** consuming verifier results to avoid wasting effort on
known issues:

- Skip any issue that matches one recorded in `PENDING_FILE` or **rejected by the
  user** in a previous Phase 7. Previously fixed issues are not excluded — if a
  reviewer reports a new problem in already-fixed code, it is treated as a new issue.
- **PR comment de-duplication** (PR mode): compare each confirmed issue against
  `EXISTING_PR_COMMENTS`. If an issue matches an existing comment (same file, same
  general location, same topic), exclude it — do not present it to the user.
- **Associated PR comment verification** (Local mode with associated PR, **first
  round only**): for each PR review comment retrieved in 1.2, verify against the
  current code whether the issue still exists. Verified issues are added to the fix
  queue as additional known issues (using the same risk assessment flow). Skip this
  step in subsequent rounds — these comments are only processed once.

### 3.1 Existence check — is the issue real?

Use the verifier's verdict as the primary signal:

- **Verifier CONFIRM**: coordinator does a quick plausibility check — verify that the
  reviewer's description is consistent with the cited code snippet and that the
  conclusion logically follows. If anything looks off, read the full code to confirm.
  Otherwise accept and proceed to 3.2.
- **Verifier REJECT (HIGH confidence)**: drop the issue — do not fix or record.
- **Verifier REJECT (MEDIUM/LOW confidence)** or **reviewer–verifier disagree**:
  coordinator must read the cited code **independently** and form its own judgment.
  Do not default to either side — the reviewer's report and the verifier's rebuttal
  are equally weighted inputs. Read the code, understand the actual behavior, then
  decide based on what the code does, not on which argument sounds more persuasive.

The verifier only judges whether the issue exists. It does not assess value or risk.

### 3.2 Value & risk assessment — should we fix it, and how risky is it?

This is exclusively the coordinator's responsibility. For each issue that passes 3.1,
consult `references/judgment-matrix.md` for the full matrix, risk level definitions,
and special rules. Determine:
- Whether the issue is worth fixing (judgment matrix criteria)
- Risk level (low / medium / high)
- Handling based on the user's auto-fix threshold

### Routing by risk level

The user's chosen auto-fix threshold determines handling for each fixable issue:
- Issues at or below the threshold -> queued for auto-fix (Phase 4)
- Issues above the threshold -> recorded to `PENDING_FILE` with the reason
  (e.g., "above auto-fix threshold"), presented to the user in Phase 7

### Mode-specific routing

- **PR mode**: all issues are recorded to `PENDING_FILE`. Skip Phase 4-6.
  If `PENDING_FILE` is empty -> Phase 8; otherwise -> Phase 7.
- **Local mode**: if no auto-fix issues remain, skip Phase 4-5 and go to Phase 6
  (which routes to Phase 7 or Phase 8 based on `PENDING_FILE` state).

### Additional checks

- For each fixable issue, check whether the change also requires updates to comments
  or documentation outside the fixer's module — if so, create a cross-module follow-up
  fix task.
- Previously rolled-back issues -> do not attempt again this round

---

## Phase 4: Fix

If the auto-fix queue is empty (all issues were recorded to `PENDING_FILE`),
skip Phase 4-5 and go directly to Phase 6.

### Fixer assignment

Run one fixer per fix or file group (in parallel when possible). Each fixer receives:
- The issue description and cited code
- The full content of the file(s) to modify
- `references/fixer-instructions.md` verbatim

Each fixer commits **per issue** (one commit per fix, immediately after applying it).
This enables fine-grained bisection and revert in Phase 5.

Assignment strategy:
- Group issues by file to minimize concurrent edit conflicts
- Cross-file issues or multi-file renames -> single atomic task to one fixer
- Multiple fixers can run in parallel as long as they work on different files

Coordinator collects skipped issues and records them to `PENDING_FILE`.

---

## Phase 5: Validate

- For code/mixed modules: build + test using the project's commands. If no build/test
  commands are available (warned in 1.1), skip validation entirely.
- For doc-only modules: skip build/test; validation is done through review phases

**All pass** (or no code modules to validate) -> Phase 6.

**Commit counting**: only commits that survive validation count as "commits produced"
for Phase 6 routing. Reverted commits do not count.

**Failures**: record the HEAD before Phase 4 starts as the "last known good" commit.
Identify the failing commit (bisect against the last-known-good if multiple commits,
direct revert if only one), revert it, and send failure info to a new fixer for retry
(max 2 retries). If still failing, revert and record to `PENDING_FILE`. If the issue was
already in `PENDING_FILE` (a retry from Phase 7), revert and ask the user: show the
failure details and offer options — provide additional context or direction for another
attempt, or skip (default). Skipped issues are added to the rejected list so they
won't be reported again in subsequent rounds.

---

## Phase 6: Decide

This phase is the **single routing authority** for the review-fix loop.

Count the commits produced during this round (Phase 4 → 5).

- **Commits produced AND round count < 100**:
  -> Back to **Phase 2** for a new round (fresh review).
- **No commits produced AND `PENDING_FILE` has entries**:
  -> **Phase 7** (Confirm).
- **No commits produced AND `PENDING_FILE` is empty**:
  -> **Phase 8** (Report & Exit).
- **Max 100 rounds reached**:
  -> **Phase 7** if `PENDING_FILE` has entries, otherwise **Phase 8**.

---

## Phase 7: Remaining Issue Confirmation

Collect all issues from `PENDING_FILE` that were not auto-fixed during the loop.
Each issue has a reason explaining why it was deferred (e.g., above threshold, fix
failed, rolled back). Deduplicate and present to the user. If the file is empty,
skip to Phase 8.

1. Present issues in a compact numbered list. Each entry should fit on one line:
   `[number] [file:line] [risk] [reason] — [description]`

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
     user's conversation language, with a specific fix suggestion. -> Phase 8
   - **Local mode**: if no issues were approved for fix (user skipped all) -> Phase 8.
     Otherwise -> **Phase 4** (Fix) with the user-approved issues as the fix queue.
     These issues were already verified in a previous Phase 3 — skip directly to fix.
     From there the normal flow resumes: Phase 4 -> 5 -> 6, and Phase 6 routes as
     usual (new round if commits were produced, Phase 7 again if new pending issues
     accumulated, or Phase 8 if done).

---

## Phase 8: Report & Exit

### Summary Report

- Total rounds and per-round fix count/type statistics
- Issues found vs issues fixed (also show issues the user declined or skipped)
- Rolled-back issues and reasons
- Issues that failed fix repeatedly and were skipped
- Final test result
- **PR mode**: list review comments submitted
- **Local mode with associated PR**: note which issues originated from PR comments

### Cleanup

**Worktree cleanup (PR mode only)**:
If `WORKTREE_DIR` was created, clean up:
```
cd {original_directory}
git worktree remove {WORKTREE_DIR}
git branch -D pr-{number}
```

Delete `PENDING_FILE` from the temporary directory.

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
