# Code Review Checklist

Review in priority order: A (highest impact) → B → C. The reviewer prompt specifies
which levels to check. Test code: only check for obvious implementation errors.

Items tagged `[C/C++]`, `[Web]`, etc. apply only to that language — skip non-matching
items. Project rules loaded in context override this checklist.

---

## A. Correctness & Safety

Issues that directly affect runtime behavior.

### A1. Code Correctness
- Return values / out-parameters set correctly in all branches (including error paths)
- Implementation matches behavior described by function name / comments
- Conditional logic free of && / || mix-ups, missing negation, precedence errors
- switch/case covers all branches with no unintended fall-through

### A2. Boundary Conditions
- Division-by-zero protected (both float and integer)
- Empty container checked before front() / back() / operator[] `[C/C++]`
- Null / nil / undefined dereference guarded
- Integer overflow / underflow handled (especially unsigned subtraction) `[C/C++]`
- Array / string bounds checked

### A3. Error Handling
- I/O operation results checked for errors
- Parse results validated before use
- External input validated for legality
- Failed calls have reasonable fallback / safe return
- Promises / async calls properly awaited with error handling `[async]`

### A4. Injection & Sensitive Data `[Web]`
- User input sanitized before DOM insertion (innerHTML, dangerouslySetInnerHTML,
  v-html, [innerHTML], document.write, etc.)
- URL parameters, localStorage, postMessage data validated before use
- No hard-coded API keys, tokens, or credentials in client-side code

### A5. Resource Management
- Manually allocated resources released on all paths (prefer RAII) `[C/C++]`
- File handles / system resources properly closed
- Lock acquire and release properly paired `[C/C++/Java/Go]`
- Database connections / network sockets released in finally / defer blocks
- Event listeners, timers, subscriptions, observers cleaned up on unmount / scope
  exit `[Web]`

### A6. Memory Safety `[C/C++]`
- No use-after-move (moved-from object not reused)
- No dangling reference / pointer to local variable
- Container element reference not used after container modification

### A7. Thread Safety
- Shared data protected against concurrent access
- No race conditions on mutable shared state

### A8. Public API Comment Accuracy
- Public API comments accurately describe current behavior, parameters, return values
- Value ranges, constraints, error conditions in comments match implementation
- Comments updated when corresponding API behavior changes

---

## B. Refactoring & Optimization

Improvements to code quality, performance, and maintainability.

### B1. Performance
> Only flag with high confidence in semantic equivalence. Prefer precision over recall.
- Container space pre-allocated when size is predictable
- No unnecessary deep copies (must be certain of semantic equivalence)
- Loop-invariant expressions hoisted outside loops
- Frequent string concatenation inside loops optimized
- No unnecessary temporary object construction `[C/C++]`
- Large objects passed by const& instead of by value (for const conventions see C7)
  `[C/C++]`
- No unnecessary re-renders from missing memoization, unstable references, or inline
  object/function creation in props `[React/Vue/Web]`
- No full imports of large dependencies when only a small part is used `[Web]`

### B2. Code Simplification
- Clearly duplicated logic extracted (judge by complexity and maintenance cost, not
  count threshold)
- Deep nested if/else simplified with early return
- Redundant conditional checks merged or eliminated
- Overly long functions split into single-responsibility sub-methods
- Similar branch logic merged

### B3. Module Architecture
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

### B8. Rendering Correctness `[Web]`
- List items rendered with stable, unique key (not array index)
- Side effects correctly placed in lifecycle hooks / useEffect with proper dependency
  arrays
- Component state derived correctly (no stale closures, no out-of-sync derived state)

---

## C. Conventions & Documentation

Coding standards and documentation consistency.

### C1. Naming Conventions
- Code follows project's existing naming style (per loaded project rules)
- Variable names semantically clear, no unnecessary abbreviations
- Names in new code consistent with style in the same file

### C2. Initialization Conventions
- Variables assigned initial value at declaration (per project rules)
- Class member variables initialized at declaration or in constructor `[C/C++/Java]`

### C3. Project Language Conventions
- Code complies with language usage restrictions in project rules
- New code consistent with existing patterns in the project

### C4. Comment Conventions
- Public APIs have sufficient parameter and return value descriptions
- Design intent explanations present where code alone is insufficient

### C5. File Organization
- Function order in implementation files matches declaration order `[C/C++]`
- Header files have appropriate include guards `[C/C++]`
- Include / import dependencies necessary and reasonable

### C6. Type Safety
- No implicit narrowing conversions (large type → small type) `[C/C++/Java]`
- No signed / unsigned mixed comparisons `[C/C++]`
- Magic numbers extracted as named constants (unless context already makes meaning
  clear)

### C7. Const Correctness `[C/C++]`
- Methods that don't modify state marked const
- Unmodified parameters passed as const references
- Unmodified local variables declared const

### C8. Documentation Consistency
- Type names / enum names in code consistent with project documentation
- Value ranges in comments consistent with specification documents

### C9. Accessibility `[Web]`
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
