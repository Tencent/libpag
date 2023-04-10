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

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.os.Build;
import android.os.Looper;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.animation.LinearInterpolator;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import org.libpag.PAGImageViewHelper.CacheInfo;
import org.libpag.PAGImageViewHelper.DecoderInfo;

public class PAGImageView extends View {
    private final static String TAG = "PAGImageView";
    private final static float DEFAULT_MAX_FRAMERATE = 30f;
    private ValueAnimator animator;
    private boolean isCacheAllFramesInMemory = false;
    private volatile boolean _isPlaying = false;
    private volatile Boolean _isAnimatorPreRunning = null;
    private volatile boolean progressExplicitlySet = true;
    private final Object updateTimeLock = new Object();
    private static final Object g_HandlerLock = new Object();
    private int _currentFrame;
    private float _maxFrameRate = 30;
    private PAGComposition _composition;
    private String _pagFilePath;
    private int _scaleMode = PAGScaleMode.LetterBox;
    private volatile Matrix _matrix;
    private ArrayList<WeakReference<Future>> saveCacheTasks = new ArrayList<>();
    private float renderScale = 1.0f;
    private AtomicBoolean freezeDraw = new AtomicBoolean(false);
    protected volatile CacheManager.CacheItem cacheItem;
    protected volatile KeyItem lastKeyItem;
    protected volatile DecoderInfo decoderInfo = new DecoderInfo();
    protected volatile CacheManager cacheManager;
    private volatile Bitmap renderBitmap;
    private ConcurrentHashMap<Integer, Bitmap> bitmapCache = new ConcurrentHashMap<>();
    protected static long g_MaxDiskCacheSize = 1 * 1024 * 1024 * 1024;

    public interface PAGImageViewListener {
        /**
         * Notifies the beginning of the animation.
         */
        void onAnimationStart(PAGImageView view);

        /**
         * Notifies the end of the animation.
         */
        void onAnimationEnd(PAGImageView view);

        /**
         * Notifies the cancellation of the animation.
         */
        void onAnimationCancel(PAGImageView view);

        /**
         * Notifies the repetition of the animation.
         */
        void onAnimationRepeat(PAGImageView view);

        /**
         * Notifies the frame updating of the animation.
         */
        void onAnimationUpdate(PAGImageView view);
    }


    private Matrix renderMatrix;

    /**
     * This value defines the scale factor for the size of the cached image frames, which ranges
     * from 0.0 to 1.0. A scale factor less than 1.0 may result in blurred output, but it can reduce
     * graphics memory usage, increasing the rendering performance. The default value is 1.0.
     */
    public float renderScale() {
        return renderScale;
    }

    /**
     * Sets the value of the renderScale property.
     */
    public void setRenderScale(float renderScale) {
        if (this.renderScale == renderScale) {
            return;
        }
        if (renderScale < 0.0f || renderScale > 1.0f) {
            renderScale = 1.0f;
        }
        this.renderScale = renderScale;
        width = (int) (viewWidth * renderScale);
        height = (int) (viewHeight * renderScale);
        refreshMatrixFromScaleMode();
        if (renderScale < 1.0f) {
            renderMatrix = new Matrix();
            renderMatrix.setScale(1 / renderScale, 1 / renderScale);
        }
    }

    /**
     * Returns the size limit of the disk cache in bytes.
     */
    public static long MaxDiskCache() {
        return g_MaxDiskCacheSize;
    }

    /**
     * Sets the size limit of the disk cache in bytes. The default disk cache limit is 1 GB.
     */
    public static void SetMaxDiskCache(long maxDiskCache) {
        g_MaxDiskCacheSize = maxDiskCache;
    }

    /**
     * If set to true, the PAGImageView loads all image frames into the memory, which will
     * significantly increase the rendering performance but may cost lots of additional memory. Set
     * it to true if you prefer rendering speed over memory usage. If set to false, the PAGImageView
     * loads only one image frame at a time into the memory. The default value is false.
     */
    public boolean cacheAllFramesInMemory() {
        return isCacheAllFramesInMemory;
    }

    /**
     * Sets the value of the cacheAllFramesInMemory property.
     */
    public void setCacheAllFramesInMemory(boolean enable) {
        isCacheAllFramesInMemory = enable;
    }

    /**
     * Returns the current frame index the PAGImageView is rendering.
     */
    public int currentFrame() {
        return _currentFrame;
    }

    /**
     * Sets the frame index for the PAGImageView to render.
     */
    public void setCurrentFrame(int currentFrame) {
        if (!decoderInfo.isValid() || currentFrame < 0 || currentFrame >= decoderInfo.numFrames || cacheItem == null) {
            return;
        }
        _currentFrame = currentFrame;
        float value = (float) (decoderInfo.duration * 0.001f * PAGImageViewHelper.FrameToProgress(_currentFrame, decoderInfo.numFrames));
        value = Math.max(0, Math.min(value, 1));
        currentPlayTime = (long) (value * animator.getDuration());
        synchronized (updateTimeLock) {
            animator.setCurrentPlayTime(currentPlayTime);
            progressExplicitlySet = true;
        }
        PAGImageViewHelper.NeedsUpdateView(this);
    }

    /**
     * Returns a bitmap capturing the contents of the current PAGImageView.
     */
    public Bitmap currentImage() {
        return renderBitmap;
    }

    /**
     * Returns the current PAGComposition in the PAGImageView. Returns nil if the internal
     * composition was loaded from a pag file path.
     */
    public PAGComposition getComposition() {
        return _composition;
    }

    /**
     * Sets a new PAGComposition to the PAGImageView with the maxFrameRate set to 30 fps. Note: If
     * the composition is already added to another PAGImageView, it will be removed from the
     * previous PAGImageView.
     */
    public void setComposition(PAGComposition newComposition) {
        setComposition(newComposition, DEFAULT_MAX_FRAMERATE);
    }

    /**
     * Sets a new PAGComposition and the maxFrameRate limit to the PAGImageView. Note: If the
     * composition is already added to another PAGImageView, it will be removed from the previous
     * PAGImageView.
     */
    public void setComposition(PAGComposition newComposition, float maxFrameRate) {
        if (newComposition == _composition) {
            return;
        }
        freezeDraw.set(true);
        _composition = newComposition;
        progressExplicitlySet = true;
        animator.setCurrentPlayTime(0);
        releaseAllTask();
        _matrix = null;
        PAGImageViewHelper.SendMessage(PAGImageViewHelper.MSG_REFRESH_DECODER, this);
        renderBitmap = null;
    }

    /**
     * Returns the file path set by the setPath() method.
     */
    public String getPath() {
        return _pagFilePath;
    }

    /**
     * Loads a pag file from the specified path with the maxFrameRate limit, returns false if the
     * file does not exist, or it is not a valid pag file.
     */
    public boolean setPath(String path, float maxFrameRate) {
        if (path == null) {
            return false;
        }
        if (path.equals(_pagFilePath) && _maxFrameRate == maxFrameRate && decoderInfo.isValid()) {
            return true;
        }
        freezeDraw.set(true);
        _pagFilePath = path;
        _maxFrameRate = maxFrameRate;
        progressExplicitlySet = true;
        animator.setCurrentPlayTime(0);
        releaseAllTask();
        _matrix = null;
        renderBitmap = null;
        PAGImageViewHelper.SendMessage(PAGImageViewHelper.MSG_REFRESH_DECODER, this);
        return true;
    }

    /**
     * Loads a pag file from the specified path, returns false if the file does not exist, or it is
     * not a valid pag file.
     */
    public boolean setPath(String path) {
        return setPath(path, DEFAULT_MAX_FRAMERATE);
    }

    /**
     * Sets the number of times the animation will repeat. The default is 1, which means the
     * animation will play only once. 0 means the animation will play infinity times.
     */
    public void setRepeatCount(int value) {
        if (value < 0) {
            value = 0;
        }
        animator.setRepeatCount(value - 1);
    }

    /**
     * Renders the current image frame immediately. Note that all the changes previously made to the
     * PAGImageView will only take effect after this method is called. If the play() method is
     * already called, there is no need to call it manually since it will be automatically called
     * every frame. Returns true if the content has changed.
     */
    public boolean flush() {
        if (!decoderInfo.isValid()) {
            return false;
        }
        synchronized (updateTimeLock) {
            if (progressExplicitlySet) {
                progressExplicitlySet = false;
                animator.setCurrentPlayTime((long) (decoderInfo.duration * 0.001f * PAGImageViewHelper.FrameToProgress(_currentFrame, decoderInfo.numFrames)));
            }
            int currentFrame = PAGImageViewHelper.ProgressToFrame(animator.getAnimatedFraction(), decoderInfo.numFrames);
            if (currentFrame == _currentFrame && !forceFlush) {
                return false;
            }
            forceFlush = false;
            _currentFrame = currentFrame;
            if (!handleFrame(_currentFrame)) {
                return false;
            }
            postInvalidate();
        }
        notifyAnimationUpdate();
        return true;
    }

    /**
     * Returns the current scale mode.
     */
    public int scaleMode() {
        return _scaleMode;
    }

    /**
     * Specifies the rule of how to scale the pag content to fit the PAGImageView's size. The
     * current matrix of the PAGImageView changes when this method is called.
     */
    public void setScaleMode(int scaleMode) {
        if (scaleMode == _scaleMode) {
            return;
        }
        _scaleMode = scaleMode;
        if (hasSize()) {
            refreshMatrixFromScaleMode();
            postInvalidate();
        } else {
            _matrix = null;
        }
    }

    /**
     * Returns a copy of the current matrix.
     */
    public Matrix matrix() {
        return _matrix;
    }

    /**
     * Sets the transformation which will be applied to the composition. The scaleMode property
     * will be set to PAGScaleMode::None when this method is called.
     */
    public void setMatrix(Matrix matrix) {
        _matrix = matrix;
        _scaleMode = PAGScaleMode.None;
        if (hasSize()) {
            postInvalidate();
        }
    }

    @Override
    public void onVisibilityAggregated(boolean isVisible) {
        super.onVisibilityAggregated(isVisible);
        if (preAggregatedVisible == isVisible) {
            return;
        }
        preAggregatedVisible = isVisible;
        Log.i(TAG, "onVisibilityAggregated isVisible=" + isVisible);
        if (isVisible) {
            resumeAnimator();
        } else {
            pauseAnimator();
        }
    }

    public PAGImageView(Context context) {
        super(context);
        init();
    }

    public PAGImageView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public PAGImageView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private float getAnimationScale(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return Settings.Global.getFloat(context.getContentResolver(), Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f);
        } else {
            // noinspection deprecation
            return Settings.System.getFloat(context.getContentResolver(), Settings.System.ANIMATOR_DURATION_SCALE, 1.0f);
        }
    }

    protected void refreshDecodeInfo() {
        if (decoderInfo != null) {
            decoderInfo.reset();
        }
        releaseCurrentDiskCache();
        lastKeyItem = null;
        initDecoderInfo();
    }

    private static ThreadPoolExecutor saveCacheExecutors;

    private static void InitCacheExecutors() {
        if (saveCacheExecutors == null) {
            synchronized (PAGImageView.class) {
                if (saveCacheExecutors == null) {
                    saveCacheExecutors = new ThreadPoolExecutor(1, 1, 2, TimeUnit.SECONDS, new LinkedBlockingQueue<Runnable>());
                    saveCacheExecutors.allowCoreThreadTimeOut(true);
                }
            }
        }
    }

    private volatile long currentPlayTime;

    private final ValueAnimator.AnimatorUpdateListener mAnimatorUpdateListener = new ValueAnimator.AnimatorUpdateListener() {
        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            if (!decoderInfo.isValid() || cacheItem == null) {
                return;
            }
            PAGImageView.this.currentPlayTime = animation.getCurrentPlayTime();
            PAGImageViewHelper.NeedsUpdateView(PAGImageView.this);
        }
    };

    private ArrayList<PAGImageViewListener> mViewListeners = new ArrayList<>();

    private final AnimatorListenerAdapter mAnimatorListenerAdapter = new AnimatorListenerAdapter() {
        @Override
        public void onAnimationStart(Animator animator) {
            super.onAnimationStart(animator);
            ArrayList<PAGImageViewListener> arrayList;
            synchronized (PAGImageView.this) {
                arrayList = new ArrayList<>(mViewListeners);
            }
            for (PAGImageViewListener listener : arrayList) {
                listener.onAnimationStart(PAGImageView.this);
            }
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            super.onAnimationEnd(animation);
            // Align with iOS platform, avoid triggering this method when stopping
            int repeatCount = ((ValueAnimator) animation).getRepeatCount();
            if (repeatCount >= 0 && (animation.getDuration() > 0) && (currentPlayTime / animation.getDuration() > repeatCount)) {
                notifyEnd();
            }
        }

        @Override
        public void onAnimationCancel(Animator animator) {
            super.onAnimationCancel(animator);
            ArrayList<PAGImageViewListener> arrayList;
            synchronized (PAGImageView.this) {
                arrayList = new ArrayList<>(mViewListeners);
            }
            for (PAGImageViewListener listener : arrayList) {
                listener.onAnimationCancel(PAGImageView.this);
            }
        }

        @Override
        public void onAnimationRepeat(Animator animator) {
            super.onAnimationRepeat(animator);
            ArrayList<PAGImageViewListener> arrayList;
            synchronized (PAGImageView.this) {
                arrayList = new ArrayList<>(mViewListeners);
            }
            for (PAGImageViewListener listener : arrayList) {
                listener.onAnimationRepeat(PAGImageView.this);
            }
        }
    };

    private void notifyEnd() {
        _isPlaying = false;
        ArrayList<PAGImageViewListener> arrayList;
        synchronized (this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGImageViewListener listener : arrayList) {
            listener.onAnimationEnd(this);
        }
    }

    private volatile int width, height;
    private volatile int viewWidth, viewHeight;

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        viewWidth = w;
        viewHeight = h;
        width = (int) (renderScale * w);
        height = (int) (renderScale * h);
        PAGImageViewHelper.SendMessage(PAGImageViewHelper.MSG_INIT_DECODER, this);
        resumeAnimator();
    }

    protected void initDecoderInfo() {
        if (!decoderInfo.isValid()) {
            if (decoderInfo.initDecoder(getContext(), _composition, _pagFilePath, width, height, _maxFrameRate)) {
                animator.setDuration(decoderInfo.duration / 1000);
                if (!decoderInfo.isValid()) {
                    return;
                }
                lastKeyItem = fetchKeyFrame();
                if (cacheItem != null || lastKeyItem == null) {
                    return;
                }
                cacheItem = cacheManager.getOrCreate(lastKeyItem.keyPrefixMD5,
                        decoderInfo._width, decoderInfo._height,
                        decoderInfo.numFrames);
            }
        } else {
            refreshMatrixFromScaleMode();
        }
        freezeDraw.set(false);
    }

    private float animationScale = 1.0f;

    private void init() {
        InitCacheExecutors();
        cacheManager = CacheManager.Get(getContext());
        animator = ValueAnimator.ofFloat(0.0f, 1.0f);
        animator.setRepeatCount(0);
        animator.setInterpolator(new LinearInterpolator());
        animator.addUpdateListener(mAnimatorUpdateListener);
        animationScale = getAnimationScale(getContext());
    }

    /**
     * Starts to play the animation.
     */
    public void play() {
        _isPlaying = true;
        _isAnimatorPreRunning = null;
        if (animator.getAnimatedFraction() == 1.0) {
            setCurrentFrame(0);
        }
        doPlay();
    }

    /**
     * Indicates whether this PAGImageView is playing.
     */
    public boolean isPlaying() {
        if (animator != null) {
            return animator.isRunning();
        }
        return false;
    }

    /**
     * Pauses the animation at the current playing position. Calling the play method can resume the
     * animation from the last paused playing position.
     */
    public void pause() {
        _isPlaying = false;
        _isAnimatorPreRunning = null;
        cancelAnimator();
    }

    /**
     * Adds a PAGViewListener to the set of listeners that are sent events through the life of an
     * animation, such as start, end repeat, and cancel.
     *
     * @param listener the PAGViewListener to be added to the current set of listeners for this
     *                 animation.
     */
    public void addListener(PAGImageViewListener listener) {
        synchronized (this) {
            mViewListeners.add(listener);
        }
    }

    /**
     * Removes the specified listener from the set of listeners.
     */
    public void removeListener(PAGImageViewListener listener) {
        synchronized (this) {
            mViewListeners.remove(listener);
        }
    }

    private boolean isAttachedToWindow = false;

    private volatile boolean forceFlush = false;

    @Override
    protected void onAttachedToWindow() {
        isAttachedToWindow = true;
        super.onAttachedToWindow();
        forceFlush = true;
        animator.addUpdateListener(mAnimatorUpdateListener);
        animator.addListener(mAnimatorListenerAdapter);
        synchronized (g_HandlerLock) {
            PAGImageViewHelper.StartHandlerThread();
        }
        resumeAnimator();
    }

    @Override
    protected void onDetachedFromWindow() {
        isAttachedToWindow = false;
        super.onDetachedFromWindow();
        PAGImageViewHelper.RemoveMessage(PAGImageViewHelper.MSG_FLUSH, this);
        pauseAnimator();
        animator.removeUpdateListener(mAnimatorUpdateListener);
        animator.removeListener(mAnimatorListenerAdapter);
        releaseAllTask();
        PAGImageViewHelper.SendMessage(PAGImageViewHelper.MSG_CLOSE_CACHE, this);
        synchronized (g_HandlerLock) {
            PAGImageViewHelper.DestroyHandlerThread();
        }
        renderBitmap = null;
        bitmapCache.clear();
        freezeDraw.set(false);
    }

    private void releaseAllTask() {
        for (Iterator<WeakReference<Future>> iterator = saveCacheTasks.iterator(); iterator.hasNext(); ) {
            WeakReference<Future> task = iterator.next();
            if (task.get() != null) {
                task.get().cancel(false);
            }
        }
        saveCacheTasks.clear();
    }

    private Runnable mAnimatorStartRunnable = new Runnable() {
        @Override
        public void run() {
            if (isAttachedToWindow) {
                animator.start();
            } else {
                Log.e(TAG, "AnimatorStartRunnable: PAGView is not attached to window");
            }
        }
    };

    private Runnable mAnimatorCancelRunnable = new Runnable() {
        @Override
        public void run() {
            currentPlayTime = animator.getCurrentPlayTime();
            animator.cancel();
        }
    };

    private void doPlay() {
        if (!isAttachedToWindow) {
            Log.e(TAG, "doPlay: View is not attached to window");
            return;
        }
        if (animationScale == 0.0f) {
            notifyEnd();
            Log.e(TAG, "doPlay: The scale of animator duration is turned off");
            return;
        }
        PAGImageViewHelper.SendMessage(PAGImageViewHelper.MSG_INIT_DECODER, this);
        Log.i(TAG, "doPlay");
        animator.setCurrentPlayTime(currentPlayTime);
        startAnimator();
    }

    private boolean isMainThread() {
        return Looper.getMainLooper().getThread() == Thread.currentThread();
    }

    private void startAnimator() {
        if (animator.getDuration() <= 0) {
            return;
        }
        if (isMainThread()) {
            animator.start();
        } else {
            removeCallbacks(mAnimatorCancelRunnable);
            post(mAnimatorStartRunnable);
        }
    }

    private void cancelAnimator() {
        if (isMainThread()) {
            currentPlayTime = animator.getCurrentPlayTime();
            animator.cancel();
        } else {
            removeCallbacks(mAnimatorStartRunnable);
            post(mAnimatorCancelRunnable);
        }
    }

    protected boolean updateView() {
        if (!isAttachedToWindow) {
            return true;
        }
        return flush();
    }

    protected KeyItem fetchKeyFrame() {
        if (!decoderInfo.isValid() || _composition == null && _pagFilePath == null) {
            return null;
        }
        if (_pagFilePath != null && lastKeyItem != null) {
            return lastKeyItem;
        }
        String keyPrefix = "_" + (decoderInfo._width << 16 | decoderInfo._height) + "_" + decoderInfo.realFrameRate;
        boolean autoClean = false;
        if (_pagFilePath != null) {
            keyPrefix = _pagFilePath + keyPrefix;
        } else if (_composition instanceof PAGFile && CacheManager.ContentVersion(_composition) == 0) {
            keyPrefix = ((PAGFile) _composition).path() + keyPrefix;
        } else {
            keyPrefix = _composition.toString() + "_" + CacheManager.ContentVersion(_composition) + keyPrefix;
            autoClean = true;
        }
        if (lastKeyItem != null && keyPrefix.equals(lastKeyItem.keyPrefix)) {
            return lastKeyItem;
        }
        KeyItem keyItem = new KeyItem();
        keyItem.keyPrefix = keyPrefix;
        keyItem.keyPrefixMD5 = PAGImageViewHelper.getMD5(keyPrefix);
        keyItem.needAutoClean = autoClean;
        return keyItem;
    }

    protected boolean inflateBitmapFromDiskCache(CacheInfo cacheInfo) {
        try {
            if (cacheInfo == null || cacheItem == null) {
                return false;
            }
            return cacheItem.inflateBitmap(cacheInfo.frame, cacheInfo.bitmap);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }


    private void releaseDecoder() {
        if (cacheItem == null || lastKeyItem == null || TextUtils.isEmpty(lastKeyItem.keyPrefix) || decoderInfo._pagDecoder == null) {
            return;
        }
        cacheItem.writeLock();
        if (bitmapCache.size() == decoderInfo.numFrames || cacheItem.isAllCached()) {
            decoderInfo.releaseDecoder();
            cacheItem.releaseSaveBuffer();
            if (bitmapCache.size() == decoderInfo.numFrames) {
                cacheItem.release();
            }
        }
        cacheItem.writeUnlock();
    }

    protected void releaseCurrentDiskCache() {
        if (cacheItem != null) {
            cacheItem.writeLock();
            if (lastKeyItem.needAutoClean) {
                try {
                    new File(cacheManager.getPath(lastKeyItem.keyPrefixMD5)).delete();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
            cacheItem.writeUnlock();
            cacheManager.remove(lastKeyItem.keyPrefixMD5);
        }
    }

    private boolean handleFrame(final int frame) {
        if (!decoderInfo.isValid() || freezeDraw.get()) {
            return false;
        }
        KeyItem keyItem = fetchKeyFrame();
        if (lastKeyItem != null && lastKeyItem.keyPrefix != null && !lastKeyItem.keyPrefix.equals(keyItem.keyPrefix)) {
            releaseCurrentDiskCache();
            cacheItem = cacheManager.getOrCreate(keyItem.keyPrefixMD5, decoderInfo._width, decoderInfo._height, decoderInfo.numFrames);
            bitmapCache.clear();
        }
        lastKeyItem = keyItem;

        if (freezeDraw.get()) {
            return false;
        }
        releaseDecoder();
        if (lastKeyItem != null && lastKeyItem.keyPrefixMD5 != null) {
            Bitmap bitmap = bitmapCache.get(frame);
            if (bitmap != null) {
                return true;
            }
            if (cacheItem != null) {
                try {
                    if (cacheItem.isCached(frame)) {
                        if (renderBitmap == null || isCacheAllFramesInMemory) {
                            renderBitmap = PAGImageViewHelper.CreateBitmap(decoderInfo._width, decoderInfo._height);
                        }
                        if (fetchCache(CacheInfo.Make(PAGImageView.this, lastKeyItem.keyPrefixMD5,
                                frame, renderBitmap))) {
                        }
                        return true;
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
        if (freezeDraw.get()) {
            return false;
        }
        if (renderBitmap == null || isCacheAllFramesInMemory) {
            renderBitmap = PAGImageViewHelper.CreateBitmap(decoderInfo._width, decoderInfo._height);
        }
        if (decoderInfo._pagDecoder == null || !decoderInfo._pagDecoder.copyFrameTo(renderBitmap, frame)) {
            return false;
        }
        if (isCacheAllFramesInMemory) {
            bitmapCache.put(frame, renderBitmap);
        }
        if (cacheItem != null && cacheItem.saveBitmap(frame, renderBitmap)) {
            saveCacheTasks.add(new WeakReference<Future>(saveCacheExecutors.submit(new Runnable() {
                @Override
                public void run() {
                    if (cacheItem != null) {
                        cacheItem.flushSave();
                    }
                }
            })));
        }
        return true;
    }

    protected static boolean fetchCache(CacheInfo cacheInfo) {
        if (cacheInfo.pagImageView == null || TextUtils.isEmpty(cacheInfo.keyPrefix)) {
            return false;
        }
        if (!cacheInfo.pagImageView.inflateBitmapFromDiskCache(cacheInfo)) {
            Log.e(TAG, "inflateBitmapFromDiskCache failed:" + cacheInfo.pagImageView._pagFilePath);
            return false;
        }
        if (cacheInfo.pagImageView.isCacheAllFramesInMemory) {
            cacheInfo.pagImageView.bitmapCache.put(cacheInfo.frame, cacheInfo.bitmap);
        }
        return true;
    }

    private void notifyAnimationUpdate() {
        if (mViewListeners.isEmpty() || !animator.isRunning()) {
            return;
        }
        ArrayList<PAGImageViewListener> arrayList;
        synchronized (PAGImageView.this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGImageViewListener listener : arrayList) {
            listener.onAnimationUpdate(PAGImageView.this);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (renderBitmap != null && !renderBitmap.isRecycled()) {
            super.onDraw(canvas);
            canvas.save();
            if (renderMatrix != null) {
                canvas.concat(renderMatrix);
            }
            if (_matrix != null) {
                canvas.concat(_matrix);
            }
            try {
                canvas.drawBitmap(renderBitmap, 0, 0, null);
            } catch (Exception e) {
                e.printStackTrace();
            }
            canvas.restore();
        }
    }

    private boolean preAggregatedVisible = true;

    private void pauseAnimator() {
        if (_isAnimatorPreRunning == null) {
            _isAnimatorPreRunning = animator.isRunning();
        }
        if (animator.isRunning()) {
            cancelAnimator();
        }
    }

    private void resumeAnimator() {
        if (width == 0 || height == 0 || !_isPlaying || animator.isRunning() || (_isAnimatorPreRunning != null && !_isAnimatorPreRunning)) {
            _isAnimatorPreRunning = null;
            return;
        }
        _isAnimatorPreRunning = null;
        doPlay();
    }

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
    }

    protected static class KeyItem {
        String keyPrefix;
        String keyPrefixMD5;
        boolean needAutoClean = false;
    }

    private boolean hasSize() {
        return width > 0 && height > 0;
    }

    private void refreshMatrixFromScaleMode() {
        if (_scaleMode == PAGScaleMode.None) {
            return;
        }
        _matrix = PAGImageViewHelper.ApplyScaleMode(_scaleMode, decoderInfo._width, decoderInfo._height, width, height);
    }

}
