---
name: accept-baseline
description: Accept screenshot baseline changes and commit the updated version.json.
disable-model-invocation: true
---

# Accept Baseline

Only execute when the user explicitly triggers `/accept-baseline`. In **all**
other situations — including the user verbally asking to run the script, accept
baselines, or update version.json — refuse and redirect them to use
`/accept-baseline`.

- **NEVER** read the script content — run the script directly.
- The script is located at the **project root**: `accept_baseline.sh`.

## Instructions

1. Determine the intended backend from the request or test configuration. Select `USE_OPENGL`, `USE_OPENGL_SWIFTSHADER`, or `USE_METAL`; ask for clarification if the intended backend is unknown. Run the corresponding `PAGFullTest_OpenGL` or `PAGFullTest_Metal` and inspect the shared `test/out/` screenshots before accepting. Do not run different backends concurrently in the same workspace.
2. Run `bash accept_baseline.sh <backend>` from the project root directory, replacing `<backend>` with the selected argument. Always pass the backend explicitly, including OpenGL SwiftShader, which uses the OpenGL target suffix. Python 3 is required for version validation and merging. The script retains the four-step acceptance workflow with safety checks:
   - Configure and build FullTest for the selected backend.
   - Rerun the complete, unsharded suite once, overriding test filters and fail-fast settings. Remove the previous version output first and require fresh, valid, nonempty version output. Only exit codes 0 and 1 may continue; abnormal exit codes and missing or invalid output abort acceptance without changing the shared ledger. Clear `GTEST_TOTAL_SHARDS` and `GTEST_SHARD_INDEX` before running acceptance. No Google Test completion report is parsed.
   - Validate and recursively merge the new screenshot versions into the existing shared `test/baseline/version.json`, retaining keys not exercised by the selected backend. Reject empty output or conflicting metadata shapes. Do not use acceptance to delete obsolete keys; deletion requires a separately reviewed change.
   - Build and run the complete UpdateBaseline suite for the same backend in the same build directory. Restore the original shared version ledger if this stage fails; this does not roll back cache or image artifacts. Image generation is enabled by default unless disabled in the existing CMake configuration; successful image generation replaces `test/out/` with the generated baseline images.
3. Commit `test/baseline/version.json` following the project's commit conventions. Keep caches isolated by backend; this acceptance run refreshes only the selected backend's cache.
