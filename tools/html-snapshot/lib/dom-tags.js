// Shared list of HTML tag names that the snapshot pipeline drops outright.
// Lives here so the main DOM walk (lib/browser-snapshot.js) and the icon
// font pre-pass (lib/icon-font.js) consult one list instead of maintaining
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

'use strict';

const DROP_TAG_NAMES = [
  'script', 'style', 'link', 'meta', 'noscript',
  'iframe', 'object', 'embed', 'video', 'audio',
  'br', 'hr', 'wbr',
  'head', 'title', 'base',
  'template', 'slot', 'dialog', 'details', 'summary',
  'map', 'area', 'source', 'track', 'param',
  'form',
];

module.exports = { DROP_TAG_NAMES };
