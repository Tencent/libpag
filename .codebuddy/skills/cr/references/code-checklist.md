# Code Review Checklist

Review in priority order: A (highest impact) → B → C. The reviewer prompt specifies
which levels to check. Test code: only check for obvious implementation errors,
plus A8 (Test Correctness).

Project rules loaded in context override this checklist.

---

## A. Correctness, Safety & Structural Integrity

Issues that directly affect runtime behavior **or** silently degrade codebase
quality while passing all tests.

### A1. Dead Code & Unreachable Paths
> Scope: the **entire changeset** (not just individual hunks). Search the project
> for call sites, enum value usage, and reachability before clearing an item.
- New function / method has at least one actual call site — verify with project-wide
  search (a function that is only defined but never called is dead code)
- No conditional branch that can never execute (e.g., checking a removed enum value,
  condition always true/false by contract, type narrowing that eliminates a case)
- No parameter accepted but never read in the function body
- No variable assigned but never subsequently used
- No leftover scaffolding from a previous approach that the current code supersedes
  (e.g., old helper now bypassed by a new code path, compatibility shim for a
  removed call site)
- switch / if-else covers only values that actually occur at runtime — no
  speculative "just in case" branches for states the system cannot produce

### A2. Incomplete Refactoring
> When any function signature, return type, enum, or shared data structure changes
> in this diff, **search the entire project for every consumer** and verify each
> one is updated. Partial updates are a top-priority finding.
- All call sites of a changed function use the new signature correctly (no silent
  fallback to a default-parameter overload that hides the omission)
- Renamed / removed enum values are not referenced anywhere else in the project
- Old code paths that the refactoring intended to replace are actually removed, not
  left alongside the new path
- Shared types / structs modified in this diff — all readers and writers updated
  consistently
- No mix of old and new API style in the same changeset (e.g., half the callers
  pass the new parameter, half still use the old form)
- When a variant / union / tagged type gains or loses a member, **every** visit /
  switch / pattern-match site handles the full set — no silent fall-through to a
  default branch that masks a missing case

### A3. Code Duplication & Consolidation
> "Duplication" includes near-identical logic with renamed variables, same algorithm
> re-derived in a different file, and same constant / formula hard-coded in
> multiple places. The reviewer **must search the project** — duplication usually
> spans files, not lines.
- No two code blocks (across the entire project, not just the diff) that perform
  the same logical operation — extract to a shared function or call the existing one
- No "almost identical" blocks that differ only in variable names or minor details —
  parameterize or template the shared logic
- Constants / magic numbers / formulas that appear in 2+ locations consolidated into
  a single named definition (e.g., a multiplier used in several files should be one
  `constexpr` constant, not repeated literals)
- Configuration-like data (default values, thresholds, format strings) that logically
  belongs together not scattered across unrelated files — centralize into a config
  struct, namespace, or file
- New helper / utility function does not duplicate an existing one elsewhere in the
  project — search for similar names and similar logic before accepting

### A4. Asymmetric & Inconsistent Implementation
> When two or more code paths handle the same *kind* of object or operation (e.g.,
> standalone text vs. text-box text, import vs. export, two similar node types),
> compare them side-by-side.
- Parallel code paths use the same validation / guard strategy (e.g., if one path
  null-checks before use, the analogous path must too — unless the contract provably
  differs)
- Error-handling style consistent across sibling functions (same error → same
  response, not silently swallowed in one place and propagated in another)
- Lifecycle operations symmetric: every acquire has a release, every open has a
  close, every register has an unregister — and the pairing style is the same across
  analogous code paths
- Data transformation (e.g., coordinate conversion, unit scaling) uses the same
  formula / helper everywhere — no ad-hoc re-derivation in a single call site
- New code follows the project's established pattern for the same kind of task
  (e.g., if the project uses callbacks for async, don't introduce a different async
  mechanism without justification)

### A5. Semantic Drift
> Check whether names still match reality **in the final state of the diff**.
> This catches cases where code evolved but identifiers kept their original names.
- Variable / field names still accurately describe the value they hold after all
  changes in this diff (e.g., a variable named `visibleCount` must not store an
  index or a flag)
- Function / method names still accurately describe their current behavior — if
  the body changed meaning, the name must follow
- When a function's contract changes (accepts wider input, returns different type,
  has new side effects), its name reflects the new contract — not the old one

### A6. Code Correctness
- Return values / out-parameters set correctly in all branches (including error paths)
- Conditional logic free of && / || mix-ups, missing negation, precedence errors
- switch/case covers all branches with no unintended fall-through

### A7. Boundary Conditions
> For internal (non-public-API) functions: if callers provably guarantee a
> precondition (e.g., non-null, non-empty, within range), the guard is
> unnecessary — do not flag. Verify the guarantee by reading actual callers.
>
> Conversely, a defensive check that silently swallows an illegal state (e.g.,
> `if (!ptr) return;` when null indicates a real bug upstream) can **mask** the
> root cause. Flag when a guard hides what should be an error.
- Division-by-zero protected (both float and integer)
- Empty container checked before front() / back() / operator[]
- Null / nil / undefined dereference guarded
- Integer overflow / underflow handled (especially unsigned subtraction)
- Array / string bounds checked
- Defensive early-return or default value does not silently hide an upstream bug —
  if the guarded condition should never happen, prefer an assertion or error over
  a silent return

### A8. Test Correctness
> Applies when the diff includes new or modified test code. Priority A because a
> wrong test is worse than no test — it provides false confidence.
- Test expected values derived from independent reasoning (specification, manual
  calculation, known reference) — not copied from the implementation under test
- Tests verify **behavior** (given input X, output should be Y) rather than
  merely exercising the code path without meaningful assertions
- Test does not re-implement the production algorithm to compute the expected
  result (if the algorithm is wrong, both production and test will agree)
- Boundary / error test cases assert the specific expected outcome — not just
  "does not crash"

### A9. Comment–Code Synchronization
> Stale comments are a correctness hazard: future maintainers (including AI) trust
> them and propagate the wrong understanding. Promoted to A-level because the harm
> compounds silently. For comment **completeness** (missing descriptions, missing
> design-intent notes), see C6.
- When a function body changes behavior, its doc-comment / header comment is updated
  in the same commit
- Inline comments adjacent to changed lines still describe the current logic — not
  a previous version
- TODO / FIXME / HACK comments reference issues that still exist — remove those
  that have been resolved

### A10. Error Handling
- I/O operation results checked for errors
- Parse results validated before use
- External input validated for legality
- Failed calls have reasonable fallback / safe return
- Promises / async calls properly awaited with error handling

### A11. Injection & Sensitive Data
- User input sanitized before DOM insertion (innerHTML, dangerouslySetInnerHTML,
  v-html, [innerHTML], document.write, etc.)
- URL parameters, localStorage, postMessage data validated before use
- No hard-coded API keys, tokens, or credentials in client-side code

### A12. Resource Management
- Manually allocated resources released on all paths (prefer RAII)
- File handles / system resources properly closed
- Lock acquire and release properly paired
- Database connections / network sockets released in finally / defer blocks
- Event listeners, timers, subscriptions, observers cleaned up on unmount / scope
  exit

### A13. Memory Safety
- No use-after-move (moved-from object not reused)
- No dangling reference / pointer to local variable
- Container element reference not used after container modification

### A14. Thread Safety
> Only flag when the access pattern is clearly unsafe.
- Shared mutable state accessed without lock or atomic
- Callback / closure captures a reference or pointer whose lifetime may end before
  invocation
- Condition variable wait without predicate (spurious wakeup)

---

## B. Refactoring & Optimization

Improvements to code quality, performance, and maintainability.

### B1. Performance
- Container space pre-allocated when size is predictable
- No unnecessary deep copies (only flag when semantic equivalence is certain)
- Loop-invariant expressions hoisted outside loops
- Frequent string concatenation inside loops optimized
- No unnecessary temporary object construction
- Large objects passed by const& instead of by value
- No unnecessary re-renders from missing memoization, unstable references, or inline
  object/function creation in props
- No full imports of large dependencies when only a small part is used

### B2. Code Simplification
- Deep nested if/else simplified with early return
- Redundant conditional checks merged or eliminated
- Overly long functions split into single-responsibility sub-methods
- Over-engineered abstraction (base class with one subclass, interface with one
  implementation, template instantiated one way, wrapper that just forwards) replaced
  with direct implementation

### B3. Module Architecture
> Only flag when the diff introduces a new dependency or moves code across module
> boundaries.
- Module responsibilities clear with no boundary violations
- Dependency direction reasonable
- No circular dependencies

### B4. Interface Usage
- Called APIs used according to their design intent and documentation
- No use of deprecated interfaces

### B5. Interface Changes
> Flag only — describe the change and its scope for the coordinator to assess.
- Public API signature or class interface changes identified and described

### B6. Test Coverage
> Flag only — report for awareness, do not auto-fix.
- Changed logic paths have corresponding test cases
- Boundary conditions have test coverage
- Error paths have test coverage

### B7. Regression Risk
> Flag only — report for awareness, do not auto-fix.
- Modification impact on other callers assessed
- Behavior changes consistent across all target platforms

### B8. Rendering Correctness
- List items rendered with stable, unique key (not array index)
- Side effects correctly placed in lifecycle hooks / useEffect with proper dependency
  arrays
- Component state derived correctly (no stale closures, no out-of-sync derived state)

---

## C. Conventions & Documentation

Coding standards and documentation consistency.

### C1. Project Conventions
- Naming follows project's existing style (per loaded project rules)
- Variable names semantically clear, no unnecessary abbreviations
- Names in new code consistent with style in the same file
- Variables assigned initial value at declaration (per project rules)
- Class member variables initialized at declaration or in constructor
- Code complies with language usage restrictions in project rules
- New code consistent with existing patterns in the project

### C2. File Organization
- Function order in implementation files matches declaration order
- Header files have appropriate include guards
- Include / import dependencies necessary and reasonable

### C3. Type Safety
- No implicit narrowing conversions (large type → small type)
- No signed / unsigned mixed comparisons
- Magic numbers extracted as named constants (unless context already makes meaning
  clear)

### C4. Const Correctness
- Methods that don't modify state marked const
- Unmodified parameters passed as const references
- Unmodified local variables declared const

### C5. Documentation Consistency
- Type names / enum names in code consistent with project documentation
- Value ranges in comments consistent with specification documents

### C6. Public API Comments
- Public API comments accurately describe current behavior, parameters, return values
- Value ranges, constraints, error conditions in comments match implementation
- Public APIs have sufficient parameter and return value descriptions
- Design intent explanations present where code alone is insufficient

### C7. Accessibility
- Images have meaningful alt text (empty alt for decorative images)
- Form inputs have associated labels
- Interactive elements keyboard-navigable with semantic HTML

---

## Exclusion List

> Project rules override this exclusion list. If project rules have explicit requirements
> for an excluded issue type, that type is **not excluded** — review per project rules.

1. Pure style preferences within formatting tool scope (not required by project rules)
2. Formatting already handled by project formatting tools (indentation, whitespace, etc.)
3. Suggestions based on assumed future requirements, not current code
4. Code following project's existing style but not matching some external standard
5. Priority C issues in test code (unless project rules require otherwise)
6. "Better alternative" suggestions for existing stable, bug-free code
7. Missing guards in internal functions when callers provably guarantee the precondition
   (only applies to non-public-API code; verify by reading actual call sites)
