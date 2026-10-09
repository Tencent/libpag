# PAGX PPT visual evaluator

This tool compares the native PAGX renderer with the PPTX exporter as painted
by LibreOffice.

Supported hosts are macOS and Linux. Native Windows is not supported: executable
discovery and timeout cleanup rely on POSIX process groups. On Windows, run the
toolchain inside WSL with Linux builds of PAGX, LibreOffice and Poppler.

Use Node.js 22.12.0 or newer (or Node.js 20.x starting at 20.19.0). The CommonJS
evaluator loads the ESM-only `pixelmatch` package through `require(esm)`, which
must be available without experimental flags.

For each case it:

1. runs `pagx render` to create the reference PNG;
2. runs `pagx export --format pptx`;
3. converts PPTX to PDF with an isolated headless LibreOffice profile;
4. rasterizes every PDF page at 96 DPI with `pdftocairo` or `pdftoppm`;
5. writes whole-page and content-ROI metrics plus visual diff images.

The repository entry point is [`test/run_ppt_eval.sh`](../../test/run_ppt_eval.sh).
Corpus definitions and baseline documentation live in
[`resources/ppt`](../../resources/ppt/README.md).

Direct invocation:

```bash
npm ci
npm test
node run.js --corpus smoke --pagx-bin ../../cmake-build-debug/pagx
```

`--skip-existing` reuses a case only when its source files, PAGX executable,
declared external resources, evaluator implementation and dependency lockfile,
exporter flags, renderer environment and scale have
the same cache key.

The shell driver and CMake `PPTTest` default to the `smoke` corpus, the only corpus
with a committed baseline, and require a matching renderer environment baseline.
Select additional corpora with shell arguments or `PPT_EVAL_CORPORA`; expand the
default set only after their baselines have been seeded and visually reviewed.
Use `PPT_EVAL_REQUIRE_BASELINE=0 test/run_ppt_eval.sh <corpus>` for an explicit
report-only run. Empty baselines and all-SKIP runs print a warning. Baseline
updates report `UPDATED` and do not run a regression gate against their own data.
Both entry points reject Git LFS pointers in corpus assets and repository fonts.

For a report-only run of all corpora from the repository root:

```bash
PPT_EVAL_REQUIRE_BASELINE=0 test/run_ppt_eval.sh features layout text cli spec smoke decks
```

The generated `out/` directory and `node_modules/` are intentionally ignored.
