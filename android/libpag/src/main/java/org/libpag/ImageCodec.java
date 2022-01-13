package org.libpag;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.ExifInterface;
import android.os.Build;

import java.io.IOException;
import java.nio.ByteBuffer;

public class ImageCodec {
    private static final int ALPHA_8 = 1;
    private static final int ARGB_8888_PREMULTIPLIED = 2;
    private static final int ARGB_8888_UNPREMULTIPLIED = 3;


    private static int GetOrientationFromPath(String path) {
        int orientation = ExifInterface.ORIENTATION_NORMAL;
        try {
            ExifInterface exifInterface = new ExifInterface(path);
            orientation = exifInterface.getAttributeInt(ExifInterface.TAG_ORIENTATION,
                    ExifInterface.ORIENTATION_NORMAL);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return orientation;
    }

    private static int GetOrientationFromBytes(ByteBuffer byteBuffer) {
        int orientation = ExifInterface.ORIENTATION_NORMAL;
        try {
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
                ByteBufferInputStream stream = new ByteBufferInputStream(byteBuffer);
                ExifInterface exifInterface = new ExifInterface(stream);
                orientation = exifInterface.getAttributeInt(ExifInterface.TAG_ORIENTATION,
                        ExifInterface.ORIENTATION_NORMAL);

            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        return orientation;
    }

    private static int[] GetSizeFromPath(String imagePath) {
        int[] size = {0, 0};
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        try {
            BitmapFactory.decodeFile(imagePath, options);
            size[0] = options.outWidth;
            size[1] = options.outHeight;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return size;
    }

    private static int[] GetSizeFromBytes(ByteBuffer byteBuffer) {
        int[] size = {0, 0};
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        try {
            ByteBufferInputStream stream = new ByteBufferInputStream(byteBuffer);
            BitmapFactory.decodeStream(stream, null, options);
            size[0] = options.outWidth;
            size[1] = options.outHeight;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return size;
    }

    private static BitmapFactory.Options MakeOptions(int colorType) {
        BitmapFactory.Options options = null;
        switch (colorType) {
            case ALPHA_8:
                options = new BitmapFactory.Options();
                options.inPreferredConfig = Bitmap.Config.ALPHA_8;
                break;
            case ARGB_8888_PREMULTIPLIED:
                options = new BitmapFactory.Options();
                options.inPreferredConfig = Bitmap.Config.ARGB_8888;
                break;
            case ARGB_8888_UNPREMULTIPLIED:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                    options = new BitmapFactory.Options();
                    options.inPreferredConfig = Bitmap.Config.ARGB_8888;
                    options.inPremultiplied = false;
                }
                break;
            default:
                break;
        }
        return options;
    }

    private static Bitmap CreateBitmapFromPath(String imagePath, int colorType) {
        BitmapFactory.Options options = MakeOptions(colorType);
        try {
            return BitmapFactory.decodeFile(imagePath, options);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    private static Bitmap CreateBitmapFromBytes(ByteBuffer byteBuffer, int colorType) {
        BitmapFactory.Options options = MakeOptions(colorType);
        try {
            ByteBufferInputStream stream = new ByteBufferInputStream(byteBuffer);
            return BitmapFactory.decodeStream(stream, null, options);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }
}
