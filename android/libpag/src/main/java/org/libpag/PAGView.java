package org.libpag;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.graphics.drawable.Drawable;
import android.opengl.EGLContext;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.provider.Settings;
import android.util.AttributeSet;
import android.util.Log;
import android.util.SparseArray;
import android.view.TextureView;
import android.view.View;
import android.view.animation.LinearInterpolator;

import org.extra.tools.Lifecycle;
import org.extra.tools.LifecycleListener;

import java.util.ArrayList;
import java.util.List;


public class PAGView extends TextureView implements TextureView.SurfaceTextureListener, LifecycleListener {

    public interface PAGViewListener {
        /**
         * Notifies the start of the animation.
         */
        void onAnimationStart(PAGView view);

        /**
         * Notifies the end of the animation.
         */
        void onAnimationEnd(PAGView view);

        /**
         * Notifies the cancellation of the animation.
         */
        void onAnimationCancel(PAGView view);

        /**
         * Notifies the repetition of the animation.
         */
        void onAnimationRepeat(PAGView view);

        /**
         * Notifies the occurrence of another frame of the animation.
         */
        void onAnimationUpdate(PAGView view);
    }

    private final static String TAG = "PAGView";
    private SurfaceTextureListener mListener;
    private PAGPlayer pagPlayer;
    private PAGSurface pagSurface;
    private PAGFile pagFile;
    private ValueAnimator animator;
    private volatile boolean _isPlaying = false;
    private volatile Boolean _isAnimatorPreRunning = null;
    private String filePath = "";
    private boolean isAttachedToWindow = false;
    private EGLContext sharedContext = null;
    private boolean mSaveVisibleState;
    private SparseArray<PAGText> textReplacementMap = new SparseArray<>();
    private SparseArray<PAGImage> imageReplacementMap = new SparseArray<>();
    private boolean isSync = false;
    private volatile boolean progressExplicitlySet = true;
    private final Object animatorLock = new Object();

    private static final Object g_HandlerLock = new Object();
    private static PAGViewHandler g_PAGViewHandler = null;
    private static HandlerThread g_PAGViewThread = null;
    private static volatile int g_HandlerThreadCount = 0;
    private static final int MSG_FLUSH = 0;
    private static final int MSG_SURFACE_DESTROY = 1;
    private static final int MSG_HANDLER_THREAD_QUITE = 2;
    private static final int ANDROID_SDK_VERSION_O = 26;
    private float animationScale = 1.0f;

    private static synchronized void StartHandlerThread() {
        g_HandlerThreadCount++;
        if (g_PAGViewThread == null) {
            g_PAGViewThread = new HandlerThread("pag-renderer");
            try {
                g_PAGViewThread.start();
            } catch (Exception | Error e) {
                e.printStackTrace();
                g_PAGViewThread = null;
            }
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

    private static void NeedsUpdateView(PAGView pagView) {
        if (pagView.isSync) {
            pagView.updateView();
            return;
        }
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
        private List<PAGView> needsUpdateViews = new ArrayList<>();

        PAGViewHandler(Looper looper) {
            super(looper);
        }

        void needsUpdateView(PAGView pagView) {
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
                case MSG_FLUSH:
                    List<PAGView> tempList;
                    synchronized (lock) {
                        tempList = new ArrayList<>(needsUpdateViews);
                        needsUpdateViews.clear();
                    }
                    List<PAGView> flushedViews = new ArrayList<>();
                    for (Object object : tempList) {
                        if (!(object instanceof PAGView)) {
                            continue;
                        }
                        PAGView pagView = (PAGView) object;
                        if (flushedViews.contains(pagView)) {
                            continue;
                        }
                        pagView.updateView();
                        flushedViews.add(pagView);
                    }
                    break;
                case MSG_SURFACE_DESTROY:
                    if (!(msg.obj instanceof SurfaceTexture)) {
                        return;
                    }
                    SurfaceTexture surfaceTexture = (SurfaceTexture) msg.obj;
                    surfaceTexture.release();
                    break;
                case MSG_HANDLER_THREAD_QUITE:
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

    /**
     * PAG flush callback listener.
     * if add this listener, the PAG View onSurfaceTextureAvailable will send pag flush async, and this will make PAG View hasn't content until the async flush end.
     */
    public interface PAGFlushListener {
        /**
         * PAG flush callback
         */
        void onFlush();
    }

    private ArrayList<PAGViewListener> mViewListeners = new ArrayList<>();
    private ArrayList<PAGFlushListener> mPAGFlushListeners = new ArrayList<>();
    private volatile long currentPlayTime = 0;

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

    private final ValueAnimator.AnimatorUpdateListener mAnimatorUpdateListener = new ValueAnimator.AnimatorUpdateListener() {
        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            PAGView.this.currentPlayTime = animation.getCurrentPlayTime();
            NeedsUpdateView(PAGView.this);
        }
    };

    private final AnimatorListenerAdapter mAnimatorListenerAdapter = new AnimatorListenerAdapter() {
        @Override
        public void onAnimationStart(Animator animator) {
            super.onAnimationStart(animator);
            ArrayList<PAGViewListener> arrayList;
            synchronized (PAGView.this) {
                arrayList = new ArrayList<>(mViewListeners);
            }
            for (PAGViewListener listener : arrayList) {
                listener.onAnimationStart(PAGView.this);
            }
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            super.onAnimationEnd(animation);
            // Align with iOS platform, avoid triggering this method when stopping
            int repeatCount = ((ValueAnimator) animation).getRepeatCount();
            if (repeatCount >= 0 && (animation.getDuration() > 0) &&
                    (currentPlayTime * 1.0 / animation.getDuration() > repeatCount)) {
                notifyEnd();
            }
        }

        @Override
        public void onAnimationCancel(Animator animator) {
            super.onAnimationCancel(animator);
            ArrayList<PAGViewListener> arrayList;
            synchronized (PAGView.this) {
                arrayList = new ArrayList<>(mViewListeners);
            }
            for (PAGViewListener listener : arrayList) {
                listener.onAnimationCancel(PAGView.this);
            }
        }

        @Override
        public void onAnimationRepeat(Animator animator) {
            super.onAnimationRepeat(animator);
            ArrayList<PAGViewListener> arrayList;
            synchronized (PAGView.this) {
                arrayList = new ArrayList<>(mViewListeners);
            }
            for (PAGViewListener listener : arrayList) {
                listener.onAnimationRepeat(PAGView.this);
            }
        }
    };

    private void setupSurfaceTexture() {
        Lifecycle.getInstance().addListener(this);
        setOpaque(false);
        pagPlayer = new PAGPlayer();
        setSurfaceTextureListener(this);
        animationScale = getAnimationScale(getContext());
        animator = ValueAnimator.ofFloat(0.0f, 1.0f);
        animator.setRepeatCount(0);
        animator.setInterpolator(new LinearInterpolator());
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

    private void notifyEnd() {
        _isPlaying = false;
        ArrayList<PAGViewListener> arrayList;
        synchronized (PAGView.this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGViewListener listener : arrayList) {
            listener.onAnimationEnd(PAGView.this);
        }
    }

    private void updateView() {
        if (!isAttachedToWindow) {
            return;
        }
        flush();
        updateTextureView();
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
        NeedsUpdateView(this);
        if (mListener != null) {
            mListener.onSurfaceTextureAvailable(surface, width, height);
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surfaceTexture, int width, int height) {
        if (pagSurface != null) {
            pagSurface.updateSize();
            pagSurface.clearAll();
            NeedsUpdateView(this);
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

        boolean surfaceDestroy = true;
        if (g_PAGViewHandler != null && surface != null) {
            SendMessage(MSG_SURFACE_DESTROY, surface);
            surfaceDestroy = false;
        }

        if (Build.VERSION.SDK_INT >= ANDROID_SDK_VERSION_O) {
            synchronized (g_HandlerLock) {
                DestroyHandlerThread();
            }
        }
        return surfaceDestroy;
    }

    @Override
    protected void onAttachedToWindow() {
        isAttachedToWindow = true;
        super.onAttachedToWindow();
        synchronized (animatorLock) {
            animator.addUpdateListener(mAnimatorUpdateListener);
            animator.addListener(mAnimatorListenerAdapter);
        }
        synchronized (g_HandlerLock) {
            StartHandlerThread();
        }
        resumeAnimator();
    }

    @Override
    protected void onDetachedFromWindow() {
        isAttachedToWindow = false;
        super.onDetachedFromWindow();
        if (pagSurface != null) {
            // 延迟释放 pagSurface，否则Android 4.4 及之前版本会在 onDetachedFromWindow() 时 Crash。https://www.jianshu.com/p/675455c225bd
            pagSurface.release();
            pagSurface = null;
        }
        pauseAnimator();
        if (Build.VERSION.SDK_INT < ANDROID_SDK_VERSION_O) {
            synchronized (g_HandlerLock) {
                DestroyHandlerThread();
            }
        }
        synchronized (animatorLock) {
            animator.removeUpdateListener(mAnimatorUpdateListener);
            animator.removeListener(mAnimatorListenerAdapter);
        }
    }

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        if (mListener != null) {
            mListener.onSurfaceTextureUpdated(surface);
        }
    }

    public boolean isPlaying() {
        if (animator != null) {
            return animator.isRunning();
        }
        return false;
    }

    public void play() {
        if (_isPlaying) {
            return;
        }
        _isPlaying = true;
        _isAnimatorPreRunning = null;
        float progress = 0;
        synchronized (animatorLock) {
            progress = animator.getAnimatedFraction();
        }
        if (progress == 1.0) {
            setProgress(0);
        }
        doPlay();
    }

    private Runnable mAnimatorStartRunnable = new Runnable() {
        @Override
        public void run() {
            if (isAttachedToWindow) {
                synchronized (animatorLock) {
                    animator.setCurrentPlayTime(currentPlayTime);
                    animator.start();
                }
            } else {
                Log.e(TAG, "AnimatorStartRunnable: PAGView is not attached to window");
            }
        }
    };

    private void doPlay() {
        pagPlayer.prepare();
        if (!isAttachedToWindow) {
            Log.w(TAG, "doPlay: PAGView is not attached to window");
            return;
        }
        Log.i(TAG, "doPlay");
        if (animationScale == 0.0f) {
            notifyEnd();
            Log.e(TAG, "doPlay: The scale of animator duration is turned off");
            return;
        }
        startAnimator();
    }

    private Runnable mAnimatorCancelRunnable = new Runnable() {
        @Override
        public void run() {
            synchronized (animatorLock) {
                currentPlayTime = animator.getCurrentPlayTime();
                animator.cancel();
            }
        }
    };

    public void stop() {
        Log.i(TAG, "stop");
        _isPlaying = false;
        _isAnimatorPreRunning = null;
        cancelAnimator();
    }

    private boolean isMainThread() {
        return Looper.getMainLooper().getThread() == Thread.currentThread();
    }

    private void startAnimator() {
        if (animator.getDuration() <= 0) {
            return;
        }
        if (isMainThread()) {
            synchronized (animatorLock) {
                animator.setCurrentPlayTime(currentPlayTime);
                animator.start();
            }
        } else {
            removeCallbacks(mAnimatorCancelRunnable);
            post(mAnimatorStartRunnable);
        }
    }

    private void cancelAnimator() {
        if (isMainThread()) {
            synchronized (animatorLock) {
                currentPlayTime = animator.getCurrentPlayTime();
                animator.cancel();
            }
        } else {
            removeCallbacks(mAnimatorStartRunnable);
            post(mAnimatorCancelRunnable);
        }
    }

    /**
     * Sets the number of times the animation will repeat. The default is 1, which means the animation
     * will play only once. 0 means the animation will play infinity times.
     */
    public void setRepeatCount(int value) {
        if (value < 0) {
            value = 0;
        }
        synchronized (animatorLock) {
            animator.setRepeatCount(value - 1);
        }
    }

    /**
     * Adds a PAGViewListener to the set of listeners that are sent events through the life of an
     * animation, such as start, end repeat, and cancel.
     *
     * @param listener the PAGViewListener to be added to the current set of listeners for this animation.
     */
    public void addListener(PAGViewListener listener) {
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
    public void removeListener(PAGViewListener listener) {
        synchronized (this) {
            mViewListeners.remove(listener);
        }
    }

    /**
     * Adds a PAGFlushListener to the set of listeners that are sent events when pag flush called.
     * the listener onFlush is called on main thread.
     *
     * @param listener the PAGFlushListener to be added to the current PAGView.
     */
    public void addPAGFlushListener(PAGFlushListener listener) {
        synchronized (this) {
            mPAGFlushListeners.add(listener);
        }
    }

    /**
     * Removes a PAGFlushListener to the set of listeners that are sent events when pag flush called.
     * the listener onFlush is called on main thread.
     *
     * @param listener the PAGFlushListener to be added to the current PAGView.
     */
    public void removePAGFlushListener(PAGFlushListener listener) {
        synchronized (this) {
            mPAGFlushListeners.remove(listener);
        }
    }

    /**
     * The path string of a pag file set by setPath().
     */
    public String getPath() {
        return filePath;
    }

    /**
     * Loads a pag file from the specified path, returns false if the file does not exist or the data is not a pag file.
     * The path starts with "assets://" means that it is located in assets directory.
     * Note: All PAGFiles loaded by the same path share the same internal cache. The internal cache is alive until all
     * PAGFiles are released. Use 'PAGFile.Load(byte[])' instead if you don't want to load a
     * PAGFile from the internal caches.
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
     * Returns the current PAGComposition for PAGView to render as content.
     */
    public PAGComposition getComposition() {
        return pagPlayer.getComposition();
    }

    /**
     * Sets a new PAGComposition for PAGView to render as content.
     * Note: If the composition is already added to another PAGView, it will be removed from the previous PAGView.
     */
    public void setComposition(PAGComposition newComposition) {
        filePath = null;
        pagFile = null;
        pagPlayer.setComposition(newComposition);
        progressExplicitlySet = true;
        long duration = pagPlayer.duration();
        synchronized (animatorLock) {
            animator.setDuration(duration / 1000);
            animator.setCurrentPlayTime(0);
            currentPlayTime = 0;
            if (newComposition == null) {
                _isPlaying = false;
            }
        }
    }

    /**
     * If set to false, PAGPlayer skips rendering for video composition.
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
     * If set to true, PAG renderer caches an internal bitmap representation of the static content for each layer.
     * This caching can increase performance for layers that contain complex vector content. The execution speed can
     * be significantly faster depending on the complexity of the content, but it requires extra graphics memory.
     * The default value is true.
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
     * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The scale factors less than
     * 1.0 may result in blurred output, but it can reduce the usage of graphics memory which leads to better performance.
     * The default value is 1.0.
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
        synchronized (animatorLock) {
            pagPlayer.setProgress(value);
            syncCurrentTime();
            progressExplicitlySet = true;
        }
        NeedsUpdateView(this);
    }

    /**
     * Returns the current frame.
     */
    public long currentFrame() {
        return pagPlayer.currentFrame();
    }

    /**
     * Returns true if PAGView is playing in the main thread. The default value is false.
     */
    public boolean isSync() {
        return isSync;
    }

    /**
     * Sets the sync flag for the PAGView.
     */
    public void setSync(boolean isSync) {
        this.isSync = isSync;
    }

    /**
     * Call this method to render current position immediately. Note that all the changes previously
     * made to the PAGView will only take effect after this method is called. If the play() method
     * is already called, there is no need to call it manually since it will be automatically called
     * every frame. Returns true if the content has changed.
     */
    public boolean flush() {
        boolean result;
        if (pagSurface == null) {
            result = pagPlayer.flush();
            return result;
        }
        if (progressExplicitlySet) {
            result = pagPlayer.flush();
            progressExplicitlySet = false;
            synchronized (animatorLock) {
                syncCurrentTime();
                animator.setCurrentPlayTime(currentPlayTime);
            }
        } else {
            synchronized (animatorLock) {
                pagPlayer.setProgress(animator.getAnimatedFraction());
            }
            result = pagPlayer.flush();

        }
        ArrayList<PAGViewListener> arrayList;
        synchronized (PAGView.this) {
            arrayList = new ArrayList<>(mViewListeners);
        }
        for (PAGViewListener listener : arrayList) {
            listener.onAnimationUpdate(PAGView.this);
        }
        return result;
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

    @Override
    public void setBackgroundDrawable(Drawable background) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N && background != null) {
            super.setBackgroundDrawable(background);
        }
    }

    private boolean preAggregatedVisible = true;

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

    private void pauseAnimator() {
        if (_isAnimatorPreRunning == null) {
            _isAnimatorPreRunning = animator.isRunning();
        }
        if (animator.isRunning()) {
            cancelAnimator();
        }
    }

    private void resumeAnimator() {
        if (!_isPlaying || animator.isRunning() ||
                (_isAnimatorPreRunning != null && !_isAnimatorPreRunning)) {
            _isAnimatorPreRunning = null;
            updateTextureView();
            return;
        }
        _isAnimatorPreRunning = null;
        doPlay();
    }

    private void syncCurrentTime() {
        long playTime = 0;
        if (animator.getDuration() > 0) {
            long repeatCount = currentPlayTime / animator.getDuration();
            if (animator.getAnimatedFraction() == 1.0f) {
                repeatCount = Math.round(currentPlayTime * 1.0 / animator.getDuration()) - 1;
            }
            playTime = (long) ((pagPlayer.getProgress() + repeatCount) * pagPlayer.duration());
        }
        currentPlayTime = playTime / 1000;
    }
}
