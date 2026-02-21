# Pending Issue Template

Use this format to record all issues awaiting user confirmation in `PENDING_FILE`.
Each issue includes a **Reason** describing why it was not auto-fixed.

```markdown
# Pending Issues

## 1. [brief description]
- **Reason**: [why not auto-fixed, e.g., "above auto-fix threshold (high risk)",
  "fix failed: build error after 2 retries", "rolled back: introduced regression"]
- **Risk**: low | medium | high
- **File**: [file path:line number]
- **Current**: [what the code does now]
- **Proposed**: [what the fix would change]
- **Impact**: [what else would be affected]
```
