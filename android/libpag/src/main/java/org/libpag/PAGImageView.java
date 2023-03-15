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
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.animation.LinearInterpolator;

import org.extra.tools.BitmapPool;
import org.extra.tools.BitmapPool.BitmapResource;
import org.extra.tools.Lifecycle;
import org.extra.tools.LifecycleListener;

import java.io.File;
import java.lang.ref.WeakReference;
import java.math.BigInteger;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class PAGImageView extends View implements LifecycleListener, ComponentCallbacks2 {
    private final static String TAG = "PAGImageView";
    private DecoderInfo decoderInfo = new DecoderInfo();
    private BitmapPool bitmapPool;
    private ValueAnimator animator;
    private PAGCacheManager cacheManager;
    private boolean isMemoryCacheOpen = false;
    private volatile boolean _isPlaying = false;
    private volatile Boolean _isAnimatorPreRunning = null;
    private boolean isAttachedToWindow = false;
    private volatile boolean progressExplicitlySet = true;
    private final Object updateTimeLock = new Object();

    private static final Object g_HandlerLock = new Object();
    private static PAGViewHandler g_PAGViewHandler = null;
    private static HandlerThread g_PAGViewThread = null;
    private static volatile int g_HandlerThreadCount = 0;
    private static final int MSG_FLUSH = 0;
    private static final int MSG_INIT_CACHE = 1;
    private static final int MSG_HANDLER_THREAD_QUITE = 2;
    private float animationScale = 1.0f;
    private volatile int width, height;
    private volatile int viewWidth, viewHeight;
    private volatile BitmapResource _currentImage;
    private int _currentFrame;
    private float _maxFrameRate = 60;
    private PAGComposition _composition;
    private String _pagFilePath;
    private int _scaleMode = PAGScaleMode.LetterBox;
    private Matrix _matrix;
    private ConcurrentHashMap<String, Set<Integer>> keysInMemoryCacheMap = new ConcurrentHashMap<>();
    private static ThreadPoolExecutor saveCacheExecutors;
    private static ThreadPoolExecutor fetchCacheExecutors;
    private ConcurrentHashMap<String, BitmapResource> memoryLruCache = new ConcurrentHashMap<>();
    private ArrayList<WeakReference<Future>> saveCacheTasks = new ArrayList<>();
    private WeakReference<Future> lastFetchCacheTask;
    private float renderScale = 1.0f;
    private PAGCacheManager.CacheItem cacheItem;
    private KeyItem lastKeyItem;

    protected static long g_MaxDiskCacheSize = 1 * 1024 * 1024 * 1024;

    private class KeyItem {
        private String keyPrefix;
        private String keyPrefixMD5;
        boolean needAutoClean = false;
    }

    private class DecoderInfo {
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

        boolean initDecoder(int width, int height) {
            if (_composition == null && _pagFilePath == null || width == 0) {
                return false;
            }
            PAGComposition composition = _composition;
            if (composition == null) {
                if (_pagFilePath.startsWith("assets://")) {
                    composition = PAGFile.Load(getContext().getAssets(), _pagFilePath.substring(9));
                } else {
                    composition = PAGFile.Load(_pagFilePath);
                }
            }
            if (composition == null) {
                return false;
            }
            float maxScale = Math.max(width * 1.0f / composition.width(),
                    height * 1.0f / composition.height());
            _pagDecoder = PAGDecoder.Make(composition, _maxFrameRate, maxScale);
            realFrameRate = composition.frameRate() > _maxFrameRate ? _maxFrameRate :
                    composition.frameRate();
            _width = _pagDecoder.width();
            _height = _pagDecoder.height();
            numFrames = _pagDecoder.numFrames();
            duration = composition.duration();
            return true;
        }

        void release() {
            if (_pagDecoder != null) {
                _pagDecoder.freeCache();
            }
            _pagDecoder = null;
        }
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
    public static void setMaxDiskCache(long maxDiskCache) {
        g_MaxDiskCacheSize = maxDiskCache;
    }

    /**
     * Clear all the disk cache.
     */
    public static void clearAllDiskCache(Context context) {
        if (context == null) {
            return;
        }
        PAGCacheManager.ClearAllDiskCache(context);
    }

    /**
     * Set the value of isMemoryCacheOpen property.
     */
    public void enableMemoryCache(boolean enable) {
        isMemoryCacheOpen = enable;
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
     * Sets the maximum frame rate for rendering. If set to a value less than the actual frame rate from
     * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
     * value is 60.
     */
    public void setMaxFrameRate(int maxFrameRate) {
        _maxFrameRate = maxFrameRate;
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
        NeedsUpdateView(this);
    }

    /**
     * Sets a new PAGComposition for PAGImageView to render as content.
     * Note: If the composition is already added to another View, it will be removed from the
     * previous View.
     */
    public void setComposition(PAGComposition newComposition) {
        if (newComposition == _composition) {
            return;
        }
        _composition = newComposition;
        refreshDecodeInfo();
    }

    /**
     * Loads a pag file from the specified path, returns false if the file does not exist or the data is not a pag file.
     * The path starts with "assets://" means that it is located in assets directory.
     * Note: All PAGFiles loaded by the same path share the same internal cache. The internal cache is alive until all
     * PAGFiles are released. Use 'PAGFile.Load(byte[])' instead if you don't want to load a
     * PAGFile from the internal caches.
     */
    public boolean setPath(String path) {
        if (path == null) {
            return false;
        }
        if (path.equals(_pagFilePath)) {
            return true;
        }
        _pagFilePath = path;
        refreshDecodeInfo();
        return true;
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

    /**
     * Returns the current PAGComposition for PAGImageView to render as content.
     */
    public PAGComposition composition() {
        return _composition;
    }

    /**
     * Returns the current scale mode.
     */
    public int scaleMode() {
        return _scaleMode;
    }

    /**
     * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
     * changes when this method is called.
     */
    public void setScaleMode(int scaleMode) {
        _scaleMode = scaleMode;
        _matrix = null;
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
        return ApplyScaleMode(_scaleMode, decoderInfo._width, decoderInfo._height, width, height);
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

    @Override
    public void onResume() {
        // When the device is locked and then unlocked, the PAGView's content may disappear,
        // use the following way to make the content appear.
        if (isAttachedToWindow && getVisibility() == View.VISIBLE) {
            setVisibility(View.INVISIBLE);
            setVisibility(View.VISIBLE);
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

    public float getAnimationScale(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return Settings.Global.getFloat(context.getContentResolver(),
                    Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f);
        } else {
            // noinspection deprecation
            return Settings.System.getFloat(context.getContentResolver(),
                    Settings.System.ANIMATOR_DURATION_SCALE, 1.0f);
        }
    }

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

    private static void handleDiskCacheStrategyChange(final Context context) {
        InitCacheExecutors();
    }

    private void handleMemoryCacheStrategyChange() {
        clearMemoryCache();
    }

    private void handleCacheStrategyChange(final Context context) {
        handleDiskCacheStrategyChange(context);
        handleMemoryCacheStrategyChange();
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

    private static synchronized void StartHandlerThread() {
        g_HandlerThreadCount++;
        if (g_PAGViewThread == null) {
            g_PAGViewThread = new HandlerThread("pagImageView-renderer");
            g_PAGViewThread.start();
        }
        if (g_PAGViewHandler == null) {
            g_PAGViewHandler = new PAGViewHandler(g_PAGViewThread.getLooper());
        }
    }

    private static synchronized void DestroyHandlerThread() {
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

    private static void SendMessage(int msgId, Object obj) {
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

    private static void NeedsUpdateView(PAGImageView pagView) {
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
                    PAGCacheManager manager = PAGCacheManager.Get(null);
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

    private static class CacheInfo {
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

    private static void executeCache(CacheInfo cacheInfo, boolean notifyView) {
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

    private ArrayList<PAGImageViewListener> mViewListeners = new ArrayList<>();
    private volatile long currentPlayTime;

    private final ValueAnimator.AnimatorUpdateListener mAnimatorUpdateListener = new ValueAnimator.AnimatorUpdateListener() {
        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            if (!decoderInfo.isValid()) {
                return;
            }
            PAGImageView.this.currentPlayTime = animation.getCurrentPlayTime();
            NeedsUpdateView(PAGImageView.this);
        }
    };

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

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        viewWidth = w;
        viewHeight = h;
        width = (int) (renderScale * w);
        height = (int) (renderScale * h);
        initDecoderInfo();
        resumeAnimator();
    }

    private void initDecoderInfo() {
        if (!decoderInfo.isValid()) {
            if (decoderInfo.initDecoder(width, height)) {
                animator.setDuration(decoderInfo.duration / 1000);
                SendMessage(MSG_INIT_CACHE, this);
            }
        }
    }

    private void init() {
        if (bitmapPool == null) {
            bitmapPool = BitmapPool.Make(1);
        }
        handleCacheStrategyChange(getContext());
        Lifecycle.getInstance().addListener(this);
        cacheManager = PAGCacheManager.Get(getContext());
        animator = ValueAnimator.ofFloat(0.0f, 1.0f);
        animator.setRepeatCount(0);
        animator.setInterpolator(new LinearInterpolator());
        animator.addUpdateListener(mAnimatorUpdateListener);
        animationScale = getAnimationScale(getContext());
    }

    public void stop() {
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

    @Override
    protected void onAttachedToWindow() {
        isAttachedToWindow = true;
        super.onAttachedToWindow();
        animator.addUpdateListener(mAnimatorUpdateListener);
        animator.addListener(mAnimatorListenerAdapter);
        synchronized (g_HandlerLock) {
            StartHandlerThread();
        }
        if (cacheManager != null && cacheItem == null) {
            SendMessage(MSG_INIT_CACHE, this);
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
            DestroyHandlerThread();
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

    private boolean updateView() {
        if (!isAttachedToWindow) {
            return true;
        }
        return flush();
    }

    private KeyItem fetchKeyFrame() {
        if (!decoderInfo.isValid() || _composition == null && _pagFilePath == null) {
            return null;
        }
        String keyPrefix =
                "_" + (decoderInfo._width << 16 | decoderInfo._height) + "_" + decoderInfo.realFrameRate;
        boolean autoClean = false;
        if (_pagFilePath != null) {
            keyPrefix = _pagFilePath + keyPrefix;
        } else if (_composition instanceof PAGFile && _composition.contentVersion() == 0) {
            keyPrefix = ((PAGFile) _composition).path() + keyPrefix;
        } else {
            keyPrefix = _composition.toString() + "_" + _composition.contentVersion() + keyPrefix;
            autoClean = true;
        }
        if (lastKeyItem != null && keyPrefix.equals(lastKeyItem.keyPrefix)) {
            return lastKeyItem;
        }
        KeyItem keyItem = new KeyItem();
        keyItem.keyPrefix = keyPrefix;
        keyItem.keyPrefixMD5 = getMD5(keyPrefix);
        keyItem.needAutoClean = autoClean;
        return keyItem;
    }

    private boolean inflateBitmapFromDiskCache(BitmapResource bitmapResource, CacheInfo cacheInfo) {
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

    private boolean putBitmapToDiskCache(int frame, BitmapPool.BitmapResource bitmap) {
        if (cacheItem == null || bitmap == null) {
            return false;
        }
        boolean success = cacheItem.saveBitmap(frame, bitmap.get());
        return success;
    }

    private static String getMD5(String str) {
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
        if (decoderInfo._pagDecoder == null || !decoderInfo._pagDecoder.frameAtIndex(currentImage.get(),
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

    private boolean flush() {
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

    private void notifyAnimationUpdate() {
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
                Matrix scaleModeMatrix = ApplyScaleMode(_scaleMode, decoderInfo._width,
                        decoderInfo._height,
                        width, height);
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

    private static Matrix ApplyScaleMode(int scaleMode, int sourceWidth, int sourceHeight,
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
}
