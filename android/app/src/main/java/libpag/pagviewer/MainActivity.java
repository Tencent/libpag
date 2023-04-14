package libpag.pagviewer;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.opengl.GLUtils;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.Display;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.RelativeLayout;

import org.libpag.PAGFile;
import org.libpag.PAGImage;
import org.libpag.PAGImageView;
import org.libpag.PAGText;
import org.libpag.PAGView;

import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {
    RelativeLayout containerView;
    Button btPlayFirst;
    Button btPlaySecond;

    PAGView pagView;
    RelativeLayout pagImageViewGroup;

    private EGLDisplay eglDisplay = null;
    private EGLSurface eglSurface = null;
    private EGLContext eglContext = null;
    private int textureID = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_main);

        containerView = (RelativeLayout) findViewById(R.id.container_view);
        BackgroundView backgroundView = new BackgroundView(this);
        backgroundView.setLayoutParams(new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        containerView.addView(backgroundView);

        btPlayFirst = (Button) findViewById(R.id.play_first);
        if (btPlayFirst == null) {
            return;
        }
        btPlayFirst.setOnClickListener(this);
        btPlaySecond = (Button) findViewById(R.id.play_second);
        if (btPlaySecond == null) {
            return;
        }
        btPlaySecond.setOnClickListener(this);

        addPAGView();
        if (pagView != null) {
            pagView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (pagView.isPlaying()) {
                        pagView.stop();
                    } else {
                        pagView.play();
                    }
                }
            });
            pagView.play();
        }

        activatedView(btPlayFirst.getId());
    }

    private void addPAGView() {
        if (pagView == null) {
            eglSetup();
            pagView = new PAGView(this, eglContext);
            pagView.setLayoutParams(new RelativeLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

            PAGFile pagFile = PAGFile.Load(getAssets(), "alpha.pag");
            if (pagFile.numTexts() > 0) {
                PAGText pagText = pagFile.getTextData(0);
                pagText.text = "hahhha哈哈哈😆哈哈哈";
                pagText.fauxItalic = true;
                pagFile.replaceText(0, pagText);
            }
            if (pagFile.numImages() > 0) {
                PAGImage pagImage = PAGImage.FromAssets(this.getAssets(), "mountain.jpg");
//            PAGImage pagImage = makePAGImage(this, "mountain.jpg");
                pagFile.replaceImage(0, pagImage);
            }
            pagView.setComposition(pagFile);
            pagView.setRepeatCount(-1);
            containerView.addView(pagView);
        }
    }

    private void addPAGImageViewsAndPlay() {
        if (pagImageViewGroup == null) {
            pagImageViewGroup = new RelativeLayout(this);
            pagImageViewGroup.setLayoutParams(new RelativeLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            containerView.addView(pagImageViewGroup);

            Display display = getWindowManager().getDefaultDisplay();
            Point point = new Point();
            display.getSize(point);
            int sceenWidth = point.x;
            int itemWidth = sceenWidth / 4;
            int itemHeight = sceenWidth / 4;

            for (int i = 0; i < 20; i++) {
                PAGImageView pagImageView = new PAGImageView(this);
                String path = "assets://" + i + ".pag";
                pagImageView.setPath(path);

                pagImageViewGroup.addView(pagImageView);
                RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) pagImageView.getLayoutParams();
                params.width = itemWidth;
                params.height = itemHeight;
                pagImageView.setLayoutParams(params);

                pagImageView.setY((i >> 2) * itemWidth + 300);
                pagImageView.setX((i % 4) * itemHeight);

                pagImageView.setRepeatCount(-1);
                pagImageView.setCacheAllFramesInMemory(false);
                pagImageView.play();
            }
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (pagView != null) {
            pagView.freeCache();
            onRelease();
        }
        if (pagImageViewGroup != null) {
            pagImageViewGroup.removeAllViews();
        }
    }

    private void activatedView(int viewId) {
        if (viewId == R.id.play_first) {
            btPlayFirst.setActivated(true);
            btPlaySecond.setActivated(false);
        } else if (viewId == R.id.play_second) {
            btPlayFirst.setActivated(false);
            btPlaySecond.setActivated(true);
        }
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.play_second) {
            if (pagView.getVisibility() == View.VISIBLE) {
                pagView.setVisibility(View.INVISIBLE);
            }
           if (pagImageViewGroup == null) {
               addPAGImageViewsAndPlay();
           } else if (pagImageViewGroup.getVisibility() == View.INVISIBLE) {
               pagImageViewGroup.setVisibility(View.VISIBLE);
           }
           activatedView(R.id.play_second);
        } else {
            if (pagImageViewGroup != null && (pagImageViewGroup.getVisibility() == View.VISIBLE)) {
                pagImageViewGroup.setVisibility(View.INVISIBLE);
            }
            if (pagView.getVisibility() == View.INVISIBLE) {
                pagView.setVisibility(View.VISIBLE);
            }
            activatedView(R.id.play_first);
        }
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
        if (eglContext != null && EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
            if (textureID > 0) {
                int[] textures = {textureID};
                GLES20.glDeleteTextures(1, textures, 0);
            }
            EGL14.eglMakeCurrent(eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
            EGL14.eglDestroySurface(eglDisplay, eglSurface);
            EGL14.eglDestroyContext(eglDisplay, eglContext);
            eglSurface = null;
            eglContext = null;
        }
    }
}
