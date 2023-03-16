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

package org.extra.tools;

import android.graphics.Bitmap;
import android.graphics.ColorSpace;
import android.hardware.HardwareBuffer;
import android.os.Build;

import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Queue;

public class BitmapPool {
    int maxSize = 1;
    int currentSize = 0;
    HashMap<String, Queue<BitmapResource>> bitmapHashMap = new HashMap<>();

    public static BitmapPool Make(int maxSize) {
        BitmapPool pool = new BitmapPool();
        pool.maxSize = maxSize;
        return pool;
    }

    public synchronized void clear() {
        bitmapHashMap.clear();
        currentSize = 0;
    }

    public synchronized void put(BitmapResource bitmap) {
        if (bitmap == null || bitmap.get() == null || bitmap.get().isRecycled()) {
            return;
        }
        if (currentSize >= maxSize) {
            return;
        }
        String key = bitmap.get().getWidth() + "_" + bitmap.get().getHeight();
        Queue<BitmapResource> bitmaps = bitmapHashMap.get(key);
        if (bitmaps == null) {
            bitmaps = new ArrayDeque();
            bitmapHashMap.put(key, bitmaps);
        }
        currentSize++;
        bitmaps.add(bitmap);
    }

    public synchronized BitmapResource get(int width, int height) {
        String key = width + "_" + height;
        if (bitmapHashMap.get(key) == null || bitmapHashMap.get(key).isEmpty()) {
            BitmapResource resource = new BitmapResource();
            resource.bitmapPool = this;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                HardwareBuffer hardwareBuffer = HardwareBuffer.create(width, height,
                        HardwareBuffer.RGBA_8888, 1,
                        HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE | HardwareBuffer.USAGE_CPU_READ_OFTEN | HardwareBuffer.USAGE_CPU_WRITE_OFTEN);
                resource.bitmap = Bitmap.wrapHardwareBuffer(hardwareBuffer,
                        ColorSpace.get(ColorSpace.Named.SRGB));

            } else {
                resource.bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            }
            return resource;
        }
        currentSize--;
        return bitmapHashMap.get(key).poll();
    }

    public static class BitmapResource {
        private int acquired;
        private Bitmap bitmap;
        private BitmapPool bitmapPool;

        public void acquire() {
            synchronized (this) {
                ++acquired;
            }
        }

        public boolean release() {
            synchronized (this) {
                if (--acquired <= 0) {
                    acquired = 0;
                    bitmapPool.put(this);
                    return true;
                }
            }
            return false;
        }

        public Bitmap get() {
            return bitmap;
        }

    }
}
