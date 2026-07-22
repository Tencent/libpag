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

import { EditorView, keymap, lineNumbers, highlightSpecialChars, drawSelection } from '@codemirror/view';
import { EditorState } from '@codemirror/state';
import { defaultKeymap, historyKeymap, history } from '@codemirror/commands';
import { xml } from '@codemirror/lang-xml';
import { syntaxHighlighting, HighlightStyle, defaultHighlightStyle } from '@codemirror/language';
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

    constructor(host: HTMLElement) {
        this.host = host;
        // Defer EditorView creation to the first setContent call so that the initial
        // document is the loaded XML content (not an empty doc). This prevents
        // Ctrl+Z from unwinding past the load point into an empty editor.
    }

    private createState(initialContent: string): EditorState {
        return EditorState.create({
            doc: initialContent,
            extensions: [
                minimalSetup,
                lineNumbers(),
                xml(),
                syntaxHighlighting(pagxHighlightStyle),
                highlightSelectionMatches(),
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
    }

    /** Replaces the entire document content, resetting the undo history to this content. */
    setContent(text: string): void {
        const trimmed = text.charCodeAt(0) === 0xFEFF ? text.slice(1) : text;
        if (this.view === null) {
            this.createView(trimmed);
            return;
        }
        // Rebuild the state instead of dispatching a change so the undo history is rooted at this
        // content. A plain change dispatch would be recorded as an undoable edit, letting Ctrl+Z
        // revert to a previously loaded file's content.
        this.view.setState(this.createState(trimmed));
    }

    /** Returns the current document text. */
    getContent(): string {
        if (this.view === null) {
            return '';
        }
        return this.view.state.doc.toString();
    }

    /** Releases the CodeMirror instance and frees DOM nodes. */
    destroy(): void {
        if (this.view !== null) {
            this.view.destroy();
            this.view = null;
        }
    }
}
