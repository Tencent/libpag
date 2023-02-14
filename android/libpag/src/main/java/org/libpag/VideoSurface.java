package org.libpag;

import android.graphics.SurfaceTexture;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.Surface;

import org.extra.tools.LibraryLoadUtils;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;

class VideoSurface implements SurfaceTexture.OnFrameAvailableListener {
    private static HandlerThread handlerThread;
    private static int HandlerThreadCount = 0;
    private static final Object handlerLock = new Object();

    private static synchronized boolean StartHandlerThread() {
        synchronized (handlerLock) {
            if (handlerThread == null) {
                handlerThread = new HandlerThread("libpag_VideoSurface");
                try {
                    handlerThread.start();
                } catch (Exception | Error e) {
                    e.printStackTrace();
                    handlerThread = null;
                    return false;
                }
            }
            HandlerThreadCount++;
            return true;
        }

    }

    Surface outputSurface;
    private SurfaceTexture surfaceTexture;

    private boolean released = false;

    static VideoSurface Make(int width, int height) {
        if (!StartHandlerThread()) {
            return null;
        }
        return new VideoSurface(width, height);
    }

    private VideoSurface(int width, int height) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            surfaceTexture = new SurfaceTexture(false);
        } else {
            surfaceTexture = new SurfaceTexture(0);
            surfaceTexture.detachFromGLContext();
        }
        synchronized (handlerLock) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                surfaceTexture.setOnFrameAvailableListener(this, new Handler(handlerThread.getLooper()));
            } else {
                surfaceTexture.setOnFrameAvailableListener(this);
                reflectLooper();
            }
        }
        outputSurface = new Surface(surfaceTexture);
        nativeSetup(surfaceTexture, width, height);
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
            @SuppressWarnings("unchecked") Constructor eventHandlerConstructor = eventHandlerClass.getConstructor(paramTypes);
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
        notifyFrameAvailable();
    }


    void release() {
        if (released) {
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
        nativeRelease();
        if (outputSurface != null) {
            outputSurface.release();
            outputSurface = null;
        }
        if (surfaceTexture != null) {
            surfaceTexture.release();
            surfaceTexture = null;
        }
    }

    protected void finalize() {
        nativeFinalize();
    }


    private native void nativeRelease();

    private native void nativeFinalize();

    private static native void nativeInit();

    private native void nativeSetup(SurfaceTexture surfaceTexture, int width, int height);

    private native void notifyFrameAvailable();

    long nativeContext = 0;

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }

}
