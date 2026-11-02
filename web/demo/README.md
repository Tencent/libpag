# demo

Browser demo pages for every build variant (GL / WebGPU x single-thread / multi-thread).

## Generated files — do not edit

`index.html`, `index-st.html`, `index-webgpu.html`, and `index-webgpu-mt.html` are **generated**
from `index.template.html` by `script/gen.demo.html.js` (one variant per entry in
`script/targets.js`, with the output directory substituted for `__DEMO_OUT_DIR__`). They are
gitignored build outputs: edits to them are lost the next time a build regenerates them.

To change a demo page, edit `index.template.html` (or `index.ts` for behavior) and regenerate:

```bash
node script/gen.demo.html.js   # from the web/ directory
```

Any rollup build (`npm run build:st`, `npm run build:webgpu:st`, `npm run dev:*`) also runs the
generator automatically, so a normal build picks template changes up.

## Entry points

- `index.ts` is the single TypeScript entry for all variants. The lib directory and backend are
  injected at build time by the esbuild `define` option (`__PAG_DEMO_LIB_DIR__`,
  `__PAG_DEMO_BACKEND__`); do not branch on them manually — the build already does.
- Each generated html loads `../<wasmDir>/index.js`, which is why the html files cannot be merged
  into one: the script tag is the load entry itself and must point at a concrete output directory.
