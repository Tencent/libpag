// Shared list of HTML tag names that the snapshot pipeline drops outright.
// Lives here so the main DOM walk (lib/browser-snapshot.ts) and the icon
// font pre-pass (lib/icon-font.ts) consult one list instead of maintaining
// independent copies — the previous arrangement had already drifted
// (`form`, `details`, `summary` were in one list but missing from the
// other), causing icon detection to walk into subtrees that the snapshot
// proper would have skipped.
//
// Lower-case canonical form. Both consumers serialise this constant into
// the browser-side payload and rebuild a Set from it: the main pipeline
// matches against `el.tagName.toLowerCase()`, the icon pass uppercases
// once at construction time so the matching pass against `el.tagName`
// (which DOM gives back uppercase) stays branch-free.

export const DROP_TAG_NAMES: readonly string[] = [
  'script', 'style', 'link', 'meta', 'noscript',
  'iframe', 'object', 'embed', 'video', 'audio',
  'br', 'hr', 'wbr',
  'head', 'title', 'base',
  'template', 'slot', 'dialog', 'details', 'summary',
  'map', 'area', 'source', 'track', 'param',
  'form',
];

// Pre-serialised forms of `DROP_TAG_NAMES`. The browser-side payload of both
// the snapshot pipeline and the icon-font pre-pass reconstructs the set from
// these literals (lower-case for `el.tagName.toLowerCase()` matching, upper-
// case for `el.tagName` matching since DOM hands back upper-case names). They
// are computed once at module load and reused so each call site doesn't keep
// running `JSON.stringify(...)` and `.map(s => s.toUpperCase())` over the
// same constant array on every payload build.
export const DROP_TAG_NAMES_JSON: string = JSON.stringify(DROP_TAG_NAMES);
export const DROP_TAGS_UPPER_JSON: string =
  JSON.stringify(DROP_TAG_NAMES.map((s) => s.toUpperCase()));
