package org.libpag;

import android.graphics.SurfaceTexture;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.Surface;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;

public class VideoSurface implements SurfaceTexture.OnFrameAvailableListener {
    private static HandlerThread handlerThread;
    private static int HandlerThreadCount = 0;
    private static final Object handlerLock = new Object();

    private static synchronized void StartHandlerThread() {
        HandlerThreadCount++;
        if (handlerThread == null) {
            handlerThread = new HandlerThread("libpag_VideoSurface");
            handlerThread.start();
        }
    }

    private int width = 0;
    private int height = 0;
    private Surface outputSurface;
    private SurfaceTexture surfaceTexture;
    private final Object frameSyncObject = new Object();
    private boolean frameAvailable = false;
    private boolean released = false;
    private int retainCount = 1;

    private static VideoSurface Make(int width, int height) {
        VideoSurface videoSurface = new VideoSurface();
        videoSurface.width = width;
        videoSurface.height = height;
        synchronized (handlerLock) {
            StartHandlerThread();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                videoSurface.surfaceTexture = new SurfaceTexture(false);
            } else {
                videoSurface.surfaceTexture = new SurfaceTexture(0);
                videoSurface.surfaceTexture.detachFromGLContext();
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                videoSurface.surfaceTexture.setOnFrameAvailableListener(
                        videoSurface, new Handler(handlerThread.getLooper()));
            } else {
                videoSurface.surfaceTexture.setOnFrameAvailableListener(videoSurface);
                videoSurface.reflectLooper();
            }
        }
        videoSurface.outputSurface = new Surface(videoSurface.surfaceTexture);
        return videoSurface;
    }

    private void reflectLooper() {
        Class<?>[] innerClassArray = SurfaceTexture.class.getDeclaredClasses();
        Class eventHandlerClass = null;
        for (Class innerC : innerClassArray) {
            if (innerC.getName().toLowerCase().contains("handler")) {
                eventHandlerClass = innerC;
                break;
            }
        }

        if (eventHandlerClass == null) {
            return;
        }

        Class[] paramTypes = {SurfaceTexture.class, Looper.class};
        try {
            @SuppressWarnings("unchecked")
            Constructor eventHandlerConstructor = eventHandlerClass.getConstructor(paramTypes);
            Object eventHandlerObj = eventHandlerConstructor.newInstance(surfaceTexture, handlerThread.getLooper());
            Class<?> classType = surfaceTexture.getClass();
            Field field = classType.getDeclaredField("mEventHandler");
            field.setAccessible(true);
            field.set(surfaceTexture, eventHandlerObj);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void onFrameAvailable(SurfaceTexture st) {
        synchronized (frameSyncObject) {
            if (frameAvailable) {
                new RuntimeException("frameAvailable already set, frame could be dropped").printStackTrace();
                return;
            }
            frameAvailable = true;
            frameSyncObject.notifyAll();
        }
    }

    public Surface getOutputSurface() {
        return outputSurface;
    }

    private int videoWidth() {
        float[] matrix = new float[16];
        surfaceTexture.getTransformMatrix(matrix);
        float scale = Math.abs(matrix[0]);
        if (scale > 0) {
            return Math.round(width / (scale + matrix[12] * 2));
        } else {
            return width;
        }
    }

    private int videoHeight() {
        float[] matrix = new float[16];
        surfaceTexture.getTransformMatrix(matrix);
        float scale = Math.abs(matrix[5]);
        if (scale > 0) {
            return Math.round(height / (scale + (matrix[13] - scale) * 2));
        } else {
            return height;
        }
    }


    private boolean updateTexImage() {
        final int TIMEOUT_MS = 50;
        int needRetryTimes = 10;
        synchronized (frameSyncObject) {
            while (!frameAvailable && needRetryTimes > 0) {
                try {
                    // Wait for onFrameAvailable() to signal us.  Use a timeout to avoid
                    // stalling the test if it doesn't arrive.
                    needRetryTimes--;
                    frameSyncObject.wait(TIMEOUT_MS);
                } catch (InterruptedException ie) {
                    // shouldn't happen
                    ie.printStackTrace();
                }
            }
            frameAvailable = false;
            if (needRetryTimes == 0) {
                return false;
            }
        }
        try {
            surfaceTexture.updateTexImage();
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    private boolean attachToGLContext(int texName) {
        try {
            surfaceTexture.attachToGLContext(texName);
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    public void retain() {
        retainCount++;
    }

    public void release() {
        retainCount--;
        if (released || retainCount > 0) {
            return;
        }
        released = true;
        synchronized (handlerLock) {
            HandlerThreadCount--;
            if (HandlerThreadCount == 0) {
                handlerThread.quit();
                handlerThread = null;
            }
        }
        if (outputSurface != null) {
            outputSurface.release();
            outputSurface = null;
        }

        if (surfaceTexture != null) {
            surfaceTexture.release();
            surfaceTexture = null;
        }
    }
}
