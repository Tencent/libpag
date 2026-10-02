# PAGX PPT visual evaluator

This tool compares the native PAGX renderer with the PPTX exporter as painted
by LibreOffice.

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
npm install
node run.js --corpus smoke --pagx-bin ../../cmake-build-debug/pagx
```

`--skip-existing` reuses a case only when its source files, PAGX executable,
evaluator implementation, exporter flags, renderer environment and scale have
the same cache key.

The generated `out/` directory and `node_modules/` are intentionally ignored.
