import { getH264Frames } from '../h264/h264-parser';
import { H264Remuxer } from './h264-remuxer';

export const convertMp4 = (
  h264Frames: Uint8Array[],
  h264Headers: Uint8Array[],
  width: number,
  height: number,
  frameRate: number,
  ptsList: number[],
) => {
  const frames = getH264Frames([...h264Headers, ...h264Frames]);
  const remuxer = H264Remuxer.remux(frames, h264Headers, width, height, frameRate, ptsList);
  return remuxer.convertMp4();
};
