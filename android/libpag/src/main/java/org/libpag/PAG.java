package org.libpag;

import static android.os.Environment.isExternalStorageRemovable;

import android.content.Context;
import android.os.Environment;

import org.extra.tools.LibraryLoadUtils;

import java.io.File;

public class PAG {
    /**
     * Get SDK version information.
     */
    public static native String SDKVersion();

    private static String GetCacheDir() {
        Context context = LibraryLoadUtils.getAppContext();
        String cacheDir = Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState()) ||
                !isExternalStorageRemovable() ? context.getExternalCacheDir().getPath() :
                context.getCacheDir().getPath();
        return cacheDir + File.separator + "libpag";
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
