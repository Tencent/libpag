# Code Review Checklist

Reviewers check all items in this checklist. Levels A/B/C indicate review priority —
start with Level A (highest impact), then B, then C. The reviewer prompt specifies
which levels to check.

Test code has reduced review requirements — only focus on obvious implementation errors.

**Language applicability**: Items marked with a language tag (e.g., `[C/C++]`) apply only
to that language. Unmarked items apply to all languages. Skip items that do not match the
project's language.

**Public API protection**: Do not suggest changes to public API function signatures or
class interface definitions, unless it is an obvious bug or a comment issue. Focus on
internal implementation refactoring and optimization.

**Project-specific rules**: Project rules already loaded in the agent's context take
priority over this generic checklist. When project rules have explicit requirements for
a particular issue type, follow the project rules.

---

## Priority A: Correctness & Safety

Issues that directly affect runtime behavior. Highest impact — review these first.

**A1. Code Correctness**
- Are function return values / out-parameters correctly set in all branches (including
  error branches)?
- Does the implementation match the behavior described by the function name / comments?
- Are conditional logic expressions correct (&& / || mix-ups, missing negation,
  operator precedence errors)?
- Do switch/case statements cover all branches? Any unintended fall-through?

**A2. Boundary Conditions**
- Division-by-zero protection (both floating-point and integer)
- Empty container operations (check empty() before front() / back() / operator[])
  `[C/C++]`
- Null/nil/undefined dereference
- Integer overflow / underflow (especially unsigned subtraction `[C/C++]`)
- Array / string out-of-bounds access

**A3. Error Handling**
- Is error status checked after I/O operations?
- Is result validity verified after string / data parsing?
- Is external input validated for legality?
- Is there a reasonable fallback / return when function calls fail?
- Are promises / async calls properly awaited with error handling?
  `[JS/TS/Python/async languages]`

**A4. Injection & Sensitive Data** `[Web]`
- Is user input inserted into the DOM without sanitization (innerHTML,
  dangerouslySetInnerHTML, v-html, [innerHTML], document.write, etc.)?
- Are URL parameters, localStorage, or postMessage data used without validation?
- Are API keys, tokens, or credentials hard-coded in client-side code?

**A5. Resource Management**
- Are manually allocated resources correctly released on all paths (prefer RAII)?
  `[C/C++]`
- Are file handles / system resources properly closed?
- Are lock acquire and release properly paired? `[C/C++/Java/Go]`
- Is there a risk of pointer / iterator invalidation after container resize? `[C/C++]`
- Are database connections / network sockets properly released in finally/defer blocks?
- Are event listeners, timers, subscriptions, and observers cleaned up on component
  unmount or scope exit? `[Web]`

**A6. Memory Safety** `[C/C++]`
- Use-after-move: is a moved-from object used again?
- Dangling reference / pointer: returning reference / pointer to a local variable?
- Is a container element reference still used after the container is modified?

**A7. Thread Safety** (when applicable)
- Is shared data protected against concurrent access?
- Are there race conditions on mutable shared state?

**A8. Public API Comment Accuracy**
- Do public API comments accurately describe current behavior, parameters, and return
  values?
- Are value ranges, constraints, and error conditions in comments consistent with the
  actual implementation?
- Are comments updated when the corresponding API behavior changes?

---

## Priority B: Refactoring & Optimization

Improvements to code quality, performance, and maintainability. Medium impact.

**B1. Performance Optimization**
- Containers: is space pre-allocated when the size is predictable?
- Unnecessary deep copies: **must be 100% certain of semantic equivalence; annotate
  the risk of the change**
- Repeated computation inside loops: can expressions be moved outside the loop?
- String operations: can frequent concatenation inside loops be optimized?
- Unnecessary temporary object construction `[C/C++]`
- Const references: are parameters / variables that can be const& marked as such?
  `[C/C++]`
- Are components re-rendering unnecessarily due to missing memoization, unstable
  references, or inline object/function creation in props? `[React/Vue/Web]`
- Are large dependencies imported in full when only a small part is used
  (tree-shaking inefficiency)? `[Web]`

**B2. Code Simplification**
- Duplicate code: >= 3 identical patterns should be extracted into a method
- Deep nested if/else: can be simplified with early return
- Redundant conditional checks: logic branches that can be merged or eliminated
- Overly long functions: single-responsibility logic blocks can be extracted as
  sub-methods
- Similar branch logic that can be merged

**B3. Module Architecture**
- Are module responsibilities clear? Is there any responsibility boundary violation?
- Is the dependency direction reasonable?
- Are there circular dependencies?

**B4. Interface Usage**
- Are called APIs used according to their design intent and documentation?
- Are any deprecated interfaces being used?
- Parameter passing: are large objects passed by const reference instead of by value?
  `[C/C++]`

**B5. Interface Changes**
- Are public API changes necessary and justified?
- Are backward compatibility implications considered?

**B6. Test Coverage**
- Do changed logic paths have corresponding test cases?
- Do boundary conditions have test coverage?
- Do error paths have test coverage?

**B7. Regression Risk**
- Could the modification affect other callers?
- Are behavior changes consistent across all target platforms?

**B8. Rendering Correctness** `[Web]`
- Are list items rendered with a stable, unique key (not array index)?
- Are side effects correctly placed in lifecycle hooks / useEffect with proper
  dependency arrays?
- Is component state derived correctly (no stale closures, no out-of-sync
  derived state)?

---

## Priority C: Conventions & Documentation

Coding standards and documentation consistency. Lower impact on functionality.

**C1. Naming Conventions**
- Does the code follow the project's existing naming style (as defined in the loaded
  project rules)?
- Are variable names semantically clear, avoiding unnecessary abbreviations?
- Are names in new code consistent with the style in the same file?

**C2. Initialization Conventions**
- Are variables assigned an initial value at declaration (as required by project rules)?
- Are class member variables initialized at declaration or in the constructor?
  `[C/C++/Java]`

**C3. Project Language Conventions**
- Does the code comply with language usage restrictions defined in the project rules
  (varies by project)?
- Is new code consistent with existing patterns in the project?

**C4. Comment Conventions**
- Do public APIs have sufficient parameter and return value descriptions?
- Are design intent explanations present where code alone is insufficient?

**C5. File Organization**
- Is the function order in implementation files consistent with the declaration order?
  `[C/C++]`
- Do header files have appropriate include guards? `[C/C++]`
- Are include / import dependencies necessary and reasonable?

**C6. Type Safety**
- Implicit narrowing conversions (large type -> small type) `[C/C++/Java]`
- Signed / unsigned mixed comparisons `[C/C++]`
- Magic numbers: hard-coded values should be extracted as named constants (unless
  the context already makes it clear)

**C7. Const Correctness** `[C/C++]`
- Are methods that don't modify state marked const?
- Are parameters that aren't modified passed as const references?
- Are local variables that aren't modified declared const?

**C8. Documentation Consistency**
- Are type names / enum names in code consistent with project documentation?
- Are value ranges in comments consistent with specification documents?

**C9. Accessibility** `[Web]`
- Do images have meaningful alt text (or empty alt for decorative images)?
- Do form inputs have associated labels?
- Are interactive elements keyboard-navigable and using semantic HTML?

---

## Exclusion List (applies to all levels)

> **Important**: The items below are default exclusions. If the project rules loaded in
> context have explicit requirements for a particular issue type (e.g., "no lambdas",
> "variables must be initialized"), that issue type is **not excluded** and should be
> reviewed per the project rules. Project rules take priority over this exclusion list.

**Issue types excluded by default:**

1. Pure style preferences (within the scope of formatting tools, and not explicitly
   required by project rules)
2. Formatting issues already handled by the project's formatting tools (indentation,
   whitespace, line breaks, etc.)
3. "Possible" issues without code evidence to support them (speculative issues)
4. Code that follows the project's existing style but does not match some external
   standard
5. Non-critical issues in test code (variable naming, comment style, etc., unless
   project rules have specific requirements)
6. Suggestions for "better alternative implementations" for existing stable code (if
   the current implementation has no bugs)
7. Public API signature change suggestions (obvious bugs are fixed normally; non-bug
   changes are not excluded but go through `pending-issues.md` for user confirmation
   in Phase 8)
