package org.libpag;

import android.content.Context;
import android.provider.Settings;
import android.util.Log;

import org.extra.tools.LibraryLoadUtils;

/**
 * PAGAnimator provides a simple timing engine for running animations.
 */
class PAGAnimator {
    /**
     * This interface is used to receive notifications when the status of the associated PAGAnimator
     * changes, and to flush the animation frame.
     */
    public interface Listener {
        /**
         * Notifies the start of the animation. It can be called from either the UI thread or the
         * thread that calls the play() method.
         */
        void onAnimationStart(PAGAnimator animator);

        /**
         * Notifies the end of the animation. It can only be called from the UI thread.
         */
        void onAnimationEnd(PAGAnimator animator);

        /**
         * Notifies the cancellation of the animation. It can be called from either the UI thread or
         * the thread that calls the stop() method.
         */
        void onAnimationCancel(PAGAnimator animator);

        /**
         * Notifies the repetition of the animation. It can only be called from the UI thread.
         */
        void onAnimationRepeat(PAGAnimator animator);

        /**
         * Notifies another frame of the animation has occurred. It can be called from either the UI
         * thread or the thread that calls the play() method.
         */
        void onAnimationUpdate(PAGAnimator animator);

        /**
         * Notifies a new frame of the animation needs to be flushed. It may be called from an
         * arbitrary thread if the animation is running asynchronously.
         */
        void onAnimationFlush(PAGAnimator animator);
    }


    private Listener listener = null;
    private float animationScale = 1.0f;

    /**
     * Creates a new PAGAnimator with the specified updater.
     */
    public static PAGAnimator MakeFrom(Context context, Listener listener) {
        if (listener == null) {
            return null;
        }
        return new PAGAnimator(context, listener);
    }

    private PAGAnimator(Context context, Listener listener) {
        this.listener = listener;
        if (context != null) {
            animationScale = Settings.Global.getFloat(context.getContentResolver(),
                    Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f);
        }
        nativeSetup();
    }

    /**
     * Indicates whether the animation is allowed to run in the UI thread. The default value is
     * false. Regardless of whether the animation runs asynchronously, all listener callbacks will
     * be called on the UI thread.
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
     * Indicates whether the animation is playing.
     */
    public native boolean isPlaying();

    /**
     * Starts to play the animation from the current position. Calling the play() method when the
     * animation is already playing has no effect. The play() method does not alter the animation's
     * current position. However, if the animation previously reached its end, it will restart from
     * the beginning.
     */
    public void play() {
        if (animationScale == 0.0f) {
            Log.e("libpag", "PAGAnimator.play() The scale of animator duration is turned off!");
            listener.onAnimationEnd(this);
            return;
        }
        doPlay();
    }

    private native void doPlay();

    /**
     * Cancels the animation at the current position. Calling the play() method can resume the
     * animation from the last paused position.
     */
    public native void pause();

    /**
     * Cancels the animation at the current position. Unlike pause(), stop() not only cancels the
     * animation but also tries to cancel any async tasks, which may block the calling thread.
     */
    public native void stop();

    /**
     * Manually flush the animation to the current progress without altering its playing status. If
     * the animation is already running n flushing task asynchronously, this action will not have
     * any effect.
     */
    public native void flush();

    private void onAnimationStart() {
        listener.onAnimationStart(this);
    }

    private void onAnimationEnd() {
        listener.onAnimationEnd(this);
    }

    private void onAnimationCancel() {
        listener.onAnimationCancel(this);
    }

    private void onAnimationRepeat() {
        listener.onAnimationRepeat(this);
    }

    private void onAnimationUpdate() {
        listener.onAnimationUpdate(this);
    }

    private void onAnimationFlush() {
        listener.onAnimationFlush(this);
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