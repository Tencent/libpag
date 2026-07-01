// CLI argument parsing for html-snapshot. Exports a `parseArgs(argv)` that
// mirrors `process.argv`-shaped input (so callers can pass `process.argv`
// directly and we slice past the `node` + script entries internally) and a
// `printUsage()` that writes the help text to stdout.
//
// Generic helpers that used to live here (errMessage, isHttpUrl, fail,
// makeFail, parseNumber, LOG_PREFIX, SNAPSHOT_DEFAULTS) were moved to
// `lib/common.ts` so non-CLI modules (capture-listener, font-download,
// snapshot-runner, …) no longer have to depend on the CLI parser to reach
// for them. HTTP-only validators (validateCookies, validateHeaders) live in
// `lib/http-utils.ts`.

import * as path from 'path';
import { SUPPORTED_ENGINES, resolveEngine, type EngineName } from './browser-engine';
import { SNAPSHOT_DEFAULTS, fail, parseNumber, isHttpUrl } from './common';

export interface ParsedCookie {
  name: string;
  value: string;
}

export type ParsedHeader = [string, string];

export interface SnapshotCliOptions {
  input: string;
  output: string;
  outputToStdout: boolean;
  isUrl: boolean;
  viewportWidth: number;
  viewportHeight: number;
  waitMs: number;
  selector: string;
  cookies: ParsedCookie[];
  headers: ParsedHeader[];
  inlineIconFonts: boolean;
  scrollReveal: boolean;
  runtimeAnimWindowMs: number;
  runtimeAnimSampleCount: number;
  downloadFonts: boolean;
  fontDir: string;
  fontManifest: string;
  downloadImages: boolean;
  imageDir: string;
  browserEngine: EngineName;
}

// Parse `name=value` into a puppeteer-compatible cookie descriptor. The URL
// must be filled in by the caller (we do that once we know the page URL) so
// the cookie is scoped to the right origin.
function parseCookie(flagName: string, value: string | undefined): ParsedCookie {
  if (value === undefined) {
    fail(`'${flagName}' requires a 'name=value' argument`);
  }
  const eq = value.indexOf('=');
  if (eq <= 0) {
    fail(`'${flagName}' expects 'name=value', got '${value}'`);
  }
  return { name: value.slice(0, eq), value: value.slice(eq + 1) };
}

// Parse `Key: Value` (or `Key:Value`) into a [key, value] pair. The colon
// after the key is the separator; any further colons are part of the value
// (so `Authorization: Bearer xyz:abc` works as expected).
function parseHeader(flagName: string, value: string | undefined): ParsedHeader {
  if (value === undefined) {
    fail(`'${flagName}' requires a 'Key: Value' argument`);
  }
  const colon = value.indexOf(':');
  if (colon <= 0) {
    fail(`'${flagName}' expects 'Key: Value', got '${value}'`);
  }
  const key = value.slice(0, colon).trim();
  const val = value.slice(colon + 1).trim();
  if (!key) {
    fail(`'${flagName}' has empty header name in '${value}'`);
  }
  return [key, val];
}

interface FlagSpec {
  names: string[];
  takesArg?: boolean;
  set: (opts: SnapshotCliOptions, value?: string) => void;
}

// Long-option flag table. Each entry lists the names the user can type and a
// setter that mutates `opts` with the next argv token. `-h` / `--help` and
// positional handling stay inline in `parseArgs` since they affect control
// flow rather than just options.
// Boolean toggles that don't consume the next argv token. `takesArg: false`
// distinguishes them from the value-bearing flags above so `parseArgs` knows
// not to shift the index after the flag is matched. The helper here keeps
// the table declarative.
const FLAGS: FlagSpec[] = [
  { names: ['-o', '--output'],     set: (o, v) => { o.output = v as string; } },
  { names: ['--viewport-width'],   set: (o, v) => { o.viewportWidth = parseNumber('--viewport-width', v, { min: 1 }); } },
  { names: ['--viewport-height'],  set: (o, v) => { o.viewportHeight = parseNumber('--viewport-height', v, { min: 1 }); } },
  { names: ['--wait-ms'],          set: (o, v) => { o.waitMs = parseNumber('--wait-ms', v, { min: 0 }); } },
  { names: ['--selector'],         set: (o, v) => { o.selector = v as string; } },
  { names: ['--cookie'],           set: (o, v) => { o.cookies.push(parseCookie('--cookie', v)); } },
  { names: ['--header'],           set: (o, v) => { o.headers.push(parseHeader('--header', v)); } },
  // Disable the inline-icon-font pre-pass (default: enabled). When the pass
  // is on, every PUA `::before` glyph whose font is registered via
  // `@font-face` is replaced with a vector `<svg>` extracted from the font
  // file, so the resulting PAGX no longer depends on the icon font being
  // installed on the rendering machine. Disable to fall back to the
  // legacy font-named span path (faster, no font fetch, but the PAGX file
  // becomes non-portable).
  { names: ['--no-inline-icon-fonts'], takesArg: false, set: (o) => { o.inlineIconFonts = false; } },
  // Walk the page from top to bottom (then back to the top) before taking the
  // snapshot. This fires scroll-triggered reveal animations (sections kept at
  // `opacity: 0` until an IntersectionObserver flips them visible) and forces
  // `loading="lazy"` media to load, so below-the-fold content is captured
  // instead of dropped. Off by default — it adds a few seconds per page and is
  // a no-op for pages whose content is already visible at scroll (0,0).
  { names: ['--scroll-reveal'], takesArg: false, set: (o) => { o.scrollReveal = true; } },
  // Capture animations by sampling the live, still-running page over this many
  // milliseconds of wall-clock time, instead of the default single-instant
  // pass. This is the only way to record motion driven by non-seekable
  // mechanisms — raw setTimeout chains, class toggles, JS auto-demo sequences,
  // WAAPI .animate() on freshly created nodes — which expose no clock the
  // instant/seek-based collectors can read. 0 (default) = instant pass.
  { names: ['--runtime-anim-window'], set: (o, v) => { o.runtimeAnimWindowMs = parseNumber('--runtime-anim-window', v, { min: 0 }); } },
  // Explicit sample count across the real-time window. Default (0) auto-derives
  // ~20 samples/s from the window, clamped to keep the O(samples × elements)
  // probe bounded on large pages.
  { names: ['--runtime-anim-samples'], set: (o, v) => { o.runtimeAnimSampleCount = parseNumber('--runtime-anim-samples', v, { min: 2 }); } },
  // Download every web font the page actually uses (each unicode-range
  // subset the browser fetched) and write it to disk as a plain SFNT
  // (TTF/OTF). Off by default. The files can then be handed to
  // `pagx render --fallback` / `pagx font embed --fallback` so text styled
  // with an uninstalled web font renders with the correct typeface instead
  // of a system fallback. The destination defaults to a sibling
  // `<output>.fonts/` directory; override with `--font-dir`.
  { names: ['--download-fonts'], takesArg: false, set: (o) => { o.downloadFonts = true; } },
  { names: ['--font-dir'], set: (o, v) => { o.fontDir = v as string; } },
  // Download every external image the browser fetched while rendering and
  // reference it by its on-disk file path instead of inlining the bytes as a
  // base64 `data:` URI. Off by default (images are inlined, so the snapshot is
  // self-contained). Useful to keep the subset HTML — and the PAGX produced
  // from it — small when a page carries large/full-resolution images. The
  // destination defaults to a sibling `<output>.images/` directory; override
  // with `--image-dir`.
  { names: ['--download-images'], takesArg: false, set: (o) => { o.downloadImages = true; } },
  { names: ['--image-dir'], set: (o, v) => { o.imageDir = v as string; } },
  // Write the list of font files this snapshot actually uses (one absolute
  // path per line) to <path>. With a shared --font-dir, the directory may hold
  // fonts from many pages; the manifest lets a caller (eval/run.js) hand only
  // the fonts this page needs to `pagx render` / `pagx font embed`.
  { names: ['--font-manifest'], set: (o, v) => { o.fontManifest = v as string; } },
  // Pick the headless browser driver. Defaults to puppeteer; pass
  // `playwright` to drive Chromium through Playwright instead (requires
  // `playwright` to be installed — declared as an optionalDependency).
  // The same selection is also honoured via the `HTML_SNAPSHOT_BROWSER`
  // env var; the flag wins when both are set.
  { names: ['--browser-engine'], set: (o, v) => {
    try {
      o.browserEngine = resolveEngine(v);
    } catch (err) {
      fail(err && (err as Error).message ? (err as Error).message : String(err));
    }
  } },
];

const FLAG_BY_NAME = new Map<string, FlagSpec>();
for (const flag of FLAGS) {
  for (const name of flag.names) FLAG_BY_NAME.set(name, flag);
}

export function parseArgs(argv: string[]): SnapshotCliOptions {
  const opts: SnapshotCliOptions = {
    input: '',
    output: '',
    // When true, the HTML is written to process.stdout and the
    // "wrote ..." log line is routed to process.stderr so the two
    // streams don't get mixed. Triggered by `-o -`. Used by pagx's
    // html-snapshot bridge in src/cli/CommandImport.cpp so the importer
    // can read the snapshot output through a pipe instead of a temp file
    // on disk.
    outputToStdout: false,
    isUrl: false,
    viewportWidth: SNAPSHOT_DEFAULTS.viewportWidth,
    viewportHeight: SNAPSHOT_DEFAULTS.viewportHeight,
    waitMs: SNAPSHOT_DEFAULTS.waitMs,
    selector: '',
    cookies: [],
    headers: [],
    // Inline-icon-font pre-pass: convert every PUA `::before` glyph backed
    // by a webfont (Phosphor, Material Icons, Font Awesome, Lucide, …) to
    // an inline `<svg>` so the snapshot — and the PAGX downstream — no
    // longer depends on the icon font being available at render time.
    // See lib/icon-font.ts for the pipeline; toggle via
    // `--no-inline-icon-fonts`.
    inlineIconFonts: true,
    // Scroll-reveal pre-pass: when true, the page is walked top-to-bottom
    // (then scrolled back to the top) before the snapshot so scroll-triggered
    // reveal animations fire and lazy media loads. Off by default; toggle via
    // `--scroll-reveal`. See lib/snapshot-runner.ts's scrollThroughPage.
    scrollReveal: false,
    // Real-time animation capture window (ms). 0 = disabled (use the single-
    // instant pass). When > 0, lib/snapshot-runner.ts samples the live page
    // over this window so non-seekable, JS-triggered motion is captured. See
    // lib/animation-capture.ts's captureRuntimeAnimationsOnPage.
    runtimeAnimWindowMs: 0,
    runtimeAnimSampleCount: 0,
    // Web-font download: when true, every font file the browser fetched
    // while rendering is written to `fontDir` as a plain SFNT (TTF/OTF) so
    // downstream `pagx render`/`pagx font embed` can use the real typeface
    // instead of a host system fallback. See lib/font-download.ts; toggle
    // via `--download-fonts`, redirect via `--font-dir`.
    downloadFonts: false,
    fontDir: '',
    // Optional path to write the per-page font manifest (see --font-manifest).
    fontManifest: '',
    // Image download: when true, every external image the browser fetched is
    // written to `imageDir` and referenced by its local file path instead of
    // inlined as a base64 `data:` URI, keeping the snapshot small. See
    // lib/image-download.ts; toggle via `--download-images`, redirect via
    // `--image-dir`.
    downloadImages: false,
    imageDir: '',
    // Headless browser driver: 'puppeteer' (default) or 'playwright'.
    // `resolveEngine(undefined)` reads HTML_SNAPSHOT_BROWSER if set, else
    // returns the default. The `--browser-engine` flag below overrides both.
    browserEngine: resolveEngine(undefined),
  };
  const positional: string[] = [];
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-h' || a === '--help') {
      printUsage();
      process.exit(0);
    }
    const flag = FLAG_BY_NAME.get(a);
    if (flag) {
      // Boolean flags (`takesArg: false`) don't consume the next argv
      // token; value-bearing flags shift the index past their argument.
      if (flag.takesArg === false) {
        flag.set(opts);
      } else {
        flag.set(opts, argv[++i]);
      }
      continue;
    }
    if (a.startsWith('-')) {
      fail(`unknown option '${a}'`);
    }
    positional.push(a);
  }
  if (positional.length === 0) {
    printUsage();
    process.exit(2);
  }
  // `-o -` is the sentinel for "write HTML to stdout"; detect it here so the
  // URL/file branches below skip both their own default-output logic and the
  // path.resolve() call (which would turn `-` into an absolute path).
  if (opts.output === '-') {
    opts.outputToStdout = true;
    opts.output = '';
  }
  const inputArg = positional[0];
  opts.isUrl = isHttpUrl(inputArg);
  if (opts.isUrl) {
    // Remote pages have no filesystem location to derive a sibling name from,
    // so we require an explicit -o (or `-o -` for stdout). Letting it default
    // to a magic name in cwd would silently overwrite a previous run.
    opts.input = inputArg;
    if (!opts.output && !opts.outputToStdout) {
      fail(`-o/--output is required when input is a URL`);
    }
    if (opts.output) opts.output = path.resolve(opts.output);
  } else {
    if (opts.cookies.length || opts.headers.length) {
      fail(`--cookie / --header are only supported with URL inputs`);
    }
    opts.input = path.resolve(inputArg);
    if (opts.outputToStdout) {
      // No output file to derive; the script writes to process.stdout.
    } else if (!opts.output) {
      const dir = path.dirname(opts.input);
      const base = path.basename(opts.input, path.extname(opts.input));
      opts.output = path.join(dir, `${base}.subset.html`);
    } else {
      opts.output = path.resolve(opts.output);
    }
  }
  // Resolve a `--<resource>-dir` flag to an absolute path: an explicit value
  // wins (already in `opts[dirKey]`); otherwise, when the corresponding
  // `--download-<resource>` flag is on, default to a sibling
  // `<output-without-ext>.<suffix>/` directory. The trailing `.subset` is
  // stripped from the base name so `page.subset.html` yields `page.fonts/`,
  // not `page.subset.fonts/`. Stdout mode has no output path to derive from,
  // so an explicit `--<resource>-dir` is required.
  const resolveSiblingDir = (
    dirKey: 'fontDir' | 'imageDir',
    downloadKey: 'downloadFonts' | 'downloadImages',
    suffix: string,
    downloadFlag: string,
    dirFlag: string,
  ): void => {
    if (opts[dirKey]) {
      opts[dirKey] = path.resolve(opts[dirKey]);
      return;
    }
    if (!opts[downloadKey]) return;
    if (opts.outputToStdout || !opts.output) {
      fail(`${downloadFlag} requires ${dirFlag} when output is written to stdout`);
    }
    const dir = path.dirname(opts.output);
    let base = path.basename(opts.output, path.extname(opts.output));
    if (base.endsWith('.subset')) base = base.slice(0, -'.subset'.length);
    opts[dirKey] = path.join(dir, `${base}.${suffix}`);
  };
  resolveSiblingDir('fontDir',  'downloadFonts',  'fonts',  '--download-fonts',  '--font-dir');
  resolveSiblingDir('imageDir', 'downloadImages', 'images', '--download-images', '--image-dir');
  // A manifest only makes sense alongside --download-fonts (it lists the
  // captured font files). Resolve it to an absolute path so the paths it
  // records are usable from any working directory.
  if (opts.fontManifest) {
    if (!opts.downloadFonts) {
      fail(`--font-manifest requires --download-fonts`);
    }
    opts.fontManifest = path.resolve(opts.fontManifest);
  }
  return opts;
}

export function printUsage(): void {
  console.log(`Usage: html-snapshot <input.html | http(s)://url> -o <out.html> [options]

Render <input> in a headless browser and emit a flat, absolute-positioned
snapshot suitable for 'pagx import --format html'. <input> may be a local
HTML file or an http(s) URL; -o is required for URL inputs.

Options:
  -o, --output <file|->      Output path; use '-' to write the HTML to stdout
                             (required for URL inputs;
                             defaults to <input>.subset.html for files)
  --viewport-width <px>      Headless viewport width (default 1400)
  --viewport-height <px>     Headless viewport height (default 900)
  --wait-ms <ms>             Extra settle delay after networkidle (default 800)
  --selector <css>           Wait for this selector before snapshotting
  --cookie <name=value>      Set a cookie on the target URL (URL inputs only;
                             repeatable)
  --header <Key: Value>      Set an extra HTTP request header (URL inputs only;
                             repeatable)
  --no-inline-icon-fonts     Disable converting webfont icon glyphs (Phosphor,
                             Material Icons, Font Awesome, Lucide, …) to
                             inline <svg>. Default: enabled (the snapshot is
                             self-contained and renders identically on any
                             machine, regardless of which icon fonts are
                             installed).
  --scroll-reveal            Walk the page top-to-bottom (then back to the top)
                             before snapshotting so scroll-triggered reveal
                             animations fire and lazy-loaded media is fetched.
                             Default: disabled. Use for pages that keep
                             below-the-fold sections hidden (opacity:0) until
                             scrolled into view.
  --runtime-anim-window <ms> Capture animations by sampling the live, still-
                             running page over <ms> of wall-clock time instead
                             of a single instant. Records motion driven by
                             setTimeout chains / class toggles / JS auto-demo
                             sequences that the instant pass cannot see.
                             Default: 0 (instant pass). Only opacity / translate
                             / color / background-color on elements that persist
                             in the snapshot tree can be captured.
  --runtime-anim-samples <n> Sample count across --runtime-anim-window
                             (default: auto, ~20 samples/s, clamped).
  --download-fonts           Save every web font the page uses (each
                             unicode-range subset the browser fetched) to disk
                             as a plain SFNT (TTF/OTF). Default: disabled. Hand
                             the files to 'pagx render --fallback' or
                             'pagx font embed --fallback' so text in an
                             uninstalled web font renders with the right face.
  --font-dir <dir>           Destination for --download-fonts (default:
                             <output-without-ext>.fonts/). May be shared across
                             runs; identical faces are stored once (content-
                             addressed filenames).
  --download-images          Save every external image the page uses to disk and
                             reference it by local file path instead of inlining
                             it as a base64 data: URI. Default: disabled (images
                             are inlined). Keeps the subset HTML — and the PAGX
                             produced from it — small for image-heavy pages.
  --image-dir <dir>          Destination for --download-images (default:
                             <output-without-ext>.images/). May be shared across
                             runs; identical images are stored once (content-
                             addressed filenames).
  --font-manifest <file>     Write the font files this page uses (one absolute
                             path per line) to <file>. Requires
                             --download-fonts. Lets callers pass only the fonts
                             this page needs to 'pagx render' / 'pagx font
                             embed' when --font-dir is shared.
  --browser-engine <name>    Headless browser driver: one of
                             ${SUPPORTED_ENGINES.join(' | ')} (default: puppeteer;
                             override via HTML_SNAPSHOT_BROWSER env var).`);
}
