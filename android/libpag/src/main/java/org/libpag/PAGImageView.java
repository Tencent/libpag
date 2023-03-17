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
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;
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

import org.extra.tools.BitmapPool;
import org.extra.tools.BitmapPool.BitmapResource;
import org.extra.tools.Lifecycle;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import org.libpag.PAGImageViewHelper.CacheInfo;
import org.libpag.PAGImageViewHelper.DecoderInfo;

public class PAGImageView extends View implements ComponentCallbacks2 {
    private final static String TAG = "PAGImageView";
    private final static float DEFAULT_MAX_FRAMERATE = 30f;
    protected BitmapPool bitmapPool;
    private ValueAnimator animator;
    private boolean isMemoryCacheOpen = false;
    private volatile boolean _isPlaying = false;
    private volatile Boolean _isAnimatorPreRunning = null;
    private volatile boolean progressExplicitlySet = true;
    private final Object updateTimeLock = new Object();
    private static final Object g_HandlerLock = new Object();
    private volatile BitmapResource _currentImage;
    private int _currentFrame;
    private float _maxFrameRate = 30;
    private PAGComposition _composition;
    private String _pagFilePath;
    private int _scaleMode = PAGScaleMode.LetterBox;
    private Matrix _matrix;
    private ConcurrentHashMap<String, Set<Integer>> keysInMemoryCacheMap = new ConcurrentHashMap<>();
    private ConcurrentHashMap<String, BitmapResource> memoryLruCache = new ConcurrentHashMap<>();
    private ArrayList<WeakReference<Future>> saveCacheTasks = new ArrayList<>();
    private float renderScale = 1.0f;
    protected CacheManager.CacheItem cacheItem;
    protected KeyItem lastKeyItem;
    protected DecoderInfo decoderInfo = new DecoderInfo();
    protected CacheManager cacheManager;

    protected static long g_MaxDiskCacheSize = 1 * 1024 * 1024 * 1024;

    public interface PAGImageViewListener {
        /**
         * Notifies the start of the animation.
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
         * Notifies the occurrence of another frame of the animation.
         */
        void onAnimationUpdate(PAGImageView view);
    }

    /**
     * This value defines the scale factor for internal graphics renderer, ranges from 0.0 to 1.0.
     * The scale factors less than 1.0 may result in blurred output, but it can reduce the usage of
     * graphics memory which leads to better performance. The default value is 1.0.
     */
    public float renderScale() {
        return renderScale;
    }

    /**
     * Set the value of renderScale property.
     */
    public void setRenderScale(float renderScale) {
        if (renderScale < 0.0f || renderScale > 1.0f) {
            renderScale = 1.0f;
        }
        this.renderScale = renderScale;
        width = (int) (viewWidth * renderScale);
        height = (int) (viewHeight * renderScale);
    }

    /**
     * Set the value of maxDiskCache property.
     */
    public static void SetMaxDiskCache(long maxDiskCache) {
        g_MaxDiskCacheSize = maxDiskCache;
    }

    /**
     * Get the value of maxDiskCache property.
     */
    public static long MaxDiskCache() {
        return g_MaxDiskCacheSize;
    }

    /**
     * If set to true, the PAGImageView loads all image frames into the memory,
     * which will significantly increase the rendering performance but may cost lots of additional memory.
     * Use it when you prefer rendering speed over memory usage.
     * If set to false, the PAGImageView loads only one image frame at a time into the memory.
     * The default value is false.
     */
    public void setMemoryCacheEnabled(boolean enable) {
        isMemoryCacheOpen = enable;
    }

    /**
     * Return the value of isMemoryCacheOpen property.
     */
    public boolean memoryCacheEnabled() {
        return isMemoryCacheOpen;
    }

    /**
     * Returns a bitmap capturing the contents of the PAGImageView.
     */
    public Bitmap currentImage() {
        return _currentImage.get();
    }

    /**
     * Returns the current rendering frame.
     */
    public int currentFrame() {
        return _currentFrame;
    }

    /**
     * Returns the maximum frame rate for rendering.
     */
    public float maxFrameRate() {
        return _maxFrameRate;
    }

    /**
     * Sets the frame index to render.
     */
    public void setCurrentFrame(int currentFrame) {
        if (!decoderInfo.isValid() || currentFrame < 0 || currentFrame >= decoderInfo.numFrames) {
            return;
        }
        _currentFrame = currentFrame;
        float value =
                decoderInfo.duration * 0.001f * (_currentFrame * 1.0f + 0.1f) / decoderInfo.numFrames;
        value = Math.max(0, Math.min(value, 1));
        currentPlayTime = (long) (value * animator.getDuration());
        synchronized (updateTimeLock) {
            animator.setCurrentPlayTime(currentPlayTime);
            progressExplicitlySet = true;
        }
        PAGImageViewHelper.NeedsUpdateView(this);
    }

    /**
     * Sets a new PAGComposition for PAGImageView to render as content.
     * Note: If the composition is already added to another View, it will be removed from the
     * previous View.
     * Sets the maximum frame rate for rendering. If set to a value less than the actual frame rate from
     * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
     * value is from the associated Composition.
     */
    public void setComposition(PAGComposition newComposition, float maxFrameRate) {
        if (newComposition == _composition) {
            return;
        }
        _composition = newComposition;
        refreshDecodeInfo();
    }

    /**
     * Sets a new PAGComposition for PAGImageView to render as content.
     * Note: If the composition is already added to another View, it will be removed from the
     * previous View.
     */
    public void setComposition(PAGComposition newComposition) {
        setComposition(newComposition, DEFAULT_MAX_FRAMERATE);
    }

    /**
     * Returns the current PAGComposition for PAGImageView to render as content.
     */
    public PAGComposition composition() {
        return _composition;
    }

    /**
     * Loads a pag file from the specified path, returns false if the file does not exist or the data is not a pag file.
     * The path starts with "assets://" means that it is located in assets directory.
     * Note: All PAGFiles loaded by the same path share the same internal cache. The internal cache is alive until all
     * PAGFiles are released. Use 'PAGFile.Load(byte[])' instead if you don't want to load a
     * PAGFile from the internal caches.
     * Sets the maximum frame rate for rendering. If set to a value less than the actual frame rate from
     * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
     * value is from the associated Composition.
     */
    public boolean setPath(String path, float maxFrameRate) {
        if (path == null) {
            return false;
        }
        if (path.equals(_pagFilePath)) {
            return true;
        }
        _pagFilePath = path;
        _maxFrameRate = maxFrameRate;
        refreshDecodeInfo();
        return true;
    }

    /**
     * Loads a pag file from the specified path, returns false if the file does not exist or the data is not a pag file.
     * The path starts with "assets://" means that it is located in assets directory.
     * Note: All PAGFiles loaded by the same path share the same internal cache. The internal cache is alive until all
     * PAGFiles are released. Use 'PAGFile.Load(byte[])' instead if you don't want to load a
     * PAGFile from the internal caches.
     */
    public boolean setPath(String path) {
        return setPath(path, DEFAULT_MAX_FRAMERATE);
    }

    /**
     * Sets the number of times the animation will repeat. The default is 1, which means the animation
     * will play only once. 0 means the animation will play infinity times.
     */
    public void setRepeatCount(int value) {
        if (value < 0) {
            value = 0;
        }
        animator.setRepeatCount(value - 1);
    }

    /**
     * Call this method to render current position immediately. If the play() method is already
     * called, there is no need to call it. Returns true if the content has changed.
     */
    public boolean flush() {
        if (!decoderInfo.isValid()) {
            return false;
        }
        synchronized (updateTimeLock) {
            if (progressExplicitlySet) {
                progressExplicitlySet = false;
                animator.setCurrentPlayTime((long) (decoderInfo.duration * 0.001f * (_currentFrame * 1.0f + 0.1f) / decoderInfo.numFrames));
            } else {
                int currentFrame =
                        ProgressToFrame(animator.getAnimatedFraction(), decoderInfo.numFrames);
                if (currentFrame == _currentFrame) {
                    return false;
                }
                _currentFrame = currentFrame;
            }
            BitmapResource handledBitmap = handleFrame(_currentFrame, false);
            if (handledBitmap == null) {
                return false;
            }
            _currentImage = handledBitmap;
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
     * Specifies the rule of how to scale the pag content to fit the to fit the PAGImageView size.
     * The current matrix of the PAGImageView changes when this method is called.
     */
    public void setScaleMode(int scaleMode) {
        if (scaleMode == _scaleMode) {
            return;
        }
        _scaleMode = scaleMode;
        _matrix = null;
        scaleModeMatrix = null;
        postInvalidate();
    }

    /**
     * Returns a copy of current matrix.
     */
    public Matrix matrix() {
        if (_matrix != null) {
            return _matrix;
        }
        if (!decoderInfo.isValid()) {
            return new Matrix();
        }
        return PAGImageViewHelper.ApplyScaleMode(_scaleMode, decoderInfo._width,
                decoderInfo._height, width,
                height);
    }

    /**
     * Sets the transformation which will be applied to the composition. The scaleMode property
     * will be set to PAGScaleMode::None when this method is called.
     */
    public void setMatrix(Matrix matrix) {
        _matrix = matrix;
        _scaleMode = PAGScaleMode.None;
        postInvalidate();
    }

    @Override
    public void onTrimMemory(int level) {
        Log.i(TAG, "onTrimMemory:" + level);
        clearMemoryCache();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
    }

    @Override
    public void onLowMemory() {
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
            return Settings.Global.getFloat(context.getContentResolver(),
                    Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f);
        } else {
            // noinspection deprecation
            return Settings.System.getFloat(context.getContentResolver(),
                    Settings.System.ANIMATOR_DURATION_SCALE, 1.0f);
        }
    }

    private void refreshDecodeInfo() {
        progressExplicitlySet = true;
        if (decoderInfo != null) {
            decoderInfo.release();
            decoderInfo = null;
        }
        releaseCurrentDiskCache();
        decoderInfo = new DecoderInfo();
        initDecoderInfo();
    }

    private static ThreadPoolExecutor saveCacheExecutors;
    private static ThreadPoolExecutor fetchCacheExecutors;

    private static void InitCacheExecutors() {
        if (fetchCacheExecutors == null || saveCacheExecutors == null) {
            synchronized (PAGImageView.class) {
                if (fetchCacheExecutors == null) {
                    fetchCacheExecutors =
                            new ThreadPoolExecutor(1, 1, 2, TimeUnit.SECONDS,
                                    new LinkedBlockingQueue<Runnable>());
                    fetchCacheExecutors.allowCoreThreadTimeOut(true);
                }
                if (saveCacheExecutors == null) {
                    saveCacheExecutors =
                            new ThreadPoolExecutor(1, 1, 2, TimeUnit.SECONDS,
                                    new LinkedBlockingQueue<Runnable>());
                    saveCacheExecutors.allowCoreThreadTimeOut(true);
                }
            }
        }
    }

    private void handleCacheStrategyChange(final Context context) {
        InitCacheExecutors();
        clearMemoryCache();
    }

    private void clearMemoryCache() {
        if (memoryLruCache != null) {
            for (Iterator<String> it = memoryLruCache.keySet().iterator(); it.hasNext(); ) {
                String key = it.next();
                BitmapResource bitmap = memoryLruCache.get(key);
                if (bitmap != null) {
                    bitmap.release();
                    memoryLruCache.remove(key);
                }
            }
            memoryLruCache.clear();
        }
        keysInMemoryCacheMap.clear();
    }

    private void putToSelfMemoryCache(String keyPrefix, int frame, BitmapResource bitmapResource) {
        if (bitmapResource == null) {
            return;
        }
        //Prevent BitmapResource from being recycled in no cache mode
        if (keyPrefix == null) {
            keyPrefix = "";
        }
        Set<Integer> set = keysInMemoryCacheMap.get(keyPrefix);
        if (set == null) {
            set = new HashSet<>();
            keysInMemoryCacheMap.put(keyPrefix, set);
        }
        if (set.add(frame)) {
            bitmapResource.acquire();
        }
    }

    private void removeToSelfMemoryCache(String keyPrefix, int frame, BitmapResource bitmapResource) {
        if (!TextUtils.isEmpty(keyPrefix)) {
            if (keysInMemoryCacheMap.get(keyPrefix) != null) {
                if (keysInMemoryCacheMap.get(keyPrefix).remove(frame)) {
                    if (bitmapResource != null) {
                        bitmapResource.release();
                    }
                }
            }
            return;
        }

        for (Iterator<String> it = keysInMemoryCacheMap.keySet().iterator(); it.hasNext(); ) {
            String prefix = it.next();
            if (keysInMemoryCacheMap.get(prefix) != null
                    && keysInMemoryCacheMap.get(prefix).remove(frame)) {
                if (bitmapResource != null) {
                    bitmapResource.release();
                    return;
                }
            }
        }
    }

    private void clearSelfMemoryCache() {
        if (memoryLruCache == null) {
            return;
        }
        for (Iterator<String> i = keysInMemoryCacheMap.keySet().iterator(); i.hasNext(); ) {
            for (Iterator<Integer> j = keysInMemoryCacheMap.get(i.next()).iterator(); j.hasNext(); ) {
                int key = j.next();
                BitmapResource bitmapResource = memoryLruCache.get(key);
                if (bitmapResource != null && bitmapResource.release()) {
                    memoryLruCache.remove(key);
                }
            }
        }
        keysInMemoryCacheMap.clear();
    }

    private volatile long currentPlayTime;

    private final ValueAnimator.AnimatorUpdateListener mAnimatorUpdateListener = new ValueAnimator.AnimatorUpdateListener() {
        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            if (!decoderInfo.isValid()) {
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
            if (repeatCount >= 0 && (animation.getDuration() > 0) &&
                    (currentPlayTime / animation.getDuration() > repeatCount)) {
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

    private Matrix scaleModeMatrix;

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        viewWidth = w;
        viewHeight = h;
        width = (int) (renderScale * w);
        height = (int) (renderScale * h);
        scaleModeMatrix = null;
        initDecoderInfo();
        resumeAnimator();
    }

    private void initDecoderInfo() {
        if (!decoderInfo.isValid()) {
            if (decoderInfo.initDecoder(getContext(), _composition, _pagFilePath, width,
                    height, _maxFrameRate)) {
                animator.setDuration(decoderInfo.duration / 1000);
                PAGImageViewHelper.SendMessage(PAGImageViewHelper.MSG_INIT_CACHE, this);
            }
        }
    }

    private float animationScale = 1.0f;

    private void init() {
        if (bitmapPool == null) {
            bitmapPool = BitmapPool.Make(1);
        }
        handleCacheStrategyChange(getContext());
        Lifecycle.getInstance().addListener(this);
        cacheManager = CacheManager.Get(getContext());
        animator = ValueAnimator.ofFloat(0.0f, 1.0f);
        animator.setRepeatCount(0);
        animator.setInterpolator(new LinearInterpolator());
        animator.addUpdateListener(mAnimatorUpdateListener);
        animationScale = getAnimationScale(getContext());
    }

    public void pause() {
        _isPlaying = false;
        _isAnimatorPreRunning = null;
        cancelAnimator();
    }

    public void play() {
        _isPlaying = true;
        _isAnimatorPreRunning = null;
        if (animator.getAnimatedFraction() == 1.0) {
            setCurrentFrame(0);
        }
        doPlay();
    }

    /**
     * Adds a PAGViewListener to the set of listeners that are sent events through the life of an
     * animation, such as start, end repeat, and cancel.
     *
     * @param listener the PAGViewListener to be added to the current set of listeners for this animation.
     */
    public void addListener(PAGImageViewListener listener) {
        synchronized (this) {
            mViewListeners.add(listener);
        }
    }

    /**
     * Removes a PAGViewListener from the set listening to this animation.
     *
     * @param listener the PAGViewListener to be removed from the current set of listeners for this
     *                 animation.
     */
    public void removeListener(PAGImageViewListener listener) {
        synchronized (this) {
            mViewListeners.remove(listener);
        }
    }

    private boolean isAttachedToWindow = false;

    @Override
    protected void onAttachedToWindow() {
        isAttachedToWindow = true;
        super.onAttachedToWindow();
        animator.addUpdateListener(mAnimatorUpdateListener);
        animator.addListener(mAnimatorListenerAdapter);
        synchronized (g_HandlerLock) {
            PAGImageViewHelper.StartHandlerThread();
        }
        if (cacheManager != null && cacheItem == null) {
            PAGImageViewHelper.SendMessage(PAGImageViewHelper.MSG_INIT_CACHE, this);
        }
        resumeAnimator();
        getContext().registerComponentCallbacks(this);
        clearSelfMemoryCache();
    }

    @Override
    protected void onDetachedFromWindow() {
        isAttachedToWindow = false;
        super.onDetachedFromWindow();
        pauseAnimator();
        synchronized (g_HandlerLock) {
            PAGImageViewHelper.DestroyHandlerThread();
        }
        animator.removeUpdateListener(mAnimatorUpdateListener);
        animator.removeListener(mAnimatorListenerAdapter);
        getContext().unregisterComponentCallbacks(this);
        if (lastFetchCacheTask != null && lastFetchCacheTask.get() != null) {
            lastFetchCacheTask.get().cancel(false);
        }
        for (Iterator<WeakReference<Future>> iterator = saveCacheTasks.iterator(); iterator.hasNext(); ) {
            WeakReference<Future> task = iterator.next();
            if (task.get() != null) {
                task.get().cancel(false);
            }
        }
        saveCacheTasks.clear();
        releaseCurrentDiskCache();
        _currentImage = null;
        if (decoderInfo != null) {
            decoderInfo.release();
            decoderInfo = null;
        }
        clearSelfMemoryCache();
        if (bitmapPool != null) {
            bitmapPool.clear();
        }
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
        initDecoderInfo();
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
        String keyPrefix =
                "_" + (decoderInfo._width << 16 | decoderInfo._height) + "_" + decoderInfo.realFrameRate;
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

    protected boolean inflateBitmapFromDiskCache(BitmapResource bitmapResource,
                                                 CacheInfo cacheInfo) {
        try {
            if (bitmapResource == null || cacheInfo == null || cacheItem == null) {
                return false;
            }
            return cacheItem.inflateBitmap(cacheInfo.frame, bitmapResource.get());
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }

    protected boolean putBitmapToDiskCache(int frame, BitmapPool.BitmapResource bitmap) {
        if (cacheItem == null || bitmap == null) {
            return false;
        }
        boolean success = cacheItem.saveBitmap(frame, bitmap.get());
        return success;
    }


    private void handleReleaseSurface() {
        if (cacheItem == null ||
                lastKeyItem == null || TextUtils.isEmpty(lastKeyItem.keyPrefix) || decoderInfo._pagDecoder == null) {
            return;
        }
        if (keysInMemoryCacheMap.get(lastKeyItem.keyPrefixMD5) != null && keysInMemoryCacheMap.get(lastKeyItem.keyPrefixMD5).size() == decoderInfo.numFrames
                || cacheItem.isAllCached()) {
            decoderInfo.release();
        }
    }

    private void releaseCurrentDiskCache() {
        if (cacheItem != null) {
            cacheManager.remove(lastKeyItem.keyPrefixMD5);
            cacheItem.writeLock();
            cacheItem.release();
            if (lastKeyItem.needAutoClean) {
                try {
                    new File(cacheManager.getPath(lastKeyItem.keyPrefixMD5)).delete();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
            cacheItem.writeUnlock();
            cacheItem = null;
        }
    }

    private WeakReference<Future> lastFetchCacheTask;

    private BitmapPool.BitmapResource handleFrame(final int frame, boolean syncMode) {
        KeyItem keyItem = fetchKeyFrame();
        if (lastKeyItem != null && lastKeyItem.keyPrefix != null && !lastKeyItem.keyPrefix.equals(keyItem.keyPrefix)) {
            releaseCurrentDiskCache();
            cacheItem = cacheManager.getOrCreate(lastKeyItem.keyPrefixMD5, decoderInfo._width,
                    decoderInfo._height, decoderInfo.numFrames);
            keysInMemoryCacheMap.remove(lastKeyItem.keyPrefixMD5);
            clearSelfMemoryCache();
        }
        lastKeyItem = keyItem;

        handleReleaseSurface();
        if (lastKeyItem != null && lastKeyItem.keyPrefixMD5 != null) {
            if (isMemoryCacheOpen) {
                final String memoryKey = lastKeyItem.keyPrefixMD5 + "_" + frame;
                BitmapPool.BitmapResource bitmap = memoryLruCache.get(memoryKey);
                if (bitmap != null) {
                    putToSelfMemoryCache(lastKeyItem.keyPrefixMD5, frame, bitmap);
                    return bitmap;
                }
            }
            if (cacheItem != null) {
                try {
                    if (!syncMode) {
                        if (lastFetchCacheTask != null && lastFetchCacheTask.get() != null && !lastFetchCacheTask.get().isDone()) {
                            return null;
                        }
                        if (cacheItem.isCached(frame)) {
                            lastFetchCacheTask =
                                    new WeakReference<Future>(fetchCacheExecutors.submit(new Runnable() {
                                        @Override
                                        public void run() {
                                            executeCache(CacheInfo.Make(PAGImageView.this,
                                                    lastKeyItem.keyPrefixMD5, frame, null), true);
                                        }
                                    }));
                            return null;
                        }
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
        final BitmapPool.BitmapResource currentImage;
        if (lastKeyItem != null && !isMemoryCacheOpen) {
            removeToSelfMemoryCache(lastKeyItem.keyPrefixMD5, frame, _currentImage);
        }
        currentImage = bitmapPool.get(decoderInfo._width,
                decoderInfo._height);
        if (decoderInfo._pagDecoder == null || !decoderInfo._pagDecoder.copyFrameTo(currentImage.get(),
                frame)) {
            currentImage.release();
            return null;
        }
        if (currentImage == null) {
            return null;
        } else {
            putToSelfMemoryCache(lastKeyItem.keyPrefixMD5, frame, currentImage);
            currentImage.acquire();
            if (syncMode) {
                executeCache(CacheInfo.Make(this, lastKeyItem.keyPrefixMD5, frame, currentImage), false);
            } else {
                saveCacheTasks.add(new WeakReference<Future>(saveCacheExecutors.submit(new Runnable() {
                    @Override
                    public void run() {
                        executeCache(CacheInfo.Make(PAGImageView.this, lastKeyItem.keyPrefixMD5, frame,
                                        currentImage),
                                true);
                    }
                })));
            }
            if (lastKeyItem != null && isMemoryCacheOpen && memoryLruCache != null) {
                final String memoryKey = lastKeyItem.keyPrefixMD5 + "_" + frame;
                memoryLruCache.put(memoryKey, currentImage);
            } else {
                removeToSelfMemoryCache(lastKeyItem.keyPrefixMD5, frame, _currentImage);
            }
            return currentImage;
        }
    }

    protected static void executeCache(CacheInfo cacheInfo, boolean notifyView) {
        if (cacheInfo.pagImageView == null || TextUtils.isEmpty(cacheInfo.keyPrefix)) {
            if (cacheInfo.bitmap != null) {
                cacheInfo.bitmap.release();
            }
            return;
        }
        if (cacheInfo.bitmap != null) {
            // PutCache;
            cacheInfo.pagImageView.putBitmapToDiskCache(cacheInfo.frame, cacheInfo.bitmap);
            cacheInfo.bitmap.release();
        } else {
            // GetCache;
            BitmapPool.BitmapResource bitmap = cacheInfo.pagImageView._currentImage;
            boolean created = false;
            if (bitmap == null) {
                bitmap =
                        cacheInfo.pagImageView.bitmapPool.get(cacheInfo.pagImageView.decoderInfo._width,
                                cacheInfo.pagImageView.decoderInfo._height);
                created = true;
            }

            if (!cacheInfo.pagImageView.inflateBitmapFromDiskCache(bitmap, cacheInfo) && created) {
                cacheInfo.pagImageView.bitmapPool.put(bitmap);
            }
            if (bitmap != null) {
                if (created) {
                    cacheInfo.pagImageView.putToSelfMemoryCache(cacheInfo.keyPrefix, cacheInfo.frame, bitmap);
                    if (cacheInfo.pagImageView.isMemoryCacheOpen) {
                        cacheInfo.pagImageView.memoryLruCache.put(cacheInfo.key, bitmap);
                    } else {
                        cacheInfo.pagImageView.removeToSelfMemoryCache(cacheInfo.keyPrefix,
                                cacheInfo.frame,
                                cacheInfo.pagImageView._currentImage);
                    }
                }
                if (notifyView) {
                    cacheInfo.pagImageView._currentImage = bitmap;
                    cacheInfo.pagImageView.notifyAnimationUpdate();
                    cacheInfo.pagImageView.postInvalidate();
                }
            }
        }
    }

    private void notifyAnimationUpdate() {
        if (mViewListeners.isEmpty()) {
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
        if (_currentImage != null && _currentImage.get() != null && !_currentImage.get().isRecycled()) {
            super.onDraw(canvas);
            canvas.save();
            if (_matrix != null) {
                canvas.concat(_matrix);
            }
            if (_scaleMode != PAGScaleMode.None) {
                if (scaleModeMatrix == null) {
                    scaleModeMatrix = PAGImageViewHelper.ApplyScaleMode(_scaleMode,
                            decoderInfo._width,
                            decoderInfo._height,
                            width, height);
                }
                canvas.concat(scaleModeMatrix);
            }
            canvas.drawBitmap(_currentImage.get(), 0, 0, null);
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
        if (width == 0 || height == 0 || !_isPlaying || animator.isRunning() ||
                (_isAnimatorPreRunning != null && !_isAnimatorPreRunning)) {
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

    private static double fmod(double a, double b) {
        int result = (int) Math.floor(a / b);
        return a - result * b;
    }

    private static int ProgressToFrame(double progress, int totalFrames) {
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

    protected static class KeyItem {
        String keyPrefix;
        String keyPrefixMD5;
        boolean needAutoClean = false;
    }

}
