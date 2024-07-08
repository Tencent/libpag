package org.libpag;

import android.graphics.Bitmap;
import android.graphics.ColorSpace;
import android.hardware.HardwareBuffer;
import android.os.Build;
import android.util.Pair;

class BitmapHelper {

    static Pair<Bitmap, Object> CreateBitmap(int width, int height,
                                                     boolean needGetHardwareBufferFromNative) {
        if (width == 0 || height == 0) {
            return Pair.create(null, null);
        }
        // The AndroidBitmap_getHardwareBuffer() method in NDK is available since API level 30.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R || !needGetHardwareBufferFromNative && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            Bitmap bitmap = null;
            HardwareBuffer hardwareBuffer = CreateHardwareBuffer(width, height);
            if (hardwareBuffer != null) {
                try {
                    bitmap = Bitmap.wrapHardwareBuffer(hardwareBuffer, ColorSpace.get(ColorSpace.Named.SRGB));
                } catch (Exception e) {
                    bitmap = null;
                    e.printStackTrace();
                }
            }
            if (bitmap != null) {
                return Pair.create(bitmap, hardwareBuffer);
            }
        }
        return Pair.create(Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888), null);
    }

    private static HardwareBuffer CreateHardwareBuffer(int width, int height) {
        if (width > 0 && height > 0 && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                return HardwareBuffer.create(width, height, HardwareBuffer.RGBA_8888, 1,
                        HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE | HardwareBuffer.USAGE_CPU_READ_OFTEN | HardwareBuffer.USAGE_CPU_WRITE_OFTEN | HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return null;
    }

}
