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
//  license is distributed on an "AS is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// Monaco is loaded from CDN at runtime via the AMD loader (same as the monaco-test.html page).
// The npm package is kept in dependencies for TypeScript types only — `import type` is erased by
// esbuild at build time, so rollup never tries to bundle Monaco's non-standard ESM internals
// (which use bare `vs/...` specifiers that nodeResolve cannot map).
import type * as MonacoNS from 'monaco-editor';

type Monaco = typeof MonacoNS;

// Module-level Monaco singleton. Loaded once on first SourceEditor construction; all subsequent
// editors share the same instance. The AMD loader is injected as a <script> tag, then
// require(['vs/editor/editor.main']) pulls in the editor + all basic languages (including XML).
const MONACO_CDN = 'https://cdn.jsdelivr.net/npm/monaco-editor@0.47.0/min/vs';

let monacoInstance: Monaco | null = null;
let monacoLoadPromise: Promise<Monaco> | null = null;

function loadMonaco(): Promise<Monaco> {
    if (monacoInstance !== null) {
        return Promise.resolve(monacoInstance);
    }
    if (monacoLoadPromise !== null) {
        return monacoLoadPromise;
    }
    monacoLoadPromise = new Promise<Monaco>((resolve, reject) => {
        const script = document.createElement('script');
        script.src = `${MONACO_CDN}/loader.js`;
        script.onload = () => {
            const win = window as unknown as {
                require?: {
                    config: (opts: { paths: { vs: string } }) => void;
                    (deps: string[], cb: () => void): void;
                };
                monaco?: Monaco;
            };
            const amdRequire = win.require;
            if (amdRequire === undefined) {
                reject(new Error('Monaco AMD loader failed to initialize'));
                return;
            }
            amdRequire.config({ paths: { vs: MONACO_CDN } });
            amdRequire(['vs/editor/editor.main'], () => {
                const m = win.monaco;
                if (m === undefined) {
                    reject(new Error('Monaco failed to load from CDN'));
                    return;
                }
                definePagxTheme(m);
                monacoInstance = m;
                resolve(m);
            });
        };
        script.onerror = () => reject(new Error('Failed to load Monaco loader script from CDN'));
        document.head.appendChild(script);
    });
    return monacoLoadPromise;
}

// Defines a custom theme matching the VS Code Dark+ palette used by the previous CodeMirror
// highlighter, so syntax colors stay consistent across the migration.
function definePagxTheme(m: Monaco): void {
    m.editor.defineTheme('pagx-dark', {
        base: 'vs-dark',
        inherit: true,
        rules: [
            { token: 'tag', foreground: '569CD6' },
            { token: 'attribute.name', foreground: '9CDCFE' },
            { token: 'attribute.value', foreground: 'CE9178' },
            { token: 'string', foreground: 'CE9178' },
            { token: 'comment', foreground: '6A9955' },
            { token: 'delimiter', foreground: '569CD6' },
            { token: '', foreground: 'D4D4D4' },
        ],
        colors: {
            'editor.background': '#1E1E1E',
            'editor.foreground': '#D4D4D4',
            'editorLineNumber.foreground': '#6E7681',
            'editorLineNumber.activeForeground': '#CCCCCC',
            'editor.selectionBackground': '#264F78',
            'editor.lineHighlightBackground': '#2D2D2D',
            'editorCursor.foreground': '#FFFFFF',
            'editorGutter.background': '#1E1E1E',
        },
    });
}

// Start loading immediately so the editor is ready by the time the user opens the panel.
void loadMonaco();

// A 1-based inclusive line range mirrored from a node's source span, or null to clear.
interface LineRange {
    startLine: number;
    endLine: number;
}

// Decoration class names (must match styles.ts selectors).
const HOVER_LINE_CLASS = 'pagx-hover-line';
const SELECT_LINE_CLASS = 'pagx-select-line';
const EDIT_LINE_CLASS = 'pagx-edit-line';
const EDIT_LINE_FIRST_CLASS = 'pagx-edit-line-first';
const EDIT_LINE_LAST_CLASS = 'pagx-edit-line-last';

function clampLine(line: number, lineCount: number): number {
    return Math.max(1, Math.min(line, lineCount));
}

function buildLineDecos(
    range: LineRange,
    model: MonacoNS.editor.ITextModel,
    cls: string,
    out: MonacoNS.editor.IModelDeltaDecoration[],
): void {
    const last = model.getLineCount();
    const start = clampLine(range.startLine, last);
    const end = clampLine(range.endLine, last);
    out.push({
        range: {
            startLineNumber: start,
            startColumn: 1,
            endLineNumber: Math.max(start, end),
            endColumn: model.getLineMaxColumn(Math.max(start, end)),
        },
        options: { isWholeLine: true, className: cls },
    });
}

function buildEditDecos(
    range: LineRange,
    model: MonacoNS.editor.ITextModel,
    out: MonacoNS.editor.IModelDeltaDecoration[],
): void {
    const last = model.getLineCount();
    const start = clampLine(range.startLine, last);
    const end = clampLine(range.endLine, last);
    for (let line = start; line <= end; line++) {
        let cls = EDIT_LINE_CLASS;
        if (line === start) {
            cls += ' ' + EDIT_LINE_FIRST_CLASS;
        }
        if (line === end) {
            cls += ' ' + EDIT_LINE_LAST_CLASS;
        }
        out.push({
            range: {
                startLineNumber: line,
                startColumn: 1,
                endLineNumber: line,
                endColumn: model.getLineMaxColumn(line),
            },
            options: { isWholeLine: true, className: cls },
        });
    }
}

/**
 * Wraps a Monaco editor instance for editing PAGX XML source.
 * Monaco loads asynchronously from CDN; until it's ready, all methods are no-ops (the editor
 * instance is null). The first setContent() triggers creation; once Monaco resolves, the editor
 * is created with the buffered content.
 */
export class SourceEditor {
    private readonly host: HTMLElement;
    private editor: MonacoNS.editor.IStandaloneCodeEditor | null = null;
    private model: MonacoNS.editor.ITextModel | null = null;
    private onHoverLineCb: ((line: number) => void) | null = null;
    private onDblClickLineCb: ((line: number) => void) | null = null;
    private onCursorLineCb: ((line: number) => void) | null = null;
    private hoverLineRaf = 0;
    private hoverDecoIds: string[] = [];
    private selectDecoIds: string[] = [];
    private editDecoIds: string[] = [];
    private editRangeActive = false;
    private editRangeOffset: { from: number; to: number } | null = null;
    private contentChangeListener: MonacoNS.IDisposable | null = null;
    private destroyed = false;
    private creating = false;
    private readonly disposers: MonacoNS.IDisposable[] = [];

    constructor(host: HTMLElement) {
        this.host = host;
    }

    private createEditor(initialContent: string): void {
        // Guard against double-creation: if a previous createEditor is still pending (Monaco
        // hasn't loaded yet), or the editor was already created, skip.
        if (this.creating || this.editor !== null || this.destroyed) {
            return;
        }
        this.creating = true;
        loadMonaco().then((m) => {
            this.creating = false;
            if (this.destroyed || this.editor !== null) {
                return;
            }
            this.model = m.editor.createModel(initialContent, 'xml');
            this.editor = m.editor.create(this.host, {
                model: this.model,
                theme: 'pagx-dark',
                // readOnly disables Monaco's edit commands; domReadOnly additionally sets the DOM
                // contenteditable="false", which blocks IME composition (Chinese/Pinyin popup,
                // readOnly disables Monaco's edit commands; domReadOnly additionally sets the
                // editor's hidden <textarea> to readonly="true" (Monaco's textAreaHandler.js),
                // which blocks IME composition (Chinese/Pinyin popup, candidate selection) in
                // the read-only state. The virtual cursor is hidden via CSS using the
                // pagx-editor-readonly class below, because Monaco 0.47.0's cursorStyle enum
                // does not include 'hidden' (added in a later version).
                readOnly: true,
                domReadOnly: true,
                minimap: { enabled: false },
                lineNumbers: 'on',
                scrollBeyondLastLine: true,
                wordWrap: 'off',
                renderWhitespace: 'none',
                largeFileOptimizations: true,
                automaticLayout: true,
                unicodeHighlight: { ambiguousCharacters: false, invisibleCharacters: false },
                fontSize: 13,
                fontFamily: 'Menlo, Monaco, "Courier New", monospace',
                lineHeight: 18,
                scrollbar: {
                    vertical: 'auto',
                    horizontal: 'auto',
                    verticalScrollbarSize: 10,
                    horizontalScrollbarSize: 10,
                },
                padding: { top: 0, bottom: 200 },
                smoothScrolling: false,
                cursorBlinking: 'smooth',
                cursorSmoothCaretAnimation: 'on',
            });

            // Hover → editor->canvas overlay highlight (rAF-throttled).
            this.disposers.push(
                this.editor.onMouseMove((e: MonacoNS.editor.IEditorMouseEvent) => {
                    if (this.onHoverLineCb === null) {
                        return;
                    }
                    const line = e.target?.position?.lineNumber;
                    if (line === undefined) {
                        return;
                    }
                    if (this.hoverLineRaf !== 0) {
                        cancelAnimationFrame(this.hoverLineRaf);
                    }
                    this.hoverLineRaf = requestAnimationFrame(() => {
                        this.hoverLineRaf = 0;
                        this.onHoverLineCb?.(line);
                    });
                }),
            );
            this.disposers.push(
                this.editor.onMouseLeave(() => {
                    if (this.hoverLineRaf !== 0) {
                        cancelAnimationFrame(this.hoverLineRaf);
                        this.hoverLineRaf = 0;
                    }
                    this.onHoverLineCb?.(-1);
                }),
            );

            // Double-click → host resolves the enclosing node's source span.
            this.attachDomListeners();

            // Blur re-locks to read-only.
            this.disposers.push(
                this.editor.onDidBlurEditorWidget(() => {
                    if (!this.editRangeActive) {
                        return;
                    }
                    this.editRangeActive = false;
                    this.editRangeOffset = null;
                    this.editDecoIds = this.editor!.deltaDecorations(this.editDecoIds, []);
                    this.host.classList.add('pagx-editor-readonly');
                    this.editor!.updateOptions({
                        readOnly: true,
                        domReadOnly: true,
                    });
                }),
            );

            // Caret moves while editing → "edit follows caret" re-scoping.
            this.disposers.push(
                this.editor.onDidChangeCursorPosition((e: MonacoNS.editor.ICursorPositionChangedEvent) => {
                    if (!this.editRangeActive || this.onCursorLineCb === null) {
                        return;
                    }
                    this.onCursorLineCb(e.position.lineNumber);
                }),
            );

            // Span confinement: re-attached on each model swap (setContent replaces the model).
            this.attachContentListener();
            // Editor is born in the read-only state; the class is removed by enterEditRange
            // and re-added by blur / setContent to hide Monaco's virtual cursor via CSS.
            this.host.classList.add('pagx-editor-readonly');
        });
    }

    /** Registers the content-change listener that confines edits to the unlocked span. Called
     *  after each model creation/swap because onDidChangeContent lives on the model, not the
     *  editor. Uses e.isUndoing to skip the undo's own content-change event, so no manual
     *  suppressUndo flag is needed. */
    /** Re-attaches DOM-level event listeners that must survive model swaps. Monaco's
     *  editor container is stable across setModel(), but as a safety net (and to guard against
     *  any future internal DOM teardown — e.g. during editor.dispose + recreate) we re-attach
     *  the dblclick listener after every setContent. addEventListener is a no-op for an
     *  identical (target, type, handler, capture) triple, so duplicate calls are harmless. */
    private attachDomListeners(): void {
        const domNode = this.editor?.getDomNode();
        if (domNode !== null && domNode !== undefined) {
            domNode.addEventListener('dblclick', this.handleDomDblClick);
        }
    }

    private attachContentListener(): void {
        if (this.contentChangeListener !== null) {
            this.contentChangeListener.dispose();
            this.contentChangeListener = null;
        }
        if (this.model === null) {
            return;
        }
        this.contentChangeListener = this.model.onDidChangeContent(
            (e: MonacoNS.editor.IModelContentChangedEvent) => {
                if (e.isFlush) {
                    return;
                }
                if (e.isUndoing || e.isRedoing) {
                    // Undo/redo restore or replay previously-applied edits. The out-of-range check
                    // would just bounce the change back, so skip it — but the span's character
                    // offset must still be re-mapped: e.changes are still relative to the pre-event
                    // model, so the same totalDelta formula correctly tracks the new `to`.
                    if (this.editRangeOffset !== null) {
                        let totalDelta = 0;
                        for (const change of e.changes) {
                            totalDelta += change.text.length - change.rangeLength;
                        }
                        this.editRangeOffset = {
                            from: this.editRangeOffset.from,
                            to: this.editRangeOffset.to + totalDelta,
                        };
                    }
                    return;
                }
                if (!this.editRangeActive || this.editRangeOffset === null) {
                    return;
                }
                let outOfRange = false;
                let totalDelta = 0;
                for (const change of e.changes) {
                    const changeStart = change.rangeOffset;
                    const changeEnd = change.rangeOffset + change.rangeLength;
                    if (changeStart < this.editRangeOffset.from || changeEnd > this.editRangeOffset.to) {
                        outOfRange = true;
                        break;
                    }
                    totalDelta += change.text.length - change.rangeLength;
                }
                if (outOfRange) {
                    // Monaco 0.47.0's TextModel has a public undo() method at runtime, but the
                    // TypeScript declarations don't expose it (added in a later type bump). The
                    // cast goes through the model's own _undoRedoService which is more reliable
                    // than editor.trigger('undo') — that path goes through the command system
                    // and can be silently swallowed by other Monaco actions.
                    (this.model as unknown as { undo(): void }).undo();
                    return;
                }
                this.editRangeOffset = {
                    from: this.editRangeOffset.from,
                    to: this.editRangeOffset.to + totalDelta,
                };
            },
        );
    }

    /** Replaces the entire document content, resetting the undo history to this content. When
     *  preserveViewState is true, keeps the current caret and scroll offset. */
    setContent(text: string, preserveViewState = false): void {
        const trimmed = text.charCodeAt(0) === 0xFEFF ? text.slice(1) : text;
        if (this.editor === null || this.model === null) {
            // Monaco may still be loading. createEditor buffers the content and creates the
            // editor once Monaco resolves; if it's already loaded, creation is synchronous
            // inside the .then() callback.
            this.createEditor(trimmed);
            return;
        }
        if (!preserveViewState) {
            const oldModel = this.model;
            this.model = monacoInstance!.editor.createModel(trimmed, 'xml');
            this.editor.setModel(this.model);
            oldModel.dispose();
            this.attachContentListener();
            this.attachDomListeners();
            this.hoverDecoIds = [];
            this.selectDecoIds = [];
            this.editDecoIds = [];
            this.editRangeActive = false;
            this.editRangeOffset = null;
            this.host.classList.add('pagx-editor-readonly');
            this.editor.updateOptions({
                readOnly: true,
                domReadOnly: true,
            });
            return;
        }
        const savedOffset = this.model.getOffsetAt(
            this.editor.getPosition() ?? new monacoInstance!.Position(1, 1),
        );
        const savedScrollTop = this.editor.getScrollTop();
        const oldModel = this.model;
        this.model = monacoInstance!.editor.createModel(trimmed, 'xml');
        this.editor.setModel(this.model);
        oldModel.dispose();
        this.attachContentListener();
        this.attachDomListeners();
        const clampedOffset = Math.min(savedOffset, this.model.getValueLength());
        const pos = this.model.getPositionAt(clampedOffset);
        this.editor.setPosition(pos);
        this.editor.setScrollTop(savedScrollTop);
        this.hoverDecoIds = [];
        this.selectDecoIds = [];
        this.editDecoIds = [];
        this.editRangeActive = false;
        this.editRangeOffset = null;
        this.host.classList.add('pagx-editor-readonly');
        this.editor.updateOptions({
            readOnly: true,
            domReadOnly: true,
        });
    }

    /** Returns the current document text. */
    getContent(): string {
        if (this.model === null) {
            return '';
        }
        return this.model.getValue();
    }

    /** Highlights the node's source span as the transient grey hover layer. startLine <= 0 clears. */
    highlightHover(startLine: number, endLine: number): void {
        if (this.editor === null || this.model === null) {
            return;
        }
        if (startLine <= 0) {
            this.hoverDecoIds = this.editor.deltaDecorations(this.hoverDecoIds, []);
            return;
        }
        const decos: MonacoNS.editor.IModelDeltaDecoration[] = [];
        buildLineDecos({ startLine, endLine }, this.model, HOVER_LINE_CLASS, decos);
        this.hoverDecoIds = this.editor.deltaDecorations(this.hoverDecoIds, decos);
    }

    /** Clears the grey hover highlight layer. */
    clearHover(): void {
        if (this.editor === null) {
            return;
        }
        this.hoverDecoIds = this.editor.deltaDecorations(this.hoverDecoIds, []);
    }

    /** Highlights the node's source span as the sticky blue selection layer. startLine <= 0 clears. */
    highlightSelect(startLine: number, endLine: number): void {
        if (this.editor === null || this.model === null) {
            return;
        }
        if (startLine <= 0) {
            this.selectDecoIds = this.editor.deltaDecorations(this.selectDecoIds, []);
            return;
        }
        const decos: MonacoNS.editor.IModelDeltaDecoration[] = [];
        buildLineDecos({ startLine, endLine }, this.model, SELECT_LINE_CLASS, decos);
        this.selectDecoIds = this.editor.deltaDecorations(this.selectDecoIds, decos);
    }

    /** Clears the blue selection highlight layer. */
    clearSelect(): void {
        if (this.editor === null) {
            return;
        }
        this.selectDecoIds = this.editor.deltaDecorations(this.selectDecoIds, []);
    }

    /** Clears both highlight layers. */
    clearHighlight(): void {
        this.clearHover();
        this.clearSelect();
    }

    /** Unlocks the given 1-based inclusive line span for editing. */
    enterEditRange(startLine: number, endLine: number): void {
        if (this.editor === null || this.model === null || startLine <= 0) {
            return;
        }
        this.editRangeActive = true;
        this.editRangeOffset = this.computeEditRangeOffset(startLine, endLine);
        this.host.classList.remove('pagx-editor-readonly');
        this.editor.updateOptions({ readOnly: false, domReadOnly: false });
        const decos: MonacoNS.editor.IModelDeltaDecoration[] = [];
        buildEditDecos({ startLine, endLine }, this.model, decos);
        this.editDecoIds = this.editor.deltaDecorations(this.editDecoIds, decos);
        this.editor.focus();
    }

    /** Re-scopes the already-unlocked editable span to a new line range. No-op when read-only. */
    updateEditRange(startLine: number, endLine: number): void {
        if (this.editor === null || this.model === null || startLine <= 0) {
            return;
        }
        if (!this.editRangeActive) {
            return;
        }
        this.editRangeOffset = this.computeEditRangeOffset(startLine, endLine);
        const decos: MonacoNS.editor.IModelDeltaDecoration[] = [];
        buildEditDecos({ startLine, endLine }, this.model, decos);
        this.editDecoIds = this.editor.deltaDecorations(this.editDecoIds, decos);
    }

    /** Converts a 1-based inclusive line range to character offsets for span-confinement checks. */
    private computeEditRangeOffset(startLine: number, endLine: number): { from: number; to: number } | null {
        if (this.model === null || monacoInstance === null) {
            return null;
        }
        const last = this.model.getLineCount();
        const start = clampLine(startLine, last);
        const end = clampLine(endLine, last);
        const from = this.model.getOffsetAt(new monacoInstance.Position(start, 1));
        const to = this.model.getOffsetAt(
            new monacoInstance.Position(end, this.model.getLineMaxColumn(end)),
        );
        return { from, to };
    }

    /** Scrolls the given 1-based line into view. */
    scrollToLine(line: number, align: 'start' | 'nearest' = 'start'): void {
        if (this.editor === null || line <= 0) {
            return;
        }
        const targetLine = Math.min(line, this.model?.getLineCount() ?? 1);
        if (align === 'nearest') {
            this.editor.revealLineInCenterIfOutsideViewport(targetLine);
        } else {
            this.editor.revealLine(targetLine, monacoInstance!.editor.ScrollType.Smooth);
        }
    }

    onHoverLine(cb: ((line: number) => void) | null): void {
        this.onHoverLineCb = cb;
    }

    onDblClickLine(cb: ((line: number) => void) | null): void {
        this.onDblClickLineCb = cb;
    }

    onCursorLine(cb: ((line: number) => void) | null): void {
        this.onCursorLineCb = cb;
    }

    private readonly handleDomDblClick = (event: MouseEvent): void => {
        if (this.editor === null || this.onDblClickLineCb === null) {
            return;
        }
        const pos = this.editor.getTargetAtClientPoint(event.clientX, event.clientY);
        const line = pos?.position?.lineNumber;
        if (line === undefined) {
            return;
        }
        event.preventDefault();
        this.onDblClickLineCb(line);
    };

    /** Releases the Monaco editor and frees DOM nodes. */
    destroy(): void {
        this.destroyed = true;
        if (this.hoverLineRaf !== 0) {
            cancelAnimationFrame(this.hoverLineRaf);
            this.hoverLineRaf = 0;
        }
        const domNode = this.editor?.getDomNode();
        if (domNode !== null && domNode !== undefined) {
            domNode.removeEventListener('dblclick', this.handleDomDblClick);
        }
        this.disposers.forEach((d) => d.dispose());
        this.disposers.length = 0;
        if (this.contentChangeListener !== null) {
            this.contentChangeListener.dispose();
            this.contentChangeListener = null;
        }
        if (this.model !== null) {
            this.model.dispose();
            this.model = null;
        }
        if (this.editor !== null) {
            this.editor.dispose();
            this.editor = null;
        }
    }
}
