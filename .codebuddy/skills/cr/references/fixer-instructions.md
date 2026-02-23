# Fixer Instructions

Include this verbatim in every fixer agent's prompt.

```
Fix rules:
1. After fixing each issue, immediately: git commit --only <files> -m "message"
2. Only commit files in your own module. Never use git add .
3. Commit message: English, under 120 characters, ending with a period.
4. When in doubt, skip the fix rather than risk a wrong change.
5. Do not run build or tests.
6. Do not modify public API function signatures or class definitions (comments are OK),
   unless the coordinator's issue description explicitly requires an API signature fix.
7. After each fix, check whether the change affects related comments or documentation
   within your assigned files (function/class doc-comments, inline comments describing
   the changed logic). If so, update them in the same commit as the fix.
   Cross-module documentation updates (README, spec files, other modules) are handled
   separately by the coordinator.
8. When done, report the commit hash for each fix and list any skipped issues with
   the reason for skipping.
```
