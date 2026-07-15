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
import android.graphics.Matrix;
import android.hardware.HardwareBuffer;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.locks.ReentrantLock;

class PAGImageViewHelper {

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

    static class DecoderInfo {
        volatile int _width;
        volatile int _height;
        volatile long duration;
        private PAGDecoder _pagDecoder;
        private final ReentrantLock locker = new ReentrantLock();
        // resetToken is bumped by every reset(); decoderToken records the resetToken value captured
        // when the current decoder was built. isValid() treats the decoder as invalid once a reset()
        // has happened since it was built, so a lock-free reset() that races an in-flight initDecoder()
        // always wins without having to block on the lock.
        private final AtomicInteger resetToken = new AtomicInteger(0);
        private volatile int decoderToken = 0;

        void lock() {
            locker.lock();
        }

        void unlock() {
            locker.unlock();
        }

        boolean isValid() {
            // Lock-free: all fields are volatile or atomic, so this read is safe without holding
            // the lock and avoids blocking the main thread when the worker is stuck in readFrame().
            return _width > 0 && _height > 0 && decoderToken == resetToken.get();
        }

        boolean hasPAGDecoder() {
            locker.lock();
            try {
                return _pagDecoder != null;
            } finally {
                locker.unlock();
            }
        }

        boolean checkFrameChanged(int currentFrame) {
            locker.lock();
            try {
                return _pagDecoder != null && _pagDecoder.checkFrameChanged(currentFrame);
            } finally {
                locker.unlock();
            }
        }

        boolean readFrame(int currentFrame, HardwareBuffer hardwareBuffer) {
            locker.lock();
            try {
                return _pagDecoder != null && hardwareBuffer != null && _pagDecoder.readFrame(
                        currentFrame, hardwareBuffer);
            } finally {
                locker.unlock();
            }
        }

        boolean copyFrameTo(Bitmap bitmap, int currentFrame) {
            locker.lock();
            try {
                return _pagDecoder != null && bitmap != null && _pagDecoder.copyFrameTo(bitmap,
                        currentFrame);
            } finally {
                locker.unlock();
            }
        }

        boolean initDecoder(PAGComposition composition, int width, int height,
                            float maxFrameRate) {
            locker.lock();
            try {
                if (composition == null || width <= 0 || height <= 0 || maxFrameRate <= 0) {
                    return false;
                }
                // Release the previous decoder before creating a new one to avoid leaking native
                // resources when the DecoderInfo is reused, for example on a detach-then-reattach in
                // a RecyclerView, or when reset() skipped the release on a busy lock.
                releaseDecoder();
                // Capture the reset token before the slow Make() call. If a reset() bumps the token
                // while we build, decoderToken will no longer match and isValid() reports this decoder
                // as stale, so the racing reset() wins instead of being silently overwritten.
                int token = resetToken.get();
                float scaleFactor;
                if (composition.width() >= composition.height()) {
                    scaleFactor = width * 1.0f / composition.width();
                } else {
                    scaleFactor = height * 1.0f / composition.height();
                }
                PAGDecoder decoder = PAGDecoder.Make(composition, maxFrameRate, scaleFactor);
                if (decoder == null) {
                    return false;
                }
                // Re-check the reset token after the slow Make() call. If reset() was called during
                // Make(), the token will have been bumped, meaning this decoder is already stale and
                // should not overwrite the reset state.
                if (resetToken.get() != token) {
                    decoder.release();
                    return false;
                }
                _pagDecoder = decoder;
                _width = decoder.width();
                _height = decoder.height();
                duration = composition.duration();
                decoderToken = token;
                return true;
            } finally {
                locker.unlock();
            }
        }

        void releaseDecoder() {
            locker.lock();
            try {
                if (_pagDecoder != null) {
                    _pagDecoder.release();
                    _pagDecoder = null;
                }
            } finally {
                locker.unlock();
            }
        }

        void reset() {
            // tryLock avoids an ANR: a blocking reset() on the main thread would stall if the worker
            // thread is stuck in readFrame() (e.g. the hardware decoder hangs). Bump the reset token
            // first so that any initDecoder() building concurrently on the worker thread is marked
            // stale by isValid(); this wins the race even though the field writes below are lock-free.
            // On a failed lock, skip the native release but still clear the size fields; the stale
            // decoder is reclaimed by initDecoder() on reuse or PAGDecoder.finalize() on GC.
            resetToken.incrementAndGet();
            if (!locker.tryLock()) {
                _width = 0;
                _height = 0;
                duration = 0;
                return;
            }
            try {
                releaseDecoder();
                _width = 0;
                _height = 0;
                duration = 0;
            } finally {
                locker.unlock();
            }
        }

        int numFrames() {
            locker.lock();
            try {
                return _pagDecoder == null ? 0 : _pagDecoder.numFrames();
            } finally {
                locker.unlock();
            }
        }
    }
}
