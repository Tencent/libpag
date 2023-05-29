package org.libpag;

import android.animation.ValueAnimator;
import android.os.Handler;
import android.os.Looper;

class DisplayLink implements ValueAnimator.AnimatorUpdateListener {
    private ValueAnimator animator;
    private Handler handler;

    static public DisplayLink Create(long nativeContext) {
        DisplayLink link = new DisplayLink();
        link.nativeContext = nativeContext;
        return link;
    }

    private DisplayLink() {
        handler = new Handler(Looper.getMainLooper());
        animator = ValueAnimator.ofFloat(0, 1);
        animator.setDuration(1000);
        animator.addUpdateListener(this);
        animator.setRepeatCount(ValueAnimator.INFINITE);
    }

    public void start() {
        handler.post(new Runnable() {
            @Override
            public void run() {
                animator.start();
            }
        });
    }

    public void stop() {
        handler.post(new Runnable() {
            @Override
            public void run() {
                animator.cancel();
            }
        });
    }


    public void onAnimationUpdate(ValueAnimator animation) {
        onUpdate(nativeContext);
    }

    private native void onUpdate(long nativeContext);

    private long nativeContext = 0;
}
