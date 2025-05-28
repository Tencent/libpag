package org.libpag;

import android.view.Surface;

class HardwareDecoder {
    private static VideoSurface getVideoSurface(int width, int height) {
        final VideoSurface videoSurface = VideoSurface.Make(width, height);
        if (videoSurface == null) {
            return null;
        }
        return videoSurface;
    }

    private static Surface getSurface(VideoSurface videoSurface) {
        if (videoSurface == null) {
            return null;
        }
        return videoSurface.getInputSurface();
    }
}
