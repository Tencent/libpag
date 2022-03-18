package org.libpag;

import org.extra.tools.LibraryLoadUtils;

public class PAGMovie extends PAGImage {

    private PAGMovie(long nativeContext) {
        super(nativeContext);
    }

    /**
     * Creates a PAGMovie object from a PAGComposition object, returns null if the composition is null.
     */
    public static PAGMovie FromComposition(PAGComposition composition) {
        long context = MakeFromComposition(composition);
        if (context == 0) {
            return null;
        }
        return new PAGMovie(context);
    }

    /**
     * Creates a PAGMovie object from a path of a video file, return null if the file does not exist or it's not a
     * valid video file.
     */
    public static PAGMovie FromVideoPath(String filePath) {
        long context = MakeFromVideoPath(filePath);
        if (context == 0) {
            return null;
        }
        return new PAGMovie(context);
    }

    /**
     * Creates a PAGMovie object from a specified range of a video file, return null if the file does not exist or
     * it's not a valid video file.
     *
     * @param startTime start time of the movie in microseconds.
     * @param duration duration of the movie in microseconds.
     */
    public static PAGMovie FromVideoPath(String filePath, long startTime, long duration) {
        long context = MakeFromVideoPath(filePath, startTime, duration);
        if (context == 0) {
            return null;
        }
        return new PAGMovie(context);
    }

    /**
     * Returns the duration of this movie in microseconds.
     */
    public native long duration();

    private static native long MakeFromComposition(PAGComposition composition);

    private static native long MakeFromVideoPath(String filePath);

    private static native long MakeFromVideoPath(String filePath, long startTime, long duration);

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
