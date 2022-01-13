package io.pag.demo;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;

import io.pag.demo.openGL.GLRender;

public class TextureDemoActivity extends Activity {

    private GLSurfaceView glSurfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_texture_demo);
        glSurfaceView = (GLSurfaceView) findViewById(R.id.surfaceView);
        GLRender glRender = new GLRender(this.getBaseContext());
        glSurfaceView.setEGLContextClientVersion(2);
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
