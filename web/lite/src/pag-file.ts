import { decode } from './pag-codec';
import { Composition } from './base/composition';
import { LayerType } from './base/layer';
import { VectorComposition } from './base/vector-composition';
import { ByteArray } from './codec/utils/byte-array';
import { CompositionType } from './codec/types';
import { TimeRange } from './base/time-range';
import { PreComposeLayer } from './base/pre-compose-layer';
import { VideoComposition } from './base/video-composition';

export class PAGFile {
  public static fromArrayBuffer(arrayBuffer: ArrayBuffer): PAGFile {
    if (!arrayBuffer || arrayBuffer.byteLength === 0) throw new Error("Can't read empty array buffer!");
    const byteArray = new ByteArray(arrayBuffer, true);
    const { compositions, tagLevel } = decode(byteArray);
    return new PAGFile(compositions, tagLevel);
  }

  public tagLevel = 1;
  public mainComposition: Composition;
  public compositions: Array<Composition> = [];
  public numLayers = 0;
  public duration: number;
  public implDuration: number;
  public scaledTimeRange: TimeRange = { start: 0, end: 0 };

  public constructor(compositions: Array<Composition>, tagLevel: number) {
    this.mainComposition = compositions[compositions.length - 1];
    this.scaledTimeRange.start = 0;
    this.scaledTimeRange.end = this.mainComposition.duration;
    this.compositions = compositions;
    this.duration = this.mainComposition.duration;
    this.implDuration = (this.mainComposition.duration * 1000) / this.mainComposition.frameRate;
    for (const composition of compositions) {
      if (composition.type() !== CompositionType.Vector) {
        this.numLayers += 1;
        continue;
      }
      for (const layer of (composition as VectorComposition).layers) {
        if (layer.type() === LayerType.PreCompose) {
          continue;
        }
        this.numLayers += 1;
      }
    }
    this.tagLevel = tagLevel;
  }

  public getVideoSequence() {
    const compositionType = this.mainComposition.type();
    if (compositionType === CompositionType.Video) {
      return getVideoSequenceFromVideoComposition(this.mainComposition as VideoComposition);
    } else if (compositionType === CompositionType.Vector) {
      return getVideoSequenceFromVectorComposition(this.mainComposition as VectorComposition);
    }
  }
}

const getVideoSequenceFromVideoComposition = (videoComposition: VideoComposition) => {
  if (!videoComposition.sequences || videoComposition.sequences.length === 0) {
    throw new Error('PAGFile has no BMP video sequence!');
  }
  return videoComposition.sequences[videoComposition.sequences.length - 1];
};

const getVideoSequenceFromVectorComposition = (vectorComposition: VectorComposition) => {
  const videoCompositions = getVideoComposition(vectorComposition);
  if (videoCompositions.length > 1) throw new Error('PAGFile has more than one BMP video sequence!');
  if (videoCompositions.length < 1) throw new Error('PAGFile has no BMP video sequence!');
  const videoComposition = videoCompositions[0];
  return getVideoSequenceFromVideoComposition(videoComposition);
};

const getVideoComposition = (vectorComposition: VectorComposition) => {
  const videoCompositions: VideoComposition[] = [];
  vectorComposition.layers.forEach((layer) => {
    if (layer.type() !== LayerType.PreCompose) return;
    const { composition } = layer as PreComposeLayer;
    if (composition?.type() === CompositionType.Video) {
      videoCompositions.push(composition as VideoComposition);
      return;
    }
    if (composition?.type() === CompositionType.Vector) {
      videoCompositions.push(...getVideoComposition(composition as VectorComposition));
    }
  });
  return videoCompositions;
};
