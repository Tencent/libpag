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
     * Returns the duration of this movie in microseconds.
     */
    public native long duration();

    private static native long MakeFromComposition(PAGComposition composition);

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
