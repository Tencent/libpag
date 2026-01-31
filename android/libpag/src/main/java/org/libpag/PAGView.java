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

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.graphics.drawable.Drawable;
import android.opengl.EGLContext;
import android.os.Build;
import android.util.AttributeSet;
import android.view.TextureView;
import android.view.View;
import org.extra.tools.Lifecycle;
import org.extra.tools.LifecycleListener;
import java.util.ArrayList;


public class PAGView extends TextureView implements TextureView.SurfaceTextureListener,
        LifecycleListener, PAGAnimator.Listener {

    public interface PAGViewListener {
        /**
         * Notifies the start of the animation. It can be called from either the UI thread or the
         * thread that calls the play() method.
         */
        void onAnimationStart(PAGView view);

        /**
         * Notifies the end of the animation. It can only be called from the UI thread.
         */
        void onAnimationEnd(PAGView view);

        /**
         * Notifies the cancellation of the animation. It can be called from either the UI thread or
         * the thread that calls the stop() method.
         */
        void onAnimationCancel(PAGView view);

        /**
         * Notifies the repetition of the animation. It can only be called from the UI thread.
         */
        void onAnimationRepeat(PAGView view);

        /**
         * Notifies another frame of the animation has occurred. It may be called from an arbitrary
         * thread if the animation is running asynchronously.
         */
        void onAnimationUpdate(PAGView view);
    }

    private SurfaceTextureListener mListener;
    private PAGPlayer pagPlayer;
    private PAGSurface pagSurface;
    private PAGAnimator animator;
    private String filePath = "";
    private boolean isAttachedToWindow = false;
    private EGLContext sharedContext = null;

    /**
     * [Deprecated](Please use PAGViewListener's onAnimationUpdate instead.)
     * PAG flush callback listener.
     * if add this listener, the PAG View onSurfaceTextureAvailable will send pag flush async, and
     * this will make PAG View hasn't content until the async flush end.
     */
    @Deprecated
    public interface PAGFlushListener {
        /**
         * PAG flush callback
         */
        void onFlush();
    }

    private final ArrayList<PAGViewListener> mViewListeners = new ArrayList<>();
    private final ArrayList<PAGFlushListener> mPAGFlushListeners = new ArrayList<>();

    public PAGView(Context context) {
        super(context);
        setupSurfaceTexture();
    }

    public PAGView(Context context, EGLContext sharedContext) {
        super(context);
        this.sharedContext = sharedContext;
        setupSurfaceTexture();
    }

    public PAGView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setupSurfaceTexture();
    }

    public PAGView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setupSurfaceTexture();
    }

    /**
     * Adds a listener to the set of listeners that are sent events through the life of an
     * animation, such as start, repeat, and end.
     */
    public void addListener(PAGViewListener listener) {
        synchronized (this) {
            mViewListeners.add(listener);
        }
    }

    /**
     * Removes a listener from the set listening to this animation.
     */
    public void removeListener(PAGViewListener listener) {
        synchronized (this) {
            mViewListeners.remove(listener);
        }
    }

    /**
     * [Deprecated](Please use PAGViewListener's onAnimationUpdate instead.)
     * Adds a PAGFlushListener to the set of listeners that are sent events when pag flush called.
     * the listener onFlush is called on main thread.
     */
    @Deprecated
    public void addPAGFlushListener(PAGFlushListener listener) {
        synchronized (this) {
            mPAGFlushListeners.add(listener);
        }
    }

    /**
     * [Deprecated](Please use PAGViewListener's onAnimationUpdate instead.)
     * Removes a PAGFlushListener to the set of listeners that are sent events when pag flush
     * called. The listener onFlush is called on main thread.
     */
    @Deprecated
    public void removePAGFlushListener(PAGFlushListener listener) {
        synchronized (this) {
            mPAGFlushListeners.remove(listener);
        }
    }

    /**
     * Returns true if PAGView is playing in the main thread. The default value is false.
     */
    public boolean isSync() {
        return animator.isSync();
    }

    /**
     * Sets the sync flag for the PAGView.
     */
    public void setSync(boolean isSync) {
        animator.setSync(isSync);
    }

    /**
     * The total number of times the animation is set to play. The default is 1, which means the
     * animation will play only once. If the repeat count is set to 0 or a negative value, the
     * animation will play infinity times.
     */
    public int repeatCount() {
        return animator.repeatCount();
    }

    /**
     * Set the number of times the animation to play.
     */
    public void setRepeatCount(int repeatCount) {
        animator.setRepeatCount(repeatCount);
    }

    /**
     * Indicates whether the animation is playing.
     */
    public boolean isPlaying() {
        return animator.isRunning();
    }

    /**
     * Starts to play the animation from the current position. Calling the play() method when the
     * animation is already playing has no effect. The play() method does not alter the animation's
     * current position. However, if the animation previously reached its end, it will restart from
     * the beginning.
     */
    public void play() {
        pagPlayer.prepare();
        animator.start();
    }

    /**
     * Cancels the animation at the current position. Calling the play() method can resume the
     * animation from the last paused position.
     */
    public void pause() {
        animator.cancel();
    }

    /**
     * Cancels the animation at the current position. Currently, it has the same effect as pause().
     */
    public void stop() {
        animator.cancel();
    }

    /**
     * The path string of a pag file set by setPath().
     */
    public String getPath() {
        return filePath;
    }

    /**
     * Loads a pag file from the specified path, returns false if the file does not exist or the
     * data is not a pag file. The path starts with "assets://" means that it is located in assets
     * directory. Note: All PAGFiles loaded by the same path share the same internal cache. The
     * internal cache remains alive until all PAGFiles are released. Use 'PAGFile.Load(byte[])'
     * instead if you don't want to load a PAGFile from the internal caches.
     */
    public boolean setPath(String path) {
        PAGFile pagFile;
        if (path != null && path.startsWith("assets://")) {
            pagFile = PAGFile.Load(getContext().getAssets(), path.substring(9));
        } else {
            pagFile = PAGFile.Load(path);
        }
        setComposition(pagFile);
        filePath = path;
        return pagFile != null;
    }

    /**
     * Asynchronously load a pag file from the specific path.
     */
    public void setPathAsync(String path, PAGFile.LoadListener listener) {
        NativeTask.Run(() -> {
            setPath(path);
            if (listener != null) {
                listener.onLoad((PAGFile) pagPlayer.getComposition());
            }
        });
    }

    /**
     * Returns the current PAGComposition for PAGView to render as content.
     */
    public PAGComposition getComposition() {
        return pagPlayer.getComposition();
    }

    /**
     * Sets a new PAGComposition for PAGView to render as content. Note that if the composition is
     * already added to another PAGView, it will be removed from the previous PAGView.
     */
    public void setComposition(PAGComposition newComposition) {
        filePath = null;
        pagPlayer.setComposition(newComposition);
        animator.setProgress(pagPlayer.getProgress());
        if (isVisible) {
            animator.setDuration(pagPlayer.duration());
        }
    }

    /**
     * If set to false, PAGView skips rendering for video composition.
     */
    public boolean videoEnabled() {
        return pagPlayer.videoEnabled();
    }

    /**
     * Sets the value of videoEnabled property.
     */
    public void setVideoEnabled(boolean enable) {
        pagPlayer.setVideoEnabled(enable);
    }

    /**
     * If set to true, PAG renderer caches an internal bitmap representation of the static content
     * for each layer. This caching can increase performance for layers that contain complex vector
     * content. The execution speed can be significantly faster depending on the complexity of the
     * content, but it requires extra graphics memory. The default value is true.
     */
    public boolean cacheEnabled() {
        return pagPlayer.cacheEnabled();
    }

    /**
     * Sets the value of cacheEnabled property.
     */
    public void setCacheEnabled(boolean value) {
        pagPlayer.setCacheEnabled(value);
    }

    /**
     * If set to true, PAG will cache the associated rendering data into a disk file, such as the
     * decoded image frames of video compositions. This can help reduce memory usage and improve
     * rendering performance.
     */
    public boolean useDiskCache() {
        return pagPlayer.useDiskCache();
    }

    /**
     * Set the value of useDiskCache property.
     */
    public void setUseDiskCache(boolean value) {
        pagPlayer.setUseDiskCache(value);
    }

    /**
     * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
     * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of
     * graphics memory which leads to better performance. The default value is 1.0.
     */
    public float cacheScale() {
        return pagPlayer.cacheScale();
    }

    /**
     * Sets the value of cacheScale property.
     */
    public void setCacheScale(float value) {
        pagPlayer.setCacheScale(value);
    }

    /**
     * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
     * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
     * value is 60.
     */
    public float maxFrameRate() {
        return pagPlayer.maxFrameRate();
    }

    /**
     * Sets the maximum frame rate for rendering.
     */
    public void setMaxFrameRate(float value) {
        pagPlayer.setMaxFrameRate(value);
    }

    /**
     * Returns the current scale mode.
     */
    public int scaleMode() {
        return pagPlayer.scaleMode();
    }

    /**
     * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
     * changes when this method is called.
     */
    public void setScaleMode(int mode) {
        pagPlayer.setScaleMode(mode);
    }

    /**
     * Returns a copy of current matrix.
     */
    public Matrix matrix() {
        return pagPlayer.matrix();
    }

    /**
     * Sets the transformation which will be applied to the composition. The scaleMode property
     * will be set to PAGScaleMode::None when this method is called.
     */
    public void setMatrix(Matrix matrix) {
        pagPlayer.setMatrix(matrix);
    }

    /**
     * The duration of current composition in microseconds.
     */
    public long duration() {
        return pagPlayer.duration();
    }

    /**
     * Returns the current progress of play position, the value is from 0.0 to 1.0.
     */
    public double getProgress() {
        return pagPlayer.getProgress();
    }

    /**
     * Sets the progress of play position, the valid value is from 0.0 to 1.0.
     */
    public void setProgress(double value) {
        pagPlayer.setProgress(value);
        animator.setProgress(pagPlayer.getProgress());
        // TODO(domchen): Remove the next line. All pending changes should be applied in flush().
        animator.update();
    }

    /**
     * Returns the current frame.
     */
    public long currentFrame() {
        return pagPlayer.currentFrame();
    }

    /**
     * Call this method to render current position immediately. Note that all the changes previously
     * made to the PAGView will only take effect after this method is called. If the play() method
     * is already called, there is no need to call it manually since it will be automatically called
     * every frame. Returns true if the content has changed.
     */
    public boolean flush() {
        return pagPlayer.flush();
    }

    /**
     * Returns an array of layers that lie under the specified point.
     * The point is in pixels not dp.
     */
    public PAGLayer[] getLayersUnderPoint(float x, float y) {
        return pagPlayer.getLayersUnderPoint(x, y);
    }

    /**
     * Free the cache created by the pag view immediately. Can be called to reduce memory pressure.
     */
    public void freeCache() {
        if (pagSurface != null) {
            pagSurface.freeCache();
        }
    }

    /**
     * Returns a bitmap capturing the contents of the PAGView. Subsequent rendering of the
     * PAGView will not be captured. Returns null if the PAGView hasn't been presented yet.
     */
    public Bitmap makeSnapshot() {
        if (pagSurface != null) {
            return pagSurface.makeSnapshot();
        }
        return null;
    }

    /**
     * Returns a rectangle in pixels that defines the displaying area of the specified layer, which
     * is in the coordinate of the PAGView.
     */
    public RectF getBounds(PAGLayer pagLayer) {
        if (pagLayer != null) {
            return pagPlayer.getBounds(pagLayer);
        }
        return new RectF();
    }

    private void setupSurfaceTexture() {
        Lifecycle.getInstance().addListener(this);
        setOpaque(false);
        pagPlayer = new PAGPlayer();
        setSurfaceTextureListener(this);
        animator = PAGAnimator.MakeFrom(getContext(), this);
    }

    private void updateTextureView() {
        PAGView.this.post(new Runnable() {
            @Override
            public void run() {
                boolean opaque = PAGView.this.isOpaque();
                // In order to call updateLayerAndInvalidate to update the TextureView.
                PAGView.this.setOpaque(!opaque);
                PAGView.this.setOpaque(opaque);
            }
        });
    }

    @Override
    public void setSurfaceTextureListener(SurfaceTextureListener listener) {
        if (listener == this) {
            super.setSurfaceTextureListener(listener);
        } else {
            mListener = listener;
        }
    }

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
        if (pagSurface != null) {
            pagSurface.release();
            pagSurface = null;
        }
        pagSurface = PAGSurface.FromSurfaceTexture(surface, sharedContext);
        pagPlayer.setSurface(pagSurface);
        if (pagSurface == null) {
            return;
        }
        pagSurface.clearAll();
        animator.update();
        if (mListener != null) {
            mListener.onSurfaceTextureAvailable(surface, width, height);
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surfaceTexture, int width, int height) {
        if (pagSurface != null) {
            pagSurface.updateSize();
            pagSurface.clearAll();
            animator.update();
        }
        if (mListener != null) {
            mListener.onSurfaceTextureSizeChanged(surfaceTexture, width, height);
        }
    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        pagPlayer.setSurface(null);
        if (mListener != null) {
            mListener.onSurfaceTextureDestroyed(surface);
        }
        if (pagSurface != null) {
            pagSurface.freeCache();
        }
        // 延迟释放 SurfaceTexture，否则Android 4.4之前版本会在 onDetachedFromWindow() 时 Crash:
        // https://www.jianshu.com/p/675455c225bd
        post(new Runnable() {
            @Override
            public void run() {
                surface.release();
            }
        });
        return false;
    }


    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        if (mListener != null) {
            mListener.onSurfaceTextureUpdated(surface);
        }
    }

    @Override
    protected void onAttachedToWindow() {
        isAttachedToWindow = true;
        super.onAttachedToWindow();
        checkVisible();
    }

    @Override
    protected void onDetachedFromWindow() {
        isAttachedToWindow = false;
        super.onDetachedFromWindow();
        if (pagSurface != null) {
            pagSurface.release();
            pagSurface = null;
        }
        checkVisible();
    }


    @Override
    public void setBackgroundDrawable(Drawable background) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N && background != null) {
            super.setBackgroundDrawable(background);
        }
    }

    @Override
    public void onVisibilityAggregated(boolean isVisible) {
        super.onVisibilityAggregated(isVisible);
        checkVisible();
    }

    private boolean isVisible = false;

    private void checkVisible() {
        boolean visible = isAttachedToWindow && isShown();
        if (isVisible == visible) {
            return;
        }
        isVisible = visible;
        if (isVisible) {
            animator.setDuration(pagPlayer.duration());
            animator.update();
        } else {
            animator.setDuration(0);
        }
    }

    @Override
    public void onResume() {
        // When the device is locked and then unlocked, the PAGView's content may disappear,
        // use the following way to make the content appear.
        if (isVisible) {
            setVisibility(View.INVISIBLE);
            setVisibility(View.VISIBLE);
        }
    }

    public void onAnimationStart(PAGAnimator animator) {
        ArrayList<PAGViewListener> arrayList;
        synchronized (PAGView.this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGViewListener listener : arrayList) {
            listener.onAnimationStart(this);
        }
    }

    public void onAnimationEnd(PAGAnimator animator) {
        ArrayList<PAGViewListener> arrayList;
        synchronized (PAGView.this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGViewListener listener : arrayList) {
            listener.onAnimationEnd(this);
        }
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        // When calling the setVisibility function,
        // it does not necessarily trigger a callback to the onVisibilityAggregated method.
        // Therefore, it is necessary to handle this specific situation on your own.
        checkVisible();
    }

    public void onAnimationCancel(PAGAnimator animator) {
        ArrayList<PAGViewListener> arrayList;
        synchronized (PAGView.this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGViewListener listener : arrayList) {
            listener.onAnimationCancel(this);
        }
    }

    public void onAnimationRepeat(PAGAnimator animator) {
        ArrayList<PAGViewListener> arrayList;
        synchronized (PAGView.this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGViewListener listener : arrayList) {
            listener.onAnimationRepeat(this);
        }
    }

    public void onAnimationUpdate(PAGAnimator animator) {
        pagPlayer.setProgress(animator.progress());
        synchronized (PAGView.this) {
            if (!isAttachedToWindow) {
                return;
            }
        }
        if (isVisible) {
            animator.setDuration(pagPlayer.duration());
        }
        boolean changed = flush();
        if (changed) {
            updateTextureView();
        }
        ArrayList<PAGViewListener> arrayList;
        synchronized (PAGView.this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGViewListener listener : arrayList) {
            listener.onAnimationUpdate(this);
        }
        if (!mPAGFlushListeners.isEmpty()) {
            PAGView.this.post(new Runnable() {
                @Override
                public void run() {
                    ArrayList<PAGFlushListener> arrayList;
                    synchronized (PAGView.this) {
                        arrayList = new ArrayList<>(mPAGFlushListeners);
                    }
                    for (PAGFlushListener listener : arrayList) {
                        listener.onFlush();
                    }
                }
            });
        }
    }
}
