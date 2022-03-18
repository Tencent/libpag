package com.example.flutter_pag_plugin;

import android.animation.ValueAnimator;
import android.view.animation.LinearInterpolator;

import org.libpag.PAGFile;
import org.libpag.PAGPlayer;
import org.libpag.PAGScaleMode;

import io.flutter.FlutterInjector;


public class FlutterPagPlayer extends PAGPlayer {

    private final ValueAnimator animator = ValueAnimator.ofFloat(0.0F, 1.0F);
    private boolean isRelease;
    private long currentPlayTime = 0L;
    private double progress = 0;
    private double initProgress = 0;
    private ReleaseListener releaseListener;

    public void init(PAGFile file, int repeatCount, double initProgress) {
        setComposition(file);
        progress = initProgress;
        this.initProgress = initProgress;
        initAnimator(repeatCount);
    }

    private void initAnimator(int repeatCount) {
        animator.setDuration(duration() / 1000L);
        animator.setInterpolator(new LinearInterpolator());
        animator.addUpdateListener(animatorUpdateListener);
        if (repeatCount < 0) {
            repeatCount = 0;
        }
        animator.setRepeatCount(repeatCount - 1);
        setProgressValue(initProgress);
    }

    public void setProgressValue(double value) {
        this.progress = Math.max(0.0D, Math.min(value, 1.0D));
        this.currentPlayTime = (long) (progress * (double) this.animator.getDuration());
        this.animator.setCurrentPlayTime(currentPlayTime);
        setProgress(progress);
        flush();
    }

    public void start() {
        animator.start();
    }

    public void stop() {
        pause();
        setProgressValue(initProgress);
    }

    public void pause() {
        animator.pause();
    }

    @Override
    public void release() {
        super.release();
        if (releaseListener != null) {
            releaseListener.onRelease();
        }
        isRelease = true;
    }

    @Override
    public boolean flush() {
        if (isRelease) {
            return false;
        }
        return super.flush();
    }

    private ValueAnimator.AnimatorUpdateListener animatorUpdateListener = new ValueAnimator.AnimatorUpdateListener() {
        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            progress = (double) (Float) animation.getAnimatedValue();
            currentPlayTime = (long) (progress * (double) animator.getDuration());
            setProgress(progress);
            flush();
        }
    };

    public void setReleaseListener(ReleaseListener releaseListener) {
        this.releaseListener = releaseListener;
    }

    public interface ReleaseListener {
        void onRelease();
    }
}
