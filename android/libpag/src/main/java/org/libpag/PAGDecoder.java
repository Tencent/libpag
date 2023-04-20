/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

import android.graphics.Bitmap;

import org.extra.tools.LibraryLoadUtils;

/**
 * PAGDecoder provides a utility to read image frames directly from a PAGComposition.
 */
public class PAGDecoder {
    /**
     * Creates a new PAGDecoder with the specified PAGComposition, using the composition's frame
     * rate and size, and set the useDiskCache to true. You can use PAGDiskCache::SetMaxDiskSize()
     * to manage the cache limit of the disk usage. Returns null if the composition is null. Note
     * that the returned PAGDecoder may become invalid if the associated PAGComposition is added to
     * a PAGPlayer or another PAGDecoder.
     */
    public static PAGDecoder Make(PAGComposition pagComposition) {
        return Make(pagComposition, pagComposition.frameRate(), 1.0f);
    }

    /**
     * Creates a PAGDecoder with a PAGComposition, a frame rate limit, and a scale factor for the
     * decoded image size, and set the useDiskCache to true. You can use
     * PAGDiskCache::SetMaxDiskSize() to manage the cache limit of the disk usage. Returns null if
     * the composition is null. Note that the returned PAGDecoder may become invalid if the
     * associated PAGComposition is added to a PAGPlayer or another PAGDecoder.
     */
    public static PAGDecoder Make(PAGComposition pagComposition, float maxFrameRate, float scale) {
        return Make(pagComposition, maxFrameRate, scale, true);
    }

    /**
     * Creates a PAGDecoder with a PAGComposition, a frame rate limit, and a scale factor for the
     * decoded image size. If the useDiskCache is true, the returned PAGDecoder will cache image
     * frames as a sequence file on the disk, which may significantly speed up the reading process
     * depending on the complexity of the PAG files. And only keep an external reference to the
     * PAGComposition if you need to modify it in the feature. Otherwise, the internal composition
     * will not be released automatically after the associated disk cache is complete, which may
     * cost more memory than necessary. You can use the PAGDiskCache::SetMaxDiskSize() method
     * to manage the cache limit of the disk usage. Returns null if the composition is null. Note
     * that the returned PAGDecoder may become invalid if the associated PAGComposition is added to
     * a PAGPlayer or another PAGDecoder. And while the useDiskCache is true.
     */
    public static PAGDecoder Make(PAGComposition pagComposition, float maxFrameRate, float scale,
                                  boolean useDiskCache) {
        long nativeContext = MakeFrom(pagComposition, maxFrameRate, scale, useDiskCache);
        if (nativeContext == 0) {
            return null;
        }
        return new PAGDecoder(nativeContext);
    }

    private static native long MakeFrom(PAGComposition pagComposition, float maxFrameRate,
                                        float scale, boolean useDiskCache);

    private PAGDecoder(long nativeContext) {
        this.nativeContext = nativeContext;
    }

    /**
     * Returns the width of decoded image frames.
     */
    public native int width();

    /**
     * Returns the height of the decoder.
     */
    public native int height();

    /**
     * Returns the number of frames in the PAGDecoder. Note that the value may change if the
     * associated PAGComposition was modified.
     */
    public native int numFrames();

    /**
     * Returns the frame rate of decoded image frames. The value may change if the associated
     * PAGComposition was modified.
     */
    public native float frameRate();

    /**
     * Copies pixels of the image frame at the given index to the specified Bitmap. Returns false if
     * failed. Note that caller must ensure that the config of Bitmap stays the same throughout
     * every reading call. Otherwise, it may return false.
     */
    public native boolean copyFrameTo(Bitmap bitmap, int index);

    /**
     * Returns the image frame at the specified index. It's recommended to read the image frames in
     * forward order for better performance.
     */
    public Bitmap frameAtIndex(int index) {
        Bitmap bitmap = BitmapHelper.CreateBitmap(width(), height());
        if (bitmap == null) {
            return null;
        }
        if (copyFrameTo(bitmap, index)) {
            return bitmap;
        }
        return null;
    }

    /**
     * Free up resources used by the PAGDecoder instance immediately instead of relying on the
     * garbage collector to do this for you at some point in the future.
     */
    public void release() {
        nativeRelease();
    }

    protected void finalize() {
        nativeFinalize();
    }

    private native void nativeRelease();

    private native void nativeFinalize();

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }

    private long nativeContext = 0;
}
