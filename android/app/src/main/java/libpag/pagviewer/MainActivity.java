package libpag.pagviewer;

import android.app.Activity;
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
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import org.libpag.PAGDiskCache;
import org.libpag.PAGFile;
import org.libpag.PAGImage;
import org.libpag.PAGImageView;
import org.libpag.PAGText;
import org.libpag.PAGView;

import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends Activity {

    private static final String TAG = "PAGAsyncDemo";
    private static final String PAG_FILE_NAME = "zuanshi_bmp.pag";

    // PAG 文件列表（可根据需要添加更多）
    private static final String[] PAG_FILES = {
            "zuanshi_bmp.pag",
            "diamonds_bmp.pag",
            "Light_lian.pag"
    };
    private int currentPagIndex = 0;

    private GLSurfaceView glSurfaceView;
    private PAGDemoRenderer renderer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.i(TAG, "===========================================");
        Log.i(TAG, "PAG 异步渲染问题复现 Demo");
        Log.i(TAG, "===========================================");
        Log.i(TAG, "说明：");
        Log.i(TAG, "1. 观察 logcat 中是否出现 EGL_BAD_ACCESS (0x3002) 错误");
        Log.i(TAG, "2. 点击按钮切换是否使用 glFinish 修复");
        Log.i(TAG, "===========================================");

        // 创建布局
        LinearLayout rootLayout = new LinearLayout(this);
        rootLayout.setOrientation(LinearLayout.VERTICAL);

        // 创建状态文本
        TextView statusText = new TextView(this);
        statusText.setText("PAG Async Demo - 查看 logcat 输出");
        statusText.setPadding(16, 16, 16, 16);
        rootLayout.addView(statusText);

        // 创建切换按钮
        Button toggleButton = new Button(this);
        toggleButton.setText("切换 PAG 文件: " + PAG_FILE_NAME);
        toggleButton.setOnClickListener(v -> {
            // 切换到下一个 PAG 文件
            currentPagIndex = (currentPagIndex + 1) % PAG_FILES.length;
            String newPagFile = PAG_FILES[currentPagIndex];

            Log.i(TAG, "切换 PAG 文件: " + newPagFile);
            toggleButton.setText("切换 PAG 文件: " + newPagFile);

            // 通知 Renderer 加载新的 PAG 文件
            if (renderer != null) {
                renderer.switchPagFile(newPagFile);
            }
        });

        rootLayout.addView(toggleButton);

        // 创建 GLSurfaceView
        glSurfaceView = new GLSurfaceView(this);
        glSurfaceView.setEGLContextClientVersion(3); // 使用 OpenGL ES 3.0

        // 创建 Renderer
        renderer = new PAGDemoRenderer(this, PAG_FILE_NAME);

        glSurfaceView.setRenderer(renderer);
        glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

        // 设置 GLSurfaceView 的布局参数
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0,
                1.0f // weight
        );
        glSurfaceView.setLayoutParams(params);
        rootLayout.addView(glSurfaceView);

        setContentView(rootLayout);
    }



    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) {
            glSurfaceView.onResume();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (glSurfaceView != null) {
            glSurfaceView.onPause();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (renderer != null) {
            // 在 GL 线程释放资源
            glSurfaceView.queueEvent(new Runnable() {
                @Override
                public void run() {
                    renderer.release();
                }
            });
        }
    }
}