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

import android.graphics.Bitmap;
import android.hardware.HardwareBuffer;
import android.os.Build;
import android.util.Pair;

import org.extra.tools.LibraryLoadUtils;

/**
 * PAGDecoder provides a utility to read image frames directly from a PAGComposition, and caches the
 * image frames as a sequence file on the disk, which may significantly speed up the reading process
 * depending on the complexity of the PAG files. You can use the PAGDiskCache::SetMaxDiskSize()
 * method to manage the cache limit of the disk usage.
 */
public class PAGDecoder {
    /**
     * Creates a new PAGDecoder with the specified PAGComposition, using the composition's frame
     * rate and size. Please only keep an external reference to the PAGComposition if you need to
     * modify it in the feature. Otherwise, the internal composition will not be released
     * automatically after the associated disk cache is complete, which may cost more memory than
     * necessary. Returns nil if the composition is nil. Note that the returned PAGDecoder may
     * become invalid if the associated PAGComposition is added to a PAGPlayer or another
     * PAGDecoder.
     */
    public static PAGDecoder Make(PAGComposition pagComposition) {
        return Make(pagComposition, pagComposition.frameRate(), 1.0f);
    }

    /**
     * Creates a PAGDecoder with a PAGComposition, a frame rate limit, and a scale factor for the
     * decoded image size. Please only keep an external reference to the PAGComposition if you need
     * to modify it in the feature. Otherwise, the internal composition will not be released
     * automatically after the associated disk cache is complete, which may cost more memory than
     * necessary. Returns nullptr if the composition is nullptr. Note that the returned PAGDecoder
     * may become invalid if the associated PAGComposition is added to a PAGPlayer or another
     * PAGDecoder.
     */
    public static PAGDecoder Make(PAGComposition pagComposition, float maxFrameRate, float scale) {
        long nativeContext = MakeFrom(pagComposition, maxFrameRate, scale);
        if (nativeContext == 0) {
            return null;
        }
        return new PAGDecoder(nativeContext);
    }

    private static native long MakeFrom(PAGComposition pagComposition, float maxFrameRate, float scale);

    private PAGDecoder(long nativeContext) {
        this.nativeContext = nativeContext;
    }

    /**
     * Returns the width of decoded image frames.
     */
    public native int width();

    /**
     * Returns the height of decoded image frames.
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
     * Returns true if the frame at the given index has changed since the last copyFrameTo(),
     * readFrameTo(), or frameAtIndex() call. The caller should skip the corresponding call
     * if the frame has not changed.
     */
    public native boolean checkFrameChanged(int index);

    /**
     * Copies pixels of the image frame at the given index to the specified Bitmap. Returns false if
     * failed. Note that caller must ensure that the config of Bitmap stays the same throughout
     * every copying call. Otherwise, it may return false.
     */
    public native boolean copyFrameTo(Bitmap bitmap, int index);

    /**
     * Reads pixels of the image frame at the given index into the specified HardwareBuffer.
     * Returns false if failed. Reading image frames into HardwareBuffer usually has better
     * performance than reading into memory.
     */
    public native boolean readFrame(int index, HardwareBuffer hardwareBuffer);

    /**
     * Returns the image frame at the specified index. It's recommended to read the image frames in
     * forward order for better performance.
     */
    public Bitmap frameAtIndex(int index) {
        Pair<Bitmap, Object> pair = BitmapHelper.CreateBitmap(width(), height(), false);
        if (pair.first == null) {
            return null;
        }
        boolean success;
        if (pair.second != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            success = readFrame(index, (HardwareBuffer) pair.second);
            ((HardwareBuffer) pair.second).close();
        } else {
            success = copyFrameTo(pair.first, index);
        }
        return success ? pair.first : null;
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
