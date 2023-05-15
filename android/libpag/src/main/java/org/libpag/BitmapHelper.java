package org.libpag;

import android.graphics.Bitmap;
import android.graphics.ColorSpace;
import android.hardware.HardwareBuffer;
import android.os.Build;

class BitmapHelper {
    static Bitmap CreateBitmap(int width, int height) {
        if (width == 0 || height == 0) {
            return null;
        }
        // The AndroidBitmap_getHardwareBuffer() method in NDK is available since API level 30.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Bitmap bitmap = null;
            HardwareBuffer hardwareBuffer = CreateHardwareBuffer(width, height);
            if (hardwareBuffer != null) {
                try {
                    bitmap = Bitmap.wrapHardwareBuffer(hardwareBuffer, ColorSpace.get(ColorSpace.Named.SRGB));
                    // HardwareBuffer will automatically close when it is finalized, but StrictMode
                    // will incorrectly think it is not closed.
                    // So we manually call close to avoid printing annoying logs.
                    hardwareBuffer.close();
                } catch (Exception e) {
                    bitmap = null;
                    e.printStackTrace();
                }
            }
            if (bitmap != null) {
                return bitmap;
            }
        }
        return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    }

    static HardwareBuffer CreateHardwareBuffer(int width, int height) {
        if (width > 0 && height > 0 && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            HardwareBuffer hardwareBuffer = HardwareBuffer.create(width, height, HardwareBuffer.RGBA_8888, 1, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE | HardwareBuffer.USAGE_CPU_READ_OFTEN | HardwareBuffer.USAGE_CPU_WRITE_OFTEN);
            if (hardwareBuffer != null) {
                return hardwareBuffer;
            }
        }
        return null;
    }

}
