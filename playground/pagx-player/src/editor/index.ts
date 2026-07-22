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

// Source editor panel controller. Instantiated by PAGXPlayer when `enableEditor` is set.
// Renders the panel DOM (#editor-panel and children) into the player container instead of
// depending on host-provided HTML the way the original playground module did; the visual
// output matches pagx-playground/static/index.html byte-for-byte (same ids, classes, and
// nesting) so the injected editor styles keep working unmodified.
//
// Ported from pagx-playground/src/editor/index.ts. The main API shape change is that the
// module-level singleton (globals + `init()` factory function) is now a class instantiated
// per player, and the `pagx:loaded` global event has been replaced with explicit
// `setDocumentXml()` calls driven by the player.

import type { EditorCallbacks } from '../types';
import { SourceEditor } from './SourceEditor';
import { EDITOR_STYLES } from './styles';

const MOBILE_BREAKPOINT = 768;
/** Auto-hide window (ms) used by the player when it forwards editor notifications to the
 *  status pill. Matches the previous inline toast duration so users get the same reading
 *  window. Exported so PAGXPlayer wires the same value without duplicating the literal. */
export const EDITOR_STATUS_DURATION_MS = 2000;
// Resizing bounds: the panel never shrinks below MIN_PANEL_WIDTH, and always leaves at least
// MIN_CANVAS_WIDTH for the preview area on the left.
const MIN_PANEL_WIDTH = 320;
const MIN_CANVAS_WIDTH = 360;

const STYLE_ELEMENT_ID = 'pagx-player-editor-styles';

export interface EditorPanelOptions {
    /** Element the editor panel is appended to (fixed-positioned overlay; the panel places
     *  itself against the right edge of the viewport regardless of this element). */
    parent: HTMLElement;
    /** Container element that hosts the canvas: gains .with-editor class while the panel is open,
     *  causing its width to shrink so the canvas doesn't render underneath the panel. */
    canvasContainer: HTMLElement;
    callbacks: EditorCallbacks;
    /** Bridge for user-facing feedback ("Changes applied", validation errors, etc.). PAGXPlayer
     *  wires this to its unified status pill so editor feedback lives in the same visual slot
     *  as load / reload status - keeping messages horizontally aligned, avoiding overlap, and
     *  automatically respecting the canvas-area layout when the panel is open. `info` is used
     *  for sticky progress messages ("Applying...", "Saving..."); `success` for confirmations
     *  of explicit user actions (apply/discard/save); `error` for validation and host callback
     *  failures.
     *
     *  Returns an opaque token identifying the just-shown message. Callers can pass it to
     *  {@link dismiss} for scoped cleanup - only clearing the pill when the token still
     *  identifies the currently displayed message. */
    notify: (
        message: string,
        kind: 'info' | 'success' | 'error',
        options?: { sticky?: boolean },
    ) => number;

    /** Clear a previously-shown message identified by its notify() token. No-ops when a
     *  newer notify() has already replaced the message on screen. Used by the editor to
     *  drop its own sticky "Applying..." pill when a mid-flight document swap makes the
     *  Apply's own result no longer relevant. */
    dismiss: (token: number) => void;
}

/** Injects the editor stylesheet exactly once per document. Idempotent. */
function ensureEditorStylesInjected(): void {
    if (typeof document === 'undefined') return;
    if (document.getElementById(STYLE_ELEMENT_ID)) return;
    const style = document.createElement('style');
    style.id = STYLE_ELEMENT_ID;
    style.textContent = EDITOR_STYLES;
    document.head.appendChild(style);
}

/** True when the keyboard event originates from an editable element. */
function isEditableTarget(target: EventTarget | null): boolean {
    if (!(target instanceof HTMLElement)) {
        return false;
    }
    const tag = target.tagName;
    return tag === 'INPUT' || tag === 'TEXTAREA' || target.isContentEditable;
}

/** Validates XML well-formedness using DOMParser. Returns empty string on success, error
 *  message on failure. */
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

/** Yield the main thread for one frame + one macrotask so the browser can paint any DOM
 *  changes made just before the caller `await`s a heavy synchronous operation. Apply/Save
 *  callbacks typically enter player.load() which runs long synchronous wasm calls
 *  (parsePAGX, buildLayers, ...) that block the paint queue - without this yield the
 *  Applying/Saving sticky pill and the disabled button state would only reach the screen
 *  after the whole load resolves, effectively invisible to the user. */
function yieldToBrowser(): Promise<void> {
    return new Promise((resolve) => {
        requestAnimationFrame(() => {
            setTimeout(resolve, 0);
        });
    });
}

export class EditorPanel {
    private readonly parent: HTMLElement;
    private readonly canvasContainer: HTMLElement;
    private readonly callbacks: EditorCallbacks;
    private readonly notify: (
        message: string,
        kind: 'info' | 'success' | 'error',
        options?: { sticky?: boolean },
    ) => number;
    private readonly dismiss: (token: number) => void;

    private panel!: HTMLElement;
    private resizer!: HTMLElement;
    private discardBtn!: HTMLButtonElement;
    private applyBtn!: HTMLButtonElement;
    private saveBtn!: HTMLButtonElement;
    private editor: SourceEditor | null = null;
    private currentXmlText: string | null = null;
    // Bumped every time setDocumentXml() switches the accepted baseline. A pending Apply
    // captures the value on entry and drops its own result (baseline write + success toast)
    // if the counter changed while the host's callback was awaited - which means a new
    // document arrived mid-flight (e.g. SSE reload during a slow Apply) and the pre-Apply
    // XML shouldn't win over the fresh document.
    private documentGeneration = 0;
    // XML the current in-flight Apply is pushing to the host. Hosts typically re-enter the
    // player via player.load() inside onApply, which loops the applied XML back through
    // setDocumentXml on its way out. That loopback carries the Apply's own content and MUST
    // NOT bump documentGeneration, otherwise the Apply would falsely detect a document swap
    // and skip its own success report (leaving "Applying..." pinned to the status pill).
    private pendingApplyXml: string | null = null;
    private panelWidthPx: number | null = null;
    private resizing = false;
    // True while an onApply/onSave promise from the host is still pending. Prevents overlapping
    // callbacks (double-click on Apply firing two loadPAGX pipelines against the same view)
    // and keeps the buttons visibly disabled so users get feedback that work is in flight.
    private busy = false;
    private readonly boundKeydown: (event: KeyboardEvent) => void;
    private readonly boundResize: () => void;

    constructor(opts: EditorPanelOptions) {
        this.parent = opts.parent;
        this.canvasContainer = opts.canvasContainer;
        this.callbacks = opts.callbacks;
        this.notify = opts.notify;
        this.dismiss = opts.dismiss;
        ensureEditorStylesInjected();
        this.buildDom();
        this.boundKeydown = this.handleKeydown.bind(this);
        this.boundResize = this.onWindowResize.bind(this);
        document.addEventListener('keydown', this.boundKeydown);
        window.addEventListener('resize', this.boundResize);
    }

    /** Push the XML source of the currently loaded document. Called by the player after every
     *  successful load. Passing null clears the panel and destroys the CodeMirror instance so
     *  the next open starts with fresh undo history rooted at the new content. A call that
     *  actually switches the baseline bumps documentGeneration so any Apply awaiting a slow
     *  host callback can detect that a fresher document supersedes its result and drop its
     *  baseline write. Calls that push the in-flight Apply's own XML back in - the host's
     *  player.load() at the tail of onApply loops the applied XML through setDocumentXml on
     *  its way back - are recognized via pendingApplyXml and treated as no-op baselines so
     *  the swap-detection guard doesn't trip on the Apply's own result. */
    public setDocumentXml(xmlText: string | null): void {
        const isApplyLoopback =
            xmlText !== null && this.pendingApplyXml !== null && xmlText === this.pendingApplyXml;
        this.currentXmlText = xmlText;
        if (!isApplyLoopback) {
            this.documentGeneration++;
        }
        if (xmlText === null) {
            if (this.isOpen()) {
                this.close();
            }
            if (this.editor !== null) {
                this.editor.destroy();
                this.editor = null;
            }
            return;
        }
        if (this.isOpen() && this.editor !== null) {
            this.editor.setContent(xmlText);
        }
    }

    /** True when the editor panel is currently shown. */
    public isOpen(): boolean {
        return this.panel.classList.contains('visible');
    }

    public open(): void {
        if (this.editor === null) {
            const host = this.panel.querySelector('.editor-host');
            if (host instanceof HTMLElement) {
                this.editor = new SourceEditor(host);
            }
        }
        if (this.editor !== null && this.currentXmlText !== null) {
            this.editor.setContent(this.currentXmlText);
        }
        this.panel.classList.add('visible');
        // Toggles the `with-editor` class on the container. Hosts are responsible for providing
        // the corresponding CSS rule that shrinks the container (playground does this on its
        // .container.with-editor selector; preview mirrors the same shape via #container.with-editor).
        // Component-level fallback CSS was tried but couldn't beat host `#container { width: 100% }`
        // rules on specificity without resorting to !important.
        this.canvasContainer.classList.add('with-editor');
    }

    public close(): void {
        this.panel.classList.remove('visible');
        this.canvasContainer.classList.remove('with-editor');
    }

    public toggle(): void {
        if (this.isOpen()) {
            this.close();
        } else {
            this.open();
        }
    }

    /** Detach global listeners and remove the panel from the DOM. Any layout side-effects the
     *  editor may have left on the host DOM (with-editor class on the canvas container,
     *  --editor-width property, body cursor/user-select set during a drag) are cleared here
     *  so tearing the player down doesn't outlive the component with orphaned styling. */
    public destroy(): void {
        document.removeEventListener('keydown', this.boundKeydown);
        window.removeEventListener('resize', this.boundResize);
        if (this.editor !== null) {
            this.editor.destroy();
            this.editor = null;
        }
        this.canvasContainer.classList.remove('with-editor');
        this.canvasContainer.style.removeProperty('--editor-width');
        if (this.resizing) {
            document.body.style.userSelect = '';
            document.body.style.cursor = '';
            this.resizing = false;
        }
        this.panel.remove();
    }

    // --- private ---

    private buildDom(): void {
        // Full DOM subtree matches pagx-playground/static/index.html so the injected editor
        // stylesheet (styles.ts) targets these elements without any changes.
        this.panel = document.createElement('div');
        this.panel.id = 'editor-panel';
        this.panel.innerHTML = `
            <div class="editor-resizer"></div>
            <div class="editor-header">
                <span class="editor-title">Source Editor</span>
                <button class="editor-close-btn" title="Close" aria-label="Close">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                        <line x1="18" y1="6" x2="6" y2="18"></line>
                        <line x1="6" y1="6" x2="18" y2="18"></line>
                    </svg>
                </button>
            </div>
            <div class="editor-host"></div>
            <div class="editor-button-bar">
                <button class="editor-btn discard">Discard</button>
                <button class="editor-btn apply">Apply</button>
                <button class="editor-btn save">Save</button>
            </div>
        `;
        this.parent.appendChild(this.panel);

        this.resizer = this.panel.querySelector('.editor-resizer') as HTMLElement;
        this.resizer.addEventListener('pointerdown', (e) => this.onResizeStart(e));
        this.resizer.addEventListener('pointermove', (e) => this.onResizeMove(e));
        this.resizer.addEventListener('pointerup', (e) => this.onResizeEnd(e));
        this.resizer.addEventListener('pointercancel', (e) => this.onResizeEnd(e));

        const closeBtn = this.panel.querySelector('.editor-close-btn');
        closeBtn?.addEventListener('click', () => this.close());

        this.discardBtn = this.panel.querySelector('.editor-btn.discard') as HTMLButtonElement;
        this.discardBtn.addEventListener('click', () => this.handleDiscard());
        this.applyBtn = this.panel.querySelector('.editor-btn.apply') as HTMLButtonElement;
        this.applyBtn.addEventListener('click', () => {
            void this.handleApply();
        });
        this.saveBtn = this.panel.querySelector('.editor-btn.save') as HTMLButtonElement;
        this.saveBtn.addEventListener('click', () => {
            void this.handleSave();
        });
    }

    private handleKeydown(event: KeyboardEvent): void {
        if (event.key !== 'l' && event.key !== 'L') {
            return;
        }
        if (isEditableTarget(event.target)) {
            return;
        }
        // Ignore modifier combos (Ctrl+L, Shift+L, etc.); only bare L triggers.
        if (event.ctrlKey || event.metaKey || event.altKey) {
            return;
        }
        // No document loaded yet: mirror the desktop client's "enabled: hasPAGFile" gate.
        if (this.currentXmlText === null) {
            return;
        }
        if (window.innerWidth < MOBILE_BREAKPOINT) {
            this.report('Please use desktop to edit source', 'error');
            return;
        }
        event.preventDefault();
        this.toggle();
    }

    /** Applies a panel width in pixels, clamped to the resize bounds. Sets --editor-width on
     *  the player's canvas container (used by the .with-editor rule) and on the panel itself
     *  (used by its own width property). Two writes rather than one because the panel is
     *  position:fixed on <body> and doesn't inherit CSS variables from the wrapper. Scoping
     *  the variable to these two elements - instead of document.documentElement - avoids
     *  leaking layout state onto sibling players and destroy() only needs to clean those two. */
    private applyPanelWidth(px: number): void {
        const max = Math.max(MIN_PANEL_WIDTH, window.innerWidth - MIN_CANVAS_WIDTH);
        const clamped = Math.max(MIN_PANEL_WIDTH, Math.min(px, max));
        this.panelWidthPx = clamped;
        this.canvasContainer.style.setProperty('--editor-width', `${clamped}px`);
        this.panel.style.setProperty('--editor-width', `${clamped}px`);
    }

    private onResizeStart(event: PointerEvent): void {
        event.preventDefault();
        this.resizing = true;
        document.body.style.userSelect = 'none';
        document.body.style.cursor = 'ew-resize';
        this.resizer.classList.add('dragging');
        // Pointer capture routes subsequent pointermove/pointerup to the resizer even when the
        // cursor leaves the browser window, avoiding a stuck resizing state on mouseup-loss.
        this.resizer.setPointerCapture(event.pointerId);
    }

    private onResizeMove(event: PointerEvent): void {
        if (!this.resizing) {
            return;
        }
        this.applyPanelWidth(window.innerWidth - event.clientX);
    }

    private onResizeEnd(event: PointerEvent): void {
        if (!this.resizing) {
            return;
        }
        this.resizing = false;
        document.body.style.userSelect = '';
        document.body.style.cursor = '';
        this.resizer.classList.remove('dragging');
        if (this.resizer.hasPointerCapture(event.pointerId)) {
            this.resizer.releasePointerCapture(event.pointerId);
        }
    }

    private onWindowResize(): void {
        if (this.panelWidthPx !== null) {
            this.applyPanelWidth(this.panelWidthPx);
        }
    }

    /** Route a user-visible message through the host-provided notify callback. Consolidated
     *  helper so the call sites don't repeat the (kind, duration) shape and to leave a single
     *  place to attach future behavior (e.g. suppress duplicates within a short window).
     *  Returns the notify token so callers who own a sticky message can later dismiss() it
     *  precisely without stealing another producer's pill. */
    private report(
        message: string,
        kind: 'info' | 'success' | 'error',
        options?: { sticky?: boolean },
    ): number {
        return this.notify(message, kind, options);
    }

    /** Reflect an in-flight host callback in the DOM: disable all three action buttons so
     *  double-clicks / accidental double-fire can't race the pipeline, and grey them out via
     *  the CSS :disabled selector so users see the paused state. Called with `false` when the
     *  callback settles (success or failure) to re-enable the row. */
    private setBusy(busy: boolean): void {
        this.busy = busy;
        this.discardBtn.disabled = busy;
        this.applyBtn.disabled = busy;
        this.saveBtn.disabled = busy;
    }

    private handleDiscard(): void {
        if (this.editor === null || this.busy) {
            return;
        }
        if (this.currentXmlText !== null) {
            this.editor.setContent(this.currentXmlText);
        }
        this.report('Changes discarded', 'success');
    }

    private async handleApply(): Promise<void> {
        if (this.editor === null || this.busy) {
            return;
        }
        const xmlText = this.editor.getContent();
        const validationError = validateXml(xmlText);
        if (validationError !== '') {
            this.report(validationError, 'error');
            return;
        }
        // Sticky "Applying..." keeps showing until the host promise settles - a long parse +
        // build on a big pagx (seconds) would otherwise leave the pill blank. The button row
        // is disabled for the same window so users can't fire a second apply against the
        // same in-flight pipeline. The returned token lets us dismiss *this* Apply's own
        // sticky pill if a mid-flight document swap makes its result irrelevant, without
        // disturbing whatever newer message the swapping producer put up in the same slot.
        this.setBusy(true);
        const applyingToken = this.report('Applying...', 'info', { sticky: true });
        // Snapshot the document version at Apply time so a setDocumentXml() call arriving
        // while the host's onApply is still awaited (e.g. an SSE reload racing a slow Apply)
        // can be detected on resume - we drop this Apply's baseline write and success toast
        // in that case because the pre-Apply XML is no longer the truth we should promote.
        const applyDocGen = this.documentGeneration;
        // Advertise the XML the host is about to receive so setDocumentXml can recognize its
        // loopback (host.load -> player.load -> editor.setDocumentXml with this same content)
        // and skip the generation bump. See pendingApplyXml field comment for the full story.
        this.pendingApplyXml = xmlText;
        // Yield to the browser before invoking the host's onApply callback. onApply typically
        // enters player.load() which runs synchronous wasm work (parsePAGX / buildLayers)
        // long enough to block the paint queue; without this yield the sticky "Applying..."
        // pill and the disabled apply/discard/save buttons would only reach the screen once
        // the load resolves, i.e. they would never be visible to the user during a fast load
        // and only flash briefly during a slow one.
        await yieldToBrowser();
        try {
            const error = await this.callbacks.onApply(xmlText);
            const documentSwapped = this.documentGeneration !== applyDocGen;
            if (documentSwapped) {
                // A fresher document already took over: leave currentXmlText / status alone
                // so the newer content stays authoritative. The sticky "Applying..." pill is
                // dismissed unconditionally in finally, so we don't need to touch it here.
                return;
            }
            // Promote the applied XML to the new baseline immediately on success: a
            // subsequent Discard, panel close/reopen, or setDocumentXml no-op on the same
            // content would otherwise revert the editor to the pre-Apply text even though
            // the canvas already reflects the new one. Failure keeps the old baseline so
            // Discard still restores a known-good state.
            if (error === '') {
                this.currentXmlText = xmlText;
                this.report('Changes applied', 'success');
            } else {
                this.report(error, 'error');
            }
        } catch (err) {
            if (this.documentGeneration === applyDocGen) {
                this.report(err instanceof Error ? err.message : String(err), 'error');
            }
            // A reject during a document swap used to leak the sticky "Applying..." pill
            // (no report ran here, and load doesn't touch the status slot itself), so it
            // stayed pinned until the next showStatus call. The unconditional dismiss in
            // finally covers this case; it's a safe no-op on the non-swap paths because
            // report() above already replaced the pill via a fresh token, which makes
            // dismiss(applyingToken) mismatch and skip.
        } finally {
            this.pendingApplyXml = null;
            this.setBusy(false);
            this.dismiss(applyingToken);
        }
    }

    private async handleSave(): Promise<void> {
        if (this.editor === null || this.busy) {
            return;
        }
        const xmlText = this.editor.getContent();
        const validationError = validateXml(xmlText);
        if (validationError !== '') {
            this.report(validationError, 'error');
            return;
        }
        this.setBusy(true);
        const savingToken = this.report('Saving...', 'info', { sticky: true });
        // Same rationale as handleApply's yield: paint the sticky "Saving..." pill and the
        // disabled button state before the host callback (which may run heavy synchronous
        // work) enters the microtask queue.
        await yieldToBrowser();
        try {
            const error = await this.callbacks.onSave(xmlText);
            if (error === '') {
                this.report('File saved', 'success');
            } else {
                this.report(error, 'error');
            }
        } catch (err) {
            this.report(err instanceof Error ? err.message : String(err), 'error');
        } finally {
            this.setBusy(false);
            // Safety-net dismiss: successful and error paths already replaced the sticky pill
            // via report() (whose fresh token makes this dismiss a no-op), but a synchronous
            // throw before any report() would otherwise leak the "Saving..." pill.
            this.dismiss(savingToken);
        }
    }
}
