# Bot Review

Non-interactive PR code review mode designed for CI/CD pipelines. Uses multi-agent
adversarial review mechanism (reviewer–verifier) to ensure high-quality issue
detection, then posts all confirmed issues as line-level PR comments. Does NOT
apply fixes, approve, or merge — only reviews and comments.

## References

| File | Purpose |
|------|---------|
| `code-checklist.md` | Code review checklist |
| `doc-checklist.md` | Document review checklist |
| `judgment-matrix.md` | Risk levels and worth-fixing criteria |

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

If diff exceeds 200 lines, first run `git diff --stat` for overview, then read
per file using `git diff $MERGE_BASE pr-{number} -- {file}`.

### Fetch existing PR comments

```bash
gh api repos/{OWNER_REPO}/pulls/{number}/comments
```

Store as `EXISTING_COMMENTS` for de-duplication in Phase 3.

### Module partition

Partition files in scope into **review modules** for parallel review. Each
module is a self-contained logical unit. Split large files by section/function
group; group related small files together. Classify each module as `code`,
`doc`, or `mixed`.

---

## Phase 2: Review

You are the **coordinator**. Create an Agent Team and dispatch reviewer and
verifier agents. The reviewer–verifier adversarial pair is the core quality
mechanism: reviewers find issues, verifiers challenge them. This two-party check
significantly reduces false positives.

### Team setup

- One reviewer agent (`reviewer-N`) per module.
- One **verifier** agent (`verifier`), shared across all modules.

### Reviewer prompt

Stance: **thorough** — discover as many real issues as possible, self-verify
before submitting.

Each reviewer receives:
- **Scope**: file list + changed line ranges for its module. Reviewers fetch
  diffs and read additional context themselves as needed.
- **Checklist**: `code-checklist.md` for code, `doc-checklist.md` for doc, both
  for mixed.
- **PR context**: `PR_BODY` content to understand the stated motivation. Verify
  the implementation actually achieves what the author describes.
- **Evidence requirement**: every issue must have a code citation (file:line +
  snippet) from the PR branch.
- **Self-check**: before submitting, re-read the relevant code and verify each
  issue. Mark as confirmed or withdrawn. Only submit confirmed issues.
- **Output format**: `[file:line] [A1/B2/C3] — [description] — [key lines]`

### Verification (pipeline)

Stance: **adversarial** — default to doubting the reviewer, actively look for
reasons each issue is wrong. Reject with real evidence, confirm if it holds up.

The verifier runs as a **pipeline** — it does not wait for all reviewers to
finish. As each reviewer sends a report, forward it to the verifier immediately:

- **Quote verbatim**: wrap the reviewer's original content in a quote block.
- **No rewriting**: do not summarize or merge multiple reports.
- **One forward per reviewer**: each reviewer report is a separate message.

Include the following verbatim in the verifier's prompt:

```
You are a code review verifier. Your stance is adversarial — default to doubting the
reviewer's conclusion and actively look for reasons why the issue might be wrong. Your
job is to stress-test each issue so that only real problems survive.

For each issue you receive:

1. Read the cited code (file:line) and sufficient surrounding context.
2. Actively try to disprove the issue: Is the reviewer's reasoning flawed? Is there
   context that makes this a non-issue (e.g., invariants guaranteed by callers, platform
   constraints, intentional design)? Does the code actually behave as the reviewer
   claims? Look for the strongest counter-argument you can find.
3. Output for each issue:
   - Verdict: REJECT or CONFIRM
   - Reasoning: for REJECT, state the concrete counter-argument. For CONFIRM, briefly
     note what you checked and why no valid counter-argument exists.

Important constraints:
- Your counter-arguments must be grounded in real evidence from the code. Do not
  fabricate hypothetical defenses or invent caller guarantees that are not visible in
  the codebase.
- A CONFIRM verdict is not a failure — it means the reviewer found a real issue and
  your challenge validated it.
- Reviewer reports arrive incrementally via the coordinator. Do NOT produce a final
  summary until the coordinator explicitly tells you all reports have been forwarded.
  Process each report as it arrives, but wait for the completion signal before
  concluding.
```

### After review

Close all agents → Phase 3.

---

## Phase 3: Filter — coordinator only

Before entering this phase, confirm: (1) all reviewers have submitted their
final reports; (2) the verifier has given a CONFIRM/REJECT verdict for every
forwarded finding.

Your stance here is **neutral** — trust no single party. Treat reviewer reports
and verifier rebuttals as equally weighted inputs.

### 3.1 De-dup

- Remove cross-reviewer duplicates (same location, same topic)
- Remove issues already in `EXISTING_COMMENTS` (same file, same line range,
  similar description)

### 3.2 Existence check

| Verifier verdict | Action |
|-----------------|--------|
| CONFIRM | Plausibility check — verify description matches cited code. Read code if anything looks off. |
| REJECT | Read code. Evaluate both arguments. Drop only if counter-argument is sound. |

### 3.3 Worth reporting

Consult `judgment-matrix.md` for worth-fixing criteria. Discard issues that are
not worth reporting (e.g., pure style preferences, speculative optimizations).

### 3.4 Determine line numbers

For each confirmed issue, determine the exact line number in the **new** file
(right side of diff). Read the actual file in the PR branch to confirm — do not
derive from diff hunk offsets alone.

---

## Phase 4: Output

### Clean up

```bash
git branch -D pr-{number} 2>/dev/null || true
```

### If no issues found

Print success message:
```
✅ Code review passed. No issues found.
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
      "body": "[A2] Description of the issue and suggested fix"
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
⚠️ Code review found {N} issue(s). Comments posted to PR #{number}.

Issues:
1. [A2] src/foo.cpp:42 — null pointer dereference risk
2. [B1] src/bar.cpp:88 — unnecessary copy in loop
...

Summary:
- A (Correctness & Safety): X issues
- B (Refactoring & Optimization): Y issues
- C (Conventions & Documentation): Z issues
```

