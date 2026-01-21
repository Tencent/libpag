/*
 * PAGAsyncDemo.cpp
 * 
 * 用于复现 PAG 异步渲染时跨 EGL 上下文访问纹理出现 EGL_BAD_ACCESS 的问题
 * 
 * 场景说明：
 * 1. 主线程（有 EGL 上下文）创建纹理
 * 2. 使用 PAGSurface::MakeFrom(..., forAsyncThread = true) 创建 PAGSurface
 *    这会让 PAG 内部创建一个独立的 EGL 上下文
 * 3. 在异步线程中执行 PAG 渲染到这个纹理
 * 4. 主线程尝试使用这个纹理
 * 
 * 问题复现：
 * 当异步线程完成渲染后，主线程立即访问纹理时，可能出现 EGL_BAD_ACCESS 错误
 * 因为 PAG 的独立 EGL 上下文可能还在占用这个纹理
 */

#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <pag/pag.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#define LOG_TAG "PAGAsyncDemo"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

/**
 * PAGAsyncDemo 类
 * 
 * 模拟 PAGLayerRendererAsync.cpp 中的异步渲染场景
 * 用于复现跨 EGL 上下文访问纹理的问题
 */
class PAGAsyncDemo {
public:
    PAGAsyncDemo() = default;

    ~PAGAsyncDemo() {
        stop();
    }

    /**
     * 初始化（必须在有 EGL 上下文的 GL 线程中调用）
     * 
     * @param width 纹理宽度
     * @param height 纹理高度
     * @param pagData PAG 文件数据
     * @param dataSize 数据大小
     */
    void init(int width, int height, const void *pagData, size_t dataSize) {
        width_ = width;
        height_ = height;

        // ===== Step 1: 在主线程创建纹理 =====
        glGenTextures(1, &textureId_);
        glBindTexture(GL_TEXTURE_2D, textureId_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        GLenum glError = glGetError();
        if (glError != GL_NO_ERROR) {
            LOGE("GL Error after creating texture: 0x%x", glError);
        }

        LOGI("Created texture: %d (%dx%d)", textureId_, width, height);

        // ===== Step 2: 加载 PAG 文件 =====
        auto byteData = pag::ByteData::MakeCopy(pagData, dataSize);
        pagFile_ = pag::PAGFile::Load(byteData->data(), byteData->length());
        if (pagFile_ == nullptr) {
            LOGE("Failed to load PAG file from data");
            return;
        }
        LOGI("Loaded PAG file, duration: %lld ms", (long long) pagFile_->duration() / 1000);

        // ===== Step 3: 创建 PAGPlayer =====
        pagPlayer_ = std::make_shared<pag::PAGPlayer>();
        pagPlayer_->setComposition(pagFile_);

        // ===== Step 4: 关键步骤！创建 PAGSurface，使用 forAsyncThread = true =====
        // 这将让 PAG 内部创建一个独立的 EGL 上下文
        pag::GLTextureInfo textureInfo = {};
        textureInfo.id = textureId_;
        textureInfo.target = GL_TEXTURE_2D;
        textureInfo.format = GL_RGBA8;
        pag::BackendTexture backendTexture(textureInfo, width, height);

        // forAsyncThread = true，PAG 会创建独立的 EGL 上下文
        // 这是问题的关键！PAG 创建的独立上下文和主线程的上下文之间
        // 没有使用共享上下文方式创建，但共享了同一个纹理
        pagSurface_ = pag::PAGSurface::MakeFrom(
                backendTexture,
                pag::ImageOrigin::TopLeft,
                true  // forAsyncThread = true，关键参数！
        );

        if (pagSurface_ == nullptr) {
            LOGE("Failed to create PAGSurface with forAsyncThread=true");
            return;
        }

        pagPlayer_->setSurface(pagSurface_);
        LOGI("Created PAGSurface with forAsyncThread=true");

        // ===== Step 5: 启动异步渲染线程 =====
        running_ = true;
        asyncThread_ = std::thread(&PAGAsyncDemo::asyncRenderLoop, this);

        LOGI("PAGAsyncDemo initialized successfully");
    }

    /**
     * 获取渲染后的纹理（在 GL 线程调用）
     * 
     * 这里可能会触发 EGL_BAD_ACCESS 错误
     * 因为 PAG 的独立 EGL 上下文可能还在占用这个纹理
     * 
     * @param timestamp 时间戳（微秒）
     * @return 纹理 ID
     */
    GLuint getRenderedTexture(int64_t timestamp) {
        if (!running_ || pagPlayer_ == nullptr) {
            return 0;
        }

        // 通知异步线程渲染指定时间点
        {
            std::lock_guard<std::mutex> lock(mutex_);
            targetTimestamp_ = timestamp;
            needRender_ = true;
        }
        cv_.notify_one();

        // 等待渲染完成
        {
            std::unique_lock<std::mutex> lock(mutex_);
            renderDoneCv_.wait(lock, [this] { return renderDone_; });
            renderDone_ = false;
        }

        // ===== 问题可能出现在这里 =====
        // 主线程尝试使用纹理时，可能会出现 EGL_BAD_ACCESS
        // 因为 PAG 的独立 EGL 上下文可能还在占用这个纹理

        // 检查 EGL 错误
        EGLint eglError = eglGetError();
        if (eglError != EGL_SUCCESS) {
            LOGE("=== EGL Error detected: 0x%x ===", eglError);
            if (eglError == 0x3002) { // EGL_BAD_ACCESS
                LOGE(">>> EGL_BAD_ACCESS (0x3002) - 问题复现成功！<<<");
                LOGE(">>> 这表明跨 EGL 上下文访问纹理时发生冲突 <<<");
            }
        }

        // 检查 GL 错误
        GLenum glError = glGetError();
        if (glError != GL_NO_ERROR) {
            LOGE("GL Error: 0x%x", glError);
        }

        // 尝试绑定纹理（这里可能会触发错误）
        glBindTexture(GL_TEXTURE_2D, textureId_);
        glError = glGetError();
        if (glError != GL_NO_ERROR) {
            LOGE("GL Error after binding texture: 0x%x", glError);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        return textureId_;
    }

    /**
     * 获取渲染后的纹理（带 glFinish 修复版本）
     * 
     * 添加 glFinish() 强制等待 GPU 完成所有操作
     * 这是 PAGLayerRendererAsync.cpp 中当前使用的修复方案
     */
    GLuint getRenderedTextureWithFix(int64_t timestamp) {
        if (!running_ || pagPlayer_ == nullptr) {
            return 0;
        }

        // 通知异步线程渲染指定时间点
        {
            std::lock_guard<std::mutex> lock(mutex_);
            targetTimestamp_ = timestamp;
            needRender_ = true;
        }
        cv_.notify_one();

        // 等待渲染完成
        {
            std::unique_lock<std::mutex> lock(mutex_);
            renderDoneCv_.wait(lock, [this] { return renderDone_; });
            renderDone_ = false;
        }

        // ===== 修复方案：添加 glFinish() =====
        // 强制等待 GPU 完成所有操作，确保 PAG 独立上下文完全释放对共享纹理的占用
        // 注意：这个 glFinish() 应该在 PAG 的上下文中调用才有效
        // 但这里是主线程上下文，所以这个修复可能不完整
        glFinish();

        // 检查错误
        EGLint eglError = eglGetError();
        if (eglError != EGL_SUCCESS) {
            LOGW("EGL Error (with fix): 0x%x", eglError);
        }

        return textureId_;
    }

    /**
     * 停止异步渲染
     */
    void stop() {
        if (running_) {
            running_ = false;
            cv_.notify_one();
            if (asyncThread_.joinable()) {
                asyncThread_.join();
            }
            LOGI("Async render thread stopped");
        }
    }

    /**
     * 释放资源
     */
    void release() {
        stop();

        pagPlayer_ = nullptr;
        pagSurface_ = nullptr;
        pagFile_ = nullptr;

        if (textureId_ != 0) {
            glDeleteTextures(1, &textureId_);
            textureId_ = 0;
        }

        LOGI("PAGAsyncDemo released");
    }

private:
    /**
     * 异步渲染线程的主循环
     * 
     * 模拟 PAGLayerRendererAsync 中的异步渲染流程
     */
    void asyncRenderLoop() {

        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return needRender_ || !running_; });

            if (!running_) break;

            if (needRender_) {
                needRender_ = false;
                int64_t timestamp = targetTimestamp_;
                lock.unlock();

                // 在异步线程中执行 PAG 渲染
                doAsyncRender(timestamp);

                // 通知主线程渲染完成
                {
                    std::lock_guard<std::mutex> lk(mutex_);
                    renderDone_ = true;
                }
                renderDoneCv_.notify_one();
            }
        }

        LOGI("Async render thread exiting");
    }

    /**
     * 异步渲染实现
     * 
     * 模拟 PAGLayerRendererAsync::asyncFlush() 和 onFlush() 的逻辑
     */
    void doAsyncRender(int64_t timestamp) {
        if (pagPlayer_ == nullptr || pagSurface_ == nullptr || pagFile_ == nullptr) {
            LOGE("PAG objects not initialized");
            return;
        }

        // 设置当前时间（循环播放）
        int64_t duration = pagFile_->duration();
        int64_t currentTime = (duration > 0) ? (timestamp % duration) : 0;
        pagFile_->setCurrentTime(currentTime);

        // ===== 模拟 PAGLayerRendererAsync::onFlush() =====

        // 1. 等待主线程的信号（beginFlushFence）
        // 在实际代码中，这里会调用 pagPlayer_->wait(startSemaphore)
        // 但在这个简化的 demo 中，我们跳过这一步

        // 2. 执行渲染并发信号
        pag::BackendSemaphore endSemaphore = {};
        pagPlayer_->flushAndSignalSemaphore(&endSemaphore);

        // 3. 获取 fence
        void *sync = endSemaphore.glSync();
        if (endSemaphore.isInitialized() && sync != nullptr) {
            LOGI("Async render done, fence created: %p (time=%lld)", sync, (long long) currentTime);
            // 注意：这个 fence 是在 PAG 的独立 EGL 上下文中创建的
            // 主线程需要等待这个 fence 才能安全访问纹理
            // 但问题是主线程可能无法正确等待这个 fence
            // 因为两个上下文不是共享的
        } else {
            LOGI("Async render done, no fence (time=%lld)", (long long) currentTime);
        }

        // ===== 问题根源 =====
        // 此时 PAG 的独立 EGL 上下文已经向纹理写入了数据
        // 但 GPU 命令可能还没有执行完毕
        // 而且即使执行完毕，由于两个 EGL 上下文不是共享的
        // 主线程可能无法正确同步到这些变更

        renderCount_++;
        if (renderCount_ % 30 == 0) {
            LOGI("Rendered %d frames", renderCount_);
        }
    }

private:
    int width_ = 0;
    int height_ = 0;
    GLuint textureId_ = 0;

    std::shared_ptr<pag::PAGFile> pagFile_;
    std::shared_ptr<pag::PAGPlayer> pagPlayer_;
    std::shared_ptr<pag::PAGSurface> pagSurface_;

    std::thread asyncThread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable renderDoneCv_;
    bool needRender_ = false;
    bool renderDone_ = false;
    int64_t targetTimestamp_ = 0;

    int renderCount_ = 0;
};

// ===== JNI 接口实现 =====

static PAGAsyncDemo *gDemo = nullptr;

extern "C" {

/**
 * 初始化 Demo（在 GL 线程调用）
 */
JNIEXPORT void JNICALL
Java_libpag_pagviewer_PAGAsyncDemo_nativeInit(JNIEnv *env, jobject,
                                                 jint width, jint height,
                                                 jbyteArray pagData) {
    LOGI("nativeInit called: %dx%d", width, height);

    if (gDemo != nullptr) {
        gDemo->release();
        delete gDemo;
    }

    // 获取 PAG 数据
    jsize dataSize = env->GetArrayLength(pagData);
    jbyte *data = env->GetByteArrayElements(pagData, nullptr);

    gDemo = new PAGAsyncDemo();
    gDemo->init(width, height, data, dataSize);

    env->ReleaseByteArrayElements(pagData, data, JNI_ABORT);
}

/**
 * 获取渲染后的纹理（在 GL 线程调用）
 * 
 * @param useFix 是否使用 glFinish 修复
 */
JNIEXPORT jint JNICALL
Java_libpag_pagviewer_PAGAsyncDemo_nativeGetTexture(JNIEnv *, jobject ,
                                                       jlong timestamp) {
    if (gDemo == nullptr) {
        LOGE("Demo not initialized");
        return 0;
    }
    return gDemo->getRenderedTexture(timestamp);

}

/**
 * 停止 Demo
 */
JNIEXPORT void JNICALL
Java_libpag_pagviewer_PAGAsyncDemo_nativeStop(JNIEnv *, jobject ) {
    if (gDemo != nullptr) {
        gDemo->stop();
    }
}

/**
 * 释放资源
 */
JNIEXPORT void JNICALL
Java_libpag_pagviewer_PAGAsyncDemo_nativeRelease(JNIEnv *, jobject ) {
    if (gDemo != nullptr) {
        gDemo->release();
        delete gDemo;
        gDemo = nullptr;
        LOGI("Demo released");
    }
}

}
