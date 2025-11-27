import {PAGModule} from "./pag-module";
import type {VideoReader} from "./interfaces";
import {transferToArrayBuffer} from "./utils/common";
import type {VideoReaderManage as VideoReaderManageInterfaces} from './interfaces';
import {destroyVerify} from "./utils/decorators";

@destroyVerify
export class VideoReaderManage {

    public static async make(wasmIns: any): Promise<VideoReaderManageInterfaces> {
        const videoManage = new VideoReaderManage(wasmIns);
        await videoManage.createVideoReader();
        return videoManage;
    }

    public wasmIns: any;
    public videoIds: Array<number> = [];
    public isDestroyed = false;

    private videoReaderMap: Map<number, VideoReader> = new Map<number, VideoReader>();
    private softWareDecodeEnableMap:Map<number,boolean> = new Map<number,boolean>();

    public constructor(
        wasmIns: any
    ) {
        this.wasmIns = PAGModule._videoInfoManage._Make(wasmIns);
        if (!this.wasmIns) {
            throw new Error('create VideoReaderManage fail!');
        }
        this.videoIds = this.wasmIns._getVideoIds() as Array<number>;
    }

    public async createVideoReader() {
        for (const id of this.videoIds) {
            const mp4Data = this.wasmIns._getMp4DataById(id);
            if (mp4Data !== null) {
                this.videoReaderMap.set(id, await PAGModule.VideoReader.create(mp4Data, this.wasmIns._getWidthById(id), this.wasmIns._getHeightById(id),
                    this.wasmIns._getFrameRateById(id), this.wasmIns._getStaticTimeRangesById(id)));
            }

        }
    }

    public getVideoReaderById(id: number): VideoReader | undefined {
        if (this.videoReaderMap.get(id) === undefined) {
            throw new Error(`get VideoReader fail!,id:${id}`);
        }
        this.softWareDecodeEnableMap.set(id,false);
        return this.videoReaderMap.get(id);
    }

    public async getTargetFrame() {
        for (const id of this.videoIds) {
            if(this.softWareDecodeEnableMap.get(id) !== undefined && this.softWareDecodeEnableMap.get(id)){
                return;
            }
            const targetFrame = this.wasmIns._getTargetFrameById(id) as number;
            console.log("id:",id,",targetFrame:",targetFrame);
            if(targetFrame >=0){
                await this.videoReaderMap.get(id)?.prepare(targetFrame,this.wasmIns._getPlaybackRate());
            }
            if(this.softWareDecodeEnableMap.get(id) === undefined){
                this.softWareDecodeEnableMap.set(id,true);
                this.videoReaderMap.get(id)?.stop();
            }
        }
    }

    public destroy(): void {
        this.wasmIns.delete();
        for (const key of this.videoReaderMap.keys()) {
            this.videoReaderMap.get(key)?.onDestroy();
        }
        this.videoReaderMap.clear();
        this.softWareDecodeEnableMap.clear();
        this.isDestroyed = true;
    }

}