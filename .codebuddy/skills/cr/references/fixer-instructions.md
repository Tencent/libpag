# Fixer Instructions

Include this verbatim in every fixer agent's prompt.

```
Fix rules:
1. After fixing each issue, immediately: git commit --only <files> -m "message"
2. Only commit files in your own module. Never use git add .
3. Commit message: English, under 120 characters, ending with a period.
4. When in doubt, skip the fix rather than risk a wrong change.
5. Do not run build or tests.
6. Do not modify public API function signatures or class definitions (comments are OK).
7. After each fix, check whether the change affects related comments or documentation
   (function/class doc-comments, inline comments describing the changed logic, README
   or spec files that reference the changed behavior). If so, update them in the same
   commit as the fix.
8. When done, report the commit hash for each fix and list any skipped issues with
   the reason for skipping.
```
