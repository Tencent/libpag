# Bot Review

Non-interactive PR code review mode designed for CI/CD pipelines. Uses multi-agent
adversarial review mechanism (reviewer–verifier) to ensure high-quality issue
detection, then posts all confirmed issues as line-level PR comments. Does NOT
apply fixes, approve, or merge — only reviews and comments.

For shared review mechanics (team setup, reviewer/verifier prompts, filter logic),
see `teams-review.md`. This document covers bot-specific setup and output only.

## Flow

```
Setup (validate PR, fetch diff)
  │
  │ (invalid PR / auth error / empty diff)
  ├──────────────────────────────────────→ Stop
  │
  ↓
Review (reviewer–verifier pipeline)
  │
  ↓
Filter (de-dup, worth-reporting, line numbers)
  │
  │ (no issues)
  ├──────────────────────────────────────→ Clean up → Success
  │
  ↓
Output (post PR comments, print summary)
  │
  ↓
Checklist Evolution (suggest new patterns)
  │
  ↓
Clean up (delete temp branch) ──────────→ Done
```

Unlike teams mode, bot mode is a **single-pass pipeline** — no fix loop, no user
confirmation. Issues are reported as PR comments for human follow-up.

## References

| File | Purpose |
|------|---------|
| `teams-review.md` | Shared review mechanics (team setup, reviewer/verifier prompts, filter logic, checklists) |

---

## Arguments

`$ARGUMENTS` format: `bot [target]`

Where `[target]` is required and must be one of:
- PR number (e.g., `123`) — review the specified PR
- PR URL (e.g., `https://github.com/.../pull/123`) — review the specified PR

If `[target]` is empty or not a valid PR reference, print error message and stop.

---

## Prerequisites

This mode requires `gh` CLI with valid GitHub authentication. The `gh` CLI reads
credentials from these sources (in priority order):

1. `GH_TOKEN` environment variable
2. `GITHUB_TOKEN` environment variable
3. `gh auth login` cached credentials

In CI environments, set `GH_TOKEN` or `GITHUB_TOKEN` with the following
permissions:
- `repo` — read repository and PR information
- `pull_requests: write` — post PR comments

---

## Phase 1: Setup

### Check prerequisites

Verify `gh` CLI is available and authenticated:

```bash
gh auth status
```

If this command fails, print error message with setup instructions and stop.

### Validate PR

Parse `[target]` from `$ARGUMENTS` (everything after `bot`). If it's a URL,
extract the PR number.

```bash
gh repo view --json nameWithOwner --jq .nameWithOwner
gh pr view {number} --json headRefName,baseRefName,headRefOid,state,body
```

Record `OWNER_REPO`, `PR_BRANCH`, `BASE_BRANCH`, `HEAD_SHA`, `STATE`, `PR_BODY`.

Validation:
- If commands fail → print error message and stop.
- If URL contains `{owner}/{repo}` that doesn't match `OWNER_REPO` → print
  "Cross-repo PR review not supported" and stop.
- If `STATE` is not `OPEN` → print "PR is not open" and stop.

### Fetch PR diff

```bash
git fetch origin pull/{number}/head:pr-{number}
git fetch origin {BASE_BRANCH}
MERGE_BASE=$(git merge-base origin/{BASE_BRANCH} pr-{number})
git diff $MERGE_BASE pr-{number}
```

If diff is empty → print "No changes to review." and stop.

If diff exceeds 10000 lines → print "PR too large for automated review
({N} lines). Please request a manual review." and stop with exit code 2.

If diff exceeds 200 lines, first run `git diff --stat` for overview, then read
per file using `git diff $MERGE_BASE pr-{number} -- {file}`.

### Fetch existing PR comments

```bash
gh api repos/{OWNER_REPO}/pulls/{number}/comments
```

Store as `EXISTING_COMMENTS` for de-duplication in Phase 3.

### Module partition

See `teams-review.md` → Phase 1: Scope → Module partition.

---

## Phase 2: Review

See `teams-review.md` → Phase 2: Review (Team setup, Verification pipeline),
with the following bot-specific adjustments:

### Reviewer prompt additions

In addition to the base reviewer prompt from `teams-review.md`:
- **PR context**: `PR_BODY` content to understand the stated motivation. Verify
  the implementation actually achieves what the author describes. If `PR_BODY` is
  empty, skip motivation verification and focus on code-level review only.
- **Output format** (overrides base format from `teams-review.md`):
  `[file:line] [A1/B2/C3] — [description] — [key lines]`
  (include checklist item ID, e.g., A1, B2, C3)

### After review

Apply missing report detection per `teams-review.md` → After review: if a
reviewer's system notification shows "completed" but no SendMessage report was
received, send a follow-up message requesting its report before proceeding.

Close all agents (send shutdown_request + TeamDelete) → Phase 3. Unlike teams
mode, bot mode does not retain reviewers as fixers since it only comments and
never applies fixes.

---

## Phase 3: Filter — coordinator only

See `teams-review.md` → Phase 3: Filter (entry conditions, stance), with the
following bot-specific adjustments. Bot mode skips the Existence check step
(teams-review.md § 3.2) — verifiers already validated issue existence through
their CONFIRM verdicts during the review phase.

### 3.1 De-dup

- Remove cross-reviewer duplicates (same location, same topic)
- Remove issues already in `EXISTING_COMMENTS` (same file, same line range,
  similar description)

### 3.2 Worth reporting

Consult `judgment-matrix.md` for worth-fixing criteria. Discard issues that are
not worth reporting (e.g., pure style preferences, speculative optimizations).

### 3.3 Determine line numbers

For each confirmed issue, determine the exact line number in the **new** file
(right side of diff). Read the actual file in the PR branch to confirm — do not
derive from diff hunk offsets alone.

GitHub PR review API only accepts lines within a diff hunk (additions or
unchanged context lines shown in the diff). If a confirmed issue targets a line
that exists in the new file but falls outside any diff hunk, find the nearest
line within the same file's diff hunks and reference the original location in
the comment body. If no hunk exists for that file, add to `ORPHAN_ISSUES`.

### 3.4 Handle deleted lines

GitHub PR review comments can only target lines that exist in the **new** file
(additions or unchanged lines within the diff hunk). For issues involving deleted
lines:

- If the issue is about **why the deletion is wrong** (e.g., removed necessary
  code), target the nearest surviving line in the same hunk and mention the
  deleted line in the comment body.
- If no suitable line exists in the hunk, add to `ORPHAN_ISSUES` list for
  standalone comment (see Phase 4).

---

## Phase 4: Output

### If no issues found

Print success message and skip to Phase 6 (Clean up):
```
Code review passed. No issues found.
```

### If issues found

Submit all confirmed issues as **line-level PR review comments** using `gh api`.
Only individual issue comments are posted to the PR — the summary is printed to
stdout only and NOT submitted to the PR. If multiple issues target the same line,
they are merged into a single comment.

```bash
gh api repos/{OWNER_REPO}/pulls/{number}/reviews --input - <<'EOF'
{
  "commit_id": "{HEAD_SHA}",
  "event": "COMMENT",
  "comments": [
    {
      "path": "relative/file/path",
      "line": 42,
      "side": "RIGHT",
      "body": "[A2] Description of the issue — suggested fix"
    },
    {
      "path": "relative/file/path",
      "line": 88,
      "side": "RIGHT",
      "body": "[B1] First issue — fix suggestion\n[C3] Second issue on same line — fix suggestion"
    },
    ...
  ]
}
EOF
```

Comment body format:
- Single issue: `[{priority}] {description} — {suggested fix}`
- Multiple issues on same line: each issue on its own line within the same body

Where `{priority}` is the checklist item ID (e.g., A2, B1, C7).

**Field requirements**:
- `commit_id`: HEAD SHA of the PR branch (`HEAD_SHA`)
- `path`: relative to repository root
- `line`: line number in the **new** file (right side of diff)
- `side`: always `"RIGHT"`
- `body`: concise description with specific fix suggestion when possible; merge
  multiple issues on the same line into one body with line breaks

After submitting line comments, **print** (not post to PR) a summary to stdout:
```
Code review found {N} issue(s). Comments posted to PR #{number}.

Issues:
1. [A2] src/foo.cpp:42 — null pointer dereference risk
2. [B1] src/bar.cpp:88 — unnecessary copy in loop
...

Summary:
- A (Correctness & Safety): X issues
- B (Refactoring & Optimization): Y issues
- C (Conventions & Documentation): Z issues
```

**Error handling**: If the `gh api` call fails (network error, auth failure, rate
limit), print the error to stderr and exit with code 2. Posting failure is a fatal
error that requires attention.

### Orphan issues

If `ORPHAN_ISSUES` (from Phase 3.4) is not empty, post a **separate PR comment**
listing these issues:

```bash
gh pr comment {number} --body '### ⚠️ Issues Outside Diff Hunks

The following issues were found but could not be attached to specific diff lines
(the affected code was deleted or falls outside diff hunks in this PR):

1. [A2] `src/foo.cpp` (deleted line 42) — removed necessary null check
2. [B1] `src/bar.cpp` (deleted line 88) — deleted error handling code
...'
```

If this comment fails to post, print the error but continue — orphan issues are
supplementary information.

---

## Phase 5: Checklist evolution

Skip this phase if no issues were found in Phase 3 or no new patterns are identified.

Review all confirmed issues from this session. If any represent a recurring
pattern not covered by the current checklist, follow `checklist-evolution.md`
Step 1 (Draft candidates) to identify new patterns.

Since bot mode cannot modify repository files directly, post a **separate PR
comment** (not a line-level review comment) proposing the checklist update:

```bash
gh pr comment {number} --body '### 📋 Suggested Checklist Update

The following pattern was detected but is not covered by the current checklist:

- **ID**: `[suggested-id]`
- **Pattern**: [description]
- **Why it matters**: [rationale]

Consider adding this to `.codebuddy/skills/cr/references/code-checklist.md`.'
```

If the comment fails to post, skip silently — do not block the review workflow.

---

## Phase 6: Clean up

All agents were already shut down at the end of Phase 2. This phase only
cleans up the temporary git branch:

```bash
git branch -D pr-{number} 2>/dev/null || true
```

---

## Exit Codes

For CI/CD integration, the bot review process uses the following exit codes:

| Code | Meaning |
|------|---------|
| 0 | Review passed — no issues found |
| 1 | Review completed — issues found and posted to PR |
| 2 | Setup or posting failed — invalid PR, auth error, API failure, or other fatal error |
