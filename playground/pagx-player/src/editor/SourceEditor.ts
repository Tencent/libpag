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

// Monaco is loaded from CDN at runtime via the AMD loader (same as the monaco-test.html page).
// The npm package is kept in devDependencies for TypeScript types only — `import type` is erased
// by esbuild at build time, so rollup never tries to bundle Monaco's non-standard ESM internals
// (which use bare `vs/...` specifiers that nodeResolve cannot map).
import type * as MonacoNS from 'monaco-editor';
import type { SourceDiagnostic, SourceDiagnosticProvider } from '../types';

type Monaco = typeof MonacoNS;

// Module-level Monaco singleton. Loaded on demand when the first editor is created — nothing is
// fetched at import time, so hosts that never open the editor panel never touch the CDN. All
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
        // Reset the cached promise on failure so a later open() retries the CDN load; without
        // this, one transient network failure would leave every future loadMonaco() call stuck
        // on the rejected promise (editor dead until a full page reload).
        const failLoad = (message: string): void => {
            monacoLoadPromise = null;
            script.remove();
            reject(new Error(message));
        };
        script.onload = () => {
            const win = window as unknown as {
                require?: {
                    config: (opts: { paths: { vs: string } }) => void;
                    (deps: string[], cb: (...modules: unknown[]) => void,
                     errback?: (err: unknown) => void): void;
                };
                monaco?: Monaco;
            };
            const amdRequire = win.require;
            if (amdRequire === undefined) {
                failLoad('Monaco AMD loader failed to initialize');
                return;
            }
            amdRequire.config({ paths: { vs: MONACO_CDN } });
            amdRequire(['vs/editor/editor.main'], () => {
                const m = win.monaco;
                if (m === undefined) {
                    failLoad('Monaco failed to load from CDN');
                    return;
                }
                pruneBuiltInContextMenuItems(amdRequire);
                definePagxTheme(m);
                installShadowMenuStyles();
                monacoInstance = m;
                resolve(m);
            }, () => failLoad('Monaco modules failed to load from CDN'));
        };
        script.onerror = () => failLoad('Failed to load Monaco loader script from CDN');
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
            // Keep active and inactive selections identical so the double-click transition from
            // read-only browsing into edit mode does not flash between two colors.
            'editor.selectionBackground': '#A8C7FA',
            'editor.inactiveSelectionBackground': '#A8C7FA',
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

// The standalone editor offers no public API to remove built-in context menu entries (the
// clipboard trio, Change All Occurrences, Command Palette). The pagx editor replaces the whole
// menu with its own actions, so right after the AMD bundle loads - before any editor instance
// appends its own items - every entry registered under MenuId.EditorContext is cleared. This
// relies on the private MenuRegistry._menuItems structure of the pinned monaco-editor 0.47.0
// build (see MONACO_CDN); each step is shape-checked so a future Monaco upgrade degrades to
// showing the default menu instead of throwing.
function pruneBuiltInContextMenuItems(
    amdRequire: (deps: string[], cb: (...modules: unknown[]) => void) => void,
): void {
    amdRequire(['vs/platform/actions/common/actions'], (actionsModule: unknown) => {
        const actions = actionsModule as {
            MenuId?: { EditorContext?: unknown };
            MenuRegistry?: { _menuItems?: unknown };
        } | null;
        const menuId = actions?.MenuId?.EditorContext;
        const menuItems = actions?.MenuRegistry?._menuItems;
        if (menuId === undefined || !(menuItems instanceof Map)) {
            return;
        }
        const list = menuItems.get(menuId) as { clear?: () => void } | undefined;
        list?.clear?.();
    });
}

// Monaco 0.47.0 renders its context menu inside a Shadow DOM (div.shadow-root-host attached to
// <body>). Styles injected into document.head cannot cross that shadow boundary, so the plain
// EDITOR_STYLES sheet has no effect on menu items like span.keybinding. To style the shortcut
// column we watch for the shadow host to appear and adopt an extra sheet into its shadowRoot;
// the observer stops itself once the sheet is installed. Idempotent — subsequent Monaco loads
// (there is only ever one) short-circuit via the module-level flag.
let shadowMenuStylesInstalled = false;

function installShadowMenuStyles(): void {
    if (shadowMenuStylesInstalled || typeof document === 'undefined') {
        return;
    }
    const css = '.monaco-menu-container span.keybinding { color: #8A8A8A !important; }';
    const tryInstall = (host: Element): boolean => {
        const root = (host as Element & { shadowRoot?: ShadowRoot | null }).shadowRoot;
        if (root === null || root === undefined) {
            return false;
        }
        // Skip if we've already added the sheet to this root (Monaco reuses one shadow host).
        for (const child of Array.from(root.children)) {
            if (child instanceof HTMLStyleElement && child.dataset.pagxMenuStyles === 'true') {
                return true;
            }
        }
        const style = document.createElement('style');
        style.dataset.pagxMenuStyles = 'true';
        style.textContent = css;
        root.appendChild(style);
        return true;
    };
    // The host may already exist by the time Monaco resolves. Try synchronously first.
    for (const host of Array.from(document.querySelectorAll('.shadow-root-host'))) {
        if (tryInstall(host)) {
            shadowMenuStylesInstalled = true;
            return;
        }
    }
    // Otherwise wait for it to be appended to <body>. Monaco creates the host lazily the first
    // time the context menu opens; the observer disconnects as soon as we succeed.
    const observer = new MutationObserver(() => {
        for (const host of Array.from(document.querySelectorAll('.shadow-root-host'))) {
            if (tryInstall(host)) {
                shadowMenuStylesInstalled = true;
                observer.disconnect();
                return;
            }
        }
    });
    observer.observe(document.body, { childList: true, subtree: true });
}

// Clipboard write for the tag-block and whole-file copy actions. navigator.clipboard is only
// exposed in secure contexts, so plain-http hosts fall back to a temporary textarea and
// execCommand('copy') (deprecated, but the only mechanism available there).
function copyTextToClipboard(text: string): void {
    if (navigator.clipboard !== undefined) {
        navigator.clipboard.writeText(text).then(undefined, () => execCommandCopy(text));
        return;
    }
    execCommandCopy(text);
}

function execCommandCopy(text: string): void {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    textarea.select();
    document.execCommand('copy');
    textarea.remove();
}

// A 1-based inclusive line range mirrored from a node's source span, or null to clear.
export interface LineRange {
    startLine: number;
    endLine: number;
}

/** A model replacement expressed in pre-change physical line coordinates. */
export interface DraftLineChange {
    startLine: number;
    endLine: number;
    insertedLineCount: number;
}

// Decoration class names (must match styles.ts selectors).
const HOVER_LINE_CLASS = 'pagx-hover-line';
const SELECT_LINE_CLASS = 'pagx-select-line';
const EDIT_LINE_CLASS = 'pagx-edit-line';
const EDIT_SELECTION_CLASS = 'pagx-edit-selection';
const XML_MARKER_OWNER = 'pagx-xml-syntax';
const XML_VALIDATION_DELAY_MS = 400;
// Editor context key gating the Cut/Paste context menu actions to edit mode.
const EDITING_CONTEXT_KEY = 'pagxSourceEditing';

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
    decorationLine: number,
    model: MonacoNS.editor.ITextModel,
    out: MonacoNS.editor.IModelDeltaDecoration[],
): void {
    const line = clampLine(decorationLine, model.getLineCount());
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
    private onDraftLineChangesCb: ((changes: readonly DraftLineChange[] | null) => void) | null = null;
    private onApplyRequestCb: (() => void) | null = null;
    private tagSpanResolver: ((line: number) => LineRange | null) | null = null;
    private editingContextKey: MonacoNS.editor.IContextKey<boolean> | null = null;
    private editPointerDown: { lineNumber: number; column: number } | null = null;
    private editDecorationLine = -1;
    private hoverLineRaf = 0;
    private hoverDecoIds: string[] = [];
    private selectDecoIds: string[] = [];
    private editDecoIds: string[] = [];
    private editSelectionDecoIds: string[] = [];
    // Whether the whole document is currently writable. This flag has exactly one
    // responsibility: gating readOnly on/off. It is set purely by line-number comparisons
    // (double-click enters, an unmoved click on a different line exits) and never by inspecting
    // document content — well-formedness is the separate job of the syntax/Apply validators.
    private editingActive = false;
    private contentChangeListener: MonacoNS.IDisposable | null = null;
    private syntaxValidationTimer: number | null = null;
    private destroyed = false;
    private creating = false;
    private loadErrorEl: HTMLElement | null = null;
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
            this.clearLoadError();
            this.model = m.editor.createModel(initialContent, 'xml');
            this.editor = m.editor.create(this.host, {
                model: this.model,
                theme: 'pagx-dark',
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
                // Keep Monaco's matching-text highlights, but SourceEditor marks the real edit
                // selection with an inline decoration so CSS can suppress the matching-token grey
                // only under the active blue selection.
                selectionHighlight: true,
                occurrencesHighlight: 'singleFile',
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
                    if (this.editingActive || this.onHoverLineCb === null) {
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

            // Whether to leave edit mode is decided purely by comparing line numbers: an unmoved
            // left-click landing on a physical line other than editDecorationLine exits, full
            // stop — the clicked line's text is never inspected. Losing DOM focus
            // (onDidBlurEditorWidget) is intentionally NOT wired to leaveEditMode — Monaco's own
            // right-click context menu blurs the hidden textarea as a transient side effect of
            // opening it, which previously kicked the editor back to read-only mid-context-menu.
            // Right-click and drag must never change edit state, so right/middle-button presses
            // are ignored below and any pointer movement between down and up is treated as a
            // selection, not a click.
            this.disposers.push(
                this.editor.onMouseDown((e: MonacoNS.editor.IEditorMouseEvent) => {
                    if (!this.editingActive || !e.event.leftButton) {
                        this.editPointerDown = null;
                        return;
                    }
                    const position = e.target?.position;
                    this.editPointerDown = position === null || position === undefined
                        ? null
                        : { lineNumber: position.lineNumber, column: position.column };
                }),
            );
            this.disposers.push(
                this.editor.onMouseUp((e: MonacoNS.editor.IEditorMouseEvent) => {
                    if (!e.event.leftButton) {
                        this.editPointerDown = null;
                        return;
                    }
                    this.handleEditPointerUp(e.target?.position);
                }),
            );
            // Monaco paints selection backgrounds in an overlay and leaves syntax-token foregrounds
            // untouched. Mirror non-empty edit selections as inline decorations so selected text can
            // reliably use Chrome DevTools' black foreground regardless of token type.
            this.disposers.push(
                this.editor.onDidChangeCursorSelection(() => this.refreshEditSelectionDecorations()),
            );

            // Line-delta bookkeeping: re-attached on each model swap (setContent replaces the model).
            this.attachContentListener();
            this.editingContextKey = this.editor.createContextKey(EDITING_CONTEXT_KEY, false);
            this.registerContextMenuActions(m);
            // Editor is born in the read-only state; the class is removed by enterEditMode
            // and re-added by leaveEditMode / setContent to hide Monaco's virtual cursor via CSS.
            this.host.classList.add('pagx-editor-readonly');
        }).catch((error: unknown) => {
            this.creating = false;
            // Drop the rejected promise so a later setContent() retries the CDN fetch instead of
            // re-attaching to the same failure.
            monacoLoadPromise = null;
            if (this.destroyed) {
                return;
            }
            console.error('SourceEditor: failed to load Monaco from CDN.', error);
            this.showLoadError();
        });
    }

    private showLoadError(): void {
        if (this.loadErrorEl !== null) {
            return;
        }
        const el = document.createElement('div');
        el.className = 'pagx-editor-load-error';
        el.textContent = 'The source editor failed to load (Monaco CDN unreachable). Check the ' +
            'network connection; the editor retries on the next document load.';
        this.host.appendChild(el);
        this.loadErrorEl = el;
    }

    private clearLoadError(): void {
        this.loadErrorEl?.remove();
        this.loadErrorEl = null;
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

    /** Replaces Monaco's built-in context menu (cleared once in loadMonaco) with the pagx action
     *  set. Copy/Cut/Paste re-dispatch the built-in clipboard commands so their behavior matches
     *  the original menu entries exactly; the whole-file and tag-block variants are custom.
     *  Cut/Paste show only in edit mode via the pagxSourceEditing context key; the delete actions
     *  stay available while read-only, where the removal is applied to the canvas immediately
     *  (see deleteRanges) instead of staging a draft that needs a separate Apply click. */
    private registerContextMenuActions(m: Monaco): void {
        const editor = this.editor;
        if (editor === null) {
            return;
        }
        // Swallow the built-in F1 shortcut so the Command Palette stays unreachable now that its
        // menu entry is pruned.
        editor.addCommand(m.KeyCode.F1, () => undefined, 'editorTextFocus');
        // While the document is unlocked, Enter commits the edits through the host's Apply
        // pipeline (same as the Apply button); Shift+Enter keeps newline insertion available.
        // addCommand returns a command id rather than a disposable; the commands are bound to
        // this editor instance and disappear with its dispose(), like the F1 swallow above.
        editor.addCommand(m.KeyCode.Enter, () => {
            this.onApplyRequestCb?.();
        }, EDITING_CONTEXT_KEY);
        editor.addCommand(m.KeyMod.Shift | m.KeyCode.Enter, () => {
            editor.trigger('keyboard', 'type', { text: '\n' });
        }, EDITING_CONTEXT_KEY);
        // Keybindings mirror the shortcut hints shown in the context menu. CtrlCmd resolves to
        // Cmd on macOS and Ctrl elsewhere, so a single binding is cross-platform. Copy/Cut/Paste
        // reuse Monaco's built-in clipboard shortcuts (Cmd/Ctrl+C/X/V), which stay active because
        // the built-in commands are still registered; the keybindings here only opt the pagx
        // custom actions into the menu's shortcut column and never intercept the native handling.
        this.disposers.push(
            editor.addAction({
                id: 'pagx.copy',
                label: 'Copy',
                keybindings: [m.KeyMod.CtrlCmd | m.KeyCode.KeyC],
                contextMenuGroupId: '1_copy',
                contextMenuOrder: 1,
                run: (ed) => ed.trigger('contextMenu', 'editor.action.clipboardCopyAction', null),
            }),
        );
        this.disposers.push(
            editor.addAction({
                id: 'pagx.copyEntireFile',
                label: 'Copy Entire File',
                keybindings: [m.KeyMod.CtrlCmd | m.KeyMod.Shift | m.KeyCode.KeyC],
                contextMenuGroupId: '1_copy',
                contextMenuOrder: 2,
                run: () => {
                    if (this.model !== null) {
                        copyTextToClipboard(this.model.getValue());
                    }
                },
            }),
        );
        this.disposers.push(
            editor.addAction({
                id: 'pagx.copyTagBlock',
                label: 'Copy Tag Block',
                keybindings: [m.KeyMod.CtrlCmd | m.KeyMod.Alt | m.KeyCode.KeyC],
                contextMenuGroupId: '1_copy',
                contextMenuOrder: 3,
                run: (ed) => {
                    const model = this.model;
                    const position = ed.getPosition();
                    if (model === null || position === null) {
                        return;
                    }
                    const span = this.getTagBlockLineRange(position.lineNumber);
                    const text = model.getValueInRange({
                        startLineNumber: span.startLine,
                        startColumn: 1,
                        endLineNumber: span.endLine,
                        endColumn: model.getLineMaxColumn(span.endLine),
                    });
                    copyTextToClipboard(text + '\n');
                },
            }),
        );
        this.disposers.push(
            editor.addAction({
                id: 'pagx.cut',
                label: 'Cut',
                keybindings: [m.KeyMod.CtrlCmd | m.KeyCode.KeyX],
                precondition: EDITING_CONTEXT_KEY,
                contextMenuGroupId: '2_edit',
                contextMenuOrder: 1,
                run: (ed) => ed.trigger('contextMenu', 'editor.action.clipboardCutAction', null),
            }),
        );
        this.disposers.push(
            editor.addAction({
                id: 'pagx.paste',
                label: 'Paste',
                keybindings: [m.KeyMod.CtrlCmd | m.KeyCode.KeyV],
                precondition: EDITING_CONTEXT_KEY,
                contextMenuGroupId: '2_edit',
                contextMenuOrder: 2,
                run: (ed) => ed.trigger('contextMenu', 'editor.action.clipboardPasteAction', null),
            }),
        );
        this.disposers.push(
            editor.addAction({
                id: 'pagx.delete',
                label: 'Delete',
                keybindings: [m.KeyMod.CtrlCmd | m.KeyCode.Backspace],
                contextMenuGroupId: '3_delete',
                contextMenuOrder: 1,
                run: (ed) => {
                    const selections = ed.getSelections() ?? [];
                    const nonEmpty = selections.filter((selection) => !selection.isEmpty());
                    if (nonEmpty.length > 0) {
                        this.deleteRanges(nonEmpty.map((range) => ({
                            startLineNumber: range.startLineNumber,
                            startColumn: range.startColumn,
                            endLineNumber: range.endLineNumber,
                            endColumn: range.endColumn,
                        })));
                        return;
                    }
                    const position = ed.getPosition();
                    if (position !== null) {
                        this.deleteLineRange(position.lineNumber, position.lineNumber);
                    }
                },
            }),
        );
        this.disposers.push(
            editor.addAction({
                id: 'pagx.deleteEntireFile',
                label: 'Delete Entire File',
                keybindings: [m.KeyMod.CtrlCmd | m.KeyMod.Shift | m.KeyCode.Backspace],
                contextMenuGroupId: '3_delete',
                contextMenuOrder: 2,
                run: () => {
                    if (this.model !== null) {
                        this.deleteLineRange(1, this.model.getLineCount());
                    }
                },
            }),
        );
        this.disposers.push(
            editor.addAction({
                id: 'pagx.deleteTagBlock',
                label: 'Delete Tag Block',
                keybindings: [m.KeyMod.CtrlCmd | m.KeyMod.Alt | m.KeyCode.Backspace],
                contextMenuGroupId: '3_delete',
                contextMenuOrder: 3,
                run: (ed) => {
                    const position = ed.getPosition();
                    if (position !== null) {
                        const span = this.getTagBlockLineRange(position.lineNumber);
                        this.deleteLineRange(span.startLine, span.endLine);
                    }
                },
            }),
        );
    }

    /** Resolves the inclusive line span of the tag enclosing the given line via the
     *  host-registered resolver. Falls back to the line itself when no resolver is set or the
     *  host cannot identify intact boundary tags there. Malformed descendants do not invalidate
     *  an otherwise intact enclosing block. */
    private getTagBlockLineRange(line: number): LineRange {
        const model = this.model;
        const span = this.tagSpanResolver?.(line) ?? null;
        if (model === null || span === null) {
            return { startLine: line, endLine: line };
        }
        const lineCount = model.getLineCount();
        const startLine = clampLine(span.startLine, lineCount);
        const endLine = clampLine(span.endLine, lineCount);
        if (startLine > endLine) {
            return { startLine: line, endLine: line };
        }
        return { startLine, endLine };
    }

    /** Deletes the given ranges in a single undo stop. Unlike editor.executeEdits (which silently
     *  no-ops while readOnly is set), model.pushEditOperations applies at the model layer so the
     *  delete context menu actions also work in read-only browsing mode, where the change still
     *  flows into draft tracking and the undo stack like a typed edit. */
    private deleteRanges(ranges: readonly MonacoNS.IRange[]): void {
        const model = this.model;
        if (model === null || ranges.length === 0) {
            return;
        }
        model.pushEditOperations(
            null,
            ranges.map((range) => ({ range, text: '' })),
            () => null,
        );
        // pushEditOperations dispatches the content change synchronously, so the draft tracking
        // and the model text are already settled here. Read-only deletes skip the draft staging
        // and go straight through the host's Apply pipeline (validation errors surface there and
        // leave the undoable change intact); edit-mode deletes stay drafts because the user is
        // mid-editing and commits with Enter or the Apply button.
        if (!this.editingActive) {
            this.onApplyRequestCb?.();
        }
    }

    /** Deletes the whole lines [startLine, endLine] including their line breaks so the lines are
     *  removed rather than blanked. */
    private deleteLineRange(startLine: number, endLine: number): void {
        const model = this.model;
        const m = monacoInstance;
        if (model === null || m === null) {
            return;
        }
        const lineCount = model.getLineCount();
        const start = clampLine(startLine, lineCount);
        const end = clampLine(endLine, lineCount);
        let range: MonacoNS.Range;
        if (start === 1 && end === lineCount) {
            range = new m.Range(1, 1, lineCount, model.getLineMaxColumn(lineCount));
        } else if (end < lineCount) {
            range = new m.Range(start, 1, end + 1, 1);
        } else {
            range = new m.Range(
                start - 1, model.getLineMaxColumn(start - 1), end, model.getLineMaxColumn(end),
            );
        }
        this.deleteRanges([range]);
    }

    /** Line-delta bookkeeping only: every content change (typed, pasted, undone, redone — it makes
     *  no difference) is translated to inserted/removed line counts and handed to the player so it
     *  can re-project draftSourceMap. This listener never inspects what the new text says or
     *  rejects/reverts a change; whether the resulting document is well-formed is entirely the
     *  syntax validator's and Apply's job, checked independently below via
     *  scheduleSyntaxValidation. */
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
                this.onDraftLineChangesCb?.(this.getDraftLineChanges(e));
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
        const savedOffset = preserveViewState
            ? this.model.getOffsetAt(this.editor.getPosition() ?? new monacoInstance!.Position(1, 1))
            : 0;
        const savedScrollTop = preserveViewState ? this.editor.getScrollTop() : 0;
        const oldModel = this.model;
        this.model = monacoInstance!.editor.createModel(trimmed, 'xml');
        this.editor.setModel(this.model);
        oldModel.dispose();
        this.attachContentListener();
        this.attachDomListeners();
        if (preserveViewState) {
            const clampedOffset = Math.min(savedOffset, this.model.getValueLength());
            const pos = this.model.getPositionAt(clampedOffset);
            this.editor.setPosition(pos);
            this.editor.setScrollTop(savedScrollTop);
        }
        this.resetStateAfterModelSwap();
    }

    /** Returns the editor to its pristine read-only browsing state after a model swap: clears
     *  transient decorations, leaves edit mode, and notifies the host that no draft remains. */
    private resetStateAfterModelSwap(): void {
        if (this.editor === null) {
            return;
        }
        this.hoverDecoIds = [];
        this.selectDecoIds = [];
        this.editDecoIds = [];
        this.editSelectionDecoIds = [];
        this.editingActive = false;
        this.editingContextKey?.set(false);
        this.editPointerDown = null;
        this.host.classList.remove('pagx-editor-editing');
        this.host.classList.add('pagx-editor-readonly');
        this.editor.updateOptions({
            readOnly: true,
            domReadOnly: true,
        });
        this.onDraftLineChangesCb?.(null);
    }

    /** Marks the current model content as successfully applied without replacing the model. This
     *  keeps undo/redo history across Apply while adding a boundary before subsequent edits. */
    markApplied(): void {
        if (this.model === null) {
            return;
        }
        this.model.pushStackElement();
        this.leaveEditMode();
        this.onDraftLineChangesCb?.(null);
    }

    /** Restores the last applied text as one undoable edit instead of replacing the Monaco model.
     *  This keeps the complete document history; a subsequent Ctrl+Z restores the discarded draft.
     *  View state is unchanged because the existing model and editor instance remain in place. */
    discardTo(text: string): void {
        if (this.editor === null || this.model === null) {
            return;
        }
        const trimmed = text.charCodeAt(0) === 0xFEFF ? text.slice(1) : text;
        if (this.model.getValue() !== trimmed) {
            // Discard is commonly invoked after Apply has returned the editor to read-only mode.
            // editor.executeEdits() silently no-ops while readOnly is enabled; mutate the model
            // directly instead, exactly like the read-only Delete actions. pushEditOperations keeps
            // the replacement undoable, so Ctrl+Z restores the discarded draft.
            this.model.pushStackElement();
            this.model.pushEditOperations(
                null,
                [{
                    range: this.model.getFullModelRange(),
                    text: trimmed,
                    forceMoveMarkers: true,
                }],
                () => null,
            );
            this.model.pushStackElement();
        }
        this.leaveEditMode();
        this.onDraftLineChangesCb?.(null);
    }

    /** Returns the current document text. */
    getContent(): string {
        if (this.model === null) {
            return '';
        }
        return this.model.getValue();
    }

    /** Returns the current physical XML tag line, or null for text, blank and attribute-continuation
     *  lines. Used only by read-only hover preview to decide which line to highlight — this is a
     *  cosmetic concern separate from the edit-mode state machine, which never calls this. */
    getDraftTagLine(line: number): number | null {
        if (this.model === null || line <= 0 || line > this.model.getLineCount()) {
            return null;
        }
        const text = this.model.getLineContent(line);
        return /<\/?[A-Za-z_?!]/.test(text) ? line : null;
    }

    /** Mirrors Monaco's non-empty selections as inline decorations. Native selection is rendered
     *  in a separate overlay and therefore cannot override syntax-token foreground colors. */
    private refreshEditSelectionDecorations(): void {
        if (this.editor === null || !this.editingActive) {
            if (this.editor !== null) {
                this.editSelectionDecoIds = this.editor.deltaDecorations(this.editSelectionDecoIds, []);
            }
            this.host.classList.remove('pagx-editor-has-selection');
            return;
        }
        const decos: MonacoNS.editor.IModelDeltaDecoration[] = [];
        for (const selection of this.editor.getSelections() ?? []) {
            if (selection.isEmpty()) {
                continue;
            }
            decos.push({
                range: selection,
                options: { inlineClassName: EDIT_SELECTION_CLASS },
            });
        }
        this.editSelectionDecoIds = this.editor.deltaDecorations(this.editSelectionDecoIds, decos);
        this.host.classList.toggle('pagx-editor-has-selection', decos.length > 0);
    }

    /** Returns an active edit session to read-only browsing and clears its edit decorations. */
    private leaveEditMode(): void {
        if (!this.editingActive || this.editor === null) {
            return;
        }
        this.editingActive = false;
        this.editingContextKey?.set(false);
        this.editDecorationLine = -1;
        this.editPointerDown = null;
        this.editDecoIds = this.editor.deltaDecorations(this.editDecoIds, []);
        this.editSelectionDecoIds = this.editor.deltaDecorations(this.editSelectionDecoIds, []);
        this.host.classList.remove('pagx-editor-has-selection');
        this.host.classList.remove('pagx-editor-editing');
        this.host.classList.add('pagx-editor-readonly');
        this.editor.updateOptions({ readOnly: true, domReadOnly: true });
    }

    /** Handles undo and redo while the editor is read-only. Monaco disables its built-in commands
     *  in that mode, but document history must remain available after a temporary edit is re-locked. */
    handleReadOnlyUndoRedo(event: KeyboardEvent): boolean {
        if (this.editor === null || this.model === null || this.editingActive ||
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

    /** Focuses the editor and opens Monaco's find widget. Called from the panel's global Ctrl/Cmd+F
     *  handler so users get a working in-file search even when focus is on the canvas: the
     *  browser's native find would only see the currently rendered viewport because Monaco
     *  virtualises the document DOM. */
    openFind(): void {
        if (this.editor === null) {
            return;
        }
        this.editor.focus();
        this.editor.getAction('actions.find')?.run();
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

    /** Unlocks the whole document for editing, showing the amber tag decoration only on the
     *  physical line that was double-clicked. There is no per-element edit boundary: once active,
     *  every line is writable, and typing/pasting is never inspected or reverted based on its
     *  content — line-delta bookkeeping (attachContentListener) is the only thing tracking edits. */
    enterEditMode(decorationLine: number): void {
        if (this.editor === null || this.model === null || decorationLine <= 0) {
            return;
        }
        this.editingActive = true;
        this.editingContextKey?.set(true);
        this.editDecorationLine = decorationLine;
        this.host.classList.remove('pagx-editor-readonly');
        this.host.classList.add('pagx-editor-editing');
        this.editor.updateOptions({ readOnly: false, domReadOnly: false });
        const decos: MonacoNS.editor.IModelDeltaDecoration[] = [];
        buildEditDecos(decorationLine, this.model, decos);
        this.editDecoIds = this.editor.deltaDecorations(this.editDecoIds, decos);
        this.editor.focus();
        this.refreshEditSelectionDecorations();
    }

    /** Scrolls the given 1-based line into view. 'center' always recenters the line in the
     *  viewport; 'start' scrolls the minimum needed to reveal it (landing at the viewport edge);
     *  'nearest' centers only when the line is outside the viewport. */
    scrollToLine(line: number, align: 'start' | 'nearest' | 'center' = 'start'): void {
        if (this.editor === null || line <= 0) {
            return;
        }
        const targetLine = Math.min(line, this.model?.getLineCount() ?? 1);
        if (align === 'center') {
            this.editor.revealLineInCenter(targetLine, monacoInstance!.editor.ScrollType.Smooth);
        } else if (align === 'nearest') {
            this.editor.revealLineInCenterIfOutsideViewport(targetLine);
        } else {
            this.editor.revealLine(targetLine, monacoInstance!.editor.ScrollType.Smooth);
        }
    }

    /** Converts replacements to line deltas. Apply them from bottom to top because all ranges use
     *  the same pre-change model coordinates. */
    private getDraftLineChanges(e: MonacoNS.editor.IModelContentChangedEvent): DraftLineChange[] {
        return e.changes.map((change) => ({
            startLine: change.range.startLineNumber,
            endLine: change.range.endLineNumber,
            insertedLineCount: change.text.split('\n').length - 1,
        })).sort((a, b) => b.startLine - a.startLine);
    }

    /** Completes an edit-mode pointer gesture. Only an unmoved click (not a drag) landing on a
     *  physical line other than the current amber tag line exits editing; the decision is a pure
     *  line-number comparison and never looks at what that line's text is. Selections and
     *  clipboard gestures preserve Monaco focus and the writable state. */
    private handleEditPointerUp(
        position: { lineNumber: number; column: number } | null | undefined,
    ): void {
        const down = this.editPointerDown;
        this.editPointerDown = null;
        if (!this.editingActive || down === null || position === null || position === undefined ||
            down.lineNumber !== position.lineNumber || down.column !== position.column ||
            position.lineNumber === this.editDecorationLine) {
            return;
        }
        const clickedLine = position.lineNumber;
        this.leaveEditMode();
        const activeElement = document.activeElement;
        if (activeElement instanceof HTMLElement && this.editor?.getDomNode()?.contains(activeElement)) {
            activeElement.blur();
        }
        this.onHoverLineCb?.(clickedLine);
    }

    onDraftLineChanges(cb: ((changes: readonly DraftLineChange[] | null) => void) | null): void {
        this.onDraftLineChangesCb = cb;
    }

    onHoverLine(cb: ((line: number) => void) | null): void {
        this.onHoverLineCb = cb;
    }

    onDblClickLine(cb: ((line: number) => void) | null): void {
        this.onDblClickLineCb = cb;
    }

    /** Registers the callback that runs the host's Apply pipeline. Fired on Enter while editing
     *  and after a read-only delete action. Pass null to remove. */
    onApplyRequest(cb: (() => void) | null): void {
        this.onApplyRequestCb = cb;
    }

    /** Registers the resolver mapping a source line to the line span of its enclosing tag, used
     *  by the tag-block copy/delete context menu actions. The host returns null when the tag
     *  cannot be identified, in which case the actions fall back to the single line. Pass null
     *  to remove. */
    setTagSpanResolver(resolver: ((line: number) => LineRange | null) | null): void {
        this.tagSpanResolver = resolver;
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
        this.clearLoadError();
    }
}
