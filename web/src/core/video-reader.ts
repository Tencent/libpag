import {
    VIDEO_DECODE_WAIT_FRAME,
    VIDEO_DECODE_SEEK_TIMEOUT_FRAME,
    VIDEO_PLAYBACK_RATE_MAX,
    VIDEO_PLAYBACK_RATE_MIN,
} from '../constant';
import {addListener, removeListener, removeAllListeners} from '../utils/video-listener';
import {IPHONE, WECHAT, SAFARI_OR_IOS_WEBVIEW} from '../utils/ua';
import {PAGModule} from '../pag-module';

import type {TimeRange, VideoReader as VideoReaderInterfaces} from '../interfaces';
import type {PAGPlayer} from '../pag-player';
import {isInstanceOf} from '../utils/type-utils';
import {destroyVerify} from "../utils/decorators";

const UHD_RESOLUTION = 3840;

// Get video initiated token on Wechat browser.
const getWechatNetwork = () => {
    return new Promise<void>((resolve) => {
        window.WeixinJSBridge.invoke(
            'getNetworkType',
            {},
            () => {
                resolve();
            },
            () => {
                resolve();
            },
        );
    });
};

const waitVideoCanPlay = (videoElement: HTMLVideoElement) => {
    return new Promise((resolve) => {
        const canplayHandle = () => {
            removeListener(videoElement, 'canplay', canplayHandle);
            clearTimeout(timer);
            resolve(true);
        };
        addListener(videoElement, 'canplay', canplayHandle);
        const timer = setTimeout(() => {
            removeListener(videoElement, 'canplay', canplayHandle);
            resolve(false);
        }, 1000);
    });
};

@destroyVerify
export class VideoReader {
    public static async create(
        source: Uint8Array | HTMLVideoElement,
        width: number,
        height: number,
        frameRate: number,
        staticTimeRanges: TimeRange[],
    ): Promise<VideoReaderInterfaces> {
        return new VideoReader(source, width, height, frameRate, staticTimeRanges);
    }

    public isSought = false;
    public isPlaying = false;
    public bitmap: ImageBitmap | null = null;
    public isDestroyed = false;


    private videoEl: HTMLVideoElement | null = null;
    private frameRate = 0;
    private canplay = false;
    private staticTimeRanges: StaticTimeRanges | null = null;
    private disablePlaybackRate = false;
    private error: any = null;
    private player: PAGPlayer | null = null;
    private width = 0;
    private height = 0;
    private bitmapCanvas: OffscreenCanvas | null = null;
    private bitmapCtx: OffscreenCanvasRenderingContext2D | null = null;
    private currentFrame = -1;
    private targetFrame = -1;

    public constructor(
        source: Uint8Array | HTMLVideoElement,
        width: number,
        height: number,
        frameRate: number,
        staticTimeRanges: TimeRange[],
    ) {
        if (isInstanceOf(source, globalThis.HTMLVideoElement)) {
            this.videoEl = source as HTMLVideoElement;
            this.canplay = true;
        } else {
            this.videoEl = document.createElement('video');
            this.videoEl.style.display = 'none';
            this.videoEl.muted = true;
            this.videoEl.playsInline = true;
            this.videoEl.preload = 'auto'; // use load() will make a bug on Chrome.
            this.videoEl.width = width;
            this.videoEl.height = height;
            waitVideoCanPlay(this.videoEl).then(() => {
                this.canplay = true;
            });
            const buffer = (source as Uint8Array).slice();
            const blob = new Blob([buffer], {type: 'video/mp4'});
            this.videoEl.src = URL.createObjectURL(blob);
            if (IPHONE) {
                // use load() will make a bug on Chrome.
                this.videoEl.load();
            }
        }
        this.frameRate = frameRate;
        this.width = width;
        this.height = height;
        this.staticTimeRanges = new StaticTimeRanges(staticTimeRanges);
        if (UHD_RESOLUTION < width || UHD_RESOLUTION < height) {
            this.disablePlaybackRate = true;
        }
        this.linkPlayer(PAGModule.currentPlayer);
    }

    public async prepare(targetFrame: number, playbackRate: number): Promise<void> {
        if (targetFrame === this.currentFrame) {
            return;
        }
        const promise = new Promise<void>(async (resolve, reject) => {
            this.setError(null); // reset error
            this.isSought = false; // reset seek status
            const {currentTime} = this.videoEl!;
            const targetTime = targetFrame / this.frameRate;

            if (currentTime === 0 && targetTime === 0) {
                if (!this.canplay && !SAFARI_OR_IOS_WEBVIEW) {
                    await waitVideoCanPlay(this.videoEl!);
                } else {
                    try {
                        await waitVideoCanPlay(this.videoEl!);
                        await this.play();
                    } catch (e) {
                        this.setError(e);
                        this.currentFrame = targetFrame;
                        reject(e);
                        return;
                    }
                    await new Promise<void>((resolveInner) => {
                        requestAnimationFrame(() => {
                            this.pause();
                            resolveInner();
                        });
                    });
                }
            } else {
                if (Math.round(targetTime * this.frameRate) === Math.round(currentTime * this.frameRate)) {
                    // Current frame
                } else if (this.staticTimeRanges?.contains(targetFrame)) {
                    // Static frame
                    await this.seek(targetTime, false);
                    this.currentFrame = targetFrame;
                    resolve();  // Ensure promise resolves
                    return;
                } else if ((Math.abs(currentTime - targetTime) < (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME) && !this.videoEl!.paused) {
                    // Within tolerable frame rate deviation
                } else {
                    // Seek and play
                    this.isSought = true;
                    await this.seek(targetTime);
                    this.currentFrame = targetFrame;
                    resolve();
                    return;
                }
            }

            const targetPlaybackRate = Math.min(Math.max(playbackRate, VIDEO_PLAYBACK_RATE_MIN), VIDEO_PLAYBACK_RATE_MAX);
            if (!this.disablePlaybackRate && this.videoEl!.playbackRate !== targetPlaybackRate) {
                this.videoEl!.playbackRate = targetPlaybackRate;
            }

            if (this.isPlaying && this.videoEl!.paused) {
                try {
                    await this.play();
                } catch (e) {
                    this.setError(e);
                    this.currentFrame = targetFrame;
                    reject(e);
                    return;
                }
            }
            this.currentFrame = targetFrame;
            resolve();
        });
        await promise;
    }

    public getCurrentFrame(): number {
        return this.currentFrame;
    }

    public getVideo() {
        return this.videoEl;
    }

    public async play() {
        if (!this.videoEl!.paused) return;
        if (WECHAT && window.WeixinJSBridge) {
            await getWechatNetwork();
        }
        if (document.visibilityState !== 'visible') {
            const visibilityHandle = () => {
                if (document.visibilityState === 'visible') {
                    if (this.videoEl) this.videoEl.play();
                    window.removeEventListener('visibilitychange', visibilityHandle);
                }
            };
            window.addEventListener('visibilitychange', visibilityHandle);
            throw new Error('The play() request was interrupted because the document was hidden!');
        }
        await this.videoEl?.play();
    }

    public pause() {
        this.isPlaying = false;
        if (this.videoEl!.paused) return;
        this.videoEl?.pause();
    }

    public stop() {
        this.isPlaying = false;
        if (!this.videoEl!.paused) {
            this.videoEl?.pause();
        }
    }

    public getError() {
        return this.error;
    }

    public onDestroy() {
        if (this.player) {
            this.player.unlinkVideoReader(this);
            this.player = null;
        }
        removeAllListeners(this.videoEl!, 'playing');
        removeAllListeners(this.videoEl!, 'timeupdate');
        this.videoEl = null;
        this.bitmapCanvas = null;
        this.bitmapCtx = null;
        this.isDestroyed = true;
    }

    private seek(targetTime: number, play = true) {
        return new Promise<void>((resolve, reject) => {
            if (!this.videoEl) {
                reject(new Error('Video element is not initialized.'));
                return;
            }

            const onSeeked = () => {
                removeListener(this.videoEl!, 'seeked', onSeeked);
                clearTimeout(seekTimeout);
                if (play) {
                    // After seeking, the video might still be in 'ended' state
                    // Reset it by setting currentTime to itself to clear the ended flag
                    if (this.videoEl!.ended) {
                        this.videoEl!.currentTime = this.videoEl!.currentTime;
                    }
                    this.videoEl?.play().catch((e) => {
                        this.setError(e);
                    });
                } else if (!play && !this.videoEl!.paused) {
                    this.videoEl?.pause();
                }
                resolve();
            };

            const onCanPlay = () => {
                removeListener(this.videoEl!, 'canplay', onCanPlay);
                // Now that we have enough data, perform the seek.
                this.videoEl!.currentTime = targetTime;
                addListener(this.videoEl!, 'seeked', onSeeked);
            };

            const seekTimeout = setTimeout(() => {
                removeListener(this.videoEl!, 'canplay', onCanPlay);
                removeListener(this.videoEl!, 'seeked', onSeeked);
                reject(new Error('Seek operation timed out.'));
            }, (1000 / this.frameRate) * VIDEO_DECODE_SEEK_TIMEOUT_FRAME);

            // Check if we need to wait for 'canplay' event before seeking.
            if (this.videoEl.readyState < HTMLMediaElement.HAVE_FUTURE_DATA) {
                addListener(this.videoEl, 'canplay', onCanPlay);
            } else {
                // We already have enough data to perform the seek.
                this.videoEl!.currentTime = targetTime;
                addListener(this.videoEl, 'seeked', onSeeked);
            }
        });
    }

    private setError(e: any) {
        this.error = e;
    }

    private linkPlayer(player: PAGPlayer | null) {
        this.player = player;
        if (player) {
            player.linkVideoReader(this);
        }
    }
}

export class StaticTimeRanges {
    private timeRanges: TimeRange[];

    public constructor(timeRanges: TimeRange[]) {
        this.timeRanges = timeRanges;
    }

    public contains(targetFrame: number) {
        if (this.timeRanges.length === 0) return false;
        for (let timeRange of this.timeRanges) {
            if (timeRange.start <= targetFrame && targetFrame < timeRange.end) {
                return true;
            }
        }
        return false;
    }
}
