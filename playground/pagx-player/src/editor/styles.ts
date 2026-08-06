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

/** CSS rules for the editor panel, injected at runtime to avoid a separate rollup CSS plugin. */
export const EDITOR_STYLES = `
/* z-index sits above the player toolbar / playback bar (150) so a small viewport where those
   controls can't fully fit in the shrunken container gets them cleanly clipped by the panel
   instead of shown floating on top of the editor's own UI. The panel's opaque background acts
   as the actual visual clip. */
#editor-panel {
    position: fixed;
    top: 0;
    right: 0;
    bottom: 0;
    /* Width is driven by the --editor-width variable set while dragging the resizer; falls back
       to 50% before any drag. min-width guards the CSS-only case against an overly narrow panel. */
    width: var(--editor-width, 50%);
    min-width: 320px;
    display: none;
    flex-direction: column;
    background: #1E1E1E;
    border-left: 1px solid #3C3C3C;
    z-index: 300;
}

#editor-panel .editor-resizer {
    position: absolute;
    top: 0;
    bottom: 0;
    left: -3px;
    width: 6px;
    cursor: ew-resize;
    /* Sits above the panel body so the 6px-wide grabber stays clickable when the panel's
       raised z-index brings its own children in front of it. */
    z-index: 320;
    background: transparent;
    transition: background 0.15s ease;
}

#editor-panel .editor-resizer:hover,
#editor-panel .editor-resizer.dragging {
    background: #448EF9;
}

#editor-panel.visible {
    display: flex;
}

/* Note: the container shrinking rule (.with-editor { width: calc(...) }) is defined on the
   .pagx-player-root wrapper in pagx-player/src/styles.ts so the editor doesn't leak layout
   assumptions about the host container's class name. */

#editor-panel .editor-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 40px;
    padding: 0 12px;
    background: #252526;
    border-bottom: 1px solid #3C3C3C;
    flex-shrink: 0;
}

#editor-panel .editor-title {
    color: #CCCCCC;
    font-size: 13px;
    font-weight: 500;
    user-select: none;
    /* Push the close button to the far right while keeping the inspect button + title grouped on
       the left. Overrides justify-content: space-between on .editor-header. */
    margin-right: auto;
}

#editor-panel .editor-close-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 28px;
    height: 28px;
    border: none;
    background: transparent;
    color: #CCCCCC;
    cursor: pointer;
    border-radius: 4px;
    padding: 0;
}

#editor-panel .editor-close-btn:hover {
    background: #3C3C3C;
}

/* Inspect (selection) toggle inside the editor header. DevTools-style: active state mirrors
   selectMode so the user can see at a glance whether canvas hover drives XML highlighting. */
#editor-panel .editor-select-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 28px;
    height: 28px;
    border: none;
    background: transparent;
    color: #CCCCCC;
    cursor: pointer;
    border-radius: 4px;
    padding: 0;
    margin-right: 4px;
}

#editor-panel .editor-select-btn:hover {
    background: #3C3C3C;
}

#editor-panel .editor-select-btn.active {
    background: rgba(68, 142, 249, 0.3);
    color: #fff;
}

#editor-panel .editor-host {
    flex: 1;
    overflow: hidden;
    position: relative;
}

#editor-panel .editor-host .monaco-editor {
    height: 100%;
    background: #1E1E1E;
}

#editor-panel .editor-host .monaco-editor .margin {
    background: #1E1E1E;
    border-right: 1px solid #3C3C3C;
    color: #6E7681;
}

/* In the read-only state, hide Monaco's virtual cursor (<div class="cursor">) so a single
   click doesn't show a blinking caret the user can never type at. Monaco 0.47.0's
   cursorStyle enum doesn't include 'hidden' (added in a later version), so the toggle
   is driven by the pagx-editor-readonly class that SourceEditor adds/removes on the
   editor-host element when entering/leaving edit mode. */
#editor-panel .editor-host.pagx-editor-readonly .monaco-editor .cursor {
    display: none !important;
}

/* Node-span highlights mirrored from the canvas selection state. Monaco applies the decoration's
   className to each .view-line, so these selectors target the line within the editor. Class
   order matters: select after hover so the blue wins on overlap; edit after select. */
#editor-panel .editor-host .monaco-editor .pagx-hover-line {
    background-color: rgba(140, 140, 140, 0.28);
}

#editor-panel .editor-host .monaco-editor .pagx-select-line {
    background-color: rgba(68, 142, 249, 0.22);
    box-shadow: inset 2px 0 0 rgba(68, 142, 249, 0.9);
}

/* Active editable span (after double-click). Draws a single rectangle around the whole block:
   middle lines get only the background fill, the first line draws the top edge + left/right
   verticals, the last line draws the bottom edge + left/right verticals, and a single-line
   span (first === last) draws all four edges. This matches the original CodeMirror look —
   border rings the block, not every line. */
#editor-panel .editor-host .monaco-editor .pagx-edit-line {
    background-color: rgba(68, 142, 249, 0.3);
}

#editor-panel .editor-host .monaco-editor .pagx-edit-line.pagx-edit-line-first:not(.pagx-edit-line-last) {
    box-shadow: inset 0 1px 0 rgba(68, 142, 249, 0.95),
        inset 1px 0 0 rgba(68, 142, 249, 0.95), inset -1px 0 0 rgba(68, 142, 249, 0.95);
}

#editor-panel .editor-host .monaco-editor .pagx-edit-line.pagx-edit-line-last:not(.pagx-edit-line-first) {
    box-shadow: inset 0 -1px 0 rgba(68, 142, 249, 0.95),
        inset 1px 0 0 rgba(68, 142, 249, 0.95), inset -1px 0 0 rgba(68, 142, 249, 0.95);
}

#editor-panel .editor-host .monaco-editor .pagx-edit-line.pagx-edit-line-first.pagx-edit-line-last {
    box-shadow: inset 0 1px 0 rgba(68, 142, 249, 0.95), inset 0 -1px 0 rgba(68, 142, 249, 0.95),
        inset 1px 0 0 rgba(68, 142, 249, 0.95), inset -1px 0 0 rgba(68, 142, 249, 0.95);
}

/* Editor feedback ("Changes applied", validation errors, etc.) now flows through the player's
   unified status pill (pagx-player styles.ts .pagx-player-status), so no dedicated .editor-toast
   styles live here anymore. */

#editor-panel .editor-button-bar {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 12px;
    height: 48px;
    background: #16161D;
    border-top: 1px solid #3C3C3C;
    flex-shrink: 0;
}

#editor-panel .editor-btn {
    width: 80px;
    height: 32px;
    border: 1px solid;
    border-radius: 4px;
    color: #FFFFFF;
    font-size: 12px;
    cursor: pointer;
    transition: background 0.1s, transform 0.1s;
    padding: 0;
}

#editor-panel .editor-btn:hover {
    transform: scale(1.05);
}

#editor-panel .editor-btn:active {
    transform: scale(1.0);
}

/* While the host's onApply/onSave is in flight the buttons are disabled to prevent
   overlapping callbacks. Fades them and neutralizes hover so the pause is visible. */
#editor-panel .editor-btn:disabled {
    opacity: 0.45;
    cursor: not-allowed;
}

#editor-panel .editor-btn:disabled:hover {
    transform: none;
}

#editor-panel .editor-btn.discard {
    background: #3C3C3C;
    border-color: #4B4B5A;
}

#editor-panel .editor-btn.discard:hover {
    background: #5C5C6A;
    border-color: #8B8B9A;
}

#editor-panel .editor-btn.apply {
    background: #448EF9;
    border-color: #5BA3FF;
}

#editor-panel .editor-btn.apply:hover {
    background: #5BA3FF;
    border-color: #8BC4FF;
}

#editor-panel .editor-btn.save {
    background: #388E3C;
    border-color: #4CAF50;
}

#editor-panel .editor-btn.save:hover {
    background: #4CAF50;
    border-color: #81C784;
}

/* Monaco's scrollbar is styled via editor options (scrollbar.verticalScrollbarSize etc.),
   not CSS. No custom scrollbar rules needed here. */
`;
