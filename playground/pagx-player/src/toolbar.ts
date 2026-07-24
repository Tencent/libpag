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

// Top-right toolbar rendering: the built-in Reset View + Source Editor buttons, plus slots for
// host-provided items (Samples / Open / Leave on the official site). Buttons and dividers use
// the same class names / DOM shape as pagx-playground/static/index.html so the injected CSS
// (see styles.ts) reaches every element without special-casing.

import type { ToolbarSlot } from './types';
import { RESET_VIEW_ICON_SVG, SOURCE_EDITOR_ICON_SVG } from './icons';

export interface ToolbarCallbacks {
    onResetView: () => void;
    /** Called when the Source Editor button is clicked. When absent, the button is omitted. */
    onToggleEditor?: () => void;
}

export function buildToolbar(
    callbacks: ToolbarCallbacks,
    slots: { before?: ToolbarSlot[]; after?: ToolbarSlot[] } = {},
): HTMLDivElement {
    const root = document.createElement('div');
    root.className = 'pagx-player-toolbar hidden';

    for (const slot of slots.before ?? []) {
        appendSlot(root, slot);
    }

    const resetBtn = document.createElement('button');
    resetBtn.id = 'reset-btn';
    resetBtn.className = 'toolbar-btn';
    resetBtn.title = 'Reset View';
    resetBtn.innerHTML = RESET_VIEW_ICON_SVG;
    resetBtn.addEventListener('click', () => {
        callbacks.onResetView();
        // Release focus so keyboard shortcuts (Space to toggle playback) keep firing.
        resetBtn.blur();
    });
    root.appendChild(resetBtn);

    if (callbacks.onToggleEditor) {
        const sourceEditorBtn = document.createElement('button');
        sourceEditorBtn.id = 'source-editor-btn';
        sourceEditorBtn.className = 'toolbar-btn';
        sourceEditorBtn.title = 'Source Editor (L)';
        sourceEditorBtn.innerHTML = SOURCE_EDITOR_ICON_SVG;
        const toggle = callbacks.onToggleEditor;
        sourceEditorBtn.addEventListener('click', () => {
            toggle();
            sourceEditorBtn.blur();
        });
        root.appendChild(sourceEditorBtn);
    }

    for (const slot of slots.after ?? []) {
        appendSlot(root, slot);
    }

    return root;
}

function appendSlot(root: HTMLDivElement, slot: ToolbarSlot): void {
    if (slot.divider) {
        const div = document.createElement('span');
        div.className = 'toolbar-divider';
        if (slot.id) {
            div.id = slot.id;
        }
        root.appendChild(div);
        return;
    }
    const btn = document.createElement('button');
    btn.id = slot.id;
    btn.className = 'toolbar-btn' + (slot.extraClass ? ' ' + slot.extraClass : '');
    if (slot.title) {
        btn.title = slot.title;
    }
    if (slot.html) {
        btn.innerHTML = slot.html;
    }
    if (slot.onClick) {
        const onClick = slot.onClick;
        btn.addEventListener('click', (event) => {
            onClick(event);
            btn.blur();
        });
    }
    root.appendChild(btn);
}

export function setToolbarVisible(root: HTMLDivElement, visible: boolean): void {
    root.classList.toggle('hidden', !visible);
}
