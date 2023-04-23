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
import android.graphics.Matrix;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

import java.math.BigInteger;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;

class PAGImageViewHelper {
    private static PAGViewHandler g_PAGViewHandler = null;
    private static HandlerThread g_PAGViewThread = null;
    private static volatile int g_HandlerThreadCount = 0;
    private static final int MSG_HANDLER_THREAD_QUITE = 0;
    protected static final int MSG_FLUSH = 1;
    protected static final int MSG_CLOSE_CACHE = 2;
    protected static final int MSG_INIT_DECODER = 3;
    protected static final int MSG_REFRESH_DECODER = 4;

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

    protected static void RemoveMessage(int msgId, Object obj) {
        if (g_PAGViewHandler == null) {
            return;
        }
        g_PAGViewHandler.removeMessages(msgId, obj);
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

    private static double fmod(double a, double b) {
        int result = (int) Math.floor(a / b);
        return a - result * b;
    }

    protected static int ProgressToFrame(double progress, int totalFrames) {
        if (totalFrames <= 1) {
            return 0;
        }
        double percent = fmod(progress, 1.0);
        if (percent <= 0 && progress != 0) {
            percent += 1.0;
        }
        // 'progress' ranges in [0, 1], but 'frame' ranges in [frame, frame+1), so the last frame needs
        // special handling.
        int currentFrame = (int) Math.floor(percent * totalFrames);
        return currentFrame == totalFrames ? totalFrames - 1 : currentFrame;
    }

    protected static double FrameToProgress(int currentFrame, int totalFrames) {
        if (totalFrames <= 1 || currentFrame < 0) {
            return 0;
        }
        if (currentFrame >= totalFrames - 1) {
            return 1;
        }
        return (currentFrame * 1.0 + 0.1) / totalFrames;
    }


    protected static class CacheInfo {
        static CacheInfo Make(PAGImageView pagImageView,
                              String keyPrefix,
                              int frame,
                              Bitmap bitmap) {
            CacheInfo cacheInfo = new CacheInfo();
            cacheInfo.pagImageView = pagImageView;
            cacheInfo.keyPrefix = keyPrefix;
            cacheInfo.frame = frame;
            cacheInfo.bitmap = bitmap;
            return cacheInfo;
        }

        PAGImageView pagImageView;
        String keyPrefix;
        int frame;
        Bitmap bitmap;
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
                case MSG_INIT_DECODER: {
                    PAGImageView imageView = (PAGImageView) msg.obj;
                    imageView.initDecoderInfo();
                    break;
                }
                case MSG_FLUSH: {
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
                }
                case MSG_CLOSE_CACHE: {
                    PAGImageView imageView = (PAGImageView) msg.obj;
                    if (imageView != null) {
                        if (imageView.decoderInfo != null) {
                            imageView.decoderInfo.reset();
                        }
                    }
                    break;
                }
                case MSG_REFRESH_DECODER: {
                    PAGImageView imageView = (PAGImageView) msg.obj;
                    if (imageView != null) {
                        imageView.refreshDecodeInfo();
                    }
                    break;
                }
                case MSG_HANDLER_THREAD_QUITE: {
                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                        @Override
                        public void run() {
                            HandlerThreadQuit();
                        }
                    });
                    break;
                }
                default:
                    break;
            }
        }
    }

    static class DecoderInfo {
        int _width;
        int _height;
        float realFrameRate;
        int numFrames;
        long duration;
        PAGDecoder _pagDecoder;

        boolean isValid() {
            return _width > 0 && _height > 0;
        }

        boolean initDecoder(PAGComposition composition,
                            int width, int height, float maxFrameRate, boolean usdDiskCache) {
            if (composition == null || width == 0) {
                return false;
            }
            float maxScale = Math.max(width * 1.0f / composition.width(),
                    height * 1.0f / composition.height());
            if (maxFrameRate <= 0) {
                realFrameRate = composition.frameRate();
            } else {
                realFrameRate = Math.min(composition.frameRate(), maxFrameRate);
            }
            _pagDecoder = PAGDecoder.Make(composition, realFrameRate, maxScale, usdDiskCache);
            _width = _pagDecoder.width();
            _height = _pagDecoder.height();
            numFrames = _pagDecoder.numFrames();
            duration = composition.duration();
            return true;
        }

        void releaseDecoder() {
            if (_pagDecoder != null) {
                _pagDecoder.release();
            }
            _pagDecoder = null;
        }

        void reset() {
            releaseDecoder();
            _width = 0;
            _height = 0;
            realFrameRate = 0;
            numFrames = 0;
            duration = 0;
        }
    }
}
