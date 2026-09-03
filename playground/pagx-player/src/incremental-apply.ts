/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// Incremental-apply classifier: decides whether a source-editor XML edit is a pure set of
// attribute-value changes on incrementable channels, and if so derives the channel writes that
// reproduce it without a full reparse+rebuild. Every rejection path returns null (or a reason)
// so the caller falls back to the full Apply pipeline, which is always the authoritative final
// state.

import type { NodeSourceEntry } from './pagx-view-types';

/** One incremental channel write derived from a source-editor edit: set channel on nodes[index]
 *  to the raw attribute string value. */
export interface ChannelEdit {
    index: number;
    channel: string;
    value: string;
}

/** Parses a node's source-span fragment and returns its root element, or null when the fragment is
 *  not a single well-formed element (malformed XML, or extra content around the root). */
function parseSpanElement(span: string): Element | null {
    const doc = new DOMParser().parseFromString(span, 'application/xml');
    if (doc.querySelector('parsererror') !== null) {
        return null;
    }
    return doc.documentElement;
}

/** Checks only the selected node's own opening/closing boundary tags. Tag-block actions use a
 *  source-map span that was established by the last valid parse, so an unrelated malformed child
 *  must not make its intact parent undeletable. Full DOM parsing remains required for Apply and
 *  incremental edits; this intentionally answers only whether the mapped outer boundary survived. */
export function hasMatchingSpanBoundaries(span: string): boolean {
    const opening = /^\s*<([A-Za-z_][\w:.-]*)(?=[\s/>])/.exec(span);
    if (opening === null) {
        return false;
    }
    let quote = '';
    let openingEnd = -1;
    for (let i = opening[0].length; i < span.length; i++) {
        const char = span[i];
        if (quote !== '') {
            if (char === quote) {
                quote = '';
            }
            continue;
        }
        if (char === '"' || char === "'") {
            quote = char;
        } else if (char === '>') {
            openingEnd = i;
            break;
        }
    }
    if (openingEnd < 0) {
        return false;
    }
    const beforeEnd = span.slice(0, openingEnd).trimEnd();
    if (beforeEnd.endsWith('/')) {
        return span.slice(openingEnd + 1).trim() === '';
    }
    const escapedName = opening[1].replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    const closing = new RegExp(`</\\s*${escapedName}\\s*>\\s*$`);
    return closing.test(span.slice(openingEnd + 1));
}

/** Returns an element's own attributes as a name->value map (values are XML-decoded, matching the
 *  importer, which reads decoded attribute values too). */
function attributeMap(element: Element): Record<string, string> {
    const map: Record<string, string> = {};
    for (let i = 0; i < element.attributes.length; i++) {
        const attr = element.attributes.item(i);
        if (attr !== null) {
            map[attr.name] = attr.value;
        }
    }
    return map;
}

/** One resolved sub-channel write produced by splitting a composite attribute (e.g. position ->
 *  position.x / position.y). channel is the dotted sub-channel name the engine's channel table
 *  understands; value is that component in the same string form the importer would parse. */
interface SubEdit {
    channel: string;
    value: string;
}

/** The composite XML attributes and how each expands into sub-channels. A composite attribute is a
 *  single XML attribute (e.g. position="10 20") that the channel table addresses as several dotted
 *  sub-channels (position.x / position.y). The kind selects the split/expand rule below. left /
 *  right / top / bottom / centerX / centerY and Text's x / y are plain scalars, not composites, and
 *  are handled by the normal whitelist path. matrix / matrix3D have no sub-channels and are
 *  intentionally absent so they keep falling back to a full reparse.
 *
 *  This table mirrors the dotted sub-channels declared by the FIELD_POINT_X/Y, FIELD_SIZE_W/H and
 *  FIELD_PADDING_L/T/R/B entries in src/pagx/PAGXNodeChannel.cpp. When a composite channel is added
 *  or renamed there, update this table too — the round-trip check in classifyNodeSpan plus the
 *  full-reparse fallback keep a stale entry safe (never wrong), but the edit silently stops going
 *  incremental until both sides agree again. */
const COMPOSITE_ATTRIBUTES = new Map<string, 'point' | 'size' | 'padding'>([
    ['position', 'point'],
    ['scale', 'point'],
    ['anchor', 'point'],
    ['startPoint', 'point'],
    ['endPoint', 'point'],
    ['center', 'point'],
    ['baselineOrigin', 'point'],
    ['size', 'size'],
    ['padding', 'padding'],
]);

/** Splits a whitespace/comma-separated numeric string into floats, matching the importer's
 *  ParseFloatList (strtof over each token). Returns null when a token is not a leading-number, so a
 *  malformed composite value forces a full reparse instead of a wrong incremental write. */
function parseFloatTokens(value: string): number[] | null {
    const tokens = value.trim().split(/[\s,]+/).filter((token) => token.length > 0);
    const result: number[] = [];
    for (const token of tokens) {
        // parseFloat mirrors strtof's leading-number semantics (trailing junk after a valid number
        // is ignored by the importer too); a token with no leading number is rejected.
        const parsed = Number.parseFloat(token);
        if (Number.isNaN(parsed)) {
            return null;
        }
        result.push(parsed);
    }
    return result;
}

/** Expands a padding attribute into its four [top, right, bottom, left] components, replicating the
 *  importer's CSS-shorthand rule (PaddingFromString): 1 value = all four; 2 = [v0,v1,v0,v1] as
 *  top/bottom then right/left; 4 = [top,right,bottom,left]. Returns null for any other count so the
 *  edit falls back to a full reparse (the importer rejects those too). Components are emitted as the
 *  original token strings (not reformatted numbers) so an unchanged component is byte-identical to
 *  what setNodeChannel would parse. */
function expandPadding(value: string): { top: string; right: string; bottom: string; left: string } | null {
    const tokens = value.trim().split(/[\s,]+/).filter((token) => token.length > 0);
    if (parseFloatTokens(value) === null) {
        return null;
    }
    if (tokens.length === 1) {
        return { top: tokens[0], right: tokens[0], bottom: tokens[0], left: tokens[0] };
    }
    if (tokens.length === 2) {
        return { top: tokens[0], right: tokens[1], bottom: tokens[0], left: tokens[1] };
    }
    if (tokens.length === 4) {
        return { top: tokens[0], right: tokens[1], bottom: tokens[2], left: tokens[3] };
    }
    return null;
}

/** Splits a composite attribute's old and new values into the sub-channel writes for only the
 *  components that actually changed (e.g. editing just y in position="10 20" -> position="10 30"
 *  emits a single position.y write). attrName is the XML attribute (position / size / padding /
 *  scale / center / ...); kind selects the parse+expand rule. Returns null when either value is
 *  malformed or has the wrong component count, so the caller falls back to a full reparse. An empty
 *  array means the values are equivalent (e.g. padding "10" vs "10 10 10 10"): no write is needed.
 *  Emitting only changed components keeps each sub-channel's own layout/anim flag accurate (a
 *  position.y edit must not redundantly re-drive position.x). */
function splitCompositeChange(
    attrName: string,
    kind: 'point' | 'size' | 'padding',
    oldValue: string,
    newValue: string,
): SubEdit[] | null {
    if (kind === 'padding') {
        const oldPad = expandPadding(oldValue);
        const newPad = expandPadding(newValue);
        if (oldPad === null || newPad === null) {
            return null;
        }
        const edits: SubEdit[] = [];
        const sides: Array<keyof typeof newPad> = ['left', 'top', 'right', 'bottom'];
        for (const side of sides) {
            if (Number.parseFloat(oldPad[side]) !== Number.parseFloat(newPad[side])) {
                edits.push({ channel: `${attrName}.${side}`, value: newPad[side] });
            }
        }
        return edits;
    }
    const oldTokens = pointTokens(oldValue);
    const newTokens = pointTokens(newValue);
    if (oldTokens === null || newTokens === null) {
        return null;
    }
    // point -> .x/.y, size -> .width/.height, matching the channel table's sub-channel names.
    const suffixes = kind === 'size' ? ['width', 'height'] : ['x', 'y'];
    const edits: SubEdit[] = [];
    for (let i = 0; i < suffixes.length; i++) {
        if (Number.parseFloat(oldTokens[i]) !== Number.parseFloat(newTokens[i])) {
            edits.push({ channel: `${attrName}.${suffixes[i]}`, value: newTokens[i] });
        }
    }
    return edits;
}

/** Returns the two component token strings of a point/size value, or null when it does not hold
 *  exactly two numbers (the importer's ParseTwoFloats requires both). Token strings (not reparsed
 *  numbers) are returned so an unchanged component round-trips byte-identically. */
function pointTokens(value: string): [string, string] | null {
    const tokens = value.trim().split(/[\s,]+/).filter((token) => token.length > 0);
    if (tokens.length !== 2 || parseFloatTokens(value) === null) {
        return null;
    }
    return [tokens[0], tokens[1]];
}

/** Result of classifying one node's span: either the incremental channel writes, or a human-
 *  readable reason the edit cannot go incremental. */
type NodeSpanClassification = { edits: ChannelEdit[] } | { reason: string };

/** Classifies the change to one node's source span into per-attribute channel writes, or a reason
 *  it cannot go incremental. Only pure own-attribute value changes on whitelisted channels qualify:
 *  the tag name and attribute key set must be unchanged, every changed attribute must be an
 *  incrementable channel, and — crucially — after applying those value changes to the old element
 *  it must serialize identically to the new one. That exact-match check guarantees no other
 *  difference (child element, text content, attribute reordering, or a change inside embedded
 *  non-node content like an <svg> subtree) is silently dropped: any residual difference forces a
 *  full reparse. */
function classifyNodeSpan(
    oldSpan: string,
    newSpan: string,
    channels: string[],
): NodeSpanClassification {
    const oldElement = parseSpanElement(oldSpan);
    const newElement = parseSpanElement(newSpan);
    if (oldElement === null || newElement === null) {
        return { reason: 'node span is not a single well-formed element (unparseable fragment)' };
    }
    if (oldElement.tagName !== newElement.tagName) {
        return { reason: `tag name changed (${oldElement.tagName} -> ${newElement.tagName})` };
    }
    const oldAttrs = attributeMap(oldElement);
    const newAttrs = attributeMap(newElement);
    const oldKeys = Object.keys(oldAttrs);
    const newKeys = Object.keys(newAttrs);
    if (oldKeys.length !== newKeys.length) {
        return { reason: `attribute added or removed on <${oldElement.tagName}>` };
    }
    const edits: ChannelEdit[] = [];
    for (const key of newKeys) {
        if (!(key in oldAttrs)) {
            return { reason: `attribute renamed on <${oldElement.tagName}> (new key "${key}")` };
        }
        if (oldAttrs[key] === newAttrs[key]) {
            continue;
        }
        if (channels.includes(key)) {
            edits.push({ index: -1, channel: key, value: newAttrs[key] });
            continue;
        }
        // Composite attribute (position / size / padding / ...): the channel table addresses it as
        // dotted sub-channels, so split the change into per-component writes and keep only the
        // components that actually changed. The value is re-parsed exactly as the importer would, so
        // an incremental sub-channel write reproduces the full-reparse result.
        const compositeKind = COMPOSITE_ATTRIBUTES.get(key);
        if (compositeKind !== undefined) {
            const subEdits = splitCompositeChange(key, compositeKind, oldAttrs[key], newAttrs[key]);
            if (subEdits === null) {
                return {
                    reason: `composite attribute "${key}" on <${oldElement.tagName}> is malformed or has an unexpected component count`,
                };
            }
            for (const subEdit of subEdits) {
                if (!channels.includes(subEdit.channel)) {
                    return {
                        reason: `composite attribute "${key}" maps to sub-channel "${subEdit.channel}" which is not incrementable on <${oldElement.tagName}>`,
                    };
                }
                edits.push({ index: -1, channel: subEdit.channel, value: subEdit.value });
            }
            continue;
        }
        return {
            reason: `attribute "${key}" on <${oldElement.tagName}> is not an incrementable channel (composite or unsupported)`,
        };
    }
    // Round-trip check against the ORIGINAL attribute names (not the split sub-channels): write each
    // changed attribute's new value back onto the old element under its own name, then require a
    // byte-identical serialization. This guarantees the only differences between the spans are the
    // attribute values we accounted for above; any residual difference (child element, text content,
    // attribute reorder, embedded <svg> subtree change) still forces a full reparse. Done with the
    // original names so a composite like position stays a single attribute here even though it was
    // emitted as position.x / position.y sub-channel writes.
    for (const key of newKeys) {
        if (oldAttrs[key] !== newAttrs[key]) {
            oldElement.setAttribute(key, newAttrs[key]);
        }
    }
    const serializer = new XMLSerializer();
    if (serializer.serializeToString(oldElement) !== serializer.serializeToString(newElement)) {
        return {
            reason: `<${oldElement.tagName}> span differs beyond the edited attributes (child/text/embedded content changed)`,
        };
    }
    return { edits };
}

/** Returns the index of the innermost node whose [startLine, endLine] span contains the given
 *  1-based line, or -1 if none. endLine < 0 (programmatic nodes) falls back to a single-line
 *  match at startLine. */
export function findNodeIndexForLine(sourceMap: readonly NodeSourceEntry[], line: number): number {
    let bestIndex = -1;
    let bestSpan = Number.POSITIVE_INFINITY;
    for (const entry of sourceMap) {
        const start = entry.startLine;
        if (start <= 0 || start > line) {
            continue;
        }
        const end = entry.endLine > 0 ? entry.endLine : start;
        if (line > end) {
            continue;
        }
        const span = end - start;
        if (span < bestSpan) {
            bestSpan = span;
            bestIndex = entry.index;
        }
    }
    return bestIndex;
}

/** Classifies a text edit into a flat list of incremental channel writes, or null when it
 *  cannot go incremental. First-version constraints (each a null-return): the line count
 *  changed (would shift the cached source spans), a changed line lies outside any node, or a
 *  node's own change is not a pure whitelisted-channel value edit (see classifyNodeSpan).
 *  sourceMap is the caller's draft projection of the runtime source map. */
export function classifyEdits(
    oldXml: string,
    newXml: string,
    sourceMap: readonly NodeSourceEntry[],
): ChannelEdit[] | null {
    const oldLines = oldXml.split('\n');
    const newLines = newXml.split('\n');
    if (oldLines.length !== newLines.length) {
        return null;
    }
    // Look up entries by their native index rather than the array position: the runtime
    // guarantees Node::index equals the sourceMap array offset today, but a future getNodeSourceMap
    // that filters entries (e.g. programmatic nodes) would break a positional access silently.
    const entryByIndex = new Map<number, NodeSourceEntry>();
    for (const entry of sourceMap) {
        entryByIndex.set(entry.index, entry);
    }
    const affected = new Set<number>();
    for (let i = 0; i < oldLines.length; i++) {
        if (oldLines[i] === newLines[i]) {
            continue;
        }
        const idx = findNodeIndexForLine(sourceMap, i + 1);
        if (idx < 0) {
            return null;
        }
        affected.add(idx);
    }
    const edits: ChannelEdit[] = [];
    for (const idx of affected) {
        const entry = entryByIndex.get(idx);
        if (!entry) {
            return null;
        }
        const start = entry.startLine;
        const end = entry.endLine > 0 ? entry.endLine : start;
        if (start <= 0) {
            return null;
        }
        const oldSpan = oldLines.slice(start - 1, end).join('\n');
        const newSpan = newLines.slice(start - 1, end).join('\n');
        const result = classifyNodeSpan(oldSpan, newSpan, entry.channels);
        if ('reason' in result) {
            return null;
        }
        for (const nodeEdit of result.edits) {
            edits.push({ index: idx, channel: nodeEdit.channel, value: nodeEdit.value });
        }
    }
    return edits;
}
