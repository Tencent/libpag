# User Questions — Phase 0.2

Present all applicable questions in **a single interactive prompt**.

## Question 1 — Review priority

Always show.

(code/mixed modules):
- Option 1 — "Full review (A + B + C)": correctness, refactoring, and conventions.
  e.g., null checks, duplicate code extraction, naming style, comment quality.
- Option 2 — "Correctness + optimization (A + B)": skip conventions and documentation.
  e.g., null checks, resource leaks, duplicate code, unnecessary copies.
- Option 3 — "Correctness only (A)": only safety and correctness issues.
  e.g., null dereference, out-of-bounds, resource leaks, race conditions.

## Question 2 — Auto-fix threshold (skip in PR mode)

In PR mode, add a note alongside Q1: "PR mode — issues will be submitted as
line-level PR review comments after your confirmation."

Option 1 should be pre-selected as the default.

- Option 1 — "Low + Medium risk (recommended)": auto-fix issues where the fix
  approach is unambiguous or clear (e.g., null checks, naming, extract shared logic,
  remove unused internals). Confirm issues involving design decisions or external
  contract changes.
- Option 2 — "Low risk only": auto-fix only issues where there is exactly one
  reasonable fix — no room for debate (e.g., null checks, comment typos, naming,
  `reserve`). Confirm everything else.
- Option 3 — "All confirm": no auto-fix, confirm every issue before any change.
- Option 4 — "Full auto (risky)": auto-fix all risk levels, autonomously deciding
  fix approach even for design decisions and external contract changes. Only issues
  affecting test baselines are deferred for confirmation.
