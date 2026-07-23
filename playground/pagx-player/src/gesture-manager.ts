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

// Gesture handling for the pagx-player canvas: wheel / mouse drag / touch pinch-pan / Safari
// gesture events. Preserves the zoom and pan state and pushes it into the active PAGXView.
// Ported from pagx-playground/src/index.ts (GestureManager) with the only structural change
// being that the view accessor is a callback instead of a shared PlaygroundState reference, so
// the component doesn't require hosts to hand over their global state singleton.

import type { PlayerView } from './pagx-view-types';

/** Zoom clamp: matches the range that pagx-playground has been tuned against. */
export const MIN_ZOOM = 0.001;
export const MAX_ZOOM = 1000.0;

/** Movement threshold below which a pointer-down/up pair is treated as a click that toggles
 *  playback rather than a pan gesture. */
export const CLICK_MOVE_THRESHOLD = 5;

/** Safari's non-standard GestureEvent. Not present in lib.dom.d.ts, so we type it inline. */
export interface GestureEvent extends UIEvent {
    scale: number;
    rotation: number;
    clientX: number;
    clientY: number;
}

enum ScaleGestureState {
    SCALE_START = 0,
    SCALE_CHANGE = 1,
    SCALE_END = 2,
}

enum ScrollGestureState {
    SCROLL_START = 0,
    SCROLL_CHANGE = 1,
    SCROLL_END = 2,
}

enum DeviceType {
    TOUCH = 0,
    MOUSE = 1,
}

export type ViewProvider = () => PlayerView | null;

export class GestureManager {
    private scaleY = 1.0;
    private pinchTimeout = 150;
    private timer: number | undefined;
    private scaleStartZoom = 1.0;
    private lastEventTime = 0;
    private lastDeltaY = 0;
    private timeThreshold = 50;
    private deltaYThreshold = 50;
    private deltaYChangeThreshold = 10;
    private mouseWheelRatio = 800;
    private touchWheelRatio = 100;

    // Drag state
    private isDragging = false;
    private dragStartX = 0;
    private dragStartY = 0;
    private dragStartOffsetX = 0;
    private dragStartOffsetY = 0;

    // Touch state
    private startTouchDistance = 0;
    private lastTouchCenterX = 0;
    private lastTouchCenterY = 0;
    private isTouchPanning = false;
    private isTouchZooming = false;

    public zoom = 1.0;
    public offsetX = 0;
    public offsetY = 0;

    private applyToView(): void {
        const view = this.getView();
        view?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
    }

    constructor(private readonly getView: ViewProvider) {}

    private handleScrollEvent(event: WheelEvent, state: ScrollGestureState): void {
        if (state === ScrollGestureState.SCROLL_CHANGE) {
            this.scaleStartZoom = this.zoom;
            this.scaleY = 1.0;
            if (event.shiftKey && event.deltaX === 0 && event.deltaY !== 0) {
                this.offsetX -= event.deltaY * window.devicePixelRatio;
            } else {
                this.offsetX -= event.deltaX * window.devicePixelRatio;
                this.offsetY -= event.deltaY * window.devicePixelRatio;
            }
            this.applyToView();
        }
    }

    private handleScaleEvent(event: WheelEvent, state: ScaleGestureState, canvas: HTMLElement): void {
        if (state === ScaleGestureState.SCALE_START) {
            this.scaleY = 1.0;
            this.scaleStartZoom = this.zoom;
        }
        if (state === ScaleGestureState.SCALE_CHANGE) {
            const rect = canvas.getBoundingClientRect();
            const pixelX = (event.clientX - rect.left) * window.devicePixelRatio;
            const pixelY = (event.clientY - rect.top) * window.devicePixelRatio;
            const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scaleStartZoom * this.scaleY));
            this.offsetX = (this.offsetX - pixelX) * (newZoom / this.zoom) + pixelX;
            this.offsetY = (this.offsetY - pixelY) * (newZoom / this.zoom) + pixelY;
            this.zoom = newZoom;
        }
        if (state === ScaleGestureState.SCALE_END) {
            this.scaleY = 1.0;
            this.scaleStartZoom = this.zoom;
        }
        this.applyToView();
    }

    public clearState(): void {
        this.scaleY = 1.0;
        this.timer = undefined;
    }

    /** Restore the view to identity transform (no zoom, no pan). Called by the toolbar Reset
     *  View button and by the player on document reload. */
    public resetTransform(): void {
        this.zoom = 1.0;
        this.offsetX = 0;
        this.offsetY = 0;
        this.clearState();
        this.applyToView();
    }

    private resetScrollTimeout(event: WheelEvent): void {
        clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            this.timer = undefined;
            this.handleScrollEvent(event, ScrollGestureState.SCROLL_END);
            this.clearState();
        }, this.pinchTimeout);
    }

    private resetScaleTimeout(event: WheelEvent, canvas: HTMLElement): void {
        clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            this.timer = undefined;
            this.handleScaleEvent(event, ScaleGestureState.SCALE_END, canvas);
            this.clearState();
        }, this.pinchTimeout);
    }

    // Heuristic: touchpad wheel events fire more frequently (short timeDifference) with smaller
    // deltas and less delta-jitter than a mouse wheel notch. Reasonable enough to pick between
    // two very different sensitivity presets; false classification just makes zoom feel a bit
    // fast/slow, never breaks correctness.
    private getDeviceType(event: WheelEvent): DeviceType {
        const now = Date.now();
        const timeDifference = now - this.lastEventTime;
        const deltaYChange = Math.abs(event.deltaY - this.lastDeltaY);
        let isTouchpad = false;
        if (event.deltaMode === event.DOM_DELTA_PIXEL && timeDifference < this.timeThreshold) {
            if (Math.abs(event.deltaY) < this.deltaYThreshold && deltaYChange < this.deltaYChangeThreshold) {
                isTouchpad = true;
            }
        }
        this.lastEventTime = now;
        this.lastDeltaY = event.deltaY;
        return isTouchpad ? DeviceType.TOUCH : DeviceType.MOUSE;
    }

    public onWheel(event: WheelEvent, canvas: HTMLElement): void {
        const deviceType = this.getDeviceType(event);
        const wheelRatio = deviceType === DeviceType.MOUSE ? this.mouseWheelRatio : this.touchWheelRatio;
        if (!event.deltaY || (!event.ctrlKey && !event.metaKey)) {
            this.resetScrollTimeout(event);
            this.handleScrollEvent(event, ScrollGestureState.SCROLL_CHANGE);
        } else {
            this.scaleY *= Math.exp(-event.deltaY / wheelRatio);
            if (!this.timer) {
                this.resetScaleTimeout(event, canvas);
                this.handleScaleEvent(event, ScaleGestureState.SCALE_START, canvas);
            } else {
                this.resetScaleTimeout(event, canvas);
                this.handleScaleEvent(event, ScaleGestureState.SCALE_CHANGE, canvas);
            }
        }
    }

    public onMouseDown(event: MouseEvent, canvas: HTMLElement): void {
        if (event.button !== 0) {
            return;
        }
        this.isDragging = true;
        this.dragStartX = event.clientX;
        this.dragStartY = event.clientY;
        this.dragStartOffsetX = this.offsetX;
        this.dragStartOffsetY = this.offsetY;
        canvas.style.cursor = 'grabbing';
    }

    public onMouseMove(event: MouseEvent): void {
        if (!this.isDragging) {
            return;
        }
        const deltaX = (event.clientX - this.dragStartX) * window.devicePixelRatio;
        const deltaY = (event.clientY - this.dragStartY) * window.devicePixelRatio;
        this.offsetX = this.dragStartOffsetX + deltaX;
        this.offsetY = this.dragStartOffsetY + deltaY;
        this.applyToView();
    }

    public onMouseUp(canvas: HTMLElement): void {
        this.isDragging = false;
        canvas.style.cursor = 'grab';
    }

    private getTouchDistance(touches: TouchList): number {
        if (touches.length < 2) {
            return 0;
        }
        const dx = touches[0].clientX - touches[1].clientX;
        const dy = touches[0].clientY - touches[1].clientY;
        return Math.sqrt(dx * dx + dy * dy);
    }

    private getTouchCenter(touches: TouchList): { x: number; y: number } {
        if (touches.length < 2) {
            return { x: touches[0].clientX, y: touches[0].clientY };
        }
        return {
            x: (touches[0].clientX + touches[1].clientX) / 2,
            y: (touches[0].clientY + touches[1].clientY) / 2,
        };
    }

    public onTouchStart(event: TouchEvent): void {
        if (event.touches.length === 1) {
            this.isTouchPanning = true;
            this.isTouchZooming = false;
            this.dragStartX = event.touches[0].clientX;
            this.dragStartY = event.touches[0].clientY;
            this.dragStartOffsetX = this.offsetX;
            this.dragStartOffsetY = this.offsetY;
        } else if (event.touches.length === 2) {
            this.isTouchPanning = false;
            this.isTouchZooming = true;
            this.startTouchDistance = this.getTouchDistance(event.touches);
            const center = this.getTouchCenter(event.touches);
            this.lastTouchCenterX = center.x;
            this.lastTouchCenterY = center.y;
            this.scaleStartZoom = this.zoom;
            this.dragStartOffsetX = this.offsetX;
            this.dragStartOffsetY = this.offsetY;
        }
    }

    public onTouchMove(event: TouchEvent, canvas: HTMLElement): void {
        if (event.touches.length === 1 && this.isTouchPanning) {
            const deltaX = (event.touches[0].clientX - this.dragStartX) * window.devicePixelRatio;
            const deltaY = (event.touches[0].clientY - this.dragStartY) * window.devicePixelRatio;
            this.offsetX = this.dragStartOffsetX + deltaX;
            this.offsetY = this.dragStartOffsetY + deltaY;
            this.applyToView();
        } else if (event.touches.length === 2 && this.isTouchZooming) {
            const currentDistance = this.getTouchDistance(event.touches);
            const center = this.getTouchCenter(event.touches);
            const rect = canvas.getBoundingClientRect();
            const pixelX = (center.x - rect.left) * window.devicePixelRatio;
            const pixelY = (center.y - rect.top) * window.devicePixelRatio;

            // Absolute-ratio zoom relative to the pinch start; combined with the recomputed
            // offset this keeps the point under the fingers fixed while scaling.
            if (this.startTouchDistance > 0) {
                const scale = currentDistance / this.startTouchDistance;
                const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scaleStartZoom * scale));
                this.offsetX = (this.offsetX - pixelX) * (newZoom / this.zoom) + pixelX;
                this.offsetY = (this.offsetY - pixelY) * (newZoom / this.zoom) + pixelY;
                this.zoom = newZoom;
            }

            const centerDeltaX = (center.x - this.lastTouchCenterX) * window.devicePixelRatio;
            const centerDeltaY = (center.y - this.lastTouchCenterY) * window.devicePixelRatio;
            this.offsetX += centerDeltaX;
            this.offsetY += centerDeltaY;

            this.lastTouchCenterX = center.x;
            this.lastTouchCenterY = center.y;

            this.applyToView();
        }
    }

    public onTouchEnd(event: TouchEvent): void {
        if (event.touches.length === 0) {
            this.isTouchPanning = false;
            this.isTouchZooming = false;
        } else if (event.touches.length === 1) {
            // Fingers were on the surface, one lifted: reseed the single-finger pan baseline
            // so the following move deltas start from the remaining finger's position.
            this.isTouchPanning = true;
            this.isTouchZooming = false;
            this.dragStartX = event.touches[0].clientX;
            this.dragStartY = event.touches[0].clientY;
            this.dragStartOffsetX = this.offsetX;
            this.dragStartOffsetY = this.offsetY;
        }
    }

    public onGestureStart(_event: GestureEvent): void {
        this.scaleStartZoom = this.zoom;
        this.dragStartOffsetX = this.offsetX;
        this.dragStartOffsetY = this.offsetY;
    }

    public onGestureChange(event: GestureEvent, canvas: HTMLElement): void {
        // event.scale is already an absolute ratio relative to gesturestart.
        const rect = canvas.getBoundingClientRect();
        const pixelX = (event.clientX - rect.left) * window.devicePixelRatio;
        const pixelY = (event.clientY - rect.top) * window.devicePixelRatio;
        const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scaleStartZoom * event.scale));
        this.offsetX = (this.offsetX - pixelX) * (newZoom / this.zoom) + pixelX;
        this.offsetY = (this.offsetY - pixelY) * (newZoom / this.zoom) + pixelY;
        this.zoom = newZoom;
        this.applyToView();
    }

    public onGestureEnd(): void {
        // Nothing to reset: scaleStartZoom and offsets are only used inside a gesture, and
        // gesturestart re-seeds them on the next pinch.
    }
}

/** Attach gesture listeners to the canvas element. Returns a cleanup function to detach them.
 *  `onCanvasTap` is invoked when a pointer down + up pair are within CLICK_MOVE_THRESHOLD px,
 *  which the player uses to toggle playback on a bare click. */
export function bindCanvasEvents(
    canvas: HTMLCanvasElement,
    manager: GestureManager,
    onCanvasTap: () => void,
): () => void {
    canvas.style.cursor = 'grab';

    let pressStartX = 0;
    let pressStartY = 0;

    const onWheel = (e: WheelEvent) => {
        e.preventDefault();
        manager.onWheel(e, canvas);
    };
    const onMouseDown = (e: MouseEvent) => {
        e.preventDefault();
        pressStartX = e.clientX;
        pressStartY = e.clientY;
        manager.onMouseDown(e, canvas);
    };
    const onMouseMove = (e: MouseEvent) => {
        manager.onMouseMove(e);
    };
    const onMouseUp = (e: MouseEvent) => {
        manager.onMouseUp(canvas);
        if (
            e.button === 0 &&
            Math.abs(e.clientX - pressStartX) < CLICK_MOVE_THRESHOLD &&
            Math.abs(e.clientY - pressStartY) < CLICK_MOVE_THRESHOLD
        ) {
            onCanvasTap();
        }
    };
    const onMouseLeave = () => {
        manager.onMouseUp(canvas);
    };

    const onTouchStart = (e: TouchEvent) => {
        e.preventDefault();
        if (e.touches.length === 1) {
            pressStartX = e.touches[0].clientX;
            pressStartY = e.touches[0].clientY;
        }
        manager.onTouchStart(e);
    };
    const onTouchMove = (e: TouchEvent) => {
        e.preventDefault();
        manager.onTouchMove(e, canvas);
    };
    const onTouchEnd = (e: TouchEvent) => {
        const wasSingleTouch = e.touches.length === 0 && e.changedTouches.length === 1;
        manager.onTouchEnd(e);
        if (wasSingleTouch) {
            const touch = e.changedTouches[0];
            if (
                Math.abs(touch.clientX - pressStartX) < CLICK_MOVE_THRESHOLD &&
                Math.abs(touch.clientY - pressStartY) < CLICK_MOVE_THRESHOLD
            ) {
                onCanvasTap();
            }
        }
    };
    const onTouchCancel = (e: TouchEvent) => {
        manager.onTouchEnd(e);
    };

    const onGestureStart = (e: Event) => {
        e.preventDefault();
        manager.onGestureStart(e as GestureEvent);
    };
    const onGestureChange = (e: Event) => {
        e.preventDefault();
        manager.onGestureChange(e as GestureEvent, canvas);
    };
    const onGestureEnd = (e: Event) => {
        e.preventDefault();
        manager.onGestureEnd();
    };

    canvas.addEventListener('wheel', onWheel, { passive: false });
    canvas.addEventListener('mousedown', onMouseDown);
    canvas.addEventListener('mousemove', onMouseMove);
    canvas.addEventListener('mouseup', onMouseUp);
    canvas.addEventListener('mouseleave', onMouseLeave);
    canvas.addEventListener('touchstart', onTouchStart, { passive: false });
    canvas.addEventListener('touchmove', onTouchMove, { passive: false });
    canvas.addEventListener('touchend', onTouchEnd);
    canvas.addEventListener('touchcancel', onTouchCancel);
    canvas.addEventListener('gesturestart', onGestureStart);
    canvas.addEventListener('gesturechange', onGestureChange);
    canvas.addEventListener('gestureend', onGestureEnd);

    return () => {
        canvas.removeEventListener('wheel', onWheel);
        canvas.removeEventListener('mousedown', onMouseDown);
        canvas.removeEventListener('mousemove', onMouseMove);
        canvas.removeEventListener('mouseup', onMouseUp);
        canvas.removeEventListener('mouseleave', onMouseLeave);
        canvas.removeEventListener('touchstart', onTouchStart);
        canvas.removeEventListener('touchmove', onTouchMove);
        canvas.removeEventListener('touchend', onTouchEnd);
        canvas.removeEventListener('touchcancel', onTouchCancel);
        canvas.removeEventListener('gesturestart', onGestureStart);
        canvas.removeEventListener('gesturechange', onGestureChange);
        canvas.removeEventListener('gestureend', onGestureEnd);
    };
}
