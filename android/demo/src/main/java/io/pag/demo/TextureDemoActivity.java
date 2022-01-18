package io.pag.demo;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

import io.pag.demo.openGL.GLRender;

public class TextureDemoActivity extends Activity {
    private final static String TAG = TextureDemoActivity.class.getSimpleName();

    private GLSurfaceView glSurfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_texture_demo);
        glSurfaceView = findViewById(R.id.surfaceView);
        final GLRender glRender = new GLRender(this.getBaseContext());
        glSurfaceView.setEGLContextClientVersion(2);
        glSurfaceView.setEGLWindowSurfaceFactory(new GLSurfaceView.EGLWindowSurfaceFactory() {
            public EGLSurface createWindowSurface(EGL10 egl, EGLDisplay display,
                                                  EGLConfig config, Object nativeWindow) {
                EGLSurface result = null;
                try {
                    result = egl.eglCreateWindowSurface(display, config, nativeWindow, null);
                } catch (IllegalArgumentException e) {
                    // This exception indicates that the surface flinger surface
                    // is not valid. This can happen if the surface flinger surface has
                    // been torn down, but the application has not yet been
                    // notified via SurfaceHolder.Callback.surfaceDestroyed.
                    // In theory the application should be notified first,
                    // but in practice sometimes it is not. See b/4588890
                    Log.e(TAG, "eglCreateWindowSurface", e);
                }
                return result;
            }

            public void destroySurface(EGL10 egl, EGLDisplay display, EGLSurface surface) {
                glRender.onDestroy();
                egl.eglDestroySurface(display, surface);
            }
        });
        glSurfaceView.setRenderer(glRender);
        glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
    }

    @Override
    protected void onResume() {
        super.onResume();
        glSurfaceView.onResume();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        glSurfaceView.onPause();

    }
}
