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

// The pagx context-menu and editing keybindings. Kept separate from the editor construction and
// interaction wiring: this module owns only Monaco action registration, with the editor's mutable
// state and model-level edit helpers supplied through deps so no editor internals are duplicated.

import type * as MonacoNS from 'monaco-editor';
import { copyTextToClipboard, type Monaco } from './monaco-loader';

export const EDITING_CONTEXT_KEY = 'pagxSourceEditing';

/** A 1-based inclusive line range mirrored from a node's source span. Structural duplicate of the
 *  player's LineRange so this module has no dependency back on the editor that owns it. */
interface LineRange {
    startLine: number;
    endLine: number;
}

/** Everything registerContextMenuActions needs from the owning editor. Grouping these in one named
 *  object keeps the call site readable and lets the actions stay independent of SourceEditor's
 *  internal layout. */
export interface ContextMenuDeps {
    monaco: Monaco;
    editor: MonacoNS.editor.IStandaloneCodeEditor;
    getModel: () => MonacoNS.editor.ITextModel | null;
    readFullText: (model: MonacoNS.editor.ITextModel) => string;
    getTagBlockLineRange: (line: number) => LineRange;
    deleteRanges: (ranges: readonly MonacoNS.IRange[]) => void;
    deleteLineRange: (startLine: number, endLine: number) => void;
    requestApply: () => void;
    disposers: MonacoNS.IDisposable[];
}

/** Replaces Monaco's built-in context menu (cleared once in loadMonaco) with the pagx action set.
 *  Copy/Cut/Paste re-dispatch the built-in clipboard commands so their behavior matches the
 *  original menu entries exactly; the whole-file and tag-block variants are custom. Cut/Paste show
 *  only in edit mode via the pagxSourceEditing context key; the delete actions stay available while
 *  read-only, where removal is applied to the canvas immediately instead of staging a draft. */
export function registerContextMenuActions(deps: ContextMenuDeps): void {
    const { monaco: m, editor } = deps;

    // Swallow the built-in F1 shortcut so the Command Palette stays unreachable now that its menu
    // entry is pruned.
    editor.addCommand(m.KeyCode.F1, () => undefined, 'editorTextFocus');
    // While the document is unlocked, Enter commits the edits through the host's Apply pipeline
    // (same as the Apply button); Shift+Enter keeps newline insertion available. addCommand returns
    // a command id rather than a disposable; the commands are bound to this editor instance and
    // disappear with its dispose(), like the F1 swallow above.
    editor.addCommand(m.KeyCode.Enter, () => deps.requestApply(), EDITING_CONTEXT_KEY);
    editor.addCommand(m.KeyMod.Shift | m.KeyCode.Enter, () => {
        editor.trigger('keyboard', 'type', { text: '\n' });
    }, EDITING_CONTEXT_KEY);
    // Keybindings mirror the shortcut hints shown in the context menu. CtrlCmd resolves to Cmd on
    // macOS and Ctrl elsewhere, so a single binding is cross-platform. Copy/Cut/Paste reuse
    // Monaco's built-in clipboard shortcuts, which stay active because the built-in commands are
    // still registered; the keybindings here only opt the pagx actions into the menu's shortcut
    // column and never intercept native handling.
    deps.disposers.push(
        editor.addAction({
            id: 'pagx.copy',
            label: 'Copy',
            keybindings: [m.KeyMod.CtrlCmd | m.KeyCode.KeyC],
            contextMenuGroupId: '1_copy',
            contextMenuOrder: 1,
            run: (ed) => ed.trigger('contextMenu', 'editor.action.clipboardCopyAction', null),
        }),
    );
    deps.disposers.push(
        editor.addAction({
            id: 'pagx.copyEntireFile',
            label: 'Copy Entire File',
            keybindings: [m.KeyMod.CtrlCmd | m.KeyMod.Shift | m.KeyCode.KeyC],
            contextMenuGroupId: '1_copy',
            contextMenuOrder: 2,
            run: () => {
                const model = deps.getModel();
                if (model !== null) {
                    copyTextToClipboard(deps.readFullText(model));
                }
            },
        }),
    );
    deps.disposers.push(
        editor.addAction({
            id: 'pagx.copyTagBlock',
            label: 'Copy Tag Block',
            keybindings: [m.KeyMod.CtrlCmd | m.KeyMod.Alt | m.KeyCode.KeyC],
            contextMenuGroupId: '1_copy',
            contextMenuOrder: 3,
            run: (ed) => {
                const model = deps.getModel();
                const position = ed.getPosition();
                if (model === null || position === null) {
                    return;
                }
                const span = deps.getTagBlockLineRange(position.lineNumber);
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
    deps.disposers.push(
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
    deps.disposers.push(
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
    deps.disposers.push(
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
                    deps.deleteRanges(nonEmpty.map((range) => ({
                        startLineNumber: range.startLineNumber,
                        startColumn: range.startColumn,
                        endLineNumber: range.endLineNumber,
                        endColumn: range.endColumn,
                    })));
                    return;
                }
                const position = ed.getPosition();
                if (position !== null) {
                    deps.deleteLineRange(position.lineNumber, position.lineNumber);
                }
            },
        }),
    );
    deps.disposers.push(
        editor.addAction({
            id: 'pagx.deleteEntireFile',
            label: 'Delete Entire File',
            keybindings: [m.KeyMod.CtrlCmd | m.KeyMod.Shift | m.KeyCode.Backspace],
            contextMenuGroupId: '3_delete',
            contextMenuOrder: 2,
            run: () => {
                const model = deps.getModel();
                if (model !== null) {
                    deps.deleteLineRange(1, model.getLineCount());
                }
            },
        }),
    );
    deps.disposers.push(
        editor.addAction({
            id: 'pagx.deleteTagBlock',
            label: 'Delete Tag Block',
            keybindings: [m.KeyMod.CtrlCmd | m.KeyMod.Alt | m.KeyCode.Backspace],
            contextMenuGroupId: '3_delete',
            contextMenuOrder: 3,
            run: (ed) => {
                const position = ed.getPosition();
                if (position !== null) {
                    const span = deps.getTagBlockLineRange(position.lineNumber);
                    deps.deleteLineRange(span.startLine, span.endLine);
                }
            },
        }),
    );
}
