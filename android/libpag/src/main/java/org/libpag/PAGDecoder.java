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

import org.extra.tools.LibraryLoadUtils;

public class PAGDecoder {
    private int _width;
    private int _height;
    private int _numFrames;
    private PAGSurface pagSurface;
    private PAGPlayer pagPlayer;
    private Bitmap lastFrameBitmap;

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
        if (pagComposition == null) {
            return null;
        }
        if (scale <= 0) {
            scale = 1.0f;
        }
        PAGDecoder pagDecoder = new PAGDecoder();
        pagDecoder._width = Math.round(pagComposition.width() * scale);
        pagDecoder._height = Math.round(pagComposition.height() * scale);
        pagDecoder._numFrames =
                (int) (pagComposition.duration() * pagComposition.frameRate() / 1000000);
        pagDecoder.pagSurface = PAGSurface.MakeOffscreen(pagDecoder._width, pagDecoder._height);
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
        return _width;
    }

    /**
     * Returns the height of the decoder.
     */
    public int height() {
        return _height;
    }

    /**
     * Returns the number of animated frames.
     */
    public int numFrames() {
        return _numFrames;
    }

    /**
     * Returns the frame image from a specified index.
     */
    public Bitmap frameAtIndex(int index) {
        if (index < 0 || index >= _numFrames) {
            return null;
        }
        float progress = (index * 1.0f + 0.1f) / _numFrames;
        pagPlayer.setProgress(progress);
        if (!pagPlayer.flush() && lastFrameBitmap != null && !lastFrameBitmap.isRecycled()) {
            return lastFrameBitmap;
        }
        lastFrameBitmap = pagSurface.makeSnapshot();
        return lastFrameBitmap;
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
