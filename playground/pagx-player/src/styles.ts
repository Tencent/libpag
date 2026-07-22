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

// Component-scoped styles injected once per document when the first PAGXPlayer mounts.
// Source of truth: pagx-playground/static/index.css (canvas / toolbar / playback bar sections).
// Anything visual in here MUST match the playground production styles byte-for-byte; the
// component's design premise is that the official site's look does not change when it adopts
// pagx-player. Ordering matches the original stylesheet so future edits can be diffed directly.

const STYLE_ELEMENT_ID = 'pagx-player-styles';

const CSS = `
/* Root wrapper the component owns inside the host container. Isolates layout state (width
   collapse when editor opens, --editor-width variable, cursor overrides while resizing) from
   the host, so hosts don't need to ship any component-specific CSS rules for those. Uses
   position: relative + 100% dimensions rather than absolute-inset so it works inside a host
   container regardless of the container's 'position' property (static, relative, absolute all
   OK); an absolute-positioned wrapper on a static host would escape upward to the nearest
   positioned ancestor. The host controls actual size by setting width/height on its container. */
.pagx-player-root {
    position: relative;
    width: 100%;
    height: 100%;
    overflow: hidden;
}

.pagx-player-root.with-editor {
    width: calc(100% - var(--editor-width, 50%));
    margin: 0;
}

.pagx-player-canvas {
    cursor: grab;
    position: absolute;
    top: 0;
    bottom: 0;
    left: 0;
    right: 0;
    z-index: 1;
    touch-action: none;
    background:
        repeating-conic-gradient(#d0d0d0 0% 25%, #fff 0% 50%) 0 0 / 32px 32px;
}

.pagx-player-canvas.hidden {
    display: none;
}

.pagx-player-canvas:active {
    cursor: grabbing;
}

.pagx-player-toolbar {
    position: absolute;
    top: 16px;
    right: 16px;
    /* Higher than the playback bar (150) so host overlays wanting to hide playback while
       keeping global controls (Samples / Leave / etc.) accessible can pick a z-index in the
       gap between them - e.g. a loading screen that covers the canvas and playback bar but
       lets the user still navigate away. Stays below the status pill (200) and editor
       panel (300) which are more prominent surfaces. */
    z-index: 175;
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 8px;
    background: rgba(0, 0, 0, 0.6);
    border-radius: 8px;
    backdrop-filter: blur(8px);
}

.pagx-player-toolbar.hidden {
    display: none;
}

.pagx-player-toolbar .toolbar-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 36px;
    height: 36px;
    padding: 6px;
    box-sizing: border-box;
    background: transparent;
    border: none;
    border-radius: 6px;
    cursor: pointer;
    text-decoration: none;
    transition: background 0.2s ease;
}

.pagx-player-toolbar .toolbar-btn svg {
    width: 24px;
    height: 24px;
    color: #ccc;
}

.pagx-player-toolbar .toolbar-btn:hover {
    background: rgba(255, 255, 255, 0.1);
}

.pagx-player-toolbar .toolbar-btn:hover svg {
    color: #fff;
}

.pagx-player-toolbar .toolbar-btn:active {
    background: rgba(255, 255, 255, 0.2);
}

.pagx-player-toolbar .toolbar-btn.hidden {
    display: none;
}

.pagx-player-toolbar .toolbar-divider {
    width: 1px;
    height: 20px;
    background: rgba(255, 255, 255, 0.2);
}

/* Playback bar - centered compact pill, mimicking the macOS player control bar */
.pagx-player-playback-bar {
    position: absolute;
    left: 50%;
    transform: translateX(-50%);
    bottom: 16px;
    z-index: 150;
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 12px 24px;
    background: #20202A;
    border-radius: 8px;
    /* Lock the pill to its natural content width so a narrow viewport clips the sides evenly
       instead of collapsing the internal controls (buttons, time display, progress bar). This
       relies on the parent container having overflow: hidden so the overflowing edges are
       clipped rather than pushing the layout wider or leaking onto the editor panel. */
    min-width: max-content;
}

.pagx-player-playback-bar.hidden {
    display: none;
}

.pagx-player-playback-bar .playback-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 0;
    background: transparent;
    border: none;
    cursor: pointer;
    opacity: 0.8;
    transition: opacity 0.15s ease;
}

.pagx-player-playback-bar .playback-btn:hover {
    opacity: 0.9;
}

.pagx-player-playback-bar .playback-btn:active {
    opacity: 1;
}

.pagx-player-playback-bar .playback-btn:disabled {
    opacity: 0.3;
    cursor: not-allowed;
}

/* Play / Pause - reuse desktop client icon (blue circle with glow) */
.pagx-player-playback-bar .playback-btn-primary {
    width: 48px;
    height: 48px;
    margin-right: 4px;
}

.pagx-player-playback-bar .playback-btn-primary img {
    display: block;
    width: 48px;
    height: 48px;
}

/* Frame-step buttons (prev/next frame) - reuse desktop client icons */
.pagx-player-playback-bar .playback-btn-step {
    width: 24px;
    height: 48px;
}

.pagx-player-playback-bar .playback-btn-step img {
    display: block;
    width: 24px;
    height: 20px;
    /* Nudge the arrow glyphs down so they sit on the progress bar center line */
    margin-top: 5px;
}

.pagx-player-playback-bar #next-frame-btn {
    margin-left: 20px;
}

.pagx-player-playback-bar .progress-wrapper {
    width: 260px;
    margin-left: 16px;
    margin-right: 8px;
}

.pagx-player-playback-bar .time-display {
    display: flex;
    align-items: center;
    /* Right-align so the frame counter stays close to the loop button; the fixed width keeps
       the bar length stable while the time/frame digit counts change, absorbing slack on the left. */
    justify-content: flex-end;
    gap: 16px;
    margin-left: 8px;
    width: 168px;
    color: #ffffff;
    font-size: 12px;
    font-variant-numeric: tabular-nums;
    white-space: nowrap;
}

.pagx-player-playback-bar .time-divider {
    width: 1px;
    height: 14px;
    background: rgba(255, 255, 255, 0.28);
}

/* Loop toggle - shows the sequence (repeat) icon when looping is off, and the single (repeat-one,
   marked with a "1") icon when looping is on. Both inline SVGs follow the button's currentColor so
   the state switch needs no swapped image assets. */
.pagx-player-playback-bar .playback-btn-loop {
    width: 24px;
    height: 48px;
    margin-left: 16px;
    color: #ffffff;
    opacity: 1;
}

.pagx-player-playback-bar .playback-btn-loop svg {
    display: block;
    width: 20px;
    height: 20px;
}

.pagx-player-playback-bar .playback-btn-loop .loop-icon-single {
    width: 24px;
    height: 18px;
}

.pagx-player-playback-bar .playback-btn-loop .loop-icon-sequence {
    display: none;
}

.pagx-player-playback-bar .playback-btn-loop.active .loop-icon-single {
    display: none;
}

.pagx-player-playback-bar .playback-btn-loop.active .loop-icon-sequence {
    display: block;
}

.pagx-player-playback-bar .progress-slider {
    -webkit-appearance: none;
    appearance: none;
    width: 100%;
    height: 4px;
    background: rgba(255, 255, 255, 0.18);
    border-radius: 2px;
    outline: none;
    cursor: pointer;
}

.pagx-player-playback-bar .progress-slider::-webkit-slider-thumb {
    -webkit-appearance: none;
    appearance: none;
    width: 12px;
    height: 12px;
    background: #fff;
    border-radius: 50%;
    cursor: pointer;
    box-shadow: 0 1px 4px rgba(0, 0, 0, 0.4);
    transition: transform 0.15s ease;
}

.pagx-player-playback-bar .progress-slider::-webkit-slider-thumb:hover {
    transform: scale(1.2);
}

.pagx-player-playback-bar .progress-slider::-moz-range-thumb {
    width: 12px;
    height: 12px;
    background: #fff;
    border: none;
    border-radius: 50%;
    cursor: pointer;
    box-shadow: 0 1px 4px rgba(0, 0, 0, 0.4);
}

/* Transient status pill anchored above the canvas area. position: absolute relative to the
   container makes it follow the same shrink-with-editor behavior as the toolbar and playback
   bar, so status text never straddles the editor panel. z-index sits below the editor (300)
   for the same reason. */
.pagx-player-status {
    position: absolute;
    left: 50%;
    top: 24px;
    transform: translateX(-50%);
    padding: 8px 16px;
    background: rgba(0, 0, 0, 0.65);
    color: #fff;
    border-radius: 999px;
    font-size: 13px;
    letter-spacing: 0.02em;
    pointer-events: none;
    z-index: 200;
    transition: opacity 0.2s ease;
    white-space: nowrap;
}

.pagx-player-status.hidden {
    opacity: 0;
}

.pagx-player-status.success {
    background: rgba(46, 125, 50, 0.9);
}

.pagx-player-status.error {
    background: rgba(220, 53, 69, 0.85);
}
`;

/** Inject the component stylesheet exactly once per document. Safe to call from every player
 *  instance's constructor. */
export function ensureStylesInjected(): void {
    if (typeof document === 'undefined') {
        return;
    }
    if (document.getElementById(STYLE_ELEMENT_ID)) {
        return;
    }
    const style = document.createElement('style');
    style.id = STYLE_ELEMENT_ID;
    style.textContent = CSS;
    document.head.appendChild(style);
}
