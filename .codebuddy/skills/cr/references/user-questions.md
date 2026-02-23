# User Questions — Phase 0.2

Present all applicable questions in **a single interactive prompt**.

## Question 1 — Review priority

Always show. Priority levels apply to both code and document review checklists.

- Option 1 — "Full review (A + B + C)": correctness, optimization, and conventions.
  Code: null checks, duplicate code, naming. Docs: factual errors, clarity, formatting.
- Option 2 — "Correctness + optimization (A + B)": skip conventions and style.
  Code: null checks, resource leaks, simplification. Docs: factual errors, clarity.
- Option 3 — "Correctness only (A)": only safety and correctness issues.
  Code: null dereference, out-of-bounds, race conditions. Docs: factual errors,
  contradictions.

## Question 2 — Auto-fix threshold (skip in PR mode)

In PR mode, add a note alongside Q1: "PR mode — issues will be submitted as
line-level PR review comments after your confirmation."

Option 1 should be pre-selected as the default.

- Option 1 — "Low + Medium risk (recommended)": auto-fix most issues, only confirm
  high-risk ones (e.g., API changes, architecture decisions).
- Option 2 — "Low risk only": auto-fix only the most straightforward issues (e.g.,
  null checks, typos, naming). Confirm everything else.
- Option 3 — "All confirm": no auto-fix, confirm every issue before any change.
- Option 4 — "Full auto (risky)": auto-fix everything. Only issues affecting test
  baselines are deferred for confirmation.
