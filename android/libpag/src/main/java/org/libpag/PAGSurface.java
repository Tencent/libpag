package org.libpag;

import android.graphics.Bitmap;
import android.graphics.SurfaceTexture;
import android.hardware.HardwareBuffer;
import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.os.Build;
import android.util.Pair;
import android.view.Surface;

import org.extra.tools.LibraryLoadUtils;

public class PAGSurface {

    public static PAGSurface MakeOffscreen(int width, int height) {
        long nativeSurface = SetupOffscreen(width, height);
        if (nativeSurface == 0) {
            return null;
        }
        return new PAGSurface(nativeSurface);
    }


    public static PAGSurface FromSurfaceTexture(SurfaceTexture surfaceTexture) {
        return FromSurfaceTexture(surfaceTexture, EGL14.EGL_NO_CONTEXT);
    }

    public static PAGSurface FromSurfaceTexture(SurfaceTexture surfaceTexture, EGLContext shareContext) {
        if (surfaceTexture == null) {
            return null;
        }
        PAGSurface pagSurface = FromSurface(new Surface(surfaceTexture), shareContext);
        if (pagSurface != null) {
            pagSurface.needsReleaseSurface = true;
        }
        return pagSurface;
    }

    public static PAGSurface FromSurface(Surface surface) {
        return FromSurface(surface, EGL14.EGL_NO_CONTEXT);
    }

    @SuppressWarnings("deprecation")
    public static PAGSurface FromSurface(Surface surface, EGLContext shareContext) {
        if (surface == null) {
            return null;
        }
        long handle = 0;
        if (shareContext != null && shareContext != EGL14.EGL_NO_CONTEXT) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                handle = shareContext.getNativeHandle();
            } else {
                handle = shareContext.getHandle();
            }
        }
        long nativeSurface = SetupFromSurfaceWithGLContext(surface, handle);
        if (nativeSurface == 0) {
            return null;
        }
        PAGSurface pagSurface = new PAGSurface(nativeSurface);
        pagSurface.surface = surface;
        return pagSurface;
    }

    /**
     * Create a PAGSurface from specified OpenGL texture, return null if there is no current OpenGL context or the
     * texture is invalid. Note:
     * 1. The target of texture must be GL_TEXTURE_2D.
     * 2. The texture should not bind to any frameBuffer.
     * 3. PAG will use the current (OpenGL) context.
     */
    public static PAGSurface FromTexture(int textureID, int width, int height) {
        return FromTexture(textureID, width, height, false);
    }

    /**
     * Create a PAGSurface from specified OpenGL texture, return null if there is no current OpenGL context or the
     * texture is invalid. Note:
     * 1. The target of texture must be GL_TEXTURE_2D.
     * 2. The texture should not bind to any frameBuffer.
     * 3. PAG will use the current (OpenGL) context.
     */
    public static PAGSurface FromTexture(int textureID, int width, int height, boolean flipY) {
        long nativeSurface = SetupFromTexture(textureID, width, height, flipY, false);
        if (nativeSurface == 0) {
            return null;
        }
        PAGSurface pagSurface = new PAGSurface(nativeSurface);
        pagSurface.textureID = textureID;
        return pagSurface;
    }

    /**
     * Create a PAGSurface from specified OpenGL texture, return null if there is no current OpenGL context or the
     * texture is invalid. Note:
     * 1. The target of texture must be GL_TEXTURE_2D.
     * 2. The texture should not bind to any frameBuffer.
     * 3. PAG will use a shared (OpenGL) context with the current.
     * 4. The caller can use fence sync objects to synchronise texture content (see
     * {@link PAGPlayer#flushAndFenceSync(long[])} and {@link PAGPlayer#waitSync(long)}).
     *
     * @see PAGPlayer#flushAndFenceSync(long[])
     * @see PAGPlayer#waitSync(long)
     */
    public static PAGSurface FromTextureForAsyncThread(int textureID, int width, int height) {
        return FromTextureForAsyncThread(textureID, width, height, false);
    }

    /**
     * Create a PAGSurface from specified OpenGL texture, return null if there is no current OpenGL context or the
     * texture is invalid. Note:
     * 1. The target of texture must be GL_TEXTURE_2D.
     * 2. The texture should not bind to any frameBuffer.
     * 3. PAG will use a shared (OpenGL) context with the current.
     * 4. The caller can use fence sync objects to synchronise texture content (see
     * {@link PAGPlayer#flushAndFenceSync(long[])} and {@link PAGPlayer#waitSync(long)}).
     *
     * @see PAGPlayer#flushAndFenceSync(long[])
     * @see PAGPlayer#waitSync(long)
     */
    public static PAGSurface FromTextureForAsyncThread(int textureID, int width, int height, boolean flipY) {
        long nativeSurface = SetupFromTexture(textureID, width, height, flipY, true);
        if (nativeSurface == 0) {
            return null;
        }
        PAGSurface pagSurface = new PAGSurface(nativeSurface);
        pagSurface.textureID = textureID;
        return pagSurface;
    }

    private static native long SetupOffscreen(int width, int height);

    private static native long SetupFromSurfaceWithGLContext(Surface surface, long shareContext);

    public static native long SetupFromTexture(int textureID, int width, int height, boolean flipY, boolean forAsyncThread);

    private PAGSurface(long nativeSurface) {
        this.nativeSurface = nativeSurface;
    }

    private Surface surface = null;
    private boolean needsReleaseSurface = false;
    private int textureID = 0;

    /**
     * The width of surface in pixels.
     */
    public native int width();

    /**
     * The height of surface in pixels.
     */
    public native int height();

    /**
     * Update the size of surface, and reset the internal surface.
     */
    public native void updateSize();

    /**
     * Erases all pixels of this surface with transparent color. Returns true if the content has changed.
     */
    public native boolean clearAll();

    /**
     * Free the cache created by the surface immediately. Call this method can reduce memory pressure.
     */
    public native void freeCache();

    /**
     * Returns a bitmap capturing the contents of the PAGSurface. Subsequent rendering of the
     * PAGSurface will not be captured.
     */
    public Bitmap makeSnapshot() {
        Pair<Bitmap, Object> pair = BitmapHelper.CreateBitmap(width(), height(), true);
        if (pair.first == null) {
            return null;
        }
        if (pair.second != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ((HardwareBuffer)pair.second).close();
        }
        return copyPixelsTo(pair.first) ? pair.first : null;
    }

    /**
     * Copies pixels from current PAGSurface to the specified bitmap.
     */
    public native boolean copyPixelsTo(Bitmap bitmap);

    /**
     * Free up resources used by the PAGSurface instance immediately instead of relying on the
     * garbage collector to do this for you at some point in the future.
     */
    public void release() {
        // Must call freeCache() here, otherwise, the cache may not be freed until the PAGPlayer is
        // garbage collected.
        freeCache();
        if (needsReleaseSurface && surface != null) {
            surface.release();
        }
        nativeRelease();
    }

    private native void nativeRelease();

    private static native void nativeInit();

    private native void nativeFinalize();

    protected void finalize() {
        nativeFinalize();
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }

    long nativeSurface = 0;
}
