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
import type { SourceDiagnostic, SourceDiagnosticProvider } from '../types';

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
            // Opaque scrollbar slider colors (DevTools style). vs-dark's defaults are
            // semi-transparent (#79797966 etc.); setting opaque hex values here makes Monaco's
            // theme system emit --vscode-scrollbarSlider-background etc. as opaque colors, which
            // the built-in scrollbars.css applies to .slider via var(). This is more reliable
            // than overriding .slider background in EDITOR_STYLES because the variable is set on
            // .monaco-editor as an inline style by the theme application, and inline style + var()
            // would need !important on a very specific selector to override — setting the token
            // here updates the variable at the source.
            'scrollbarSlider.background': '#797979',
            'scrollbarSlider.hoverBackground': '#646464',
            'scrollbarSlider.activeBackground': '#505050',
            'scrollbar.shadow': '#00000000',
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
const XML_MARKER_OWNER = 'pagx-xml-syntax';
const XML_VALIDATION_DELAY_MS = 400;

interface XmlSyntaxDiagnostic {
    message: string;
    line: number;
    column: number;
}

/** Parses XML with the browser parser and returns source-positioned syntax diagnostics. The
 *  browser-specific parsererror text is normalized into a stable message plus 1-based line and
 *  column, which Monaco can render as a gutter error marker and an inline squiggle. */
export function getXmlSyntaxDiagnostics(xmlText: string): XmlSyntaxDiagnostic[] {
    const doc = new DOMParser().parseFromString(xmlText, 'application/xml');
    const parseError = doc.querySelector('parsererror');
    if (parseError === null) {
        return [];
    }
    const errorText = parseError.textContent?.trim() || 'Invalid XML format';
    const chromeMatch = errorText.match(
        /error on line\s+(\d+)\s+at column\s+(\d+)\s*:\s*([^\n]+)/i,
    );
    if (chromeMatch !== null) {
        return [{ message: chromeMatch[3].trim(), line: Number(chromeMatch[1]), column: Number(chromeMatch[2]) }];
    }
    const firefoxPosition = errorText.match(/Line Number\s+(\d+),\s*Column\s+(\d+)/i);
    const firefoxMessage = errorText.match(/XML Parsing Error:\s*([^\n]+)/i);
    if (firefoxPosition !== null) {
        return [{
            message: firefoxMessage?.[1].trim() || 'Invalid XML format',
            line: Number(firefoxPosition[1]),
            column: Number(firefoxPosition[2]),
        }];
    }
    const firstLine = errorText.split('\n').find((line) => line.trim() !== '')?.trim() || 'Invalid XML format';
    return [{ message: firstLine, line: 1, column: 1 }];
}


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
    const line = clampLine(range.startLine, model.getLineCount());
    out.push({
        range: {
            startLineNumber: line,
            startColumn: 1,
            endLineNumber: line,
            endColumn: model.getLineMaxColumn(line),
        },
        options: { isWholeLine: true, className: EDIT_LINE_CLASS },
    });
}

/**
 * Wraps a Monaco editor instance for editing PAGX XML source.
 * Monaco loads asynchronously from CDN; until it's ready, all methods are no-ops (the editor
 * instance is null). The first setContent() triggers creation; once Monaco resolves, the editor
 * is created with the buffered content.
 */
export class SourceEditor {
    private readonly host: HTMLElement;
    private readonly diagnosticProviders: readonly SourceDiagnosticProvider[];
    private diagnosticGeneration = 0;
    private editor: MonacoNS.editor.IStandaloneCodeEditor | null = null;
    private model: MonacoNS.editor.ITextModel | null = null;
    private onHoverLineCb: ((line: number) => void) | null = null;
    private onDblClickLineCb: ((line: number) => void) | null = null;
    private hoverLineRaf = 0;
    private hoverDecoIds: string[] = [];
    private selectDecoIds: string[] = [];
    private editDecoIds: string[] = [];
    private editRangeActive = false;
    private editRangeOffset: { from: number; to: number } | null = null;
    private contentChangeListener: MonacoNS.IDisposable | null = null;
    private syntaxValidationTimer: number | null = null;
    private destroyed = false;
    private creating = false;
    private readonly disposers: MonacoNS.IDisposable[] = [];

    constructor(host: HTMLElement, diagnosticProviders: readonly SourceDiagnosticProvider[] = []) {
        this.host = host;
        this.diagnosticProviders = diagnosticProviders;
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
                // Monaco otherwise changes wordWrap to viewport wrapping when the document is
                // dominated by long Base64 image lines. Explicitly override that large-file mode:
                // PAGX source must retain one visual row per physical line and use the horizontal
                // scrollbar for long attributes.
                wordWrapOverride1: 'off',
                renderWhitespace: 'none',
                largeFileOptimizations: true,
                automaticLayout: true,
                // Monaco defaults renderValidationDecorations to 'editable', which hides marker
                // squiggles as soon as the editor re-locks to read-only on blur. Schema/XML
                // diagnostics must remain visible in both browsing and editing modes.
                renderValidationDecorations: 'on',
                unicodeHighlight: { ambiguousCharacters: false, invisibleCharacters: false },
                fontSize: 13,
                fontFamily: 'Menlo, Monaco, "Courier New", monospace',
                lineHeight: 18,
                scrollbar: {
                    // Keep tracks visible instead of fading out after scrolling.
                    vertical: 'visible',
                    horizontal: 'visible',
                    // The opaque track is wider than its 10px thumb, matching DevTools.
                    verticalScrollbarSize: 14,
                    horizontalScrollbarSize: 14,
                    useShadows: false,
                    verticalHasArrows: false,
                },
                // The overview ruler only adds a redundant canvas strip beside the scrollbar.
                overviewRulerLanes: 0,
                overviewRulerBorder: false,
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
                this.editor.onDidBlurEditorWidget(() => this.leaveEditMode()),
            );
            // A single click on another source tag leaves the old edit session before that tag is
            // browsed. This removes its amber decoration and prevents a read-only grey hover from
            // retaining an editable caret or native text selection.
            this.disposers.push(
                this.editor.onMouseDown(() => {
                    if (this.editRangeActive) {
                        this.leaveEditMode();
                        const activeElement = document.activeElement;
                        if (activeElement instanceof HTMLElement &&
                            this.editor?.getDomNode()?.contains(activeElement)) {
                            activeElement.blur();
                        }
                    }
                }),
            );

            // Span confinement: re-attached on each model swap (setContent replaces the model).
            this.attachContentListener();
            // Editor is born in the read-only state; the class is removed by enterEditRange
            // and re-added by blur / setContent to hide Monaco's virtual cursor via CSS.
            this.host.classList.add('pagx-editor-readonly');
        });
    }

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
                this.scheduleSyntaxValidation();
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
        void this.updateSyntaxDiagnostics();
    }

    /** Defers XML syntax parsing until the user pauses typing. DOMParser is fast for normal
     *  documents but can still be expensive for large PAGX files, so each new change replaces
     *  the pending check rather than parsing every keystroke. */
    private scheduleSyntaxValidation(): void {
        if (this.syntaxValidationTimer !== null) {
            window.clearTimeout(this.syntaxValidationTimer);
        }
        this.syntaxValidationTimer = window.setTimeout(() => {
            this.syntaxValidationTimer = null;
            void this.updateSyntaxDiagnostics();
        }, XML_VALIDATION_DELAY_MS);
    }

    /** Runs every registered semantic provider. A provider's id is the Monaco marker owner for
     *  its result, so it can safely refresh without clearing syntax or another provider's output.
     *  Individual provider failures do not turn the current source into an invalid document; they
     *  are ignored until that provider can produce a concrete diagnostic. */
    private async getProviderDiagnostics(xmlText: string): Promise<SourceDiagnostic[]> {
        const results = await Promise.all(
            this.diagnosticProviders.map(async (provider) => {
                try {
                    const diagnostics = await provider.validate(xmlText);
                    return diagnostics.map((diagnostic) => ({ ...diagnostic, owner: provider.id }));
                } catch {
                    return [];
                }
            }),
        );
        return results.flat();
    }

    /** Builds Monaco markers at the source locations supplied by a diagnostic provider. */
    private makeErrorMarkers(diagnostics: ReadonlyArray<SourceDiagnostic>): MonacoNS.editor.IMarkerData[] {
        const model = this.model;
        const monaco = monacoInstance;
        if (model === null || monaco === null) {
            return [];
        }
        return diagnostics.map((diagnostic) => {
            const startLine = clampLine(diagnostic.startLine, model.getLineCount());
            const endLine = clampLine(diagnostic.endLine ?? startLine, model.getLineCount());
            const startColumn = Math.min(
                Math.max(1, diagnostic.startColumn),
                model.getLineMaxColumn(startLine),
            );
            const endColumn = Math.min(
                Math.max(startColumn + 1, diagnostic.endColumn ?? startColumn + 1),
                model.getLineMaxColumn(endLine),
            );
            const severity = diagnostic.severity === 'warning'
                ? monaco.MarkerSeverity.Warning
                : diagnostic.severity === 'info'
                    ? monaco.MarkerSeverity.Info
                    : monaco.MarkerSeverity.Error;
            return {
                severity,
                message: diagnostic.message,
                startLineNumber: startLine,
                startColumn,
                endLineNumber: endLine,
                endColumn,
            };
        });
    }

    /** Publishes XML well-formedness and all registered semantic provider diagnostics. Each
     *  provider is independently owned in Monaco. XML syntax failure skips semantic providers,
     *  avoiding misleading schema errors while a tag or attribute is being typed. Stale async
     *  validation results are dropped if the model text changes before they resolve. */
    private async updateSyntaxDiagnostics(): Promise<SourceDiagnostic[]> {
        const model = this.model;
        const monaco = monacoInstance;
        if (model === null || monaco === null) {
            return [];
        }
        const xmlText = model.getValue();
        const generation = ++this.diagnosticGeneration;
        const syntaxDiagnostics = getXmlSyntaxDiagnostics(xmlText);
        const syntaxMarkers = syntaxDiagnostics.map((diagnostic): SourceDiagnostic => ({
            owner: XML_MARKER_OWNER,
            severity: 'error',
            message: diagnostic.message,
            startLine: diagnostic.line,
            startColumn: diagnostic.column,
        }));
        monaco.editor.setModelMarkers(model, XML_MARKER_OWNER, this.makeErrorMarkers(syntaxMarkers));
        if (syntaxMarkers.length !== 0) {
            for (const provider of this.diagnosticProviders) {
                monaco.editor.setModelMarkers(model, provider.id, []);
            }
            return syntaxMarkers;
        }
        const providerDiagnostics = await this.getProviderDiagnostics(xmlText);
        if (this.destroyed || generation !== this.diagnosticGeneration || this.model !== model || model.getValue() !== xmlText) {
            return [];
        }
        for (const provider of this.diagnosticProviders) {
            const diagnostics = providerDiagnostics.filter((diagnostic) => diagnostic.owner === provider.id);
            monaco.editor.setModelMarkers(model, provider.id, this.makeErrorMarkers(diagnostics));
        }
        return providerDiagnostics;
    }

    /** Returns the first blocking diagnostic for Apply and Save, after synchronously awaiting the
     *  current validation generation so the toast and Monaco marker always describe the same text. */
    public async getValidationError(): Promise<string> {
        const diagnostics = await this.updateSyntaxDiagnostics();
        const error = diagnostics.find((diagnostic) => diagnostic.severity === 'error');
        if (error === undefined) {
            return '';
        }
        return `Line ${error.startLine}, column ${error.startColumn}: ${error.message}`;
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
            this.host.classList.remove('pagx-editor-editing');
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
        this.host.classList.remove('pagx-editor-editing');
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

    /** Returns the current physical XML tag line, or null for text, blank and attribute-continuation
     *  lines. This gives each displayed tag line one owner instead of treating every line inside a
     *  parent element as part of that parent block. */
    getDraftTagLine(line: number): number | null {
        if (this.model === null || line <= 0 || line > this.model.getLineCount()) {
            return null;
        }
        const text = this.model.getLineContent(line);
        return /<\/?[A-Za-z_?!]/.test(text) ? line : null;
    }

    /** Resolves the source span owned by a physical XML tag line. This fallback covers document-level
     *  constructs (the XML declaration and <pagx> root) that intentionally have no runtime node in
     *  the player source map, while ordinary layer nodes continue to use that authoritative map. */
    getDraftTagEditRange(line: number): LineRange | null {
        const tagLine = this.getDraftTagLine(line);
        if (this.model === null || tagLine === null) {
            return null;
        }
        const lineText = this.model.getLineContent(tagLine);
        if (/^\s*<\?/.test(lineText)) {
            return { startLine: tagLine, endLine: tagLine };
        }
        const xmlText = this.model.getValue();
        const tags = /<\s*(\/?)\s*([A-Za-z_][\w:.-]*)\b[^>]*?>/g;
        const openTags: { name: string; startLine: number }[] = [];
        let match: RegExpExecArray | null;
        while ((match = tags.exec(xmlText)) !== null) {
            const token = match[0];
            const tokenLine = xmlText.slice(0, match.index).split('\n').length;
            const isClosing = match[1] === '/';
            const isSelfClosing = /\/\s*>$/.test(token);
            const name = match[2];
            if (!isClosing && !isSelfClosing) {
                openTags.push({ name, startLine: tokenLine });
                continue;
            }
            if (!isClosing) {
                if (tokenLine === tagLine) {
                    return { startLine: tagLine, endLine: tagLine };
                }
                continue;
            }
            for (let i = openTags.length - 1; i >= 0; i--) {
                if (openTags[i].name !== name) {
                    continue;
                }
                const opening = openTags[i];
                openTags.length = i;
                if (opening.startLine === tagLine || tokenLine === tagLine) {
                    return { startLine: opening.startLine, endLine: tokenLine };
                }
                break;
            }
        }
        return null;
    }

    /** Returns an active edit session to read-only browsing and clears its amber tag decoration. */
    private leaveEditMode(): void {
        if (!this.editRangeActive || this.editor === null) {
            return;
        }
        this.editRangeActive = false;
        this.editRangeOffset = null;
        this.editDecoIds = this.editor.deltaDecorations(this.editDecoIds, []);
        this.host.classList.remove('pagx-editor-editing');
        this.host.classList.add('pagx-editor-readonly');
        this.editor.updateOptions({ readOnly: true, domReadOnly: true });
    }

    /** Handles undo and redo while the editor is read-only. Monaco disables its built-in commands
     *  in that mode, but document history must remain available after a temporary edit is re-locked. */
    handleReadOnlyUndoRedo(event: KeyboardEvent): boolean {
        if (this.editor === null || this.model === null || this.editRangeActive ||
            !this.editor.hasTextFocus() || (!event.ctrlKey && !event.metaKey) || event.altKey) {
            return false;
        }
        const key = event.key.toLowerCase();
        const undo = key === 'z' && !event.shiftKey;
        const redo = (key === 'z' && event.shiftKey) || (key === 'y' && event.ctrlKey && !event.metaKey);
        if (!undo && !redo) {
            return false;
        }
        event.preventDefault();
        event.stopPropagation();
        const history = this.model as unknown as { undo(): void; redo(): void };
        if (undo) {
            history.undo();
        } else {
            history.redo();
        }
        return true;
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

    /** Unlocks the enclosing element's full source span for editing while showing amber only on the
     *  physical opening/closing tag line that was double-clicked. */
    enterEditRange(startLine: number, endLine: number, decorationLine: number): void {
        if (this.editor === null || this.model === null || startLine <= 0) {
            return;
        }
        this.editRangeOffset = this.computeEditRangeOffset(startLine, endLine);
        if (this.editRangeOffset === null) {
            return;
        }
        this.editRangeActive = true;
        this.host.classList.remove('pagx-editor-readonly');
        this.host.classList.add('pagx-editor-editing');
        this.editor.updateOptions({ readOnly: false, domReadOnly: false });
        const decos: MonacoNS.editor.IModelDeltaDecoration[] = [];
        buildEditDecos({ startLine: decorationLine, endLine: decorationLine }, this.model, decos);
        this.editDecoIds = this.editor.deltaDecorations(this.editDecoIds, decos);
        this.editor.focus();
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
        if (this.syntaxValidationTimer !== null) {
            window.clearTimeout(this.syntaxValidationTimer);
            this.syntaxValidationTimer = null;
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
