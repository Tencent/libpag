/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

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
     * Sets the disk cache directory. This should be called before any disk cache operations.
     * If the directory does not exist, it will be created automatically.
     * Note: Changing the cache directory after cache operations have started may cause
     * existing cached files to become inaccessible.
     * @param dir The absolute path of the cache directory. Pass null or empty string to use the
     * platform default cache directory.
     */
    public static native void SetCacheDir(String dir);

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

    static native byte[] ReadFile(String key);

    static native boolean WriteFile(String key, byte[] bytes);

    private static String GetCacheDir() {
        Context context = LibraryLoadUtils.getAppContext();
        if (context == null) {
            return "";
        }
        File cacheFile = null;
        if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState()) ||
                !isExternalStorageRemovable()) {
            cacheFile = context.getExternalCacheDir();
        }
        if (cacheFile == null) {
            cacheFile = context.getCacheDir();
        }
        return cacheFile == null ? "" : cacheFile.getPath();
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
