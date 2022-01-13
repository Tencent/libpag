package org.libpag;

public class PAGMarker {
    public long mStartTime;
    public long mDuration;
    public String mComment;

    public PAGMarker(long startTime, long duration, String comment) {
        mStartTime = startTime;
        mDuration = duration;
        mComment = comment;
    }
}
