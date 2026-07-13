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

import { EditorView, keymap } from '@codemirror/view';
import { EditorState } from '@codemirror/state';
import { basicSetup } from 'codemirror';
import { xml } from '@codemirror/lang-xml';
import { syntaxHighlighting, HighlightStyle } from '@codemirror/language';
import { searchKeymap, highlightSelectionMatches } from '@codemirror/search';
import { tags } from '@lezer/highlight';

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
 * The editor is created lazily on first open to avoid impacting initial page load.
 */
export class SourceEditor {
    private host: HTMLElement;
    private view: EditorView | null = null;

    constructor(host: HTMLElement) {
        this.host = host;
    }

    /** Creates the CodeMirror view if it does not exist yet. */
    private ensureView(): void {
        if (this.view !== null) {
            return;
        }
        this.view = new EditorView({
            parent: this.host,
            state: EditorState.create({
                doc: '',
                extensions: [
                    basicSetup,
                    xml(),
                    syntaxHighlighting(pagxHighlightStyle),
                    highlightSelectionMatches(),
                    keymap.of([...searchKeymap]),
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
                        '.cm-gutters': {
                            backgroundColor: '#1E1E1E',
                            color: '#6E7681',
                            border: 'none',
                            borderRight: '1px solid #3C3C3C',
                        },
                        '.cm-activeLine': {
                            backgroundColor: '#2D2D2D',
                        },
                        '.cm-activeLineGutter': {
                            backgroundColor: '#2D2D2D',
                        },
                        '.cm-selectionBackground, ::selection': {
                            backgroundColor: '#264F78',
                        },
                        '&.cm-focused .cm-selectionBackground': {
                            backgroundColor: '#264F78',
                        },
                    }),
                ],
            }),
        });
    }

    /** Replaces the entire document content. */
    setContent(text: string): void {
        this.ensureView();
        if (this.view === null) {
            return;
        }
        const trimmed = text.charCodeAt(0) === 0xFEFF ? text.slice(1) : text;
        this.view.dispatch({
            changes: {
                from: 0,
                to: this.view.state.doc.length,
                insert: trimmed,
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

    /** Moves keyboard focus into the editor. */
    focus(): void {
        if (this.view !== null) {
            this.view.focus();
        }
    }

    /** Releases the CodeMirror instance and frees DOM nodes. */
    destroy(): void {
        if (this.view !== null) {
            this.view.destroy();
            this.view = null;
        }
    }
}
