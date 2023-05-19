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
import android.hardware.HardwareBuffer;
import android.os.Build;
import android.os.Looper;
import android.provider.Settings;
import android.util.AttributeSet;
import android.util.Log;
import android.util.Pair;
import android.view.View;
import android.view.animation.LinearInterpolator;

import java.util.ArrayList;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;

import org.extra.tools.LibraryLoadUtils;
import org.libpag.PAGImageViewHelper.DecoderInfo;

public class PAGImageView extends View {

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

    private final static String TAG = "PAGImageView";
    private final static float DEFAULT_MAX_FRAMERATE = 30f;
    private ValueAnimator animator;
    private volatile boolean _isPlaying = false;
    private volatile Boolean _isAnimatorPreRunning = null;
    private volatile boolean progressExplicitlySet = true;
    private final Object updateTimeLock = new Object();
    private static final Object g_HandlerLock = new Object();
    private float _maxFrameRate = DEFAULT_MAX_FRAMERATE;
    private final AtomicBoolean freezeDraw = new AtomicBoolean(false);
    protected volatile DecoderInfo decoderInfo = new DecoderInfo();
    private volatile Bitmap renderBitmap;
    private volatile HardwareBuffer renderHardwareBuffer;
    private volatile Bitmap backgroundBitmap;
    private volatile HardwareBuffer backgroundHardwareBuffer;
    private Matrix renderMatrix;
    private final ConcurrentHashMap<Integer, Bitmap> bitmapCache = new ConcurrentHashMap<>();

    /**
     * [Deprecated](Please use PAGDiskCache.MaxDiskSize() instead.)
     * Returns the size limit of the disk cache in bytes.
     */
    @Deprecated
    public static long MaxDiskCache() {
        return PAGDiskCache.MaxDiskSize();
    }

    /**
     * [Deprecated](Please use PAGDiskCache.SetMaxDiskSize() instead.)
     * Sets the size limit of the disk cache in bytes. The default disk cache limit is 1 GB.
     */
    @Deprecated
    public static void SetMaxDiskCache(long maxDiskCache) {
        PAGDiskCache.SetMaxDiskSize(maxDiskCache);
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

    private String _pagFilePath;

    /**
     * Returns the file path set by the setPath() method.
     */
    public String getPath() {
        return _pagFilePath;
    }

    /**
     * Loads a pag file from the specified path, returns false if the file does not exist, or it is
     * not a valid pag file.
     */
    public boolean setPath(String path) {
        return setPath(path, DEFAULT_MAX_FRAMERATE);
    }

    /**
     * Loads a pag file from the specified path with the maxFrameRate limit, returns false if the
     * file does not exist, or it is not a valid pag file.
     */
    public boolean setPath(String path, float maxFrameRate) {
        PAGComposition composition = getCompositionFromPath(path);
        refreshResource(path, composition, maxFrameRate);
        return composition != null;
    }

    private PAGComposition _composition;

    /**
     * Returns the current PAGComposition in the PAGImageView. Returns null if the internal
     * composition was loaded from a pag file path.
     */
    public PAGComposition getComposition() {
        return _pagFilePath != null ? null : _composition;
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
        if (newComposition == _composition && _maxFrameRate == maxFrameRate && decoderInfo.isValid()) {
            return;
        }
        refreshResource(null, newComposition, maxFrameRate);
    }

    private int _scaleMode = PAGScaleMode.LetterBox;
    private volatile Matrix _matrix;

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


    private float _renderScale = 1.0f;

    /**
     * This value defines the scale factor for the size of the cached image frames, which ranges
     * from 0.0 to 1.0. A scale factor less than 1.0 may result in blurred output, but it can reduce
     * graphics memory usage, increasing the rendering performance. The default value is 1.0.
     */
    public float renderScale() {
        return _renderScale;
    }

    /**
     * Sets the value of the renderScale property.
     */
    public void setRenderScale(float renderScale) {
        if (this._renderScale == renderScale) {
            return;
        }
        if (renderScale < 0.0f || renderScale > 1.0f) {
            renderScale = 1.0f;
        }
        this._renderScale = renderScale;
        width = (int) (viewWidth * renderScale);
        height = (int) (viewHeight * renderScale);
        refreshMatrixFromScaleMode();
        if (renderScale < 1.0f) {
            renderMatrix = new Matrix();
            renderMatrix.setScale(1 / renderScale, 1 / renderScale);
        }
    }

    private boolean _cacheAllFramesInMemory = false;

    /**
     * If set to true, the PAGImageView loads all image frames into the memory, which will
     * significantly increase the rendering performance but may cost lots of additional memory. Set
     * it to true if you prefer rendering speed over memory usage. If set to false, the PAGImageView
     * loads only one image frame at a time into the memory. The default value is false.
     */
    public boolean cacheAllFramesInMemory() {
        return _cacheAllFramesInMemory;
    }

    private volatile boolean memoryCacheStatusHasChanged = false;

    /**
     * Sets the value of the cacheAllFramesInMemory property.
     */
    public void setCacheAllFramesInMemory(boolean enable) {
        memoryCacheStatusHasChanged = enable != _cacheAllFramesInMemory;
        _cacheAllFramesInMemory = enable;
    }

    private int _currentFrame;

    /**
     * Returns the current frame index the PAGImageView is rendering.
     */
    public int currentFrame() {
        return _currentFrame;
    }

    private int _numFrames = 0;

    /**
     * Returns the number of frames in the PAGImageView in one loop. Note that the value may change
     * if the associated PAGComposition was modified.
     */
    public int numFrames() {
        refreshNumFrames();
        return _numFrames;
    }

    /**
     * Sets the frame index for the PAGImageView to render.
     */
    public void setCurrentFrame(int currentFrame) {
        refreshNumFrames();
        if (_numFrames == 0 || !decoderInfo.isValid() || currentFrame < 0) {
            return;
        }
        if (currentFrame >= _numFrames) {
            return;
        }
        synchronized (updateTimeLock) {
            _currentFrame = currentFrame;
            syncCurrentTime();
            progressExplicitlySet = true;
        }
    }

    /**
     * Returns a bitmap capturing the contents of the current PAGImageView.
     */
    public Bitmap currentImage() {
        return renderBitmap;
    }

    /**
     * Starts to play the animation.
     */
    public void play() {
        if (_isPlaying) {
            return;
        }
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

    /**
     * Renders the current image frame immediately. Note that all the changes previously made to the
     * PAGImageView will only take effect after this method is called. If the play() method is
     * already called, there is no need to call it manually since it will be automatically called
     * every frame. Returns true if the content has changed.
     */
    public boolean flush() {
        if (!decoderInfo.isValid()) {
            initDecoderInfo();
            if (!decoderInfo.isValid()) {
                return false;
            }
        }
        if (decoderInfo._pagDecoder != null) {
            _numFrames = decoderInfo._pagDecoder.numFrames();
        }
        if (progressExplicitlySet) {
            progressExplicitlySet = false;
            if (!handleFrame(_currentFrame)) {
                forceFlush = false;
                return false;
            }
            synchronized (updateTimeLock) {
                syncCurrentTime();
                animator.setCurrentPlayTime(currentPlayTime);
            }
        } else {
            int currentFrame = 0;
            synchronized (updateTimeLock) {
                currentFrame = PAGImageViewHelper.ProgressToFrame(animator.getAnimatedFraction(), _numFrames);
            }
            if (currentFrame == _currentFrame && !forceFlush) {
                return false;
            }
            _currentFrame = currentFrame;
            if (!handleFrame(_currentFrame)) {
                forceFlush = false;
                return false;
            }
        }
        forceFlush = false;
        postInvalidate();
        notifyAnimationUpdate();
        return true;
    }


    int lastContentVersion = -1;

    private PAGComposition getCompositionFromPath(String path) {
        if (path == null) {
            return null;
        }
        PAGComposition composition;
        if (path.startsWith("assets://")) {
            composition = PAGFile.Load(getContext().getAssets(), path.substring(9));
        } else {
            composition = PAGFile.Load(path);
        }
        return composition;
    }

    private void refreshNumFrames() {
        if (!decoderInfo.isValid() && _numFrames == 0 && width > 0) {
            initDecoderInfo();
        }
        if (decoderInfo.isValid() && decoderInfo._pagDecoder != null) {
            _numFrames = decoderInfo._pagDecoder.numFrames();
        }
    }

    private void refreshResource(String path, PAGComposition composition, float maxFrameRate) {
        freezeDraw.set(true);
        decoderInfo.reset();
        _maxFrameRate = maxFrameRate;
        _matrix = null;
        releaseBitmap();
        _pagFilePath = path;
        _composition = composition;
        _currentFrame = 0;
        progressExplicitlySet = true;
        synchronized (updateTimeLock) {
            animator.setDuration(_composition == null ? 0 : _composition.duration() / 100);
            animator.setCurrentPlayTime(0);
            currentPlayTime = 0;
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

    private float getAnimationScale(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return Settings.Global.getFloat(context.getContentResolver(), Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f);
        } else {
            // noinspection deprecation
            return Settings.System.getFloat(context.getContentResolver(), Settings.System.ANIMATOR_DURATION_SCALE, 1.0f);
        }
    }

    private volatile long currentPlayTime;

    private final ValueAnimator.AnimatorUpdateListener mAnimatorUpdateListener = animation -> {
        PAGImageView.this.currentPlayTime = animation.getCurrentPlayTime();
        PAGImageViewHelper.NeedsUpdateView(PAGImageView.this);
    };

    private final ArrayList<PAGImageViewListener> mViewListeners = new ArrayList<>();

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
            if (repeatCount >= 0 && (animation.getDuration() > 0) && (currentPlayTime * 1.0 / animation.getDuration() > repeatCount)) {
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
        freezeDraw.set(true);
        decoderInfo.reset();
        viewWidth = w;
        viewHeight = h;
        width = (int) (_renderScale * w);
        height = (int) (_renderScale * h);
        releaseBitmap();
        forceFlush = true;
        resumeAnimator();
    }

    protected synchronized void initDecoderInfo() {
        if (!decoderInfo.isValid()) {
            if (_composition == null) {
                _composition = getCompositionFromPath(_pagFilePath);
            }
            if (decoderInfo.initDecoder(_composition, width, height, _maxFrameRate)) {
                if (_pagFilePath != null) {
                    _composition = null;
                }
                animator.setDuration(decoderInfo.duration / 1000);
                if (!decoderInfo.isValid()) {
                    return;
                }
            }
        }
        refreshMatrixFromScaleMode();
        freezeDraw.set(false);
    }

    private float animationScale = 1.0f;

    private void init() {
        animator = ValueAnimator.ofFloat(0.0f, 1.0f);
        animator.setRepeatCount(0);
        animator.setInterpolator(new LinearInterpolator());
        animationScale = getAnimationScale(getContext());
    }

    private volatile boolean isAttachedToWindow = false;

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
        PAGImageViewHelper.RemoveMessage(PAGImageViewHelper.MSG_CLOSE_CACHE, this);
        PAGImageViewHelper.SendMessage(PAGImageViewHelper.MSG_CLOSE_CACHE, this);
        synchronized (g_HandlerLock) {
            PAGImageViewHelper.DestroyHandlerThread();
        }
        if (_isAnimatorPreRunning == null || _isAnimatorPreRunning) {
            releaseBitmap();
        }
        bitmapCache.clear();
        lastContentVersion = -1;
        memoryCacheStatusHasChanged = false;
        freezeDraw.set(false);
    }

    private void releaseBitmap() {
        renderBitmap = null;
        backgroundBitmap = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (renderHardwareBuffer != null) {
                renderHardwareBuffer.close();
                renderHardwareBuffer = null;
            }
            if (backgroundHardwareBuffer != null) {
                backgroundHardwareBuffer.close();
                backgroundHardwareBuffer = null;
            }
        }
    }

    private final Runnable mAnimatorStartRunnable = new Runnable() {
        @Override
        public void run() {
            if (isAttachedToWindow) {
                animator.start();
            } else {
                Log.e(TAG, "AnimatorStartRunnable: PAGView is not attached to window");
            }
        }
    };

    private final Runnable mAnimatorCancelRunnable = new Runnable() {
        @Override
        public void run() {
            synchronized (updateTimeLock) {
                currentPlayTime = animator.getCurrentPlayTime();
                animator.cancel();
            }
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
            synchronized (updateTimeLock) {
                currentPlayTime = animator.getCurrentPlayTime();
                animator.cancel();
            }
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

    private boolean allInMemoryCache() {
        if (decoderInfo.isValid() && decoderInfo._pagDecoder != null) {
            _numFrames = decoderInfo._pagDecoder.numFrames();
        }
        return bitmapCache.size() == _numFrames;
    }

    private void releaseDecoder() {
        if (allInMemoryCache()) {
            if (decoderInfo._pagDecoder != null) {
                decoderInfo.releaseDecoder();
            }
        }
    }

    private void checkStatusChange() {
        boolean needResetBitmapCache = false;
        if (memoryCacheStatusHasChanged) {
            needResetBitmapCache = true;
            memoryCacheStatusHasChanged = false;
        }
        if (_pagFilePath == null && _composition != null) {
            int nVersion = ContentVersion(_composition);
            if (lastContentVersion >= 0 && lastContentVersion != nVersion) {
                needResetBitmapCache = true;
            }
            lastContentVersion = nVersion;
        }
        if (needResetBitmapCache) {
            bitmapCache.clear();
            if (decoderInfo._pagDecoder == null) {
                PAGComposition composition = _composition;
                if (composition == null) {
                    composition = getCompositionFromPath(_pagFilePath);
                }
                decoderInfo.initDecoder(composition, width, height, _maxFrameRate);
            }
        }
    }

    private boolean checkBackgroundBitmap() {
        if (backgroundBitmap == null) {
            Pair<Bitmap, HardwareBuffer> pair = BitmapHelper.CreateBitmap(decoderInfo._width, decoderInfo._height, false);
            if (pair.first == null) {
                return false;
            }
            backgroundHardwareBuffer = pair.second;
            backgroundBitmap = pair.first;
        }
        return true;
    }

    private void swapBitmap() {
        Bitmap bitmap = renderBitmap;
        renderBitmap = backgroundBitmap;
        backgroundBitmap = bitmap;
        HardwareBuffer hardwareBuffer = renderHardwareBuffer;
        renderHardwareBuffer = backgroundHardwareBuffer;
        backgroundHardwareBuffer = hardwareBuffer;
    }

    private boolean handleFrame(final int frame) {
        if (!decoderInfo.isValid() || freezeDraw.get()) {
            return false;
        }
        checkStatusChange();
        releaseDecoder();
        Bitmap bitmap = bitmapCache.get(frame);
        if (bitmap != null) {
            renderBitmap = bitmap;
            return true;
        }
        if (freezeDraw.get()) {
            return false;
        }
        if (decoderInfo._pagDecoder == null) {
            return false;
        }
        if (!forceFlush && !decoderInfo._pagDecoder.checkFrameChanged(frame)) {
            return true;
        }
        if (renderBitmap == null || _cacheAllFramesInMemory) {
            Pair<Bitmap, HardwareBuffer> pair = BitmapHelper.CreateBitmap(decoderInfo._width,
                    decoderInfo._height, false);
            if (pair.first == null) {
                return false;
            }
            renderHardwareBuffer = pair.second;
            renderBitmap = pair.first;
        }
        HardwareBuffer flushBuffer;
        Bitmap flushBitmap;
        if (!_cacheAllFramesInMemory) {
            if (!checkBackgroundBitmap()) {
                return false;
            }
            flushBuffer = backgroundHardwareBuffer;
            flushBitmap = backgroundBitmap;
        } else {
            flushBuffer = renderHardwareBuffer;
            flushBitmap = renderBitmap;
        }
        if (flushBuffer != null) {
            if (!decoderInfo._pagDecoder.readFrame(frame, flushBuffer)) {
                return false;
            }
        } else {
            if (!decoderInfo._pagDecoder.copyFrameTo(flushBitmap, frame)) {
                return false;
            }
            flushBitmap.prepareToDraw();
        }
        if (!_cacheAllFramesInMemory) {
            swapBitmap();
        }
        if (_cacheAllFramesInMemory && renderBitmap != null) {
            bitmapCache.put(frame, renderBitmap);
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
        if (!freezeDraw.get() && renderBitmap != null && !renderBitmap.isRecycled()) {
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

    private boolean hasSize() {
        return width > 0 && height > 0;
    }

    private void refreshMatrixFromScaleMode() {
        if (_scaleMode == PAGScaleMode.None) {
            return;
        }
        _matrix = PAGImageViewHelper.ApplyScaleMode(_scaleMode, decoderInfo._width, decoderInfo._height, width, height);
    }

    private static native int ContentVersion(PAGComposition pagComposition);

    private void syncCurrentTime() {
        long playTime = 0;
        if (animator.getDuration() > 0) {
            long repeatCount = currentPlayTime / animator.getDuration();
            if (animator.getAnimatedFraction() == 1.0f) {
                repeatCount = Math.round(currentPlayTime * 1.0 / animator.getDuration()) - 1;
            }
            playTime = (long) ((PAGImageViewHelper.FrameToProgress(_currentFrame, _numFrames) + repeatCount) * animator.getDuration());
        }
        currentPlayTime = playTime;
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
