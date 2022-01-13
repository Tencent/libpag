package org.libpag;

/**
 * Represents a time range from the content of PAGImageLayer.
 */
public class PAGVideoRange {
    /**
     * The start time of the source video, in microseconds.
     */
    public long startTime = 0;
    /**
     * The end time of the source video (not included), in microseconds.
     */
    public long endTime = 0;
    /**
     * The duration for playing after applying speed.
     */
    public long playDuration = 0;

    /**
     * Indicates whether the video should play backward.
     */
    public boolean reversed = false;

    public PAGVideoRange(long startTime, long endTime, long playDuration, boolean reversed) {
        this.startTime = startTime;
        this.endTime = endTime;
        this.playDuration = playDuration;
        this.reversed = reversed;
    }
}
