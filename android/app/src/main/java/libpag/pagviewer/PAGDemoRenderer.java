package libpag.pagviewer;

import android.content.Context;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.util.Log;



import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * GLSurfaceView.Renderer implementation for testing PAGSurface.FromTexture rendering.
 */
public class PAGDemoRenderer implements GLSurfaceView.Renderer {
    
    private static final String TAG = "PAGDemoRenderer";
    
    private final Context context;
    private String pagFileName;
    
    private volatile String pendingPagFileName = null;
    
    private int width = 720;
    private int height = 1280;
    private long startTime = 0;
    private boolean initialized = false;
    
    // PAG related
    private PAGAsyncDemo pagDemo = null;
    
    // Frame count
    private int frameCount = 0;
    private long lastLogTime = 0;
    
    // OpenGL rendering
    private int shaderProgram = 0;
    private int vao = 0;
    private int vbo = 0;
    private int positionHandle = -1;
    private int texCoordHandle = -1;
    private int textureUniformHandle = -1;
    
    private static final String VERTEX_SHADER =
            "#version 300 es\n" +
            "in vec4 aPosition;\n" +
            "in vec2 aTexCoord;\n" +
            "out vec2 vTexCoord;\n" +
            "void main() {\n" +
            "    gl_Position = aPosition;\n" +
            "    vTexCoord = aTexCoord;\n" +
            "}\n";
    
    private static final String FRAGMENT_SHADER =
            "#version 300 es\n" +
            "precision mediump float;\n" +
            "in vec2 vTexCoord;\n" +
            "out vec4 fragColor;\n" +
            "uniform sampler2D uTexture;\n" +
            "void main() {\n" +
            "    fragColor = texture(uTexture, vTexCoord);\n" +
            "}\n";
    
    // Vertex data: position(x,y,z) + texCoord(s,t) for fullscreen quad
    private static final float[] VERTEX_DATA = {
            -1.0f,  1.0f, 0.0f,  0.0f, 0.0f,  // top-left (flipped Y for texture)
            -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,  // bottom-left
             1.0f, -1.0f, 0.0f,  1.0f, 1.0f,  // bottom-right
            -1.0f,  1.0f, 0.0f,  0.0f, 0.0f,  // top-left
             1.0f, -1.0f, 0.0f,  1.0f, 1.0f,  // bottom-right
             1.0f,  1.0f, 0.0f,  1.0f, 0.0f   // top-right
    };
    
    public PAGDemoRenderer(Context context, String pagFileName) {
        this.context = context;
        this.pagFileName = pagFileName;
    }
    
    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        Log.i(TAG, "onSurfaceCreated");
        
        GLES30.glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        
        initShaderProgram();
        initVAO();
        
        startTime = System.currentTimeMillis();
        lastLogTime = startTime;
    }
    
    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        Log.i(TAG, "onSurfaceChanged: " + width + "x" + height);
        
        this.width = width;
        this.height = height;
        
        GLES30.glViewport(0, 0, width, height);
        
        if (!initialized) {
            initPAGDemo();
            initialized = true;
        }
    }
    
    @Override
    public void onDrawFrame(GL10 gl) {
        // 清除屏幕
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
        
        // 检查是否需要加载新的 PAG 文件
        if (pendingPagFileName != null) {
            loadNewPagFile(pendingPagFileName);
            pendingPagFileName = null;
        }
        
        if (!initialized || pagDemo == null) {
            return;
        }
        
        // 计算当前时间戳（微秒）
        long currentTime = System.currentTimeMillis();
        long timestamp = (currentTime - startTime) * 1000; // 转换为微秒
        
        // 获取渲染后的纹理
        // 这里可能会触发 EGL_BAD_ACCESS 错误
        int textureId = pagDemo.nativeGetTexture(timestamp);
        
        if (textureId > 0) {
            // 简单的绘制纹理到屏幕
            drawTexture(textureId);
        }
        
        // 帧率统计
        frameCount++;
        if (currentTime - lastLogTime >= 1000) {
            Log.i(TAG, "FPS: " + frameCount + ", textureId: " + textureId);
            frameCount = 0;
            lastLogTime = currentTime;
        }
    }
    
    private void initPAGDemo() {
        try {
            // Load PAG file data
            byte[] pagData = loadAssetFile(pagFileName);
            if (pagData == null || pagData.length == 0) {
                Log.e(TAG, "Failed to load PAG file: " + pagFileName);
                return;
            }
            
            pagDemo = new PAGAsyncDemo();
            pagDemo.nativeInit(width, height, pagData);
            
            Log.i(TAG, "PAGAsyncDemo initialized successfully: " + pagFileName);
            
        } catch (Exception e) {
            Log.e(TAG, "Error initializing PAGAsyncDemo", e);
            if (pagDemo != null) {
                pagDemo.nativeRelease();
                pagDemo = null;
            }
        }
    }
    
    private byte[] loadAssetFile(String fileName) {
        try {
            java.io.InputStream is = context.getAssets().open(fileName);
            java.io.ByteArrayOutputStream baos = new java.io.ByteArrayOutputStream();
            byte[] buffer = new byte[4096];
            int len;
            while ((len = is.read(buffer)) != -1) {
                baos.write(buffer, 0, len);
            }
            is.close();
            return baos.toByteArray();
        } catch (Exception e) {
            Log.e(TAG, "Failed to load asset: " + fileName, e);
            return null;
        }
    }
    
    private void drawTexture(int textureId) {
        GLES30.glUseProgram(shaderProgram);
        
        GLES30.glActiveTexture(GLES30.GL_TEXTURE0);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, textureId);
        GLES30.glUniform1i(textureUniformHandle, 0);
        
        GLES30.glBindVertexArray(vao);
        GLES30.glDrawArrays(GLES30.GL_TRIANGLES, 0, 6);
        
        GLES30.glBindVertexArray(0);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, 0);
        GLES30.glUseProgram(0);
        
        int error = GLES30.glGetError();
        if (error != GLES30.GL_NO_ERROR) {
            Log.e(TAG, "GL Error when drawing texture: 0x" + Integer.toHexString(error));
        }
    }
    
    private void initShaderProgram() {
        int vertexShader = GLES30.glCreateShader(GLES30.GL_VERTEX_SHADER);
        GLES30.glShaderSource(vertexShader, VERTEX_SHADER);
        GLES30.glCompileShader(vertexShader);
        checkShaderCompileStatus(vertexShader, "Vertex");
        
        int fragmentShader = GLES30.glCreateShader(GLES30.GL_FRAGMENT_SHADER);
        GLES30.glShaderSource(fragmentShader, FRAGMENT_SHADER);
        GLES30.glCompileShader(fragmentShader);
        checkShaderCompileStatus(fragmentShader, "Fragment");
        
        shaderProgram = GLES30.glCreateProgram();
        GLES30.glAttachShader(shaderProgram, vertexShader);
        GLES30.glAttachShader(shaderProgram, fragmentShader);
        GLES30.glLinkProgram(shaderProgram);
        checkProgramLinkStatus(shaderProgram);
        
        positionHandle = GLES30.glGetAttribLocation(shaderProgram, "aPosition");
        texCoordHandle = GLES30.glGetAttribLocation(shaderProgram, "aTexCoord");
        textureUniformHandle = GLES30.glGetUniformLocation(shaderProgram, "uTexture");
        
        GLES30.glDeleteShader(vertexShader);
        GLES30.glDeleteShader(fragmentShader);
        
        Log.i(TAG, "Shader program initialized");
    }
    
    private void initVAO() {
        FloatBuffer vertexBuffer = ByteBuffer.allocateDirect(VERTEX_DATA.length * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer();
        vertexBuffer.put(VERTEX_DATA).position(0);
        
        int[] vaos = new int[1];
        GLES30.glGenVertexArrays(1, vaos, 0);
        vao = vaos[0];
        GLES30.glBindVertexArray(vao);
        
        int[] vbos = new int[1];
        GLES30.glGenBuffers(1, vbos, 0);
        vbo = vbos[0];
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vbo);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, VERTEX_DATA.length * 4, vertexBuffer, GLES30.GL_STATIC_DRAW);
        
        int stride = 5 * 4;
        GLES30.glVertexAttribPointer(positionHandle, 3, GLES30.GL_FLOAT, false, stride, 0);
        GLES30.glEnableVertexAttribArray(positionHandle);
        
        GLES30.glVertexAttribPointer(texCoordHandle, 2, GLES30.GL_FLOAT, false, stride, 3 * 4);
        GLES30.glEnableVertexAttribArray(texCoordHandle);
        
        GLES30.glBindVertexArray(0);
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, 0);
        
        Log.i(TAG, "VAO and VBO initialized");
    }
    
    private void checkShaderCompileStatus(int shader, String type) {
        int[] status = new int[1];
        GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, status, 0);
        if (status[0] == 0) {
            String log = GLES30.glGetShaderInfoLog(shader);
            Log.e(TAG, type + " shader compile error: " + log);
            GLES30.glDeleteShader(shader);
        }
    }
    
    private void checkProgramLinkStatus(int program) {
        int[] status = new int[1];
        GLES30.glGetProgramiv(program, GLES30.GL_LINK_STATUS, status, 0);
        if (status[0] == 0) {
            String log = GLES30.glGetProgramInfoLog(program);
            Log.e(TAG, "Program link error: " + log);
            GLES30.glDeleteProgram(program);
        }
    }
    
    public void switchPagFile(String newPagFileName) {
        Log.i(TAG, "Request to switch PAG file: " + newPagFileName);
        this.pendingPagFileName = newPagFileName;
    }
    
    private void loadNewPagFile(String newPagFileName) {
        Log.i(TAG, "Loading new PAG file: " + newPagFileName);
        
        // 先释放旧的 PAG 资源
        pagDemo.nativeRelease();
        
        this.pagFileName = newPagFileName;
        startTime = System.currentTimeMillis();
        
        initPAGDemo();
    }
    

    public String getCurrentPagFileName() {
        return pagFileName;
    }
    
    public void release() {
        Log.i(TAG, "release");
        
        // 释放 OpenGL 资源
        if (vao != 0) {
            int[] vaos = {vao};
            GLES30.glDeleteVertexArrays(1, vaos, 0);
            vao = 0;
        }
        if (vbo != 0) {
            int[] vbos = {vbo};
            GLES30.glDeleteBuffers(1, vbos, 0);
            vbo = 0;
        }
        if (shaderProgram != 0) {
            GLES30.glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
        
        pagDemo.nativeRelease();
        initialized = false;
    }
}
