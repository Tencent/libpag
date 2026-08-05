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

import { EditorView, keymap, lineNumbers, highlightSpecialChars, drawSelection, Decoration } from '@codemirror/view';
import { EditorState, StateField, StateEffect, Compartment, Range } from '@codemirror/state';
import { defaultKeymap, historyKeymap, history } from '@codemirror/commands';
import { xml } from '@codemirror/lang-xml';
import { syntaxHighlighting , HighlightStyle, defaultHighlightStyle } from '@codemirror/language';
import { highlightSelectionMatches } from '@codemirror/search';
import { tags } from '@lezer/highlight';

// Inline replacement for the `codemirror` meta-package's `minimalSetup`. We depend directly on
// the @codemirror/* sub-packages so npm never has to reconcile the meta-package's own version
// pins against ours (mismatched pins spawn duplicate EditorState instances, which then trip
// instanceof checks inside CodeMirror at runtime). Sourced from codemirror v6's minimalSetup.
const minimalSetup = [
    highlightSpecialChars(),
    history(),
    drawSelection(),
    syntaxHighlighting(defaultHighlightStyle, { fallback: true }),
    keymap.of([...defaultKeymap, ...historyKeymap]),
];

// A 1-based inclusive line range mirrored from a node's source span, or null to clear.
interface LineRange {
    startLine: number;
    endLine: number;
}

// Two independent highlight layers, mirrored from the canvas selection state: 'hover' (grey,
// transient) and 'select' (blue, sticky). Both can be shown at once on different lines; when they
// overlap the same line, CSS ordering lets the blue select layer win visually.
const setHover = StateEffect.define<LineRange | null>();
const setSelect = StateEffect.define<LineRange | null>();

function buildRangeDecos(doc: EditorState['doc'], range: LineRange, cls: string): Range<Decoration>[] {
    const first = Math.max(1, range.startLine);
    const last = Math.min(doc.lines, range.endLine);
    const decos: Range<Decoration>[] = [];
    for (let n = first; n <= last; n++) {
        decos.push(Decoration.line({ class: cls }).range(doc.line(n).from));
    }
    return decos;
}

const highlightField = StateField.define<{ hover: LineRange | null; select: LineRange | null }>({
    create: () => ({ hover: null, select: null }),
    update(value, tr) {
        let hover = value.hover;
        let select = value.select;
        for (const e of tr.effects) {
            if (e.is(setHover)) {
                hover = e.value;
            }
            if (e.is(setSelect)) {
                select = e.value;
            }
        }
        return { hover, select };
    },
    provide: (f) =>
        EditorView.decorations.compute([f], (state) => {
            const { hover, select } = state.field(f);
            const decos: Range<Decoration>[] = [];
            if (hover != null) {
                decos.push(...buildRangeDecos(state.doc, hover, 'cm-hover-line'));
            }
            if (select != null) {
                decos.push(...buildRangeDecos(state.doc, select, 'cm-select-line'));
            }
            // Hover and select spans are produced independently, so concatenate order does not
            // guarantee RangeSet's required ordering by `from`.
            decos.sort((a, b) => a.from - b.from);
            return Decoration.set(decos);
        }),
});

// --- Double-click-to-edit machinery ---
// The editor is read-only by default (browse/inspect mode). Double-clicking a line asks the host
// to unlock the enclosing node's source span for editing; blurring re-locks it to read-only. The
// editable/readOnly facets are swapped through a compartment so the lock toggles without
// rebuilding the state.
const editableCompartment = new Compartment();

// The character range currently unlocked for editing (the enclosing node's source span), or null
// when read-only. Stored as document offsets and mapped through edits so the range grows/shrinks
// with in-span typing.
const setEditRange = StateEffect.define<{ from: number; to: number } | null>();

const editRangeField = StateField.define<{ from: number; to: number } | null>({
    create: () => null,
    update(value, tr) {
        for (const e of tr.effects) {
            if (e.is(setEditRange)) {
                value = e.value;
            }
        }
        if (value != null && tr.docChanged) {
            value = { from: tr.changes.mapPos(value.from, -1), to: tr.changes.mapPos(value.to, 1) };
        }
        return value;
    },
    provide: (f) =>
        EditorView.decorations.compute([f], (state) => {
            const range = state.field(f);
            if (range == null) {
                return Decoration.none;
            }
            const fromLine = state.doc.lineAt(range.from).number;
            const toLine = state.doc.lineAt(Math.min(range.to, state.doc.length)).number;
            const decos: Range<Decoration>[] = [];
            for (let n = fromLine; n <= toLine; n++) {
                // Tag the span's boundary lines so CSS can draw a single outer border around the
                // whole block (top on the first line, bottom on the last, left/right on all)
                // instead of a separate box per line.
                let cls = 'cm-edit-line';
                if (n === fromLine) {
                    cls += ' cm-edit-line-first';
                }
                if (n === toLine) {
                    cls += ' cm-edit-line-last';
                }
                decos.push(Decoration.line({ class: cls }).range(state.doc.line(n).from));
            }
            return Decoration.set(decos);
        }),
});

// Confines edits to the unlocked span. When read-only (range == null) every doc change is dropped;
// while editing, a change must fall entirely within the span. Newlines are allowed since the span
// can cover multiple lines.
const spanFilter = EditorState.transactionFilter.of((tr) => {
    if (!tr.docChanged) {
        return tr;
    }
    const range = tr.startState.field(editRangeField);
    if (range == null) {
        return [];
    }
    let allowed = true;
    tr.changes.iterChanges((fromA, toA) => {
        if (fromA < range.from || toA > range.to) {
            allowed = false;
        }
    });
    return allowed ? tr : [];
});

/** Syntax highlight colors matching the desktop client (VS Code Dark+ palette). */
const pagxHighlightStyle = HighlightStyle.define([
    { tag: tags.tagName, color: '#569CD6' },
    { tag: tags.attributeName, color: '#9CDCFE' },
    { tag: tags.attributeValue, color: '#CE9178' },
    { tag: tags.string, color: '#CE9178' },
    { tag: tags.comment, color: '#6A9955' },
    { tag: tags.meta, color: '#808080' },
    { tag: tags.processingInstruction, color: '#808080' },
    { tag: tags.content, color: '#D4D4D4' },
    { tag: tags.angleBracket, color: '#569CD6' },
]);

/**
 * Wraps a CodeMirror 6 instance for editing PAGX XML source.
 * Search relies on the browser's built-in Ctrl+F.
 */
export class SourceEditor {
    private readonly host: HTMLElement;
    private view: EditorView | null = null;
    private onHoverLineCb: ((line: number) => void) | null = null;
    private onDblClickLineCb: ((line: number) => void) | null = null;
    private onCursorLineCb: ((line: number) => void) | null = null;
    private hoverLineRaf = 0;

    constructor(host: HTMLElement) {
        this.host = host;
        // Defer EditorView creation to the first setContent call so that the initial
        // document is the loaded XML content (not an empty doc). This prevents
        // Ctrl+Z from unwinding past the load point into an empty editor.
    }

    private createState(initialContent: string, selectionHead?: number): EditorState {
        return EditorState.create({
            doc: initialContent,
            selection: selectionHead !== undefined
                ? { anchor: Math.min(selectionHead, initialContent.length) }
                : undefined,
            extensions: [
                minimalSetup,
                lineNumbers(),
                xml(),
                syntaxHighlighting(pagxHighlightStyle),
                highlightSelectionMatches(),
                highlightField,
                editRangeField,
                spanFilter,
                editableCompartment.of([
                    EditorView.editable.of(false),
                    EditorState.readOnly.of(true),
                ]),
                EditorView.domEventHandlers({
                    dblclick: (event, view) => this.handleEditorDblClick(event, view),
                    blur: (_event, view) => this.handleEditorBlur(view),
                    mousemove: (event) => this.handleEditorMouseMove(event),
                    mouseleave: () => {
                        this.onHoverLineCb?.(-1);
                        return false;
                    },
                }),
                // While a span is unlocked for editing, report the caret's line whenever the
                // selection moves so the host can re-scope the editable range and blue selection
                // to the node the caret now sits in (DevTools-like "edit follows caret"). Gated on
                // editRangeField so caret moves in read-only browse mode stay silent.
                EditorView.updateListener.of((update) => {
                    if (!update.selectionSet || this.onCursorLineCb === null) {
                        return;
                    }
                    if (update.state.field(editRangeField) == null) {
                        return;
                    }
                    const head = update.state.selection.main.head;
                    this.onCursorLineCb(update.state.doc.lineAt(head).number);
                }),
                // basicSetup includes searchKeymap which intercepts Ctrl+F. Use minimalSetup
                // (which omits it) and add only the keymaps we need, so the browser's
                // built-in Ctrl+F is free to handle search.
                keymap.of([...defaultKeymap, ...historyKeymap]),
                // Theme covers only CodeMirror internals that styles.ts does not target.
                // Gutter and selection colors live in styles.ts (higher specificity overrides
                // this theme anyway), so keeping them here would create two sources of truth.
                EditorView.theme({
                    '&': {
                        backgroundColor: '#1E1E1E',
                        color: '#D4D4D4',
                        height: '100%',
                    },
                    '.cm-content': {
                        fontFamily: 'Menlo, Monaco, "Courier New", monospace',
                        fontSize: '13px',
                        caretColor: '#FFFFFF',
                    },
                    '.cm-cursor': {
                        borderLeftColor: '#FFFFFF',
                        borderLeftWidth: '2px',
                    },
                    '.cm-activeLine': {
                        backgroundColor: '#2D2D2D',
                    },
                    '.cm-activeLineGutter': {
                        backgroundColor: '#2D2D2D',
                    },
                }),
            ],
        });
    }

    private createView(initialContent: string): void {
        this.view = new EditorView({
            parent: this.host,
            state: this.createState(initialContent),
        });
        // Reserve 200px of scrollable space at the bottom of the editor. Without this, the last
        // few lines hit CodeMirror's maxScroll and scrollIntoView's 'start' alignment gets
        // clamped to the viewport bottom, leaving the highlighted line glued to the edge
        // regardless of yMargin. 200px of padding keeps maxScroll large enough that
        // 'start' + yMargin:80 lands the line ~80px from the top for any line, including the
        // document's last line.
        this.view.scrollDOM.style.paddingBottom = '200px';
    }

    /** Replaces the entire document content, resetting the undo history to this content. When
     *  preserveViewState is true, keeps the current caret and scroll offset. */
    setContent(text: string, preserveViewState = false): void {
        const trimmed = text.charCodeAt(0) === 0xFEFF ? text.slice(1) : text;
        if (this.view === null) {
            this.createView(trimmed);
            return;
        }
        if (!preserveViewState) {
            // Rebuild the state instead of dispatching a change so the undo history is rooted at
            // this content. A plain change dispatch would be recorded as an undoable edit, letting
            // Ctrl+Z revert to a previously loaded file's content.
            this.view.setState(this.createState(trimmed));
            return;
        }
        const savedHead = this.view.state.selection.main.head;
        const savedScrollTop = this.view.scrollDOM.scrollTop;
        this.view.setState(this.createState(trimmed, savedHead));
        this.view.requestMeasure({
            read: () => savedScrollTop,
            write: (scrollTop, view) => {
                view.scrollDOM.scrollTop = scrollTop;
            },
        });
    }

    /** Returns the current document text. */
    getContent(): string {
        if (this.view === null) {
            return '';
        }
        return this.view.state.doc.toString();
    }

    /** Highlights the node's source span as the transient grey hover layer. startLine <= 0 clears. */
    highlightHover(startLine: number, endLine: number): void {
        if (this.view === null) {
            return;
        }
        this.view.dispatch({
            effects: setHover.of(startLine <= 0 ? null : { startLine, endLine }),
        });
    }

    /** Clears the grey hover highlight layer. */
    clearHover(): void {
        this.view?.dispatch({ effects: setHover.of(null) });
    }

    /** Highlights the node's source span as the sticky blue selection layer. startLine <= 0 clears. */
    highlightSelect(startLine: number, endLine: number): void {
        if (this.view === null) {
            return;
        }
        this.view.dispatch({
            effects: setSelect.of(startLine <= 0 ? null : { startLine, endLine }),
        });
    }

    /** Clears the blue selection highlight layer. */
    clearSelect(): void {
        this.view?.dispatch({ effects: setSelect.of(null) });
    }

    /** Clears both highlight layers. */
    clearHighlight(): void {
        this.view?.dispatch({ effects: [setHover.of(null), setSelect.of(null)] });
    }

    /** Unlocks the given 1-based inclusive line span for editing (the enclosing node's source
     *  span). Swaps the editable/readOnly facets and records the character range so the span
     *  filter confines edits to it. Called by the host on double-click. */
    enterEditRange(startLine: number, endLine: number): void {
        if (this.view === null || startLine <= 0) {
            return;
        }
        const doc = this.view.state.doc;
        const from = doc.line(Math.min(startLine, doc.lines)).from;
        const to = doc.line(Math.min(endLine, doc.lines)).to;
        this.view.dispatch({
            effects: [
                editableCompartment.reconfigure([
                    EditorView.editable.of(true),
                    EditorState.readOnly.of(false),
                ]),
                setEditRange.of({ from, to }),
            ],
        });
        this.view.focus();
    }

    /** Re-scopes the already-unlocked editable span to a new 1-based inclusive line range
     *  without touching the editable/readOnly facets or stealing focus. Used while editing to
     *  follow the caret into a different node's span. No-op when read-only. */
    updateEditRange(startLine: number, endLine: number): void {
        if (this.view === null || startLine <= 0) {
            return;
        }
        if (this.view.state.field(editRangeField) == null) {
            return;
        }
        const doc = this.view.state.doc;
        const from = doc.line(Math.min(startLine, doc.lines)).from;
        const to = doc.line(Math.min(endLine, doc.lines)).to;
        this.view.dispatch({ effects: setEditRange.of({ from, to }) });
    }

    /** Scrolls the given 1-based line into view. 'nearest' only scrolls when the line is already
     *  off-screen. 'start' places the line ~80px from the viewport top with a fixed yMargin.
     *  Together with the 200px scroll padding in createView(), this lets every line — including
     *  the document's last line — land at a consistent, slightly-elevated spot. */
    scrollToLine(line: number, align: 'start' | 'nearest' = 'start'): void {
        if (this.view === null || line <= 0) {
            return;
        }
        const doc = this.view.state.doc;
        const pos = doc.line(Math.min(line, doc.lines)).from;
        const scrollOpt = align === 'nearest'
            ? { y: 'nearest' as const }
            : { y: 'start' as const, yMargin: 80 };
        this.view.dispatch({ effects: EditorView.scrollIntoView(pos, scrollOpt) });
    }

    /** Registers a callback fired with the 1-based line under the pointer as the user hovers the
     *  editor content (rAF-throttled), and -1 when the pointer leaves. Drives the editor->canvas
     *  overlay highlight. Only one callback is kept; the panel re-registers it after each editor
     *  rebuild. Pass null to detach. */
    onHoverLine(cb: ((line: number) => void) | null): void {
        this.onHoverLineCb = cb;
    }

    /** Registers a callback fired with the double-clicked 1-based line. The host resolves the
     *  enclosing node's source span and calls enterEditRange() to unlock it. Pass null to detach. */
    onDblClickLine(cb: ((line: number) => void) | null): void {
        this.onDblClickLineCb = cb;
    }

    /** Registers a callback fired with the caret's 1-based line whenever the selection moves
     *  while a span is unlocked for editing. Drives the "edit follows caret" re-scoping. Only one
     *  callback is kept; the panel re-registers it after each editor rebuild. Pass null to detach. */
    onCursorLine(cb: ((line: number) => void) | null): void {
        this.onCursorLineCb = cb;
    }

    /** Double-click reports the clicked line to the host, which decides the editable span. */
    private handleEditorDblClick(event: MouseEvent, view: EditorView): boolean {
        if (this.onDblClickLineCb === null) {
            return false;
        }
        const pos = view.posAtCoords({ x: event.clientX, y: event.clientY });
        if (pos == null) {
            return false;
        }
        this.onDblClickLineCb(view.state.doc.lineAt(pos).number);
        return false;
    }

    /** Blur re-locks the editor to read-only (desktop/DevTools-style: editing ends when focus
     *  leaves). The edited text stays in the document for Apply/Save to pick up. */
    private handleEditorBlur(view: EditorView): boolean {
        if (view.state.field(editRangeField) == null) {
            return false;
        }
        view.dispatch({
            effects: [
                editableCompartment.reconfigure([
                    EditorView.editable.of(false),
                    EditorState.readOnly.of(true),
                ]),
                setEditRange.of(null),
            ],
        });
        return false;
    }

    /** rAF-throttled pointer tracking that reports the hovered 1-based line to onHoverLineCb. */
    private handleEditorMouseMove(event: MouseEvent): boolean {
        if (this.onHoverLineCb === null) {
            return false;
        }
        const x = event.clientX;
        const y = event.clientY;
        if (this.hoverLineRaf !== 0) {
            cancelAnimationFrame(this.hoverLineRaf);
        }
        this.hoverLineRaf = requestAnimationFrame(() => {
            this.hoverLineRaf = 0;
            if (this.view === null || this.onHoverLineCb === null) {
                return;
            }
            const pos = this.view.posAtCoords({ x, y });
            if (pos == null) {
                this.onHoverLineCb(-1);
                return;
            }
            this.onHoverLineCb(this.view.state.doc.lineAt(pos).number);
        });
        return false;
    }

    /** Releases the CodeMirror instance and frees DOM nodes. */
    destroy(): void {
        if (this.hoverLineRaf !== 0) {
            cancelAnimationFrame(this.hoverLineRaf);
            this.hoverLineRaf = 0;
        }
        if (this.view !== null) {
            this.view.destroy();
            this.view = null;
        }
    }
}
