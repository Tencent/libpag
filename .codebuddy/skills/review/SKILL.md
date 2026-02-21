---
name: review
description: Multi-round automated code review and fix using Agent Teams. Use when the user invokes /review to review and fix code or document issues across a branch or pull request.
---

# Review — Automated Code Review & Fix

Automatically review, verify, and fix issues in code and documents across your branch
or pull request. Runs multi-round team-based iterations until no valid issues remain.

Two fix modes are available:
- **Auto-fix**: all confirmed issues are fixed automatically.
- **Selective fix**: issues are listed for the user to pick which ones to fix by number.

For PR reviews of other people's code, fixes are submitted as PR review comments instead
of direct commits.

Issues that require user judgment — such as test baseline changes or public API
modifications — are never auto-fixed in either mode. They are collected and presented
for explicit confirmation in Phase 7.

## Instructions

- All user-facing interactions must use the language the user has been using in the
  conversation. Do not default to English.
- When presenting choices with predefined options, use interactive dialogs with
  selectable options rather than plain text.
- **You (the team-lead) never modify files directly.** Delegate all changes to agents.
  Read code only for arbitration and diagnosis.
- After each round, analyze review quality and adjust the next round's reviewer prompts.

## References

| Topic | File |
|-------|------|
| Code review checklist (Level A-C + exclusions) | `references/code-checklist.md` |
| Document review checklist (Level A-C + exclusions) | `references/doc-checklist.md` |
| Issue filtering judgment matrix | `references/judgment-matrix.md` |
| Pending file templates | `references/pending-templates.md` |

---

## Phase 0: Scope Confirmation & Environment Check

### 0.1 Scope Resolution

Parse `$ARGUMENTS` to determine the review mode and scope:

| `$ARGUMENTS` | Detection | Mode |
|--------------|-----------|------|
| (empty) | — | **Local** — ask scope below |
| PR number (e.g., `123`) | `gh pr view` succeeds | **PR** |
| PR URL | Extract owner/repo/number, verify matches current repo | **PR** |
| A valid git commit/ref | `git rev-parse --verify` succeeds | **Local** |
| A file or directory path | Path exists on disk | **Local** |

**PR URL mismatch**: if the URL's owner/repo does not match the current repository
(`gh repo view --json nameWithOwner -q '.nameWithOwner'`), abort and ask the user to
run `/review` from the correct repository.

**Ambiguity**: if the argument could be both a PR number and a commit ref, try
`gh pr view` first — if it succeeds, treat as PR.

#### Local mode scope

If scope is not yet determined (empty `$ARGUMENTS`), ask the user:

**Question 1 — Review scope** (skip if already resolved):
- Option 1 — "Current branch vs main": use the diff between current branch and main
- The "Other" free-text option allows the user to type a base commit (hash or ref) or
  a folder/file path directly. Validate the input the same way as `$ARGUMENTS` parsing.

#### PR mode setup

1. **Verify `gh` is available**: run `gh --version`. If not installed, inform the user
   (macOS: `brew install gh`, others: https://cli.github.com) and abort.

2. **Fetch PR metadata** (single API call):
   ```
   gh pr view {number} --json headRefName,author,headRefOid
   ```
   Extract: `PR_BRANCH`, `PR_AUTHOR`, `HEAD_SHA`.

3. **Determine code ownership**: compare `PR_AUTHOR` with `gh api user -q '.login'`.
   Store result as `IS_OWN_PR` (true/false).

4. **Prepare working directory**:
   - If current branch equals `PR_BRANCH` -> use current directory directly.
   - Otherwise -> create a worktree:
     ```
     git fetch origin pull/{number}/head:pr-{number}
     git worktree add /tmp/pr-review-{number} pr-{number}
     ```
     All subsequent operations use the worktree directory. Record `WORKTREE_DIR` for
     cleanup in Phase 7.

5. **Fetch existing PR comments** for reviewer context:
   ```
   gh pr view {number} --comments
   ```
   Include in reviewer prompts so they avoid re-reporting already-discussed issues.

6. **Set review scope**: diff between `origin/main` and `HEAD` in the PR branch.
   ```
   git fetch origin main
   git diff $(git merge-base origin/main HEAD)
   ```

### 0.2 Review Level Selection

**Question 2 — Review level** (option labels should be concise, use descriptions to
explain the incremental scope of each level):
- Option 1 — "Correctness & Safety only": logic bugs, security vulnerabilities,
  resource leaks, memory safety, thread safety
- Option 2 — "Correctness & Safety + Refactoring": adds performance optimization,
  code simplification, architecture improvement, interface usage, test coverage,
  regression risk
- Option 3 — "All": adds coding conventions, documentation consistency, accessibility

### 0.3 Fix Mode Selection

The available fix modes depend on code ownership:

**Own code** (local mode, or PR mode with `IS_OWN_PR = true`):

**Question 3 — Fix mode**:
- Option 1 — "Auto-fix": all confirmed issues are fixed automatically, each committed
  individually for easy rollback
- Option 2 — "Selective fix": after review, all issues are listed with numbers for the
  user to choose which ones to fix

**Other's PR** (`IS_OWN_PR = false`):

Fix mode is automatically set to **selective fix** with PR comment output. Inform the
user: "This is someone else's PR. Issues will be presented for you to select which ones
to submit as PR review comments."

### 0.4 Clean Branch Check

**PR mode (worktree)**: skip — the worktree is always clean.

**PR mode (current branch is PR branch)**: verify no uncommitted changes. If dirty,
ask the user to resolve first.

**Local mode, auto-fix**: verify the working tree is clean and not on the main branch:
- If on the main/master branch, or there are uncommitted changes (staged, unstaged,
  or untracked), inform the user: "Auto-fix requires a clean, non-main branch. Each
  fix is committed individually for easy rollback and issue tracing."
- On main -> abort, ask user to create a feature branch first.
- Uncommitted changes -> ask user whether to commit or stash first. Decline = abort.

**Local mode, selective fix**: defer this check to Phase 3.5 (only needed if the user
chooses to fix any issues). Still abort immediately if on the main branch.

### 0.5 Reference Material (doc/mixed modules only)

If the scope contains doc or mixed modules, gather reference material for reviewers:

1. **Auto-discover**: search the project and web for relevant best-practice guides,
   official documentation, or specification standards that apply to the document type
   being reviewed (e.g., skill authoring guides, API design guidelines, RFC standards).
2. **Ask the user**: present what you found and ask if they have additional reference
   material (file paths, URLs, or inline instructions). The user may also choose to
   skip this step if no extra references are needed.
3. Include all gathered references in the reviewer prompts for doc/mixed modules.

After confirmation, no further user interaction until fix mode interaction point (but
see Mid-Review Supplements below). Issues requiring user judgment (test baseline
changes, API modifications) will be collected and confirmed in Phase 7, not auto-fixed.

### 0.6 Environment Verification

Run sequentially after scope is confirmed. Abort if any step fails.

1. **Automated test detection** — check whether the project has a usable test suite:
   - If the project's rules loaded in context already describe build/test commands, use
     those directly and skip exploration.
   - Otherwise, explore the codebase to identify the test framework and run commands
     (look for test directories, CMakeLists.txt test targets, package.json test scripts,
     Makefile test targets, etc.).
   - If no automated tests are found, warn the user: without tests, Phase 5 validation
     cannot verify fix correctness, and results may be unreliable. Ask the user whether
     to continue (review + fix without validation) or abort.
2. **Build verification** — use the project's build command. Fail = abort.
3. **Run tests** — use the project's test command. Fail = abort.

### 0.7 Module Partitioning

Partition files in scope into **review modules** for parallel review. The goal is to
balance workload across reviewers, not to match file boundaries.

**Review module rules:**
- Each module should be a self-contained logical unit that a reviewer can understand
  with minimal reference to other modules.
- **Large files**: if a single file is too large for one reviewer (e.g., a long spec
  document or a large implementation file), split it by sections/chapters (for docs)
  or by function groups (for code). Each section becomes its own review module.
- **Small files**: group related small files into one module to avoid under-utilizing
  reviewers.
- Balance module sizes so reviewers have roughly equal workload.
- **Module type**: classify each module as `code`, `doc`, or `mixed` based on its file
  types. This determines which checklist the reviewer uses.

**Fix module rules** (determined in Phase 3, after issues are confirmed):
- Fix modules are grouped **by file**: all confirmed issues in the same file go to the
  same fixer, regardless of which review module found them. This prevents concurrent
  edit conflicts.
- When a review module covers a section of a large file, the team-lead merges all
  issues from different review modules targeting the same file into one fix task.
- Cross-file issues (e.g., renames) -> dedicated `fixer-cross` agent.
- **Shared headers**: assign to the fixer of their primary implementation file.
- **Widely referenced files** (3+ fixers need to touch it): assign to `fixer-cross`.

### Mid-Review Supplements

The user may send additional reference material or instructions at any time during the
review process. When this happens:

1. Acknowledge receipt and assess which modules are affected.
2. Forward the material to the relevant active reviewer/verifier agents.
3. If reviews for affected modules are already complete, note the supplements for the
   next round (Phase 6 loop) rather than restarting the current round.

---

## Phase 1: Review

- Create the team. If team creation fails because Agent Teams is not enabled, prompt
  the user to enable it: run `/config` -> `[Experimental] Agent Teams` -> `true`, or
  set environment variable `CODEBUDDY_CODE_EXPERIMENTAL_AGENT_TEAMS=1`.
- One `general-purpose` reviewer agent (`reviewer-N`) per module
- Reviewer prompt includes:
  - Module file list + check items for the selected review level, using the checklist
    matching the module type: `references/code-checklist.md` for code modules,
    `references/doc-checklist.md` for doc modules, both for mixed modules
  - **Review scope rule**: reviewers must read entire files for full context, but apply
    different reporting thresholds based on whether code is within the branch diff:
    - **Code within diff**: report all issues matching the selected review level (A/B/C)
    - **Code outside diff**: report only Level A (correctness/safety) and Level B
      (refactoring/optimization) issues. Skip Level C (conventions/documentation) for
      unchanged code.
  - **Evidence requirement**: every issue must have a code citation (file:line + snippet)
  - **Exclusion list**: see the exclusion section in the corresponding checklist. Project
    rules loaded in context take priority over the exclusion list.
  - **Public API protection**: no signature/interface changes unless obvious bug or
    comment issue
  - **PR context** (PR mode only): include existing PR comments so reviewers avoid
    re-reporting already-discussed issues
  - Structured output format

### Stuck Detection

When all other agents in the batch are done but one has not responded:
1. Send a reminder
2. No response -> check for output
3. No output -> `shutdown_request` + create replacement
4. Partial output -> `shutdown_request` + create agent for remaining work

---

## Phase 2: Verification

- **Do not close reviewers yet** (may reuse as fixers in Phase 4 if their review module
  aligns with a fix module)
- Create independent verifier (`verifier-N`) per review module with issues
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

Decide which confirmed issues are fixable. Consult `references/judgment-matrix.md` for
the full matrix and special rules.

Key points:
- **Public API protection**: obvious bugs = fixable; non-bug signature changes = record
  to `pending-api-changes.md` (see `references/pending-templates.md`)
- **Cross-module doc impact**: for each fixable issue, identify whether the change
  affects comments or documentation files outside the fixer's own module. If so, create
  a follow-up task for `fixer-cross` to update those files after the original fix is
  committed.
- Previously rolled-back issues -> do not attempt again

---

## Phase 3.5: Issue Presentation & Fix Confirmation

### Auto-fix mode

Skip this phase — proceed directly to Phase 4 with all fixable issues.

### Selective fix mode

1. Present all confirmed issues to the user in a numbered list. Each entry includes:
   - Issue number
   - File path and line number
   - Brief description (one line)
   - Severity level (A/B/C)

2. Ask the user which issues to act on. The user can:
   - Enter issue numbers (e.g., "1,3,5" or "1-5,8")
   - Enter "all" to act on all issues
   - Enter "none" or leave empty to skip

3. **Action depends on mode**:
   - **Local mode / own PR**: selected issues proceed to Phase 4 (fix).
     - **Clean branch check** (deferred from 0.4): verify the working tree is clean.
       Same rules as 0.4 auto-fix mode. If not clean, ask user to resolve first.
     - **Environment verification** (if not already done): run Phase 0.6 now.
   - **Other's PR**: selected issues are submitted as PR review comments (see below),
     then skip to Phase 6.

4. If the user chose "none" -> skip Phase 4-5, go directly to Phase 6 termination
   check (which will end the workflow since there are no fixes to validate).

### PR Review Comments (other's PR)

Submit selected issues as a single GitHub PR review with line-level comments. Use
`gh api` with the reviews endpoint — do not use `gh pr comment` or `gh pr review`:

```
gh api repos/{owner}/{repo}/pulls/{number}/reviews --input - <<'EOF'
{
  "commit_id": "{HEAD_SHA}",
  "event": "COMMENT",
  "comments": [
    {"path": "file/path.cpp", "line": 42, "side": "RIGHT", "body": "description"},
    ...
  ]
}
EOF
```

Comment body should be concise, written in the user's conversation language, and include
a specific fix suggestion when possible.

---

## Phase 4: Fix

- Group selected issues into **fix modules by file** (see 0.7 fix module rules).
  If a reviewer already has full context for a file, reuse it as fixer for that file.
- Cross-file issues -> dedicated `fixer-cross` agent
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

### Auto-fix mode

- **Termination**: valid issues from Phase 3 = 0 -> Phase 7
- Otherwise -> create new team, back to Phase 1
- No round limit — driven by valid issue count

### Selective fix mode

After fixes are validated (or if user chose "none"):
- Present remaining unfixed issues (if any) and ask: "Would you like to fix any of
  the remaining issues?"
- User selects more issues -> back to Phase 3.5 step 3 (clean branch check) then
  Phase 4
- User declines -> Phase 7

Next round prompt includes: rollback blacklist, previous fix summary,
pending file contents (to avoid re-reporting), and prompt adjustments based on
review quality analysis.

---

## Phase 7: Final Report & Cleanup

### PR mode: push fixes (own PR only)

If fixes were made in PR mode and `IS_OWN_PR = true`, push all commits to the PR
branch:
```
git push origin HEAD:{PR_BRANCH}
```

### Pending item confirmation

1. Check `pending-test-updates.md` and `pending-api-changes.md`
2. Both empty -> proceed to cleanup
3. Either has content -> present to user via AskUserQuestion, confirm item by item
4. Approved fixes -> create agent to re-apply; rejected -> discard
5. Final build + test

### PR mode: worktree cleanup

If `WORKTREE_DIR` was created, clean up after all operations are complete:
```
cd {original_directory}
git worktree remove {WORKTREE_DIR}
git branch -D pr-{number}
```

### Summary Report

- Total rounds and per-round fix count/type statistics
- Issues found vs issues fixed (selective mode: also show issues the user declined)
- Rolled-back issues and reasons
- Pending items and their resolution
- Final test result
- **PR mode**: list commits pushed (own PR) or review comments submitted (other's PR)

Delete pending files after report.

---

## Troubleshooting

### Agent Teams not available
Cause: Experimental feature not enabled.
Solution: Run `/config` -> `[Experimental] Agent Teams` -> `true`.

### Build or test fails in Phase 0
Cause: Pre-existing issues in the codebase.
Solution: Fix build/test failures before running `/review`.

### Reviewer reports no issues but code has problems
Cause: Review level too restrictive or reviewer prompt insufficient.
Solution: Re-run with a higher review level. Team-lead will also auto-adjust prompts in
subsequent rounds.

### Pending items not presented at the end
Cause: No test baseline or API changes were detected.
Solution: This is expected — Phase 7 only presents pending items when they exist.

### Fixer commits break tests repeatedly
Cause: Complex semantic changes or insufficient context.
Solution: After 2 retries, `fixer-rescue-N` takes over with full error context. If the
fix is correct but changes behavior, it goes to `pending-test-updates.md` for user
confirmation.

### PR URL does not match current repository
Cause: The PR belongs to a different repository.
Solution: Navigate to the correct repository and run `/review` there.

### gh CLI not installed or not authenticated
Cause: PR mode requires the GitHub CLI.
Solution: Install with `brew install gh` (macOS) or see https://cli.github.com, then
run `gh auth login`.
