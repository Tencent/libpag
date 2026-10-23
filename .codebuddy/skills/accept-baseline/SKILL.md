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
2. Run `bash accept_baseline.sh <backend>` from the project root directory, replacing `<backend>` with the selected argument. Always pass the backend explicitly, including OpenGL SwiftShader, which uses the OpenGL target suffix. The script follows main's four-step acceptance workflow:
   - Configure and build FullTest for the selected backend.
   - Rerun that FullTest and continue even when screenshot comparisons fail.
   - Copy `test/out/version.json` over `test/baseline/version.json`; this replaces the file rather than merging keys.
   - Build and run UpdateBaseline for the same backend in the same build directory. Image generation is enabled by default unless disabled in the existing CMake configuration; successful image generation replaces `test/out/` with the generated baseline images.
3. Commit `test/baseline/version.json` following the project's commit conventions. Keep caches isolated by backend; this acceptance run refreshes only the selected backend's cache.
