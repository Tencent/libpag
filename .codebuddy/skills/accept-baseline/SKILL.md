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

1. The script requires `test/out/version.json`, which is produced by a full test run. If it is
   missing, build and run `PAGFullTest_<Backend>` for the current backend first (e.g.
   `PAGFullTest_OpenGL`, or `PAGFullTest_Metal` on the Metal build), confirm the screenshots in
   `test/out/` match expectations, then proceed. The script itself does not build or run tests.
2. Run `bash accept_baseline.sh` from the project root directory.
3. Commit `test/baseline/version.json` following the project's commit conventions.
