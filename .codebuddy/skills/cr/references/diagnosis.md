# Diagnosis

Analyze the most recent `/cr` session in this conversation to find defects in
the skill files themselves — checklist gaps, ambiguous instructions, missing
exclusion rules, etc. The goal is to make the skill more accurate and
reliable, NOT to re-review the project code.

## Prerequisites

If no `/cr` session exists in the current conversation, inform the user and
stop.

## Step 1: Gather context

1. Identify which review flow the session used (local-review, pr-review, or
   teams-review).
2. Read the corresponding flow file and all reference files it declares in its
   References table.

## Step 2: Analyze

Compare the session's behavior against the skill files you just read. Pay
attention to user rollbacks of auto-fixes (undo, manual revert, explicit
rejection) and manual corrections or overrides the user had to provide — these
often signal defects in skill files. For each finding below, identify which
skill file is deficient and what specific change would fix it. Do NOT fabricate
issues — only report what is evidenced in the session.

### 2.1 False positives

Issues reported to the user that should NOT have been reported. For each:
- What was reported and why it was wrong
- Which checklist item or exclusion rule should have prevented it
- Root cause: checklist gap, exclusion list gap, judgment-matrix gap, verifier
  misjudgment, or AI misinterpretation of an existing rule
- Suggested fix (which file, what change)

### 2.2 False negatives

Real issues visible in the diff that the session missed. For each:
- What was missed and why it matters
- Whether an existing checklist item covers it (AI failed to apply) or no item
  exists (checklist gap)
- Root cause: checklist gap, scope error, or verifier over-rejection
- Suggested fix (which file, what change)

### 2.3 Judgment errors

Issues where risk level or worth-fixing assessment was wrong. Skip if the
session flow does not perform these assessments. For each:
- Assigned vs correct judgment
- Whether judgment-matrix.md rules are ambiguous or the AI misapplied them
- Suggested fix to judgment-matrix.md

### 2.4 Flow deviations

Steps that the AI skipped, reordered, or executed incorrectly — including
scope, review, filter, fix, validate, user interaction, and checklist
evolution. For each:
- Which step in which file was violated
- What actually happened vs what should have happened
- Root cause: instruction ambiguity, conflicting instructions, or AI deviation
- Suggested fix to the flow file

### 2.5 Suggested improvements

Improvements not triggered by a specific error but observed from the session.
Only include suggestions that would measurably improve review accuracy or
reduce token consumption. For each:
- Current state and proposed change
- Which file to modify
- Expected impact

## Step 3: Apply

If any finding has a concrete file edit, present all actionable edits and ask
the user via **a single multi-select question** which ones to apply. Each
option label is a one-line summary of the edit. Unchecked edits are discarded.

Apply selected edits.
