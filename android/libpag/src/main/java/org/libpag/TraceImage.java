package org.libpag;

import android.graphics.Bitmap;
import android.util.Log;

import java.nio.ByteBuffer;

public class TraceImage {
    public static void Trace(String tag, ByteBuffer byteBuffer, int width, int height) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.copyPixelsFromBuffer(byteBuffer);
        Log.i(tag, "Image(width = " + bitmap.getWidth() + ", height = " + bitmap.getHeight() + ")");
    }
}
