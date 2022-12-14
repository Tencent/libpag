/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

import org.extra.tools.LibraryLoadUtils;

public class PAGDecoder {
    private int width;
    private int height;
    private int numFrames;
    private PAGSurface pagSurface;
    private PAGPlayer pagPlayer;

    /**
     * Make a decoder from pagComposition.
     */
    public static PAGDecoder Make(PAGComposition pagComposition) {
        return Make(pagComposition, 1.0f);
    }

    /**
     * Make a decoder from pagComposition.
     * The size of decoder will be scaled.
     */
    public static PAGDecoder Make(PAGComposition pagComposition, float scale) {
        if (scale <= 0) {
            scale = 1.0f;
        }
        PAGDecoder pagDecoder = new PAGDecoder();
        pagDecoder.width = (int) (pagComposition.width() * scale);
        pagDecoder.height = (int) (pagComposition.height() * scale);
        pagDecoder.numFrames =
                (int) (pagComposition.duration() * pagComposition.frameRate() / 1000000);
        pagDecoder.pagSurface = createSurface(pagDecoder.width, pagDecoder.height);
        if (pagDecoder.pagSurface == null) {
            return null;
        }
        pagDecoder.pagPlayer = new PAGPlayer();
        pagDecoder.pagPlayer.setSurface(pagDecoder.pagSurface);
        pagDecoder.pagPlayer.setComposition(pagComposition);
        return pagDecoder;
    }

    /**
     * Returns the width of the decoder.
     */
    public int width() {
        return width;
    }

    /**
     * Returns the height of the decoder.
     */
    public int height() {
        return height;
    }

    /**
     * Returns the number of animated frames.
     */
    public int numFrames() {
        return numFrames;
    }

    /**
     * Returns the frame image from a specified index.
     */
    public Bitmap frameAtIndex(int index) {
        if (index < 0 || index >= numFrames) {
            return null;
        }
        float progress = (index * 1.0f + 0.1f) / numFrames;
        pagPlayer.setProgress(progress);
        pagPlayer.flush();
        return pagSurface.makeSnapshot();
    }

    private static PAGSurface createSurface(int width, int height) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && false) {
            HardwareBuffer hardwareBuffer = HardwareBuffer.create(width, height,
                    HardwareBuffer.RGBA_8888
                    , 1,
                    HardwareBuffer.USAGE_CPU_READ_OFTEN | HardwareBuffer.USAGE_CPU_WRITE_OFTEN |
                            HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE | HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
            return PAGSurface.FromHardwareBuffer(hardwareBuffer);
        } else {
            return PAGSurface.FromSize(width, height);
        }
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
