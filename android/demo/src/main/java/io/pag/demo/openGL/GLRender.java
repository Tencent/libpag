package io.pag.demo.openGL;

import android.content.Context;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.util.Log;

import org.libpag.PAGFile;
import org.libpag.PAGPlayer;
import org.libpag.PAGSurface;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class GLRender implements GLSurfaceView.Renderer {

    private static final String TAG = "GLRender";
    private int textureId;
    private PAGSurface pagSurface;
    private PAGPlayer pagPlayer;
    private Context context;
    private long duration;
    int mWidth = 0;
    int mHeight = 0;
    private long timestamp = 0;

    private static final String VERTEX_MAIN =
            "attribute vec2  vPosition;\n" +
                    "attribute vec2  vTexCoord;\n" +
                    "varying vec2    texCoord;\n" +
                    "\n" +
                    "void main() {\n" +
                    "    texCoord = vTexCoord;\n" +
                    "    gl_Position = vec4 ( vPosition.x, vPosition.y, 0.0, 1.0 );\n" +
                    "}";

    private static final String FRAGMENT_MAIN =
            "precision mediump float;\n" +
                    "\n" +
                    "varying vec2                texCoord;\n" +
                    "uniform sampler2D sTexture;\n" +
                    "\n" +
                    "void main() {\n" +
                    "    gl_FragColor = texture2D(sTexture, texCoord);\n" +
                    "}";

    static final float SQUARE_COORDS[] = {
            1.0f, -1.0f,
            -1.0f, -1.0f,
            1.0f, 1.0f,
            -1.0f, 1.0f,
    };

    static final float TEXTURE_COORDS[] = {
            1f, 1f,
            0f, 1f,
            1f, 0f,
            0f, 0f
    };

    private int mProgram;
    static FloatBuffer VERTEX_BUF, TEXTURE_COORD_BUF;

    private void initShader() {

        if (VERTEX_BUF == null) {
            VERTEX_BUF = ByteBuffer.allocateDirect(SQUARE_COORDS.length * 4)
                    .order(ByteOrder.nativeOrder()).asFloatBuffer();
            VERTEX_BUF.put(SQUARE_COORDS);
            VERTEX_BUF.position(0);
        }
        if (TEXTURE_COORD_BUF == null) {
            TEXTURE_COORD_BUF = ByteBuffer.allocateDirect(TEXTURE_COORDS.length * 4)
                    .order(ByteOrder.nativeOrder()).asFloatBuffer();
            TEXTURE_COORD_BUF.put(TEXTURE_COORDS);
            TEXTURE_COORD_BUF.position(0);
        }
        if (mProgram == 0) {
            mProgram = GLUtil.buildProgram(VERTEX_MAIN, FRAGMENT_MAIN);
        }
    }

    public GLRender(Context context){
        this.context = context;
    }

    public void onDestroy() {
        if (pagPlayer != null) {
            pagPlayer.release();
        }
        if (pagSurface != null) {
            pagSurface.release();
        }
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        PAGFile pagFile = PAGFile.Load(context.getAssets(), "replacement.pag");
        mWidth = pagFile.width();
        mHeight = pagFile.height();
        duration = pagFile.duration();

        textureId =  initRenderTarget();

        pagSurface = PAGSurface.FromTexture(textureId, mWidth, mHeight);
        pagPlayer = new PAGPlayer();
        pagPlayer.setComposition(pagFile);
        pagPlayer.setSurface(pagSurface);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        GLES20.glViewport(0, 0, width, height);
        Log.d(TAG,"width is " + width + " height is " + height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        if (timestamp == 0) {
            timestamp = System.currentTimeMillis();
        }
        long playTime = (System.currentTimeMillis() - timestamp) * 1000;
        pagPlayer.setProgress(playTime % duration * 1.0f / duration);
        pagPlayer.flush();

        initShader();
        Log.d(TAG,"draw texture id is " + textureId );
        GLES20.glUseProgram(mProgram);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,0);
        int vPositionLocation = GLES20.glGetAttribLocation(mProgram, "vPosition");
        GLES20.glEnableVertexAttribArray(vPositionLocation);
        GLES20.glVertexAttribPointer(vPositionLocation, 2, GLES20.GL_FLOAT, false, 4 * 2, VERTEX_BUF);
        int vTexCoordLocation = GLES20.glGetAttribLocation(mProgram, "vTexCoord");
        GLES20.glEnableVertexAttribArray(vTexCoordLocation);
        GLES20.glVertexAttribPointer(vTexCoordLocation, 2, GLES20.GL_FLOAT, false, 4 * 2, TEXTURE_COORD_BUF);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    }

    private int initRenderTarget() {
        int[] id = {0};
        GLES20.glGenTextures(1, id, 0);
        if (id[0] == 0) {
            return 0;
        }
        int textureId = id[0];
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER, GL10.GL_LINEAR);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER, GL10.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S, GL10.GL_REPEAT);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T, GL10.GL_REPEAT);
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0,GLES20.GL_RGBA,mWidth,mHeight,0,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,null);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        return textureId;
    }

}

