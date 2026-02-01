package org.libpag;

import android.content.Context;
import android.provider.Settings;
import android.util.Log;

import org.extra.tools.LibraryLoadUtils;

import java.lang.ref.WeakReference;

/**
 * PAGAnimator provides a simple timing engine for running animations.
 */
public class PAGAnimator {
    /**
     * This interface is used to receive notifications when the status of the associated PAGAnimator
     * changes, and to flush the animation frame.
     */
    public interface Listener {
        /**
         * Notifies the start of the animation. This may be called from any thread that invokes the
         * start() method.
         */
        void onAnimationStart(PAGAnimator animator);

        /**
         * Notifies the end of the animation. This will only be called from the UI thread.
         */
        void onAnimationEnd(PAGAnimator animator);

        /**
         * Notifies the cancellation of the animation. This may be called from any thread that
         * invokes the cancel() method.
         */
        void onAnimationCancel(PAGAnimator animator);

        /**
         * Notifies the repetition of the animation. This will only be called from the UI thread.
         */
        void onAnimationRepeat(PAGAnimator animator);

        /**
         * Notifies another frame of the animation has occurred. This may be called from an
         * arbitrary thread if the animation is running asynchronously.
         */
        void onAnimationUpdate(PAGAnimator animator);
    }


    private WeakReference<Listener> weakListener = null;
    private float animationScale = 1.0f;

    /**
     * Creates a new PAGAnimator with the specified listener. PAGAnimator only holds a weak
     * reference to the listener.
     */
    public static PAGAnimator MakeFrom(Context context, Listener listener) {
        if (listener == null) {
            return null;
        }
        return new PAGAnimator(context, listener);
    }

    private PAGAnimator(Context context, Listener listener) {
        this.weakListener = new WeakReference<>(listener);
        if (context != null) {
            animationScale = Settings.Global.getFloat(context.getContentResolver(),
                    Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f);
        }
        nativeSetup();
    }

    /**
     * Indicates whether the animation is allowed to run in the UI thread. The default value is
     * false.
     */
    public native boolean isSync();

    /**
     * Set whether the animation is allowed to run in the UI thread.
     */
    public native void setSync(boolean value);

    /**
     * Returns the length of the animation in microseconds.
     */
    public native long duration();

    /**
     * Sets the length of the animation in microseconds.
     */
    public native void setDuration(long duration);

    /**
     * The total number of times the animation is set to play. The default is 1, which means the
     * animation will play only once. If the repeat count is set to 0 or a negative value, the
     * animation will play infinity times.
     */
    public native int repeatCount();

    /**
     * Set the number of times the animation to play.
     */
    public native void setRepeatCount(int repeatCount);

    /**
     * Returns the current position of the animation, which is a number between 0.0 and 1.0.
     */
    public native double progress();

    /**
     * Set the current progress of the animation.
     */
    public native void setProgress(double value);

    /**
     * Indicates whether the animation is running.
     */
    public native boolean isRunning();

    /**
     * Starts the animation from the current position. Calling the start() method when the animation
     * is already started has no effect. The start() method does not alter the animation's current
     * position. However, if the animation previously reached its end, it will restart from the
     * beginning.
     */
    public void start() {
        if (animationScale == 0.0f) {
            Log.e("libpag", "PAGAnimator.play() The scale of animator duration is turned off!");
            Listener listener = weakListener.get();
            if (listener != null) {
                listener.onAnimationUpdate(this);
                listener.onAnimationEnd(this);
            }
            return;
        }
        doStart();
    }

    private native void doStart();

    /**
     * Cancels the animation at the current position. Calling the start() method can resume the
     * animation from the last canceled position.
     */
    public native void cancel();

    /**
     * Manually update the animation to the current progress without altering its playing status. If
     * isSync is set to false, the calling thread won't be blocked. Please note that if the
     * animation already has an ongoing asynchronous flushing task, this action won't have any
     * effect.
     */
    public native void update();

    private void onAnimationStart() {
        Listener listener = weakListener.get();
        if (listener != null) {
            listener.onAnimationStart(this);
        }
    }

    private void onAnimationEnd() {
        Listener listener = weakListener.get();
        if (listener != null) {
            listener.onAnimationEnd(this);
        }
    }

    private void onAnimationCancel() {
        Listener listener = weakListener.get();
        if (listener != null) {
            listener.onAnimationCancel(this);
        }
    }

    private void onAnimationRepeat() {
        Listener listener = weakListener.get();
        if (listener != null) {
            listener.onAnimationRepeat(this);
        }
    }

    private void onAnimationUpdate() {
        Listener listener = weakListener.get();
        if (listener != null) {
            listener.onAnimationUpdate(this);
        }
    }

    /**
     * Free up resources used by the PAGAnimator instance immediately instead of relying on the
     * garbage collector to do this for you at some point in the future.
     */
    public void release() {
        nativeRelease();
    }

    protected void finalize() {
        nativeFinalize();
    }

    private native void nativeRelease();

    private native void nativeFinalize();

    private native void nativeSetup();

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }

    private long nativeContext = 0;
}
