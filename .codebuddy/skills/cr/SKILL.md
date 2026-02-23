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

## Flow

```
  ┌─────────────────────────────── Round N ──────────────────────────────┐
  │                                                                      │
  │  Phase 2       Phase 3       Phase 4       Phase 5       Phase 6     │
  │  Review ──────> Filter ──────> Fix ────────> Validate ───> Decide    │
  │                                ^                             │       │
  └────────────────────────────────│─────────────────────────────│───────┘
                                   │                             │
Phase 0 ──> Phase 1 ──> ┌─────────│──── Phase 6 exit routes ───┘
  Setup       Scope      │         │
                         │         │  1. commits produced, round < 100
                         │         │     -> close team, back to Phase 2 (new round)
                         │         │
                         │         │  2. no commits, PENDING_FILE not empty
                         │         │     -> Phase 7 (Confirm)
                         │         │
                         │         │  3. no commits, PENDING_FILE empty
                         │         │     -> Phase 8 (Report & Exit)
                         │         │
                         │         │
                         │    Phase 7: Confirm
                         │      user approves fix(es) -> Phase 4 (Fix)
                         │      user skips all ──────────> Phase 8
                         │                                   │
                         └───────── Phase 4 -> 5 -> 6 ───────┘
                                    (normal Decide routing)

  Phase 8: Report & Exit
```

**Phase 6 (Decide) is the single routing authority.** All loop/exit decisions are
made there. Phase 7 (Confirm) feeds back into the loop via Phase 4 → 5 → 6.
Safety valve: max 100 rounds before forcing exit to Phase 7/8.

## Instructions

**Delegation**: You (the team-lead) never modify files directly. Delegate all changes
to agents. Read code only for arbitration and diagnosis.

**Autonomy**: Between Phase 0 (user confirmation) and Phase 7 (remaining issue
confirmation), the review-fix loop runs without user interaction unless a previously
failed issue fails again in Phase 5 (which prompts inline). Issues above the auto-fix
threshold and failed fixes are recorded to `PENDING_FILE` across rounds and presented
to the user in Phase 7.

**Error handling**: Handle all unexpected situations autonomously — agent stuck or
unresponsive, fix breaks the build, git operation fails, agent produces invalid output.
Use your judgment: terminate and replace, revert and retry, skip and move on, etc.
Anything that cannot be resolved automatically should be recorded to `PENDING_FILE`
for user review in Phase 7.

**Agent lifecycle**: When closing agents, send the shutdown message and continue
immediately without waiting for acknowledgment. Do not block the workflow on agent
responses. Late replies from already-shutdown agents should be ignored. When closing
the team (Phase 6 end-of-round, Phase 8), force-terminate (TaskStop) any agents that
are still running.

**User interaction**: All user-facing text must use the language the user has been
using. When presenting choices with predefined options, use interactive dialogs with
selectable options rather than plain text. The user may send additional material
(files, URLs, context) at any time. If reviewers are still active (Phase 2), forward
it directly; otherwise include the material in the next round.

**No-team fallback**: If Agent Teams is not available, the entire flow still applies.
Team-lead uses Task tool sub-agents instead of team agents:
- **Reviewers**: launch one Task sub-agent per module. Multiple sub-agents can run in
  parallel (send multiple Task calls in a single message). Each sub-agent completes
  and returns results — it cannot be kept alive for later reuse.
- **Verifier**: team-lead performs verification directly in Phase 3 — read the cited
  code for each issue and apply the devil's advocate check before risk assessment.
  (Sub-agents are stateless, so a persistent verifier is not possible.)
- **Fixers**: launch one Task sub-agent per fix (can also run in parallel). Since
  reviewers cannot be kept alive, provide each fixer with the necessary file context
  in its prompt.
- **Team lifecycle**: no team to create or close. Skip all team setup/teardown steps
  (Phase 2 team setup, Phase 6 Step 1, Phase 8 cleanup). The verifier role merges
  into the team-lead's Phase 3 work.
- All other phases (scope, filter, validate, decide, confirm, report) work identically.

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

### 0.1 Mode detection (pure string parsing, no tools)

Parse `$ARGUMENTS` to determine the review mode:
- Purely numeric or URL (contains `/`) -> **PR mode**
- Everything else (empty, commit-like, paths, `..` range) -> **Local mode**

### 0.2 Ask questions

**STOP. Do NOT call any tools (Bash, Read, Glob, Grep, Task, etc.) before receiving
user answers.** The questions below do not depend on any repository state.

Present all applicable questions in **one AskUserQuestion call**.

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

Option 1 should be pre-selected as the default.

- Option 1 — "Low + Medium risk (recommended)": auto-fix unambiguous fixes and clear
  cross-location refactors (e.g., null checks, naming, extract shared logic,
  remove unused internals). Confirm high-risk issues.
- Option 2 — "Low risk only": auto-fix only fixes with a single correct approach
  (e.g., null checks, comment typos, naming, `reserve`). Confirm anything whose
  impact extends beyond the immediate locality.
- Option 3 — "All confirm": no auto-fix, confirm every issue before any change.
- Option 4 — "Full auto (risky)": auto-fix all risk levels, autonomously deciding
  fix approach for high-risk issues (e.g., API changes, architecture restructuring,
  algorithm trade-offs). Only issues affecting test baselines are deferred for
  confirmation.

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
- Check if Agent Teams is enabled. If not available, warn the user and switch to
  no-team fallback mode (see Instructions).

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
     cleanup in Phase 8.

3. **Set review scope**: diff against `BASE_BRANCH`.
   ```
   git fetch origin {BASE_BRANCH}
   git diff $(git merge-base origin/{BASE_BRANCH} HEAD)
   ```

4. **Fetch existing PR review comments** for de-duplication in Phase 3:
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
These are used in Phase 3 as reference information — each comment is verified against
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
- **Cross-round exclusion**: the only information carried across rounds is: skip issues
  already recorded in `PENDING_FILE` or rejected by the user in a previous Phase 7.
  Previously fixed issues are **not excluded** — if a reviewer independently finds a
  new problem in code that was fixed last round, that is a valid new issue.

### Team setup

- Create a new team for this round. Each round gets a fresh team — do not reuse
  agents from prior rounds. (No-team fallback: skip team creation.)
- One `general-purpose` reviewer agent (`reviewer-N`) per module.
- One `general-purpose` **verifier** agent (`verifier`) shared across all modules.
  The verifier persists across rounds (not recreated each round) to accumulate
  project context and improve judgment over time. Create it in the first round's
  team; in subsequent rounds, send new issues to the existing verifier.

### Reviewer prompt

Include in each reviewer's prompt:
- Module file list + checklist matching the module type:
  `references/code-checklist.md` for code modules, `references/doc-checklist.md` for
  doc modules, both for mixed modules. Only include priority levels selected by the
  user (e.g., if user chose "A + B", do not include Priority C items). For doc-only
  modules, always include all priority levels (A+B+C) regardless of user selection.
- **Review scope rule**: reviewers must read entire files for full context and
  report issues at the selected priority levels for all code in the reviewed files.
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

The verifier runs as a **pipeline** — it does not wait for all reviewers to finish.
As soon as any reviewer reports an issue, the team-lead forwards it to the verifier
for challenge. Multiple issues can be in-flight simultaneously.

The verifier acts as a **devil's advocate**: for each issue, it reads the cited code
and actively looks for reasons the issue is **not** a real problem. Its prompt:

```
You are a code review verifier. Your goal is to produce accurate verdicts — not to
maximize rejections. For each issue you receive:

1. Read the cited code (file:line) and sufficient surrounding context.
2. Challenge the issue: Is the reviewer's reasoning wrong? Is there context that
   makes this a non-issue (e.g., invariants guaranteed by callers, platform
   constraints, intentional design)? Does the code actually behave as described?
3. Verdict:
   - REJECT: you found a concrete reason the issue is not a real problem.
   - CONFIRM: after investigation, the issue holds up — no valid counter-argument.
   Both verdicts are equally valid outcomes. A high CONFIRM rate does not indicate
   failure — it means the reviewers are doing good work.
4. Confidence: HIGH / MEDIUM / LOW.

Important: do not stretch to find counter-arguments that are technically possible but
practically unrealistic. Base your judgment on how the code is actually used, not on
theoretical edge cases in your favor.
```

### After review

- Keep the verifier alive — it persists across rounds.
- Keep all reviewers alive — they will be reused as fixers in Phase 4.

---

## Phase 3: Filter & Risk Assessment — Team-Lead Only

Two responsibilities, applied to every issue in order:

### 3.1 Existence check — is the issue real?

Use the verifier's verdict as the primary signal:

- **Verifier CONFIRM**: team-lead does a quick plausibility check — verify that the
  reviewer's description is consistent with the cited code snippet and that the
  conclusion logically follows. If anything looks off, read the full code to confirm.
  Otherwise accept and proceed to 3.2.
- **Verifier REJECT (HIGH confidence)**: drop the issue — do not fix or record.
- **Verifier REJECT (MEDIUM/LOW confidence)** or **reviewer–verifier disagree**:
  team-lead must read the cited code **independently** and form its own judgment.
  Do not default to either side — the reviewer's report and the verifier's rebuttal
  are equally weighted inputs. Read the code, understand the actual behavior, then
  decide based on what the code does, not on which argument sounds more persuasive.

The verifier only judges whether the issue exists. It does not assess value or risk.

### 3.2 Value & risk assessment — should we fix it, and how risky is it?

This is exclusively the team-lead's responsibility. For each issue that passes 3.1,
consult `references/judgment-matrix.md` for the full matrix, risk level definitions,
and special rules. Determine:
- Whether the issue is worth fixing (judgment matrix criteria)
- Risk level (low / medium / high)
- Handling based on the user's auto-fix threshold

### De-duplication

- Skip any issue that matches one recorded in `PENDING_FILE` or **rejected by the
  user** in a previous Phase 7. Previously fixed issues are not excluded — if a
  reviewer reports a new problem in already-fixed code, it is treated as a new issue.
- **PR comment de-duplication** (PR mode): compare each confirmed issue against
  `EXISTING_PR_COMMENTS`. If an issue matches an existing comment (same file, same
  general location, same topic), exclude it — do not present it to the user.
- **Associated PR comment verification** (Local mode with associated PR): for each
  PR review comment retrieved in 1.2, verify against the current code whether the
  issue still exists. Verified issues are added to the fix queue as additional known
  issues (using the same risk assessment flow).

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
  or documentation outside the fixer's module — if so, create a follow-up for
  `fixer-cross`.
- Previously rolled-back issues -> do not attempt again this round

---

## Phase 4: Fix

If the auto-fix queue is empty (all issues were recorded to `PENDING_FILE`),
skip Phase 4-5 and go directly to Phase 6.

### Agent assignment

Reuse surviving reviewers as fixers — each reviewer already has context on the files
it reviewed. Team-lead dynamically assigns fix tasks to the most suitable agent:
- Issue in a file that a reviewer already read -> assign to that reviewer
- Cross-file issues or issues with no matching reviewer -> assign to a `fixer-cross`
  agent (create one if needed)
- Multi-file renames -> single atomic task assigned to one agent

One agent may receive multiple fix tasks if it covers several files. Avoid assigning
the same file to multiple agents to prevent concurrent edit conflicts.

### Fixer Instructions (include in every fixer prompt)

Include `references/fixer-instructions.md` verbatim in every fixer agent's prompt.
Team-lead collects skipped issues and includes them in the next round's context.

---

## Phase 5: Validate

- For code/mixed modules: build + test using the project's commands. If no build/test
  commands are available (warned in 1.1), skip validation entirely.
- For doc-only modules: skip build/test; validation is done through review phases

**All pass** (or no code modules to validate) -> Phase 6.

**Failures**: identify the failing commit (bisect if multiple commits, direct revert
if only one), revert it, and send failure info back to the original fixer for retry
(max 2 retries). If still failing, revert and record to `PENDING_FILE`. If the
issue was already in `PENDING_FILE` (a retry from Phase 7), revert and ask the
user: show the failure details and offer options — provide additional context or
direction for another attempt, or skip (default). Skipped issues are added to the
rejected list so they won't be reported again in subsequent rounds.

---

## Phase 6: Decide

This phase is the **single routing authority** for the review-fix loop.

### Step 1: Close the current round's team

Close the team to release reviewers and fixers. Force-terminate any unresponsive
agents. **Do not close the verifier** — it persists across rounds.
(No-team fallback: skip this step.)

### Step 2: Route

Count the commits produced during this round (Phase 4 → 5).

- **Commits produced AND round count < 100**:
  -> Back to **Phase 2** for a new round (new team, fresh review).
- **No commits produced AND `PENDING_FILE` has entries**:
  -> **Phase 7** (Confirm).
- **No commits produced AND `PENDING_FILE` is empty** (or max 100 rounds reached):
  -> **Phase 8** (Report & Exit).

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
     From there the normal flow resumes: Phase 4 -> 5 -> 6, and Phase 6 routes as
     usual (new round if commits were produced, Phase 7 again if new pending issues
     accumulated, or Phase 8 if done).

---

## Phase 8: Report & Exit

### Cleanup

- Close the verifier agent (it persisted across rounds; close it now).
- Force-terminate any other agents still running.

**Worktree cleanup (PR mode only)**:

If `WORKTREE_DIR` was created, clean up:
```
cd {original_directory}
git worktree remove {WORKTREE_DIR}
git branch -D pr-{number}
```

### Summary Report

- Total rounds and per-round fix count/type statistics
- Issues found vs issues fixed (also show issues the user declined or skipped)
- Rolled-back issues and reasons
- Issues that failed fix repeatedly and were skipped
- Final test result
- **PR mode**: list review comments submitted
- **Local mode with associated PR**: note which issues originated from PR comments

### Pending file cleanup

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
