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

// Playback bar: renders play/pause, prev/next frame, progress slider, time/frame counters and
// loop toggle. Owns the 100ms UI tick that keeps the slider and counters in sync with the
// underlying PAGXView. The bar hides itself when the loaded document has no animation (duration
// is zero), matching the playground's behavior for static pagx.
//
// This module intentionally has no direct dependency on the pagx-player root class; it takes a
// `getView()` accessor so the same instance can survive across document reloads (`load()`
// swapping the underlying PAGXView) without needing to be recreated.

import type { PlayerView } from './pagx-view-types';
import { LOOP_SEQUENCE_ICON_SVG, LOOP_SINGLE_ICON_SVG } from './icons';

export interface PlaybackBarCallbacks {
    /** Called after any user interaction that changes the current time or play state. Emits
     *  the 'framechange' / 'play' / 'pause' / 'seek' player-level events. */
    onFrameChange?: (currentTimeMicros: number) => void;
    onPlay?: () => void;
    onPause?: () => void;
    onSeek?: (currentTimeMicros: number) => void;
}

export interface PlaybackBarOptions {
    /** Root DOM element to append the playback bar into. */
    parent: HTMLElement;
    /** Base URL for play/pause/previous/next.png. Trailing slash optional. */
    iconBaseUrl: string;
    /** Callback returning the active PAGXView, or null when nothing is loaded. */
    getView: () => PlayerView | null;
    callbacks?: PlaybackBarCallbacks;
}

/** Resolves the icon URL against `iconBaseUrl`, tolerating both '/foo/' and '/foo' inputs. */
function iconUrl(base: string, name: string): string {
    if (!base) return name;
    return base.endsWith('/') ? base + name : base + '/' + name;
}

export class PlaybackBar {
    private readonly parent: HTMLElement;
    private readonly getView: () => PlayerView | null;
    private readonly callbacks: PlaybackBarCallbacks;
    private readonly iconBaseUrl: string;

    private root!: HTMLDivElement;
    private playPauseBtn!: HTMLButtonElement;
    private playPauseImg!: HTMLImageElement;
    private prevFrameBtn!: HTMLButtonElement;
    private nextFrameBtn!: HTMLButtonElement;
    private progressSlider!: HTMLInputElement;
    private timeText!: HTMLSpanElement;
    private frameText!: HTMLSpanElement;
    private loopBtn!: HTMLButtonElement;

    private isDraggingSlider = false;
    private wasPlayingBeforeDrag = false;
    private wasPlaying = false;
    private tickHandle: number | null = null;

    constructor(opts: PlaybackBarOptions) {
        this.parent = opts.parent;
        this.getView = opts.getView;
        this.callbacks = opts.callbacks ?? {};
        this.iconBaseUrl = opts.iconBaseUrl;
        this.build();
        this.startTick();
    }

    /** Whether the bar is currently visible to the user. Guards keyboard shortcuts so they
     *  don't fire when the bar is intentionally hidden (static pagx or no document loaded). */
    public isVisible(): boolean {
        return !this.root.classList.contains('hidden');
    }

    /** Force the bar visible / hidden. Called by the player based on the loaded document's
     *  duration (>0 = visible). */
    public setVisible(visible: boolean): void {
        this.root.classList.toggle('hidden', !visible);
        if (visible) {
            this.updateAll();
            this.updateLoopIcon();
        }
    }

    /** Push all current PAGXView state onto the DOM. Called on visibility change and after
     *  user actions to keep the UI in sync. */
    public updateAll(): void {
        this.updatePlayPauseIcon();
        this.updateProgressSlider();
        this.updateTimeDisplay();
    }

    /** Toggle play/pause, wrapping around from the last frame back to the start so a single
     *  playthrough that ended at the tail becomes replay-able. */
    public togglePlayback(): void {
        const view = this.getView();
        if (!view || view.durationMicros() <= 0) {
            return;
        }
        if (view.isPlaying()) {
            view.pause();
            this.callbacks.onPause?.();
        } else {
            if (view.currentTimeMicros() >= view.durationMicros()) {
                view.setCurrentTimeMicros(0);
            }
            view.play();
            this.callbacks.onPlay?.();
        }
        this.updateAll();
    }

    /** Step one frame in either direction (-1 previous, +1 next). Always pauses first so the
     *  render loop can't advance the playhead past the destination frame. */
    public stepFrame(direction: number): void {
        const view = this.getView();
        if (!view) {
            return;
        }
        const rate = view.frameRate();
        const duration = view.durationMicros();
        if (rate <= 0 || duration <= 0) {
            return;
        }
        view.pause();
        this.callbacks.onPause?.();
        const frameDurationUs = 1_000_000 / rate;
        const target = view.currentTimeMicros() + direction * frameDurationUs;
        const clamped = Math.max(0, Math.min(duration, target));
        view.setCurrentTimeMicros(clamped);
        this.callbacks.onSeek?.(clamped);
        this.updateAll();
    }

    /** Detach listeners and stop the tick. */
    public destroy(): void {
        if (this.tickHandle !== null) {
            window.clearInterval(this.tickHandle);
            this.tickHandle = null;
        }
        this.root.remove();
    }

    private build(): void {
        this.root = document.createElement('div');
        this.root.id = 'playback-bar';
        this.root.className = 'pagx-player-playback-bar playback-bar hidden';

        this.playPauseBtn = document.createElement('button');
        this.playPauseBtn.id = 'play-pause-btn';
        this.playPauseBtn.className = 'playback-btn playback-btn-primary';
        this.playPauseImg = document.createElement('img');
        this.playPauseImg.id = 'play-pause-img';
        this.playPauseImg.src = iconUrl(this.iconBaseUrl, 'play.png');
        this.playPauseImg.alt = '';
        this.playPauseBtn.appendChild(this.playPauseImg);
        this.playPauseBtn.addEventListener('click', () => {
            this.togglePlayback();
            this.playPauseBtn.blur();
        });

        this.prevFrameBtn = document.createElement('button');
        this.prevFrameBtn.id = 'prev-frame-btn';
        this.prevFrameBtn.className = 'playback-btn playback-btn-step';
        const prevImg = document.createElement('img');
        prevImg.src = iconUrl(this.iconBaseUrl, 'previous.png');
        prevImg.alt = '';
        this.prevFrameBtn.appendChild(prevImg);
        this.prevFrameBtn.addEventListener('click', () => {
            this.stepFrame(-1);
            this.prevFrameBtn.blur();
        });

        this.nextFrameBtn = document.createElement('button');
        this.nextFrameBtn.id = 'next-frame-btn';
        this.nextFrameBtn.className = 'playback-btn playback-btn-step';
        const nextImg = document.createElement('img');
        nextImg.src = iconUrl(this.iconBaseUrl, 'next.png');
        nextImg.alt = '';
        this.nextFrameBtn.appendChild(nextImg);
        this.nextFrameBtn.addEventListener('click', () => {
            this.stepFrame(1);
            this.nextFrameBtn.blur();
        });

        const progressWrapper = document.createElement('div');
        progressWrapper.className = 'progress-wrapper';
        this.progressSlider = document.createElement('input');
        this.progressSlider.id = 'progress-slider';
        this.progressSlider.className = 'progress-slider';
        this.progressSlider.type = 'range';
        this.progressSlider.min = '0';
        this.progressSlider.max = '1000';
        this.progressSlider.value = '0';
        this.progressSlider.step = '1';
        this.bindSlider();
        progressWrapper.appendChild(this.progressSlider);

        const timeDisplay = document.createElement('div');
        timeDisplay.id = 'time-display';
        timeDisplay.className = 'time-display';
        this.timeText = document.createElement('span');
        this.timeText.id = 'time-text';
        this.timeText.className = 'time-text';
        this.timeText.textContent = '0.00s / 0.00s';
        const divider = document.createElement('span');
        divider.className = 'time-divider';
        this.frameText = document.createElement('span');
        this.frameText.id = 'frame-text';
        this.frameText.className = 'frame-text';
        this.frameText.textContent = '0 / 0';
        timeDisplay.appendChild(this.timeText);
        timeDisplay.appendChild(divider);
        timeDisplay.appendChild(this.frameText);

        this.loopBtn = document.createElement('button');
        this.loopBtn.id = 'loop-btn';
        this.loopBtn.className = 'playback-btn playback-btn-loop';
        this.loopBtn.setAttribute('aria-label', 'Loop');
        this.loopBtn.innerHTML = LOOP_SEQUENCE_ICON_SVG + LOOP_SINGLE_ICON_SVG;
        this.loopBtn.addEventListener('click', () => {
            const view = this.getView();
            if (!view) return;
            view.setLoop(!view.isLoop());
            this.updateLoopIcon();
            this.loopBtn.blur();
        });

        this.root.appendChild(this.playPauseBtn);
        this.root.appendChild(this.prevFrameBtn);
        this.root.appendChild(this.nextFrameBtn);
        this.root.appendChild(progressWrapper);
        this.root.appendChild(timeDisplay);
        this.root.appendChild(this.loopBtn);
        this.parent.appendChild(this.root);
    }

    private bindSlider(): void {
        const seekToSliderValue = () => {
            const view = this.getView();
            if (!view) return;
            const duration = view.durationMicros();
            if (duration <= 0) return;
            const value = parseFloat(this.progressSlider.value);
            const targetTime = (value / 1000) * duration;
            view.setCurrentTimeMicros(targetTime);
            this.callbacks.onSeek?.(targetTime);
        };
        this.progressSlider.addEventListener('input', () => {
            const view = this.getView();
            if (!view) return;
            // First input event of a drag: pause the view so subsequent frames follow the slider
            // instead of the render loop. Remember the pre-drag playing state so 'change' can
            // restore it.
            if (!this.isDraggingSlider) {
                this.isDraggingSlider = true;
                this.wasPlayingBeforeDrag = view.isPlaying();
                if (this.wasPlayingBeforeDrag) {
                    view.pause();
                    this.callbacks.onPause?.();
                }
            }
            this.updateSliderFill();
            seekToSliderValue();
            this.updateTimeDisplay();
            this.updatePlayPauseIcon();
        });
        this.progressSlider.addEventListener('change', () => {
            this.isDraggingSlider = false;
            const view = this.getView();
            if (!view) return;
            seekToSliderValue();
            if (this.wasPlayingBeforeDrag) {
                view.play();
                this.callbacks.onPlay?.();
                this.wasPlayingBeforeDrag = false;
            }
            this.updateAll();
            // Release focus so Space keeps toggling playback after a scrub.
            this.progressSlider.blur();
        });
    }

    private startTick(): void {
        // 100ms cadence matches the playground; smoother updates would just waste DOM work.
        // Also fires once on the play -> stop transition so the slider/counters land on the
        // final frame after a single (non-looping) playback ends, and hosts subscribing to
        // 'framechange' see the terminal frame explicitly.
        this.tickHandle = window.setInterval(() => {
            const view = this.getView();
            if (!view || !this.isVisible()) {
                return;
            }
            const playing = view.isPlaying();
            if (!this.isDraggingSlider && (playing || this.wasPlaying)) {
                this.updateAll();
                // Emit onFrameChange on both playing ticks and the play -> stop transition so
                // hosts can react to the final frame settling; skipping the transition case
                // would leave subscribers stuck on the last "playing" frame value.
                this.callbacks.onFrameChange?.(view.currentTimeMicros());
            }
            this.wasPlaying = playing;
        }, 100);
    }

    // --- UI updaters (private) ---

    private updatePlayPauseIcon(): void {
        const view = this.getView();
        if (!view) return;
        const playing = view.isPlaying();
        this.playPauseImg.src = iconUrl(this.iconBaseUrl, playing ? 'pause.png' : 'play.png');
    }

    private updateSliderFill(): void {
        const pct = (parseFloat(this.progressSlider.value) / 1000) * 100;
        this.progressSlider.style.background =
            'linear-gradient(90deg, #4c7cf3, #8b5cf0, #ec5b9c) no-repeat 0 0 / ' +
            pct + '% 100%, rgba(255, 255, 255, 0.18)';
    }

    private updateProgressSlider(): void {
        const view = this.getView();
        if (!view) return;
        const duration = view.durationMicros();
        if (duration <= 0) {
            this.progressSlider.value = '0';
            this.updateSliderFill();
            return;
        }
        const current = Math.max(0, view.currentTimeMicros());
        this.progressSlider.value = String(Math.round((current / duration) * 1000));
        this.updateSliderFill();
    }

    private updateTimeDisplay(): void {
        const view = this.getView();
        if (!view) return;
        const current = view.currentTimeMicros();
        const duration = view.durationMicros();
        this.timeText.textContent = formatTime(current) + ' / ' + formatTime(duration);
        this.frameText.textContent =
            String(getCurrentFrame(view)) + ' / ' + String(getTotalFrames(view));
    }

    private updateLoopIcon(): void {
        const view = this.getView();
        if (!view) return;
        this.loopBtn.classList.toggle('active', view.isLoop());
    }
}

function formatTime(microseconds: number): string {
    const seconds = Math.max(0, microseconds / 1_000_000);
    return seconds.toFixed(2) + 's';
}

function getCurrentFrame(view: PlayerView): number {
    const rate = view.frameRate();
    if (rate <= 0) return 0;
    return Math.round(Math.max(0, view.currentTimeMicros()) * rate / 1_000_000);
}

function getTotalFrames(view: PlayerView): number {
    const rate = view.frameRate();
    if (rate <= 0) return 0;
    return Math.ceil(view.durationMicros() * rate / 1_000_000);
}
