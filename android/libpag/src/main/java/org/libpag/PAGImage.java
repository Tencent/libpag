package org.libpag;

import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.SurfaceTexture;
import android.opengl.EGLContext;
import android.os.Build;
import android.util.Log;

import org.extra.tools.LibraryLoadUtils;

public class PAGImage {
    /**
     * Creates a PAGImage object from an android.graphics.Bitmap object, return null if it's not
     * valid Bitmap.
     */
    public static PAGImage FromBitmap(Bitmap bitmap) {
        if (bitmap != null && bitmap.getConfig() == Bitmap.Config.ARGB_4444) {
            bitmap = bitmap.copy(Bitmap.Config.ARGB_8888, false);
        }
        long context = LoadFromBitmap(bitmap);
        if (context == 0) {
            return null;
        }
        return new PAGImage(context);
    }

    /**
     * Creates a PAGImage object from a path of a image file, return null if the file does not exist
     * or it's not a valid image file.
     */
    public static PAGImage FromPath(String path) {
        long context = LoadFromPath(path);
        if (context == 0) {
            return null;
        }
        return new PAGImage(context);
    }

    /**
     * Creates a PAGImage object from the specified byte data, return null if the bytes is empty or it's not a valid
     * image file.
     */
    public static PAGImage FromBytes(byte[] bytes) {
        if (bytes == null || bytes.length == 0) {
            return null;
        }

        long context = LoadFromBytes(bytes, bytes.length);
        if (context == 0) {
            return null;
        }
        return new PAGImage(context);
    }

    /***
     * Load a pag file from assets, returns null if the file does not exist or the data is not a
     * pag file.
     */
    public static PAGImage FromAssets(AssetManager manager, String fileName) {
        long context = LoadFromAssets(manager, fileName);
        if (context == 0) {
            return null;
        }
        return new PAGImage(context);
    }

    /**
     * Creates a PAGImage object from the specified OpenGL texture ID, returns null if the texture
     * is invalid or there is no current openGL context. Note:
     * 1. The caller must make sure that the texture outlive the lifetime of the returned PAGImage
     * object.
     * 2. The current openGL context should be the same as the shareContext parameter of
     * {@link PAGSurface#FromSurfaceTexture(SurfaceTexture, EGLContext)} or the context which PAGSurface.FromTexture() uses,
     * so that the returned PAGImage object can be drawn on to that PAGSurface.
     * 3. The caller can use fence sync objects to synchronise texture content (see {@link
     * PAGPlayer#flushAndFenceSync(long[])} and {@link PAGPlayer#waitSync(long)}).
     *
     * @param textureID   the id of the texture.
     * @param textureTarget either GL_TEXTURE_EXTERNAL_OES or GL_TEXTURE_2D.
     * @param width       the width of the texture.
     * @param height      the height of the texture.
     *
     * @see PAGSurface#FromSurfaceTexture(SurfaceTexture, EGLContext)
     * @see PAGSurface#FromTexture(int, int, int)
     * @see PAGSurface#FromTexture(int, int, int, boolean)
     * @see PAGSurface#FromTextureForAsyncThread(int, int, int)
     * @see PAGSurface#FromTextureForAsyncThread(int, int, int, boolean)
     * @see PAGPlayer#flushAndFenceSync(long[])
     * @see PAGPlayer#waitSync(long)
     */
    public static PAGImage FromTexture(int textureID, int textureTarget, int width, int height) {
        return FromTexture(textureID, textureTarget, width, height, false);
    }

    /**
     * Creates a PAGImage object from the specified OpenGL texture ID, returns null if the texture
     * is invalid or there is no current openGL context. Note:
     * 1. The caller must make sure that the texture outlive the lifetime of the returned PAGImage
     * object.
     * 2. The current openGL context should be the same as the shareContext parameter of
     * {@link PAGSurface#FromSurfaceTexture(SurfaceTexture, EGLContext)} or the context which PAGSurface.FromTexture() uses,
     * so that the returned PAGImage object can be drawn on to that PAGSurface.
     * 3. The caller can use fence sync objects to synchronise texture content (see {@link
     * PAGPlayer#flushAndFenceSync(long[])} and {@link PAGPlayer#waitSync(long)}).
     *
     * @param textureID   the id of the texture.
     * @param textureTarget either GL_TEXTURE_EXTERNAL_OES or GL_TEXTURE_2D.
     * @param width       the width of the texture.
     * @param height      the height of the texture.
     * @param flipY indicates the origin of the texture, true means 'bottom-left', false means 'top-left'.
     *
     * @see PAGSurface#FromSurfaceTexture(SurfaceTexture, EGLContext)
     * @see PAGSurface#FromTexture(int, int, int)
     * @see PAGSurface#FromTexture(int, int, int, boolean)
     * @see PAGSurface#FromTextureForAsyncThread(int, int, int)
     * @see PAGSurface#FromTextureForAsyncThread(int, int, int, boolean)
     * @see PAGPlayer#flushAndFenceSync(long[])
     * @see PAGPlayer#waitSync(long)
     */
    public static PAGImage FromTexture(int textureID, int textureTarget, int width, int height, boolean flipY) {
        long context = LoadFromTexture(textureID, textureTarget, width, height, flipY);
        if (context == 0) {
            return null;
        }
        return new PAGImage(context);
    }


    private static native long LoadFromBitmap(Bitmap bitmap);

    private static native long LoadFromPath(String path);

    private static native long LoadFromBytes(byte[] bytes, int length);

    private static native long LoadFromAssets(AssetManager manager, String fileName);

    private static native long LoadFromTexture(int textureID, int textureType,
                                               int width, int height, boolean flipY);

    PAGImage(long nativeContext) {
        this.nativeContext = nativeContext;
    }

    /**
     * Returns the width in pixels of the image.
     */
    public native int width();

    /**
     * Returns the height in pixels of the image.
     */
    public native int height();

    /**
     * Returns the current scale mode.
     */
    public native int scaleMode();

    /**
     * Specifies the rule of how to scale the image content to fit the original image size. The matrix
     * changes when this method is called.
     */
    public native void setScaleMode(int mode);

    /**
     * Returns a copy of current matrix.
     */
    public Matrix matrix() {
        float[] values = new float[9];
        nativeGetMatrix(values);
        Matrix matrix = new Matrix();
        matrix.setValues(values);
        return matrix;
    }

    private native void nativeGetMatrix(float[] values);

    /**
     * Set the transformation which will be applied to the image content. The scaleMode property
     * will be set to PAGScaleMode::None when this method is called.
     */
    public void setMatrix(Matrix matrix) {
        float[] values = new float[9];
        matrix.getValues(values);
        nativeSetMatrix(values[0], values[3], values[1], values[4], values[2], values[5]);
    }

    private native void nativeSetMatrix(float a, float b, float c,
                                        float d, float tx, float ty);

    /**
     * Free up resources used by the PAGImage instance immediately instead of relying on the
     * garbage collector to do this for you at some point in the future.
     */
    public void release() {
        nativeRelease();
    }

    private native final void nativeRelease();

    private static native final void nativeInit();

    protected void finalize() {
        nativeFinalize();
    }

    private native void nativeFinalize();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }

    long nativeContext = 0;
}
