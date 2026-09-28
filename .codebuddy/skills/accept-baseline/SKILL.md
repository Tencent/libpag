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

1. Run `bash accept_baseline.sh` from the project root directory.
2. Commit `test/baseline/version.json` following the project's commit conventions.
