# PR Review Comment Format

Submit issues as a **single** GitHub PR review with line-level comments.

**Must** use `gh api` + heredoc. **Do not** use `gh pr comment`, `gh pr review`, or any
non-line-level comment command.

```bash
gh api repos/{owner}/{repo}/pulls/{number}/reviews --input - <<'EOF'
{
  "commit_id": "{HEAD_SHA}",
  "event": "COMMENT",
  "comments": [
    {
      "path": "relative/file/path",
      "line": 42,
      "side": "RIGHT",
      "body": "Description of the issue and suggested fix"
    }
  ]
}
EOF
```

**Field notes**:
- `commit_id`: use the HEAD SHA of the PR branch (`HEAD_SHA` from Phase 0)
- `path`: relative to repository root
- `line`: the line number in the **new** file (right side of the diff)
- `side`: always `"RIGHT"`
- `body`: concise, in the user's conversation language, with a specific fix suggestion
  when possible
