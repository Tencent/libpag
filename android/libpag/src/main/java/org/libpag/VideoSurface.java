package org.libpag;

import android.graphics.SurfaceTexture;
import android.view.Surface;

import org.extra.tools.LibraryLoadUtils;

class VideoSurface implements SurfaceTexture.OnFrameAvailableListener {
    static VideoSurface Make(int width, int height) {
        VideoSurface videoSurface = new VideoSurface(width, height);
        if (videoSurface.nativeContext == 0) {
            return null;
        }
        return videoSurface;
    }

    private VideoSurface(int width, int height) {
        nativeSetup(width, height);
    }

    public void onFrameAvailable(SurfaceTexture st) {
        notifyFrameAvailable();
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

    private native void nativeSetup(int width, int height);

    private native void notifyFrameAvailable();

    long nativeContext = 0;

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }

}
