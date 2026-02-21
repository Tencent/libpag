# Pending File Templates

## pending-test-updates.md

Use this format when a fix is confirmed correct but alters expected test output.

```markdown
# Pending Test-Impact Fixes

The following fixes are confirmed correct in code logic but change the expected output
of test cases. User confirmation is needed before applying.

## Issue 1: [description]
- **Original commit**: [hash] (reverted)
- **Affected tests**: [test case names]
- **Behavior change**: [why the test fails, how expected output changes]
- **Code change**: [brief description of the modification]
```

## pending-api-changes.md

Use this format when an issue involves public API signature changes (non-bug).

```markdown
# Pending Public API Changes

The following issues involve public API signature changes and require user confirmation.

## Issue 1: [description]
- **File**: [file path:line number]
- **Current signature**: [existing function signature]
- **Suggested change**: [proposed new signature]
- **Reason**: [why the change is needed]
- **Impact scope**: [which callers need to be updated]
```
