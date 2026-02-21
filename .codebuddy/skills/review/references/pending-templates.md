# Pending Issue Template

Use this format to record high-risk issues that require user confirmation before fixing.
Each entry should clearly describe the issue, the proposed fix, and why it needs user
judgment.

```markdown
# Pending Issues

Issues deferred for user confirmation. Each was assessed as high-risk during Phase 3
because the fix involves a design decision or changes an external contract.

## Issue 1: [brief description]
- **File**: [file path:line number]
- **Risk reason**: [why this needs user confirmation, e.g., public API change,
  test output change, multiple fix approaches, architecture impact]
- **Current behavior**: [what the code does now]
- **Proposed fix**: [what the fix would change]
- **Impact scope**: [what else would be affected — callers, tests, docs, etc.]
```
