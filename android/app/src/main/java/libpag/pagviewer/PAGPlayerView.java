package libpag.pagviewer;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.opengl.GLUtils;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RelativeLayout;

import org.libpag.PAGComposition;
import org.libpag.PAGFile;
import org.libpag.PAGImage;
import org.libpag.PAGText;
import org.libpag.PAGView;
import org.libpag.VideoDecoder;

import java.io.IOException;
import java.io.InputStream;

/**
 * Copyright © 2019 tencent. All rights reserved.
 * <p>
 * File: PAGPlayer.java <p>
 * Project: android <p>
 * Package: libpag.pagviewer <p>
 * Description:
 * <p>
 * author: hamlingong <p>
 * date: 2019/4/19 3:46 PM <p>
 * version: V1.0 <p>
 */
public class PAGPlayerView {
    private PAGView mPagView;
    private int repeatCount = 1;
    private PAGComposition composition;
    private long factory = 0;
    private int maxHardwareDecoderCount = 0;
    private EGLDisplay eglDisplay = null;
    private EGLSurface eglSurface = null;
    private EGLContext eglContext = null;
    private int textureID = 0;

    public View createView(Context context, String pagPath) {
        eglSetup();
        mPagView = new PAGView(context, eglContext);
        if (composition != null) {
            mPagView.setComposition(composition);
        } else {
            PAGFile pagFile = PAGFile.Load(context.getAssets(), pagPath);
            if (pagFile.numTexts() > 0) {
                PAGText pagText = pagFile.getTextData(0);
                pagText.text = "hahhha哈哈哈😆哈哈哈";
                pagText.fauxItalic = true;
                pagFile.replaceText(0, pagText);
            }
            if (pagFile.numImages() > 0) {
                PAGImage pagImage = PAGImage.FromAssets(context.getAssets(), "rotation.jpg");
//                PAGImage pagImage = makePAGImage(context, "mountain.jpg");
                pagFile.replaceImage(0, pagImage);
            }
            mPagView.setComposition(pagFile);
        }
        mPagView.setLayoutParams(new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        mPagView.setRepeatCount(repeatCount);
        mPagView.addPAGFlushListener(new PAGView.PAGFlushListener() {
            @Override
            public void onFlush() {
            }
        });
        if (factory != 0) {
            VideoDecoder.RegisterSoftwareDecoderFactory(factory);
            VideoDecoder.SetMaxHardwareDecoderCount(maxHardwareDecoderCount);
        }
        return mPagView;
    }

    private void eglSetup() {
        eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        int[] version = new int[2];
        EGL14.eglInitialize(eglDisplay, version, 0, version, 1);
        EGL14.eglBindAPI(EGL14.EGL_OPENGL_ES_API);
        int[] attributeList = {
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_STENCIL_SIZE, 8,
                EGL14.EGL_SAMPLE_BUFFERS, 1,
                EGL14.EGL_SAMPLES, 4,
                EGL14.EGL_NONE
        };
        EGLConfig[] configs = new EGLConfig[1];
        int[] numConfigs = new int[1];
        EGL14.eglChooseConfig(eglDisplay, attributeList, 0, configs, 0,
                configs.length, numConfigs, 0);

        int[] attribute_list = {
                EGL14.EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL14.EGL_NONE
        };

        eglContext = EGL14.eglCreateContext(eglDisplay, configs[0], EGL14.EGL_NO_CONTEXT,
                attribute_list, 0);

        int[] surfaceAttributes = {
                EGL14.EGL_WIDTH, 1,
                EGL14.EGL_HEIGHT, 1,
                EGL14.EGL_NONE
        };
        eglSurface = EGL14.eglCreatePbufferSurface(eglDisplay, configs[0],
                surfaceAttributes, 0);
    }

    private PAGImage makePAGImage(Context context, String filePath) {
        if (eglContext == null || !EGL14.eglMakeCurrent(eglDisplay, eglSurface,
                eglSurface, eglContext)) {
            return null;
        }
        AssetManager assetManager = context.getAssets();
        InputStream stream;
        Bitmap bitmap = null;
        try {
            stream = assetManager.open(filePath);
            bitmap = BitmapFactory.decodeStream(stream);
        } catch (IOException e) {
            // handle exception
        }
        if (bitmap == null) {
            return null;
        }
        int[] mTexturesBitmap = {0};
        GLES20.glGenTextures(1, mTexturesBitmap, 0);
        textureID = mTexturesBitmap[0];
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureID);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        return PAGImage.FromTexture(mTexturesBitmap[0], GLES20.GL_TEXTURE_2D,
                bitmap.getWidth(), bitmap.getHeight());
    }

    public void onRelease() {
        if (mPagView != null) {
            mPagView.stop();
            mPagView.freeCache();
            mPagView = null;
        }
        if (eglContext != null && EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
            if (textureID > 0) {
                int[] textures = {textureID};
                GLES20.glDeleteTextures(1, textures, 0);
            }
            EGL14.eglDestroySurface(eglDisplay, eglSurface);
            EGL14.eglDestroyContext(eglDisplay, eglContext);
        }
    }

    public void play() {
        if (mPagView != null) {
            mPagView.play();
        }
    }

    public void stop() {
        if (mPagView != null) {
            mPagView.stop();
        }
    }

    public void setRepeatCount(int count) {
        this.repeatCount = count;
        if (mPagView != null) {
            mPagView.setRepeatCount(count);
        }
    }

    public void setComposition(PAGComposition composition) {
        this.composition = composition;
        if (mPagView != null) {
            mPagView.setComposition(composition);
        }
    }

    public void registerSoftwareDecoderFactory(long factory, int maxHardwareDecoderCount) {
        this.factory = factory;
        this.maxHardwareDecoderCount = maxHardwareDecoderCount;
        VideoDecoder.RegisterSoftwareDecoderFactory(factory);
        VideoDecoder.SetMaxHardwareDecoderCount(maxHardwareDecoderCount);
    }
}
