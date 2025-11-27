import {PAGModule} from "./pag-module";
import type {VideoReader} from "./interfaces";
import {transferToArrayBuffer} from "./utils/common";
import type {VideoReaderManage as VideoReaderManageInterfaces} from './interfaces';
import {destroyVerify} from "./utils/decorators";

/**
 * VideoReaderManage is responsible for managing multiple VideoReader instances.
 * It handles video decoder initialization, frame preparation, and lifecycle management.
 */
@destroyVerify
export class VideoReaderManage {

    /**
     * Factory method to create and initialize a VideoReaderManage instance.
     * @param wasmIns - The WebAssembly instance containing video information
     * @returns A promise that resolves to a fully initialized VideoReaderManage instance
     */
    public static async make(wasmIns: any): Promise<VideoReaderManageInterfaces> {
        const videoManage = new VideoReaderManage(wasmIns);
        await videoManage.createVideoReader();
        return videoManage;
    }

    /** WebAssembly instance for video information management */
    public wasmIns: any;
    /** Array of video IDs managed by this instance */
    public videoIds: Array<number> = [];
    /** Flag indicating whether this instance has been destroyed */
    public isDestroyed = false;

    /** Map storing VideoReader instances indexed by video ID */
    private videoReaderMap: Map<number, VideoReader> = new Map<number, VideoReader>();
    /** Map tracking whether software decoding is enabled for each video ID */
    private softWareDecodeEnableMap: Map<number, boolean> = new Map<number, boolean>();

    /**
     * Constructor for VideoReaderManage.
     * Initializes the WASM instance and retrieves all video IDs.
     * @param wasmIns - The WebAssembly instance
     * @throws Error if VideoReaderManage creation fails
     */
    public constructor(
        wasmIns: any
    ) {
        this.wasmIns = PAGModule._videoInfoManage._Make(wasmIns);
        if (!this.wasmIns) {
            throw new Error('create VideoReaderManage fail!');
        }
        this.videoIds = this.wasmIns._getVideoIds() as Array<number>;
    }

    /**
     * Creates VideoReader instances for all videos in the PAG file.
     * For each video ID, retrieves MP4 data and initializes the corresponding VideoReader.
     * Also prepares the first frame with the initial playback rate.
     */
    public async createVideoReader() {
        for (const id of this.videoIds) {
            // Retrieve MP4 data for the current video ID
            const mp4Data = this.wasmIns._getMp4DataById(id);
            if (mp4Data !== null) {
                // Create VideoReader with video metadata (width, height, frame rate, static time ranges)
                this.videoReaderMap.set(id, await PAGModule.VideoReader.create(mp4Data, this.wasmIns._getWidthById(id), this.wasmIns._getHeightById(id),
                    this.wasmIns._getFrameRateById(id), this.wasmIns._getStaticTimeRangesById(id)));
                // Prepare the first frame (frame 0) with the current playback rate
                await this.videoReaderMap.get(id)?.prepare(0, this.wasmIns._getPlaybackRateById(id));
            }

        }
    }

    /**
     * Retrieves a VideoReader instance by video ID.
     * Marks the video as using hardware decoding (software decode disabled).
     * @param id - The video ID
     * @returns The VideoReader instance or undefined if not found
     * @throws Error if VideoReader is not found for the given ID
     */
    public getVideoReaderById(id: number): VideoReader | undefined {
        if (this.videoReaderMap.get(id) === undefined) {
            throw new Error(`get VideoReader fail!,id:${id}`);
        }
        // Mark as hardware decoding (software decode disabled)
        this.softWareDecodeEnableMap.set(id, false);
        return this.videoReaderMap.get(id);
    }

    /**
     * Prepares target frames for all videos based on current playback position.
     * This method is called during rendering to ensure the correct frames are decoded.
     * It handles both hardware and software decoding modes.
     */
    public async getTargetFrame() {
        for (const id of this.videoIds) {
            // Skip videos that are already using software decoding
            if (this.softWareDecodeEnableMap.get(id) !== undefined && this.softWareDecodeEnableMap.get(id)) {
                continue;
            }
            // Get the target frame index from WASM
            const targetFrame = this.wasmIns._getTargetFrameById(id) as number;
            if (targetFrame < 0) {
                continue;
            }
            if (this.videoReaderMap.get(id) !== undefined) {
                // Prepare the target frame with current playback rate
                await this.videoReaderMap.get(id)?.prepare(targetFrame, this.wasmIns._getPlaybackRateById(id));
                // If this is the first time preparing a frame, enable software decoding and stop the decode loop
                if (this.softWareDecodeEnableMap.get(id) === undefined) {
                    this.softWareDecodeEnableMap.set(id, true);
                    this.videoReaderMap.get(id)?.stop();
                }
            } else {
                console.error("videoReader is undefined,id:", id);
            }

        }
    }

    /**
     * Destroys the VideoReaderManage instance and cleans up all resources.
     * This includes destroying the WASM instance, all VideoReader instances,
     * and clearing internal maps.
     */
    public destroy(): void {
        // Delete the WASM instance
        this.wasmIns.delete();
        // Destroy all VideoReader instances
        for (const key of this.videoReaderMap.keys()) {
            this.videoReaderMap.get(key)?.onDestroy();
        }
        // Clear all internal maps
        this.videoReaderMap.clear();
        this.softWareDecodeEnableMap.clear();
        // Mark as destroyed
        this.isDestroyed = true;
    }

}