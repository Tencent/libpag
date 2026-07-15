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

import { SourceEditor } from './SourceEditor';
import { EDITOR_STYLES } from './styles';

const MOBILE_BREAKPOINT = 768;
const TOAST_DURATION_MS = 2000;

let panel: HTMLElement | null = null;
let editor: SourceEditor | null = null;
let currentXmlText: string | null = null;
let toastTimer: number | undefined;
let toastEl: HTMLElement | null = null;

/** Callbacks provided by the host application to apply/save XML changes. */
export interface EditorCallbacks {
    /** Applies XML text to the PAGX view. Returns empty string on success, error message on failure. */
    onApply: (xmlText: string) => string;
    /** Applies and saves XML text to a file. Returns empty string on success, error message on failure. */
    onSave: (xmlText: string) => string;
}

let callbacks: EditorCallbacks | null = null;

/** Dispatched by index.ts after a PAGX file is loaded or cleared. */
interface PagxLoadedDetail {
    xmlText: string | null;
}

declare global {
    interface WindowEventMap {
        'pagx:loaded': CustomEvent<PagxLoadedDetail>;
    }
}

/** True when the keyboard event originates from an editable element. */
function isEditableTarget(target: EventTarget | null): boolean {
    if (!(target instanceof HTMLElement)) {
        return false;
    }
    const tag = target.tagName;
    return tag === 'INPUT' || tag === 'TEXTAREA' || target.isContentEditable;
}

/** True when the editor panel is currently shown. */
function isPanelOpen(): boolean {
    return panel !== null && panel.classList.contains('visible');
}

function showToast(message: string, success: boolean): void {
    if (toastEl === null) {
        return;
    }
    toastEl.textContent = message;
    toastEl.className = `editor-toast visible ${success ? 'success' : 'error'}`;
    window.clearTimeout(toastTimer);
    toastTimer = window.setTimeout(() => {
        if (toastEl !== null) {
            toastEl.className = 'editor-toast';
        }
    }, TOAST_DURATION_MS);
}

function openPanel(): void {
    if (panel === null) {
        return;
    }
    if (editor === null) {
        const host = panel.querySelector('.editor-host');
        if (host instanceof HTMLElement) {
            editor = new SourceEditor(host);
        }
    }
    if (editor !== null && currentXmlText !== null) {
        editor.setContent(currentXmlText);
    }
    panel.classList.add('visible');
    const container = document.getElementById('container');
    if (container !== null) {
        container.classList.add('with-editor');
    }
}

function closePanel(): void {
    if (panel === null) {
        return;
    }
    panel.classList.remove('visible');
    const container = document.getElementById('container');
    if (container !== null) {
        container.classList.remove('with-editor');
    }
}

export function togglePanel(): void {
    if (isPanelOpen()) {
        closePanel();
    } else {
        openPanel();
    }
}

function handleKeydown(event: KeyboardEvent): void {
    if (event.key !== 'l' && event.key !== 'L') {
        return;
    }
    // Ignore when the user is typing inside an input/textarea/contenteditable.
    if (isEditableTarget(event.target)) {
        return;
    }
    // Ignore modifier combos (Ctrl+L, Shift+L, etc.) - only bare L triggers.
    if (event.ctrlKey || event.metaKey || event.altKey) {
        return;
    }
    // No file loaded yet - silently ignore, matching desktop "enabled: hasPAGFile".
    if (currentXmlText === null) {
        return;
    }
    // Mobile screens are too narrow for side-by-side layout.
    if (window.innerWidth < MOBILE_BREAKPOINT) {
        showToast('Please use desktop to edit source', false);
        return;
    }
    event.preventDefault();
    togglePanel();
}

function handlePagxLoaded(event: Event): void {
    const detail = (event as CustomEvent<PagxLoadedDetail>).detail;
    currentXmlText = detail ? detail.xmlText : null;
    if (currentXmlText === null) {
        // File cleared (goHome) - close panel and destroy the editor instance so that
        // the next file load creates a fresh view with clean undo history rooted at the
        // new content (instead of an empty doc reachable via Ctrl+Z).
        if (isPanelOpen()) {
            closePanel();
        }
        if (editor !== null) {
            editor.destroy();
            editor = null;
        }
    } else if (isPanelOpen() && editor !== null) {
        // Panel already open and a new file loaded - refresh content.
        editor.setContent(currentXmlText);
    }
}

/** Validates XML well-formedness using DOMParser. Returns empty string on success, error message on failure. */
function validateXml(xmlText: string): string {
    const parser = new DOMParser();
    const doc = parser.parseFromString(xmlText, 'application/xml');
    const parseError = doc.querySelector('parsererror');
    if (parseError) {
        const errorText = parseError.textContent || 'Invalid XML';
        const firstLine = errorText.trim().split('\n')[0].trim();
        return firstLine || 'Invalid XML format';
    }
    return '';
}

function handleDiscard(): void {
    if (editor === null) {
        return;
    }
    if (currentXmlText !== null) {
        editor.setContent(currentXmlText);
    }
    showToast('Changes discarded', true);
}

function handleApply(): void {
    if (editor === null || callbacks === null) {
        return;
    }
    const xmlText = editor.getContent();
    const validationError = validateXml(xmlText);
    if (validationError !== '') {
        showToast(validationError, false);
        return;
    }
    const error = callbacks.onApply(xmlText);
    showToast(error === '' ? 'Changes applied' : error, error === '');
}

function handleSave(): void {
    if (editor === null || callbacks === null) {
        return;
    }
    const xmlText = editor.getContent();
    const validationError = validateXml(xmlText);
    if (validationError !== '') {
        showToast(validationError, false);
        return;
    }
    const error = callbacks.onSave(xmlText);
    showToast(error === '' ? 'File saved' : error, error === '');
}

/**
 * Initializes the Source Editor module. Wires up DOM elements, keyboard
 * shortcut (L to toggle) and the pagx:loaded event. Safe to call once
 * after DOM is ready.
 */
export function init(cb: EditorCallbacks): void {
    callbacks = cb;
    // Inject editor styles once. CSS lives in styles.ts to keep the module self-contained
    // without requiring a rollup CSS plugin.
    const styleEl = document.createElement('style');
    styleEl.textContent = EDITOR_STYLES;
    document.head.appendChild(styleEl);

    panel = document.getElementById('editor-panel');
    if (panel === null) {
        console.warn('SourceEditor: #editor-panel element not found, skipping init.');
        return;
    }
    const closeBtn = panel.querySelector('.editor-close-btn');
    if (closeBtn !== null) {
        closeBtn.addEventListener('click', closePanel);
    }
    const discardBtn = panel.querySelector('.editor-btn.discard');
    if (discardBtn !== null) {
        discardBtn.addEventListener('click', handleDiscard);
    }
    const applyBtn = panel.querySelector('.editor-btn.apply');
    if (applyBtn !== null) {
        applyBtn.addEventListener('click', handleApply);
    }
    const saveBtn = panel.querySelector('.editor-btn.save');
    if (saveBtn !== null) {
        saveBtn.addEventListener('click', handleSave);
    }
    toastEl = document.querySelector('.editor-toast');
    if (toastEl === null) {
        toastEl = document.createElement('div');
        toastEl.className = 'editor-toast';
        document.body.appendChild(toastEl);
    }
    document.addEventListener('keydown', handleKeydown);
    window.addEventListener('pagx:loaded', handlePagxLoaded);
}
