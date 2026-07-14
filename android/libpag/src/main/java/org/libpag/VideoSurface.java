package org.libpag;

import android.graphics.SurfaceTexture;
import android.view.Surface;

import java.lang.ref.WeakReference;

import org.extra.tools.LibraryLoadUtils;

class VideoSurface {
    // WeakFrameListener holds VideoSurface via a WeakReference to break the
    // circular reference between the native SurfaceTexture (which holds a
    // global reference to the Java SurfaceTexture) and the Java VideoSurface
    // (registered as the OnFrameAvailableListener on the Java SurfaceTexture).
    // Without this, the Java VideoSurface can never be reclaimed by GC, and
    // the underlying SurfaceTextureReader (along with its GPU resources) stays
    // alive indefinitely.
    private static class WeakFrameListener implements SurfaceTexture.OnFrameAvailableListener {
        private final WeakReference<VideoSurface> target;

        WeakFrameListener(VideoSurface target) {
            this.target = new WeakReference<>(target);
        }

        @Override
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            VideoSurface videoSurface = target.get();
            if (videoSurface != null) {
                videoSurface.notifyFrameAvailable();
            }
        }
    }

    static VideoSurface Make(int width, int height) {
        VideoSurface videoSurface = new VideoSurface(width, height);
        if (videoSurface.nativeContext == 0) {
            return null;
        }
        return videoSurface;
    }

    // Hold a strong reference to the listener so it lives as long as this
    // VideoSurface. The native SurfaceTexture also holds a global reference
    // to the same listener via SurfaceTexture.setOnFrameAvailableListener,
    // which keeps it alive during frame delivery.
    private final WeakFrameListener frameListener;

    private VideoSurface(int width, int height) {
        frameListener = new WeakFrameListener(this);
        nativeSetup(width, height, frameListener);
    }

    native Surface getInputSurface();


    void release() {
        nativeRelease();
    }

    @Override
    protected void finalize() {
        nativeFinalize();
    }


    private native void nativeRelease();

    private native void nativeFinalize();

    private static native void nativeInit();

    private native void nativeSetup(int width, int height, Object listener);

    private native void notifyFrameAvailable();

    long nativeContext = 0;

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }

}
