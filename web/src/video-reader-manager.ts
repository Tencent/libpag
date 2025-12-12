import {PAGModule} from "./pag-module";
import type {VideoReader} from "./interfaces";
import {transferToArrayBuffer} from "./utils/common";
import type {VideoReaderManager as VideoReaderManageInterfaces} from './interfaces';
import {destroyVerify} from "./utils/decorators";

/**
 * VideoReaderManager is responsible for managing multiple VideoReader instances.
 * It handles video decoder initialization, frame preparation, and lifecycle management.
 */
@destroyVerify
export class VideoReaderManager {

    /**
     * Checks whether the given PAG composition contains any video content.
     * @param wasmIns - The WebAssembly instance to check
     * @returns True if the composition contains video, false otherwise
     */
    public static HasVideo(wasmIns: any){
        return PAGModule._videoInfoManager._HasVideo(wasmIns) as boolean;
    }


    /**
     * Factory method to create and initialize a VideoReaderManager instance.
     * @param wasmIns - The WebAssembly instance containing video information
     * @returns A promise that resolves to a fully initialized VideoReaderManager instance
     */
    public static async make(wasmIns: any): Promise<VideoReaderManager> {
        const videoManage = new VideoReaderManager(wasmIns);
        await videoManage.createVideoReader();
        return videoManage;
    }

    /** WebAssembly instance for video information management */
    public wasmIns: any;
    /** Array of video IDs managed by this instance */
    public videoIDs: Array<number> = [];
    /** Flag indicating whether this instance has been destroyed */
    public isDestroyed = false;

    /** Map storing VideoReader instances indexed by video ID */
    private videoReaderMap: Map<number, VideoReader> = new Map<number, VideoReader>();

    /**
     * Constructor for VideoReaderManager.
     * Initializes the WASM instance and retrieves all video IDs.
     * @param wasmIns - The WebAssembly instance
     * @throws Error if VideoReaderManager creation fails
     */
    public constructor(
        wasmIns: any
    ) {
        this.wasmIns = PAGModule._videoInfoManager._Make(wasmIns);
        if (!this.wasmIns) {
            throw new Error('create VideoReaderManager fail!');
        }
        this.videoIDs = this.wasmIns._getVideoIDs() as Array<number>;
        if (this.wasmIns._hasTimeRangeOverlap() as boolean) {
            console.error("The current file contains multiple layers referencing the same video with overlapping timelines. " +
                "This scenario is not supported, and the rendered result may not match expectations. It is recommended to offset the timelines of these layers.");
        }
    }

    /**
     * Creates VideoReader instances for all videos in the PAG file.
     * For each video ID, retrieves MP4 data and initializes the corresponding VideoReader.
     * Also prepares the first frame with the initial playback rate.
     */
    public async createVideoReader() {
        for (const id of this.videoIDs) {
            // Retrieve MP4 data for the current video ID
            const mp4Data = this.wasmIns._getMp4DataByID(id);
            if (mp4Data !== null) {
                // Create VideoReader with video metadata (width, height, frame rate, static time ranges)
                this.videoReaderMap.set(id, await PAGModule.VideoReader.create(mp4Data, this.wasmIns._getWidthByID(id), this.wasmIns._getHeightByID(id),
                    this.wasmIns._getFrameRateByID(id), this.wasmIns._getStaticTimeRangesByID(id)));
                // Prepare the first frame (frame 0) with the current playback rate
                await this.videoReaderMap.get(id)?.prepare(0, this.wasmIns._getPlaybackRateByID(id));
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
    public getVideoReaderByID(id: number): VideoReader | undefined {
        if (this.videoReaderMap.get(id) === undefined) {
            console.error(`get VideoReader fail!,id:${id}`);
            return undefined;
        }
        return this.videoReaderMap.get(id);
    }

    /**
     * Prepares target frames for all videos based on current playback position.
     * This method is called during rendering to ensure the correct frames are decoded.
     * It handles both hardware and software decoding modes.
     */
    public async prepareTargetFrame() {
        for (const id of this.videoIDs) {
            // Get the target frame index from WASM
            const targetFrame = this.wasmIns._getTargetFrameByID(id) as number;
            if (targetFrame < 0) {
                continue;
            }
            if (this.videoReaderMap.get(id) !== undefined) {
                // Prepare the target frame with current playback rate.
                await this.videoReaderMap.get(id)?.prepare(targetFrame, this.wasmIns._getPlaybackRateByID(id));
            } else {
                console.error("videoReader is undefined,id:", id);
            }

        }
    }

    /**
     * Destroys the VideoReaderManager instance and cleans up all resources.
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
        // Mark as destroyed
        this.isDestroyed = true;
    }

}