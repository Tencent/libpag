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

    /**
     * Create PAGDecoder from specified PAGComposition and size.
     */
    public static PAGDecoder Make(PAGComposition pagComposition, int width,
                                  int height) {
        long handler = MakeNative(pagComposition, width, height);
        return handler <= 0 ? null : new PAGDecoder(handler);
    }

    /**
     * Sets the progress of decoder, the value ranges from 0.0 to 1.0.
     * Decoder will keep return the same bitmap object.
     * But if returned Bitmap is recycled by caller, decoder will create a new bitmap object.
     */
    public native Bitmap decode(double progress);


    private static native long MakeNative(PAGComposition pagComposition, int width,
                                          int height);

    private PAGDecoder(long nativeHandler) {
        this.nativeHandler = nativeHandler;
    }

    long nativeHandler;

    private native void nativeFinalize();

    private static native void nativeInit();

    @Override
    protected void finalize() {
        nativeFinalize();
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
