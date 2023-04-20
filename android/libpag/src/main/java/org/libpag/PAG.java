package org.libpag;

import org.extra.tools.LibraryLoadUtils;

public class PAG {
    /**
     * Get SDK version information.
     */
    public static native String SDKVersion();

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
