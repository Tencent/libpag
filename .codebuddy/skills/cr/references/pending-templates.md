# Pending Issue Template

Use this format to record issues in the `# Issues` section of `CR_STATE_FILE`.
Each issue includes a **Status** tracking its lifecycle and a **Reason** describing
why it was not auto-fixed.

**Status values**:
- `pending` — recorded, awaiting user decision in Phase 7
- `approved` — user approved fix in Phase 7, sent to Phase 4
- `fixed` — fix applied and passed validation
- `failed` — fix attempted but failed validation after retries
- `skipped` — user declined or issue rejected (do not re-report)

```markdown
# Issues

## 1. [brief description]
- **Status**: pending | approved | fixed | failed | skipped
- **Reason**: [why not auto-fixed, e.g., "above auto-fix threshold (high risk)",
  "fix failed: build error after 2 retries", "rolled back: introduced regression"]
- **Risk**: low | medium | high
- **File**: [file path:line number]
- **Current**: [what the code does now]
- **Proposed**: [what the fix would change — for medium/high risk, include the
  specific approach chosen by the coordinator and the reasoning]
- **Impact**: [what else would be affected]
```
