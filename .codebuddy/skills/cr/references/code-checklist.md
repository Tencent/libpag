# Code Review Checklist

Review in priority order: A (highest impact) → B → C. The reviewer prompt specifies
which levels to check. Test code: only check for obvious implementation errors.

Project rules loaded in context override this checklist.

---

## A. Correctness & Safety

Issues that directly affect runtime behavior.

### A1. Code Correctness
- Return values / out-parameters set correctly in all branches (including error paths)
- Implementation matches behavior described by function name / comments
- Conditional logic free of && / || mix-ups, missing negation, precedence errors
- switch/case covers all branches with no unintended fall-through

### A2. Boundary Conditions
> For internal (non-public-API) functions: if callers provably guarantee a
> precondition (e.g., non-null, non-empty, within range), the guard is
> unnecessary — do not flag. Verify the guarantee by reading actual callers.
- Division-by-zero protected (both float and integer)
- Empty container checked before front() / back() / operator[]
- Null / nil / undefined dereference guarded
- Integer overflow / underflow handled (especially unsigned subtraction)
- Array / string bounds checked

### A3. Error Handling
- I/O operation results checked for errors
- Parse results validated before use
- External input validated for legality
- Failed calls have reasonable fallback / safe return
- Promises / async calls properly awaited with error handling

### A4. Injection & Sensitive Data
- User input sanitized before DOM insertion (innerHTML, dangerouslySetInnerHTML,
  v-html, [innerHTML], document.write, etc.)
- URL parameters, localStorage, postMessage data validated before use
- No hard-coded API keys, tokens, or credentials in client-side code

### A5. Resource Management
- Manually allocated resources released on all paths (prefer RAII)
- File handles / system resources properly closed
- Lock acquire and release properly paired
- Database connections / network sockets released in finally / defer blocks
- Event listeners, timers, subscriptions, observers cleaned up on unmount / scope
  exit

### A6. Memory Safety
- No use-after-move (moved-from object not reused)
- No dangling reference / pointer to local variable
- Container element reference not used after container modification

### A7. Thread Safety
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
- Clearly duplicated or similar logic extracted (judge by complexity and maintenance
  cost, not count threshold)
- Deep nested if/else simplified with early return
- Redundant conditional checks merged or eliminated
- Overly long functions split into single-responsibility sub-methods

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
- Comments updated when corresponding API behavior changes
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
