package org.libpag;

import static android.os.Environment.isExternalStorageRemovable;

import android.content.Context;
import android.os.Environment;

import org.extra.tools.LibraryLoadUtils;

import java.io.File;

/**
 * Defines methods to manage the disk cache capabilities.
 */
public class PAGDiskCache {
    /**
     * Returns the size limit of the disk cache in bytes. The default value is 1 GB.
     */
    public static native long MaxDiskSize();

    /**
     * Sets the size limit of the disk cache in bytes, which will triggers the cache cleanup if the
     * disk usage exceeds the limit. The opened files are not removed immediately, even if their
     * disk usage exceeds the limit, and they will be rechecked after they are closed.
     */
    public static native void SetMaxDiskSize(long size);

    /**
     * Removes all cached files from the disk. All the opened files will be also removed after they
     * are closed.
     */
    public static native void RemoveAll();

    private static String GetCacheDir() {
        Context context = LibraryLoadUtils.getAppContext();
        String cacheDir = Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState()) ||
                !isExternalStorageRemovable() ? context.getExternalCacheDir().getPath() : context.getCacheDir().getPath();
        return cacheDir + File.separator + "libpag";
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
