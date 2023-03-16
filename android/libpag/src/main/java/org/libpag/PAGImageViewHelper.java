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

import android.content.Context;
import android.graphics.Matrix;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

import org.extra.tools.BitmapPool;

import java.math.BigInteger;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;

class PAGImageViewHelper {
    private static PAGViewHandler g_PAGViewHandler = null;
    private static HandlerThread g_PAGViewThread = null;
    private static volatile int g_HandlerThreadCount = 0;
    private static final int MSG_FLUSH = 0;
    protected static final int MSG_INIT_CACHE = 1;
    private static final int MSG_HANDLER_THREAD_QUITE = 2;

    protected static synchronized void StartHandlerThread() {
        g_HandlerThreadCount++;
        if (g_PAGViewThread == null) {
            g_PAGViewThread = new HandlerThread("pagImageView-renderer");
            g_PAGViewThread.start();
        }
        if (g_PAGViewHandler == null) {
            g_PAGViewHandler = new PAGViewHandler(g_PAGViewThread.getLooper());
        }
    }

    protected static synchronized void DestroyHandlerThread() {
        g_HandlerThreadCount--;
        if (g_HandlerThreadCount != 0) {
            return;
        }
        if (g_PAGViewHandler == null || g_PAGViewThread == null) {
            return;
        }
        if (!g_PAGViewThread.isAlive()) {
            return;
        }
        SendMessage(MSG_HANDLER_THREAD_QUITE, null);
    }

    protected static void SendMessage(int msgId, Object obj) {
        if (g_PAGViewHandler == null) {
            return;
        }
        Message message = g_PAGViewHandler.obtainMessage();
        message.arg1 = msgId;
        if (obj != null) {
            message.obj = obj;
        }
        g_PAGViewHandler.sendMessage(message);
    }

    protected static void NeedsUpdateView(PAGImageView pagView) {
        if (g_PAGViewHandler == null) {
            return;
        }
        g_PAGViewHandler.needsUpdateView(pagView);
    }

    private static void HandlerThreadQuit() {
        if (g_HandlerThreadCount != 0) {
            return;
        }
        if (g_PAGViewHandler == null || g_PAGViewThread == null) {
            return;
        }
        if (!g_PAGViewThread.isAlive()) {
            return;
        }
        g_PAGViewHandler.removeCallbacksAndMessages(null);
        if (Build.VERSION.SDK_INT > 18) {
            g_PAGViewThread.quitSafely();
        } else {
            g_PAGViewThread.quit();
        }
        g_PAGViewThread = null;
        g_PAGViewHandler = null;
    }

    protected static Matrix ApplyScaleMode(int scaleMode, int sourceWidth, int sourceHeight,
                                           int targetWidth, int targetHeight) {
        Matrix matrix = new Matrix();
        if (scaleMode == PAGScaleMode.None || sourceWidth <= 0 || sourceHeight <= 0 ||
                targetWidth <= 0 || targetHeight <= 0) {
            return matrix;
        }
        float scaleX = targetWidth * 1.0f / sourceWidth;
        float scaleY = targetHeight * 1.0f / sourceHeight;
        switch (scaleMode) {
            case PAGScaleMode.Stretch: {
                matrix.setScale(scaleX, scaleY);
            }
            break;
            case PAGScaleMode.Zoom: {
                float scale = Math.max(scaleX, scaleY);
                matrix.setScale(scale, scale);
                if (scaleX > scaleY) {
                    matrix.postTranslate(0, (targetHeight - sourceHeight * scale) * 0.5f);
                } else {
                    matrix.postTranslate((targetWidth - sourceWidth * scale) * 0.5f, 0);
                }
            }
            break;
            default: {
                float scale = Math.min(scaleX, scaleY);
                matrix.setScale(scale, scale);
                if (scaleX < scaleY) {
                    matrix.postTranslate(0, (targetHeight - sourceHeight * scale) * 0.5f);
                } else {
                    matrix.postTranslate((targetWidth - sourceWidth * scale) * 0.5f, 0);
                }
            }
            break;
        }
        return matrix;
    }

    protected static String getMD5(String str) {
        if (str == null) {
            return null;
        }
        byte[] digest;
        try {
            MessageDigest md5 = MessageDigest.getInstance("md5");
            digest = md5.digest(str.getBytes("utf-8"));
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
        String md5Str = new BigInteger(1, digest).toString(16);
        return md5Str;
    }

    protected static class CacheInfo {
        static CacheInfo Make(PAGImageView pagImageView,
                              String keyPrefix,
                              int frame,
                              BitmapPool.BitmapResource bitmap) {
            CacheInfo cacheInfo = new CacheInfo();
            cacheInfo.pagImageView = pagImageView;
            cacheInfo.keyPrefix = keyPrefix;
            cacheInfo.frame = frame;
            cacheInfo.bitmap = bitmap;
            return cacheInfo;
        }

        PAGImageView pagImageView;
        String keyPrefix;
        String key;
        int frame;
        BitmapPool.BitmapResource bitmap;
    }

    static class PAGViewHandler extends Handler {

        private final Object lock = new Object();
        private List<PAGImageView> needsUpdateViews = new ArrayList<>();

        PAGViewHandler(Looper looper) {
            super(looper);
        }

        void needsUpdateView(PAGImageView pagView) {
            synchronized (lock) {
                if (needsUpdateViews.isEmpty()) {
                    Message message = obtainMessage();
                    message.arg1 = MSG_FLUSH;
                    sendMessage(message);
                }
                needsUpdateViews.add(pagView);
            }
        }

        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            switch (msg.arg1) {
                case MSG_INIT_CACHE:
                    PAGImageView imageView = (PAGImageView) msg.obj;
                    if (!imageView.decoderInfo.isValid()) {
                        return;
                    }
                    imageView.lastKeyItem = imageView.fetchKeyFrame();
                    if (imageView.cacheItem != null || imageView.lastKeyItem == null) {
                        return;
                    }
                    imageView.cacheItem = imageView.cacheManager.getOrCreate(imageView.lastKeyItem.keyPrefixMD5,
                            imageView.decoderInfo._width, imageView.decoderInfo._height,
                            imageView.decoderInfo.numFrames);
                    break;
                case MSG_FLUSH:
                    List<PAGImageView> tempList;
                    synchronized (lock) {
                        tempList = new ArrayList<>(needsUpdateViews);
                        needsUpdateViews.clear();
                    }
                    List<PAGImageView> flushedViews = new ArrayList<>();
                    for (Object object : tempList) {
                        if (!(object instanceof PAGImageView)) {
                            continue;
                        }
                        PAGImageView pagView = (PAGImageView) object;
                        if (flushedViews.contains(pagView)) {
                            continue;
                        }
                        pagView.updateView();
                        flushedViews.add(pagView);
                    }
                    break;
                case MSG_HANDLER_THREAD_QUITE:
                    CacheManager manager = CacheManager.Get(null);
                    if (manager != null) {
                        manager.autoClean();
                    }
                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                        @Override
                        public void run() {
                            HandlerThreadQuit();
                        }
                    });
                    break;
                default:
                    break;
            }
        }
    }

    static class DecoderInfo {
        int _width;
        int _height;
        float realFrameRate;
        float scale;
        int numFrames;
        long duration;
        PAGDecoder _pagDecoder;

        boolean isValid() {
            return _width > 0;
        }

        boolean initDecoder(Context context, PAGComposition _composition,
                            String pagFilePath, int width, int height, float maxFrameRate) {
            if (_composition == null && pagFilePath == null || width == 0) {
                return false;
            }
            PAGComposition composition = _composition;
            if (composition == null) {
                if (pagFilePath.startsWith("assets://")) {
                    composition = PAGFile.Load(context.getAssets(), pagFilePath.substring(9));
                } else {
                    composition = PAGFile.Load(pagFilePath);
                }
            }
            if (composition == null) {
                return false;
            }
            float maxScale = Math.max(width * 1.0f / composition.width(),
                    height * 1.0f / composition.height());
            _pagDecoder = PAGDecoder.Make(composition, maxFrameRate, maxScale);
            realFrameRate = composition.frameRate() > maxFrameRate ? maxFrameRate :
                    composition.frameRate();
            _width = _pagDecoder.width();
            _height = _pagDecoder.height();
            numFrames = _pagDecoder.numFrames();
            duration = composition.duration();
            return true;
        }

        void release() {
            if (_pagDecoder != null) {
                _pagDecoder.release();
            }
            _pagDecoder = null;
        }
    }
}
