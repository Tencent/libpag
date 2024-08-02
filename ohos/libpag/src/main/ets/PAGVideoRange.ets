/**
 * Represents a time range from the content of PAGImageLayer.
 */

export interface  PAGVideoRange {
  /**
   * The start time of the source video, in microseconds.
   */
  startTime: number;
  /**
   * The end time of the source video (not included), in microseconds.
   */
  endTime: number;
  /**
   * The duration for playing after applying speed.
   */
  playDuration: number;
  /**
   * Indicates whether the video should play backward.
   */
  reversed: boolean;
}