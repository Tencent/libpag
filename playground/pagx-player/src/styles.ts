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

.pagx-player-toolbar .toolbar-btn.active {
    background: rgba(255, 255, 255, 0.18);
}

.pagx-player-toolbar .toolbar-btn.active svg {
    color: #fff;
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

/* Fallback mode (a default timeline without a queryable duration, e.g. a state machine):
   the slider and loop toggle are disabled and dimmed, while play/pause and frame stepping
   stay fully styled and usable. */
.pagx-player-playback-bar.is-untimed .progress-slider {
    opacity: 0.35;
    cursor: default;
}

.pagx-player-playback-bar.is-untimed .time-display {
    color: rgba(255, 255, 255, 0.35);
}

.pagx-player-playback-bar.is-untimed .progress-slider::-webkit-slider-thumb {
    cursor: default;
}

/* Dimmed overlay: the SM panel's play button "parked" a preview, so the bar stays visible but
   is fully greyed out and non-interactive. The dim reads as "this progress belongs to a
   paused preview; click a chip / double-click a state to bring it back". */
.pagx-player-playback-bar.is-dimmed {
    pointer-events: none;
}

.pagx-player-playback-bar.is-dimmed > * {
    opacity: 0.35;
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

/* Selection overlay: an axis-aligned rect drawn over the canvas to mirror the hovered/selected
   layer. pointer-events:none (also set inline) so it never steals hover/click from the canvas.
   z-index sits above the canvas (1) but below the toolbar (175) and status pill (200). */
.pagx-select-overlay {
    position: absolute;
    z-index: 50;
    display: none;
    pointer-events: none;
    box-sizing: border-box;
}

/* Colors follow Chrome DevTools' inspect overlay: the fill is its content-box blue; the sticky
   selection adds a solid accent border to distinguish it from the transient hover. */
.pagx-select-overlay.is-hover {
    background: rgba(111, 168, 220, 0.66);
}

.pagx-select-overlay.is-selected {
    background: rgba(111, 168, 220, 0.66);
    border: 1px solid rgba(26, 115, 232, 1);
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

/* --- State-machine blueprint (default SM timeline) --- */

/* Content-driven panel: width and height follow the graph inside, so a two-state SM shows a
   compact card and a big SM grows into a large canvas (Rive / Stately Inspector convention).
   min-width keeps the header readable; max-width/max-height cap the growth and hand off to
   wheel panning inside the viewport for anything larger. */
.sm-blueprint {
    position: absolute;
    top: 16px;
    left: 16px;
    width: fit-content;
    min-width: 220px;
    max-width: calc(100% - 32px);
    max-height: calc(100% - 120px);
    z-index: 150;
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    background: rgba(32, 32, 42, 0.92);
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 10px;
    backdrop-filter: blur(12px);
}

.sm-header {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 10px 12px;
    /* No border-bottom: the first collapsible section below already carries its own top
       border, so two borders would double-draw at the same line. */
}

.sm-title {
    color: #ccc;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 0.04em;
    text-transform: uppercase;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
}

.sm-play-btn {
    flex-shrink: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    width: 32px;
    height: 32px;
    padding: 0;
    background: transparent;
    border: none;
    cursor: pointer;
    opacity: 0.85;
    transition: opacity 0.15s ease;
}

.sm-play-btn:hover {
    opacity: 0.95;
}

.sm-play-btn:active {
    opacity: 1;
}

.sm-play-btn img {
    display: block;
    width: 32px;
    height: 32px;
}

/* Collapsible sections stacked below the panel header. Each section has a clickable header
   (chevron + title) and a body that hides via .sm-section-collapsed. Multiple sections are
   separated by a subtle top border so they read as distinct panels. */
.sm-section {
    border-top: 1px solid rgba(255, 255, 255, 0.08);
}

.sm-section-header {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 6px 12px;
    color: rgba(255, 255, 255, 0.7);
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 0.04em;
    text-transform: uppercase;
    cursor: pointer;
    user-select: none;
}

.sm-section-header:hover {
    color: #eee;
}

.sm-section-chevron {
    font-size: 10px;
    color: rgba(255, 255, 255, 0.5);
    width: 12px;
    display: inline-block;
    text-align: center;
}

.sm-section-body {
    padding-bottom: 4px;
}

.sm-section-body.sm-section-collapsed {
    display: none;
}

.sm-anim-list {
    display: flex;
    flex-direction: column;
    padding: 0 6px;
    min-width: 200px;
}

.sm-anim-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    padding: 6px 10px;
    border-radius: 4px;
    font-size: 12px;
    color: rgba(255, 255, 255, 0.75);
    cursor: pointer;
    user-select: none;
}

.sm-anim-row:hover {
    background: rgba(255, 255, 255, 0.06);
}

.sm-anim-row-current {
    color: #7db8ff;
    background: rgba(77, 159, 255, 0.16);
}

.sm-anim-name {
    flex: 1 1 auto;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font-weight: 600;
}

.sm-anim-meta {
    flex-shrink: 0;
    font-size: 10px;
    color: rgba(255, 255, 255, 0.45);
}

.sm-anim-row-current .sm-anim-meta {
    color: rgba(125, 184, 255, 0.7);
}

.sm-viewport {
    position: relative;
    overflow: hidden;
    /* Content-driven: the region canvas inside dictates width and height. Panel-level
       max-height clamps it and hands off to wheel panning when content overflows. */
    width: fit-content;
    max-width: 100%;
}

.sm-content {
    /* Static positioning so the viewport can measure content height for the content-driven
       panel size. Wheel panning is achieved via the transform property set inline (see
       applyContentOffset), not via absolute positioning. */
    padding: 6px;
    display: flex;
    flex-direction: row;
    will-change: transform;
}

.sm-region {
    border: 1px dashed rgba(255, 255, 255, 0.25);
    border-radius: 8px;
    padding: 16px 8px 8px;
    position: relative;
    width: fit-content;
    min-width: 100px;
}

.sm-region-label {
    position: absolute;
    top: 2px;
    left: 10px;
    font-size: 11px;
    letter-spacing: 0.04em;
    color: rgba(255, 255, 255, 0.5);
    user-select: none;
}

.sm-region-canvas {
    position: relative;
}

.sm-edges {
    position: absolute;
    left: 0;
    top: 0;
    pointer-events: none;
}

.sm-edge {
    fill: none;
    stroke: rgba(255, 255, 255, 0.38);
    stroke-width: 1.2;
}

.sm-edge-arrow {
    fill: rgba(255, 255, 255, 0.38);
}

.sm-edge-label {
    font-size: 10px;
    fill: rgba(255, 255, 255, 0.75);
    text-anchor: middle;
    dominant-baseline: middle;
    user-select: none;
}

/* Solid backdrop rendered under the label so the edge path doesn't visually cross the
   glyphs. Fully opaque near-black tone (the panel body is #20202A over a checkerboard) so
   the stroke behind is completely masked; a subtle stroke gives the pill a defined edge. */
.sm-edge-label-bg {
    fill: #20202A;
    stroke: rgba(255, 255, 255, 0.12);
    stroke-width: 1;
}

.sm-state {
    position: absolute;
    width: 128px;
    height: 50px;
    box-sizing: border-box;
    border: 1px solid rgba(255, 255, 255, 0.28);
    border-radius: 6px;
    background: rgba(255, 255, 255, 0.05);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 2px;
    cursor: default;
    user-select: none;
}

.sm-state:not(.sm-state-disabled) {
    cursor: pointer;
}

.sm-state:not(.sm-state-disabled):hover {
    border-color: rgba(77, 159, 255, 0.9);
    background: rgba(77, 159, 255, 0.1);
}

.sm-state-name {
    font-size: 13px;
    font-weight: 600;
    color: #eee;
    max-width: 116px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
}

.sm-state-sub {
    font-size: 10px;
    color: rgba(255, 255, 255, 0.45);
    max-width: 116px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
}

.sm-state.sm-state-current {
    border-color: #4d9fff;
    border-width: 2px;
    background: rgba(77, 159, 255, 0.2);
}

.sm-state.sm-state-current .sm-state-name {
    color: #7db8ff;
}

.sm-state.sm-state-disabled {
    opacity: 0.4;
}

.sm-entry-dot {
    position: absolute;
    width: 8px;
    height: 8px;
    box-sizing: border-box;
    border-radius: 50%;
    background: rgba(255, 255, 255, 0.6);
    transform: translate(-50%, -50%);
}

.sm-tooltip {
    position: absolute;
    z-index: 20;
    padding: 6px 10px;
    border-radius: 6px;
    background: rgba(10, 10, 14, 0.95);
    border: 1px solid rgba(255, 255, 255, 0.12);
    color: rgba(255, 255, 255, 0.85);
    font-size: 11px;
    line-height: 1.5;
    white-space: pre;
    pointer-events: none;
    max-width: 280px;
    overflow: hidden;
}

/* --- Preview chip bar (the "attached strip" on top of the playback bar) --- */

/* The chips form a horizontal strip attached to the top of the playback bar: chips share the
   bar's dark surface, sit directly on top of it (bottom radius flush zero so the two rectangles
   read as one control), and separate from each other with a hairline vertical divider. Each
   chip is just a name + close glyph (no per-chip rounded pill). Shows only when at least one
   chip exists AND the SM has a preview on stage (active or parked). */
.sm-chip-bar {
    position: absolute;
    left: 50%;
    transform: translateX(-50%);
    bottom: 88px;
    /* Below the playback bar (z:150) so the bar covers the chip strip's bottom edge and
       there's no visible seam between the two rectangles. syncWidth() drives the bottom
       offset so the strip's bottom sits inside the playback bar's top border. */
    z-index: 149;
    box-sizing: border-box;
    display: flex;
    flex-direction: row;
    align-items: stretch;
    /* Bottom padding reserves the 12px overlap that gets covered by the playback bar so
       chip content (labels + close buttons) stays visible in the top segment; only the
       filled background reaches into the bar's territory. */
    padding: 0 0 12px 0;
    background: #20202A;
    /* Only round the top corners; the bottom is intentionally covered by the playback bar. */
    border-radius: 8px 8px 0 0;
    /* Width is set by JS (setBarAnchor) to match the playback bar; no CSS bounds so we don't
       fight the observer. */
    overflow: hidden;
    box-shadow: 0 -2px 8px rgba(0, 0, 0, 0.25);
}

.sm-chip {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 6px 8px 6px 14px;
    font-size: 12px;
    color: rgba(255, 255, 255, 0.55);
    /* Chips share the bar width evenly: when there are N chips the strip carves into N equal
       segments. flex-basis 0 makes flex-grow purely proportional to available width. */
    flex: 1 1 0;
    min-width: 0;
}

.sm-chip + .sm-chip {
    border-left: 1px solid rgba(255, 255, 255, 0.1);
}

.sm-chip-active {
    color: #ffffff;
}

.sm-chip-label {
    cursor: pointer;
    flex: 1 1 auto;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
}

.sm-chip-close {
    flex-shrink: 0;
    border: none;
    background: transparent;
    color: rgba(255, 255, 255, 0.55);
    font-size: 14px;
    line-height: 1;
    padding: 0 2px;
    margin-left: 6px;
    cursor: pointer;
}

.sm-chip-active .sm-chip-close {
    color: #ffffff;
}

.sm-chip-close:hover {
    color: #ffffff;
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
