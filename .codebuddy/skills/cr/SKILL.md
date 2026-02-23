---
name: cr
description: Automated code review and fix for local branches, PRs, commits, and files. Supports multi-round iteration with risk-based auto-fix. Use when the user says "review", "code review", "/cr", "review this PR", "check my code", or "review my changes".
---

# Review — Automated Code Review & Fix

Use `/cr` to start. Accepts a PR number/URL, commit (range), file/directory paths,
or no argument (current branch vs upstream). See Phase 1 for full argument parsing.

Automated code review with multi-round iteration. Each issue gets a risk level —
issues within the user's chosen auto-fix threshold are fixed automatically, the rest
are presented for user confirmation. In PR mode, issues are submitted as line-level
PR review comments instead of direct commits.

## Roles

Four roles participate in the review-fix loop:

| Role | Stance | Goal |
|------|--------|------|
| **Coordinator** | **Neutral** — trust no single party | Orchestrate flow, arbitrate disputes, ensure all valuable issues get fixed |
| **Reviewer** | **Thorough** — self-verify, then maximize coverage | Discover as many real issues as possible |
| **Verifier** | **Adversarial** — default to doubting the reviewer | Challenge every issue; reject with real evidence, confirm if it holds up |
| **Fixer** | **Precise** — fix thoroughly, do not expand scope | Apply each approved fix completely and correctly |

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

**Autonomy**: Between Ask (Phase 0) and Confirm (Phase 7), the review-fix loop runs
without user interaction unless a previously failed issue fails again in Validate
(Phase 5, which prompts inline). Issues above the auto-fix threshold and failed fixes
are recorded to the Issues section of `CR_STATE_FILE` across rounds and presented to
the user in Confirm.

**Parallelism**: Run reviewer, verifier, and fixer tasks in parallel whenever possible.
Reviewers for different modules, verifiers for different issues, and fixers for
different files are all independent and can run concurrently.

**Error handling**: Handle all unexpected situations autonomously — role execution
timeout or failure, fix breaks the build, git operation fails, invalid output. Use
your judgment: retry, skip and move on, etc. Anything that cannot be resolved
automatically should be recorded to `CR_STATE_FILE` for user review in Confirm.

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

## Flow

```
Ask → Scope → [Round: Review → Filter → Fix → Validate → Continue?] → Confirm → Report
```

The multi-round loop exists primarily to **discover missed issues** — each round is a
fresh review from a new perspective. In local mode, fixes between rounds also allow
detecting newly introduced problems, but that is a secondary benefit.

PR mode and local mode share the same multi-round loop. The only difference is that
PR mode skips Fix and Validate within each round (no code modifications), and outputs
line-level PR comments instead of commits at the end.

---

## Phase 0: Ask

### 0.1 Mode detection (pure string parsing, no tools)

Parse `$ARGUMENTS` to determine the review mode:
- Purely numeric (e.g., `123`) -> **PR mode** (PR number)
- URL containing a PR path (e.g., `github.com/.../pull/123`) -> **PR mode**
- Everything else (empty, commit-like, file/directory paths, `..` range) -> **Local mode**

### 0.2 Ask questions

**STOP. Do NOT call any tools before receiving user answers.** The questions below
do not depend on any repository state.

Present all applicable questions in **a single interactive prompt**. See
`references/user-questions.md` for the full question definitions and option details.

- **Question 1 — Review priority** (always show): A+B+C / A+B / A only
- **Question 2 — Auto-fix threshold** (skip in PR mode): Low+Medium / Low only /
  All confirm / Full auto

**Next**: after all questions are answered, go to Scope. No further user interaction
until Confirm (except when a previously failed issue fails again in Validate, which
prompts the user inline).

---

## Phase 1: Scope

**Prerequisite**: Phase 0 questions have been answered. If not, STOP and return to
Phase 0.2.

### 1.1 Pre-flight Checks

Automated checks — no user interaction.

**Initialize `CR_STATE_FILE`**: create `.cr-cache/` if it does not exist. Derive the
filename from the review scope:
- **PR mode**: `.cr-cache/pr-{number}.md`
- **Local mode**: `.cr-cache/{branch}.md` (sanitize `/` to `-`, e.g.,
  `feature/dom_text_box` → `feature-dom_text_box.md`)

If the file already exists (leftover from a crashed session, or another session is
running on the same scope), ask the user whether to overwrite or abort. Record the
chosen path as `CR_STATE_FILE` for all subsequent phases.

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
  committed even in all-confirm mode after user approval in Confirm).

### 1.2 Scope Preparation

See `references/scope-preparation.md` for detailed commands and argument handling
for both PR mode and local mode.

Key steps:
- **PR mode**: fetch PR metadata, prepare worktree if needed, diff against base
  branch, fetch existing PR comments for de-duplication.
- **Local mode**: validate arguments (empty/commit/range/path), fetch diff. Optionally
  fetch associated PR comments if `gh` is available.

**Recent fix context (local mode only)**: read the git log on the current branch
(since diverging from upstream) to detect cross-session cycles — avoid re-flagging
issues that a previous `/cr` session already fixed. Genuinely problematic prior commits
should still be flagged. This context is for the coordinator only.

**Empty scope check**: if the diff is empty or the PR has no file changes, inform the
user that there is nothing to review and exit.

**Build + test baseline** (skip for PR mode, doc-only scope, or no build/test commands):
Run build and test in the working directory to establish a passing baseline. Fail = abort.

### 1.3 Module Partitioning

Partition files in scope into **review modules** for parallel review.

- Each module is a self-contained logical unit. Split large files by section/function
  group; group related small files together. Balance workload across reviewers.
- Classify each module as `code`, `doc`, or `mixed` to determine which checklist to use.

The scope determined here is persisted in `CR_STATE_FILE` (see 1.4) and reused by
all subsequent rounds.

### 1.4 Persist State

Write `CR_STATE_FILE` (determined in 1.1) with two sections:

**`# Session`** — written once, read every round to restore context:

- **Mode**: PR or Local
- **User choices**: review priority, auto-fix threshold
- **PR metadata** (PR mode): PR number, `HEAD_SHA`, `BASE_BRANCH`, `PR_BRANCH`,
  `WORKTREE_DIR` (if created)
- **Scope**: file list with module assignments and types (code/doc/mixed)
- **Diff summary**: for each file, the range of changed lines (not the full diff —
  reviewers will read files themselves)
- **Build/test commands**: the commands used for baseline validation

**`# Issues`** — updated incrementally as issues are discovered, fixed, or rejected.
See `references/pending-templates.md` for the entry format.

This file is owned by the coordinator. Sub-agents (reviewer, verifier, fixer) never
read or write it directly — the coordinator extracts relevant information and passes
it via task prompts. Update the Issues section whenever issue status changes.

**Next**: enter the review-fix loop starting with Review.

---

## Phase 2: Review

### Round invariants

These apply to every round including the first:

- **Restore state (coordinator only)**: at the start of each round, the coordinator
  reads `CR_STATE_FILE` to restore session context (scope, settings, issue list).
  Reviewers do **not** read this file directly.
- **Known-issue exclusion list**: from the second round onward, the coordinator
  extracts a minimal exclusion list from `CR_STATE_FILE` — each entry contains only
  the file path, line range, and a one-line description. No risk levels, no verifier
  verdicts, no fix outcomes, no rejection reasons. Reviewers use this list solely to
  avoid re-reporting known issues and focus on finding new ones.
- **Cross-round de-duplication (coordinator, Phase 3)**: after collecting reviewer
  results, the coordinator still de-duplicates against `CR_STATE_FILE` as a safety
  net — reviewers may report a known issue in different words. Issues rejected by
  the user in a previous Confirm are also excluded. Previously fixed issues (local
  mode) are **not excluded** — if a reviewer finds a new problem in code that was
  fixed last round, that is a valid new issue.

### Reviewers

Run one reviewer per module (in parallel when possible). Each reviewer receives:
- The **scope summary** for its module: file list with changed line ranges (from
  `CR_STATE_FILE` Session section). Reviewers fetch diffs and read full files
  themselves — the coordinator does **not** pass raw diff content.
- Checklist matching the module type:
  `references/code-checklist.md` for code modules, `references/doc-checklist.md` for
  doc modules, both for mixed modules. Only include priority levels selected by the
  user (e.g., if user chose "A + B", do not include Priority C items).
- **Evidence requirement**: every issue must have a code citation (file:line + snippet)
- **Known-issue exclusion** (from round 2 onward): skip issues matching the exclusion
  list provided by the coordinator. Focus on finding new issues not already covered.
- **Checklist exclusion**: see the exclusion section in the corresponding checklist.
  Project rules loaded in context take priority over the exclusion list.
- **PR context** (PR mode only): include existing PR review comments so reviewers
  have context on already-discussed topics
- **Self-check**: before submitting results, re-read the relevant code and verify each
  reported issue. Mark each as confirmed or withdrawn. Only submit confirmed issues.
- **Output format**: for each confirmed issue, report:
  `[file:line] [priority A/B/C] — [description] — [key lines]`

### Verification (pipeline)

Verification runs as a **pipeline** — as soon as any reviewer finishes and submits
its issues, immediately launch one verifier for that reviewer's batch. Do not wait
for all reviewers to finish. Each verifier runs independently and can work in parallel
with other verifiers and still-running reviewers.

**Verifier failure/timeout**: retry once with a new verifier. If the retry also fails,
skip verification for the affected issues and pass them directly to Filter — the
coordinator treats them as unverified and applies its own judgment in the existence
check (3.1).

Each verifier receives the full batch of issues from one reviewer. See
`references/verifier-prompt.md` for the full verifier prompt.

**Next**: go to Filter.

---

## Phase 3: Filter & Assess (coordinator only)

Three steps, applied to every issue in order:

### 3.0 De-duplication (before verification results)

Run de-duplication **before** consuming verifier results to avoid wasting effort on
known issues:

- Skip any issue that matches one recorded in `CR_STATE_FILE` or **rejected by the
  user** in a previous Confirm. Previously fixed issues are not excluded — if a
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
  conclusion logically follows. If anything looks off, read the relevant code to
  confirm. Otherwise accept and proceed to 3.2.
- **Verifier REJECT (HIGH confidence)**: coordinator does a brief independent check —
  review the verifier's reasoning and the cited code. If the counter-argument is sound,
  drop the issue. If anything is questionable, escalate to full independent review
  (same as MEDIUM/LOW below).
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
- Issues at or below the threshold -> queued for auto-fix (Fix)
- Issues above the threshold -> recorded to `CR_STATE_FILE` with the reason
  (e.g., "above auto-fix threshold"), presented to the user in Confirm

### Additional checks

- For each fixable issue, check whether the change also requires updates to comments
  or documentation outside the fixer's module — if so, create a cross-module follow-up
  fix task.
- Previously rolled-back issues -> do not attempt again this round

**Next**:
- **PR mode**: all confirmed issues are recorded to `CR_STATE_FILE`. Skip Fix and
  Validate, go directly to Continue?.
- **Local mode**: if auto-fix queue is not empty, go to Fix. If auto-fix queue is
  empty, skip Fix and Validate, go directly to Continue?.

---

## Phase 4: Fix

If the auto-fix queue is empty (all issues were recorded to `CR_STATE_FILE`),
skip Fix and Validate and go directly to Continue?.

### Fixer assignment

Run one fixer per fix or file group (in parallel when possible). Each fixer receives:
- The issue description and the file path(s) + line range(s) to modify (fixers read
  file content themselves)
- `references/fixer-instructions.md` verbatim

Each fixer commits **per issue** (one commit per fix, immediately after applying it).
This enables fine-grained bisection and revert in Validate.

Assignment strategy:
- Group issues by file to minimize concurrent edit conflicts
- Cross-file issues or multi-file renames -> single atomic task to one fixer
- Multiple fixers can run in parallel as long as they work on different files

Coordinator collects skipped issues and records them to `CR_STATE_FILE`.

**Next**: go to Validate.

---

## Phase 5: Validate

- For code/mixed modules: build + test using the project's commands. If no build/test
  commands are available (warned in 1.1), skip validation entirely.
- For doc-only modules: skip build/test; validation is done through review phases

**All pass** (or no code modules to validate) -> remove successfully fixed issues from
`CR_STATE_FILE` (they may have been recorded there from a previous Confirm approval),
then proceed to Continue?.

**Commit counting**: only commits that survive validation count as "commits produced"
for Continue? routing. Reverted commits do not count.

**Failures**: record the HEAD **once, before any fixer starts** in Fix, as the
"last known good" commit (this is the baseline for bisection regardless of how many
fixers run in parallel).
Identify the failing commit (bisect against the last-known-good if multiple commits,
direct revert if only one), revert it, and send failure info to a new fixer for retry
(max 2 retries). If still failing, revert and record to `CR_STATE_FILE`. If the issue was
already in `CR_STATE_FILE` (a retry from Confirm), revert and ask the user: show the
failure details and offer options — provide additional context or direction for another
attempt, or skip (default). Skipped issues are added to the rejected list so they
won't be reported again in subsequent rounds.

**Next**: go to Continue?.

---

## Phase 6: Continue?

This phase is the **single routing authority** for the review-fix loop.

Determine whether this round made meaningful progress:
- **New confirmed issues were found this round** (issues that passed Filter existence
  check and were not de-duplicated away — regardless of whether they were auto-fixed
  or recorded to `CR_STATE_FILE`):
  -> Back to **Review** for a new round (fresh review to find further missed issues).
- **No new issues AND `CR_STATE_FILE` has pending entries**:
  -> **Confirm**.
- **No new issues AND `CR_STATE_FILE` has no pending entries**:
  -> **Report**.

**Cycle detection**: before starting a new round, check whether the loop is making
meaningful progress. Force exit to Confirm/Report if any of these apply:
- The same files and issue types keep appearing across consecutive rounds with no net
  reduction in issues.
- Multiple consecutive rounds produce only reverted commits (all fixes fail validation)
  and no new issues are found beyond those already in `CR_STATE_FILE`.

When forcing exit, go to Confirm if `CR_STATE_FILE` has pending entries, otherwise Report.

---

## Phase 7: Confirm

Collect all issues from `CR_STATE_FILE` that have not been resolved during the loop
(in PR mode: all confirmed issues across rounds; in local mode: issues above threshold,
failed fixes, rolled-back fixes). Deduplicate and present to the user. If the file is
empty, skip to Report.

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
   - **PR mode**: submit selected issues as **line-level** PR review comments via
     `gh api` (see `references/pr-comment-format.md` for the exact command). **Do not**
     use `gh pr comment` or `gh pr review` — these create general comments, not
     line-level annotations. Then go to Report.
   - **Local mode**: if no issues were approved for fix (user skipped all), go to
     Report. Otherwise, send the user-approved issues to **Fix** as the fix queue.
     These issues were already verified in a previous Filter — skip directly to fix.
     From there the normal flow resumes: Fix, Validate, Continue?, and Continue?
     routes as usual (new round if new issues were found, Confirm again if new pending
     issues accumulated, or Report if done).

---

## Phase 8: Report

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

Delete `CR_STATE_FILE`. If `.cr-cache/` contains no other files, remove the directory
as well.

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
