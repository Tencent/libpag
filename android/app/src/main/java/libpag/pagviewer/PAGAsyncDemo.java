package libpag.pagviewer;

/**
 * JNI wrapper for PAG async rendering demo.
 * Tests cross-EGL context texture access issues.
 */
public class PAGAsyncDemo {
    
    static {
        System.loadLibrary("pag");
    }
    
    /**
     * Initialize PAG async demo.
     * @param width texture width
     * @param height texture height  
     * @param pagData PAG file data
     */
    public native void nativeInit(int width, int height, byte[] pagData);
    
    /**
     * Get rendered texture at specified timestamp.
     * @param timestamp time in microseconds
     * @return OpenGL texture ID
     */
    public native int nativeGetTexture(long timestamp);
    
    /**
     * Stop async rendering.
     */
    public native void nativeStop();
    
    /**
     * Release resources.
     */
    public native void nativeRelease();
}