# User Questions — Phase 0.2

Present all applicable questions in **one AskUserQuestion call**.

## Question 1 — Review priority

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

## Question 2 — Auto-fix threshold (skip in PR mode)

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
