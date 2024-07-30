#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <cstdint>
#include <list>
#include <multimedia/player_framework/native_avcapability.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include <multimedia/player_framework/native_avcodec_videodecoder.h>
#include <queue>
#include <thread>
#include "pag/file.h"
#include "src/base/utils/Log.h"
#include "src/rendering/video/VideoFormat.h"
#include "tgfx/opengl/egl/EGLWindow.h"
#include "pag/pag.h"
#include "drawers/AppHost.h"
#include "drawers/Drawer.h"

#include "src/platform/ohos/GPUDrawable.h"

static float screenDensity = 1.0f;
static std::shared_ptr<drawers::AppHost> appHost = nullptr;
static std::shared_ptr<tgfx::Window> window = nullptr;

static std::shared_ptr<drawers::AppHost> CreateAppHost();

static pag::PAGPlayer *pagPlayer = nullptr;
static std::shared_ptr<pag::PAGSurface> pagSurface = nullptr;


struct CodecBufferInfo {
    uint32_t bufferIndex = 0;
    OH_AVBuffer *buffer = nullptr;
    OH_AVCodecBufferAttr attr = {0, 0, 0, AVCODEC_BUFFER_FLAGS_NONE};

    CodecBufferInfo(uint32_t argBufferIndex, OH_AVBuffer *argBuffer) : bufferIndex(argBufferIndex), buffer(argBuffer) {
        OH_AVBuffer_GetBufferAttr(argBuffer, &attr);
    };
};

class CodecUserData {
public:
    std::mutex inputMutex;
    std::condition_variable inputCondition;
    std::queue<CodecBufferInfo> inputBufferInfoQueue;

    std::mutex outputMutex;
    std::condition_variable outputCondition;
    std::queue<CodecBufferInfo> outputBufferInfoQueue;

    void clearQueue() {
        {
            std::unique_lock<std::mutex> lock(inputMutex);
            auto emptyQueue = std::queue<CodecBufferInfo>();
            inputBufferInfoQueue.swap(emptyQueue);
        }
        {
            std::unique_lock<std::mutex> lock(outputMutex);
            auto emptyQueue = std::queue<CodecBufferInfo>();
            outputBufferInfoQueue.swap(emptyQueue);
        }
    }
};

void OnError(OH_AVCodec *, int32_t, void *) {}

void OnStreamChanged(OH_AVCodec *, OH_AVFormat *, void *) {}

void OnNeedInputBuffer(OH_AVCodec *, uint32_t index, OH_AVBuffer *buffer, void *userData) {
    if (userData == nullptr) {
        return;
    }
    CodecUserData *codecUserData = static_cast<CodecUserData *>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->inputMutex);
    codecUserData->inputBufferInfoQueue.emplace(index, buffer);
    codecUserData->inputCondition.notify_all();
}

void OnNewOutputBuffer(OH_AVCodec *, uint32_t index, OH_AVBuffer *buffer, void *userData) {
    if (userData == nullptr) {
        return;
    }
    CodecUserData *codecUserData = static_cast<CodecUserData *>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->outputMutex);
    codecUserData->outputBufferInfoQueue.emplace(index, buffer);
    codecUserData->outputCondition.notify_all();
}

static void VideoDecodeTest() {
    std::string filePath = "/data/storage/el1/bundle/hello2d/resources/resfile/data_video.pag";
    auto pagFile = pag::PAGFile::Load(filePath);
    if (pagFile == nullptr) {
        return;
    }
    pagSurface = pag::PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    if (pagSurface == nullptr) {
        return;
    }
    pagPlayer = new pag::PAGPlayer();
    pagPlayer->setComposition(pagFile);
    pagPlayer->setSurface(pagSurface);
    int numFrames = static_cast<int>(pagFile->frameRate() * static_cast<float>(pagFile->duration()) / 1000000);
    for (int i = 0; i < 1; i++) {
        pagPlayer->setProgress((i + 0.1) / numFrames);
        auto status = pagPlayer->flush();
        if (status) {
            LOGI("pag flush success!, frame:%d", i);
        } else {
            LOGE("pag flush failed!");
        }
    }
    delete pagPlayer;
}

static OH_AVCodec *videoCodec = nullptr;
static CodecUserData *codecUserData = nullptr;
static std::list<int64_t> pendingFrames{};
static std::mutex listMutex;

static void OutputFunc() {
    LOGI("--------------- OutputFunc ---------------------");
    size_t currentIndex = 0;
    while (true) {
        std::unique_lock<std::mutex> lock(codecUserData->outputMutex);
        codecUserData->outputCondition.wait(lock, [] {
            LOGI("--------------- OutputFunc wait, size:%d", codecUserData->outputBufferInfoQueue.size());
            return codecUserData->outputBufferInfoQueue.size() > 0;
        });
        uint32_t index = codecUserData->outputBufferInfoQueue.front().bufferIndex;
        CodecBufferInfo bufferInfo = codecUserData->outputBufferInfoQueue.front();
        lock.unlock();
        OH_AVCodecBufferAttr attr = {0, 0, 0, AVCODEC_BUFFER_FLAGS_NONE};
        int ret = OH_AVBuffer_GetBufferAttr(bufferInfo.buffer, &attr);
        ret = OH_VideoDecoder_FreeOutputBuffer(videoCodec, index);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_FreeOutputBuffer failed!, ret:%d", ret);
            break;
        } else {
            std::unique_lock<std::mutex> listLock(listMutex);
            pendingFrames.remove(attr.pts);
            listLock.unlock();
            LOGI("---------------OH_VideoDecoder_FreeOutputBuffer---success---currentIndex:%d， timestamp:%d, size:%d",
                 currentIndex++, attr.pts, attr.size);
        }
        lock.lock();
        codecUserData->outputBufferInfoQueue.pop();
        lock.unlock();
        if (bufferInfo.attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
            LOGI("---------------OH_VideoDecoder_FreeOutputBuffer---end-");
            break;
        }
    }
}

static void pagVideoSequenceDecodeTest() {
    std::string filePath = "/data/storage/el1/bundle/hello2d/resources/resfile/data_video.pag";
    auto file = pag::File::Load(filePath);
    if (file && file->compositions.size() > 0) {
        pag::VideoComposition *videoComposition = (pag::VideoComposition *)(file->compositions[0]);
        pag::VideoSequence *videoSequence = videoComposition->sequences[0];
        auto headers = videoSequence->headers;
        auto frames = videoSequence->frames;

        OH_AVCapability *capability =
            OH_AVCodec_GetCapabilityByCategory(OH_AVCODEC_MIMETYPE_VIDEO_AVC, false, HARDWARE);
        const char *name = OH_AVCapability_GetName(capability);
        videoCodec = OH_VideoDecoder_CreateByName(name);

        OH_AVFormat *format = OH_AVFormat_Create();
        LOGI("---------------videoSequence->width:%d, height:%d", videoSequence->width, videoSequence->height);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_WIDTH, videoSequence->width);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_HEIGHT, videoSequence->height);
        OH_AVFormat_SetDoubleValue(format, OH_MD_KEY_FRAME_RATE, videoSequence->frameRate);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_NV12);
        int ret = OH_VideoDecoder_Configure(videoCodec, format);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_Configure failed!");
            OH_AVFormat_Destroy(format);
            format = nullptr;
            return;
        }
        OH_AVFormat_Destroy(format);
        format = nullptr;

        codecUserData = new CodecUserData();
        ret = OH_VideoDecoder_RegisterCallback(
            videoCodec, {OnError, OnStreamChanged, OnNeedInputBuffer, OnNewOutputBuffer}, codecUserData);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_RegisterCallback failed!");
            delete codecUserData;
            return;
        }
        ret = OH_VideoDecoder_Prepare(videoCodec);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_Prepare failed!");
            delete codecUserData;
            return;
        }
        ret = OH_VideoDecoder_Start(videoCodec);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_Start failed!");
            delete codecUserData;
            return;
        }
        std::thread work(OutputFunc);
        size_t startFrame = 0; // keyFrame: 0, 23, 46
        size_t endFrame = frames.size() + 2;
        size_t currentIndex = 0;
        while (true) {
            std::unique_lock<std::mutex> lock(codecUserData->inputMutex);
            codecUserData->inputCondition.wait(lock, []() {
                LOGI("--------------- inputFunc wait, size:%d", codecUserData->inputBufferInfoQueue.size());
                return codecUserData->inputBufferInfoQueue.size() > 0;
            });

            CodecBufferInfo codecBufferInfo = codecUserData->inputBufferInfoQueue.front();
            lock.unlock();

            uint32_t index = codecBufferInfo.bufferIndex;
            auto buffer = codecBufferInfo.buffer;

            int64_t pts = 0;

            OH_AVCodecBufferAttr info;
            if (currentIndex < 2) {
                info.size = static_cast<int32_t>(headers[currentIndex]->length());
                info.offset = 0;
                info.pts = 0;
                info.flags = AVCODEC_BUFFER_FLAGS_NONE;
                memcpy(OH_AVBuffer_GetAddr(reinterpret_cast<OH_AVBuffer *>(buffer)), headers[currentIndex]->data(),
                       headers[currentIndex]->length());

            } else if (currentIndex + startFrame < (headers.size() + frames.size()) &&
                       currentIndex + startFrame <= endFrame) {
                info.size = static_cast<int32_t>(frames[currentIndex - 2 + startFrame]->fileBytes->length());
                info.offset = 0;
                pts = static_cast<int64_t>(static_cast<float>(frames[currentIndex - 2 + startFrame]->frame) * 1000000 /
                                           videoSequence->frameRate);
                info.pts = pts;
                info.flags = AVCODEC_BUFFER_FLAGS_NONE;
                memcpy(OH_AVBuffer_GetAddr(reinterpret_cast<OH_AVBuffer *>(buffer)),
                       frames[currentIndex - 2 + startFrame]->fileBytes->data(),
                       frames[currentIndex - 2 + startFrame]->fileBytes->length());
                std::unique_lock<std::mutex> listLock(listMutex);
                pendingFrames.push_back(info.pts);
                listLock.unlock();
            } else {
                info.size = 0;
                info.offset = 0;
                info.pts = 0;
                info.flags = AVCODEC_BUFFER_FLAGS_EOS;
                std::unique_lock<std::mutex> listLock(listMutex);
                pendingFrames.push_back(info.pts);
                listLock.unlock();
            }
            ret = OH_AVBuffer_SetBufferAttr(reinterpret_cast<OH_AVBuffer *>(buffer), &info);
            if (ret != AV_ERR_OK) {
                LOGE("OH_AVBuffer_SetBufferAttr failed!");
                break;
            }
            LOGI("---------------OH_VideoDecoder_PushInputBuffer--index:%d, length:%d, currentIndex:%d, pts:%ld", index,
                 info.size, currentIndex, pts);
            ret = OH_VideoDecoder_PushInputBuffer(videoCodec, index);
            if (ret != AV_ERR_OK) {
                LOGE("OH_VideoDecoder_PushInputBuffer failed!");
                break;
            }
            if (currentIndex >= (headers.size() + frames.size())) {
                break;
            }
            lock.lock();
            codecUserData->inputBufferInfoQueue.pop();
            lock.unlock();
            currentIndex++;
        }

        work.join();
        ret = OH_VideoDecoder_Destroy(videoCodec);
        LOGI("---------------OH_VideoDecoder_Destroy--ret:%d", ret);
        delete codecUserData;
    }
}

static void pagVideoSequenceDecodeTest_1() {
    std::string filePath = "/data/storage/el1/bundle/hello2d/resources/resfile/data_video.pag";
    auto file = pag::File::Load(filePath);
    if (file && file->compositions.size() > 0) {
        pag::VideoComposition *videoComposition = (pag::VideoComposition *)(file->compositions[0]);
        pag::VideoSequence *videoSequence = videoComposition->sequences[0];
        auto headers = videoSequence->headers;
        auto frames = videoSequence->frames;

        OH_AVCapability *capability =
            OH_AVCodec_GetCapabilityByCategory(OH_AVCODEC_MIMETYPE_VIDEO_AVC, false, SOFTWARE);
        const char *name = OH_AVCapability_GetName(capability);
        videoCodec = OH_VideoDecoder_CreateByName(name);

        OH_AVFormat *format = OH_AVFormat_Create();
        LOGI("---------11------videoSequence->width:%d, height:%d", videoSequence->width, videoSequence->height);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_WIDTH, videoSequence->width);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_HEIGHT, videoSequence->height);
        OH_AVFormat_SetDoubleValue(format, OH_MD_KEY_FRAME_RATE, videoSequence->frameRate);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_NV12);
        int ret = OH_VideoDecoder_Configure(videoCodec, format);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_Configure failed!");
            OH_AVFormat_Destroy(format);
            format = nullptr;
            return;
        }
        OH_AVFormat_Destroy(format);
        format = nullptr;

        codecUserData = new CodecUserData();
        ret = OH_VideoDecoder_RegisterCallback(
            videoCodec, {OnError, OnStreamChanged, OnNeedInputBuffer, OnNewOutputBuffer}, codecUserData);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_RegisterCallback failed!");
            delete codecUserData;
            return;
        }
        ret = OH_VideoDecoder_Prepare(videoCodec);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_Prepare failed!");
            delete codecUserData;
            return;
        }
        ret = OH_VideoDecoder_Start(videoCodec);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_Start failed!");
            delete codecUserData;
            return;
        }
        size_t startFrame = 0; // keyFrame: 0, 23, 46
        size_t endFrame = 13 + 2;
        size_t currentIndex = 0;
        while (true) {
            std::unique_lock<std::mutex> lock(codecUserData->inputMutex);
            codecUserData->inputCondition.wait(lock, []() {
                LOGI("--------------- inputFunc wait, size:%d", codecUserData->inputBufferInfoQueue.size());
                return codecUserData->inputBufferInfoQueue.size() > 0;
            });

            CodecBufferInfo codecBufferInfo = codecUserData->inputBufferInfoQueue.front();
            lock.unlock();

            uint32_t index = codecBufferInfo.bufferIndex;
            auto buffer = codecBufferInfo.buffer;

            int64_t pts = 0;

            OH_AVCodecBufferAttr info;
            if (currentIndex < 2) {
                info.size = static_cast<int32_t>(headers[currentIndex]->length());
                info.offset = 0;
                info.pts = 0;
                info.flags = AVCODEC_BUFFER_FLAGS_NONE;
                memcpy(OH_AVBuffer_GetAddr(reinterpret_cast<OH_AVBuffer *>(buffer)), headers[currentIndex]->data(),
                       headers[currentIndex]->length());

            } else if (currentIndex + startFrame < (headers.size() + frames.size())) {
                info.size = static_cast<int32_t>(frames[currentIndex - 2 + startFrame]->fileBytes->length());
                info.offset = 0;
                pts = static_cast<int64_t>(static_cast<float>(frames[currentIndex - 2 + startFrame]->frame) * 1000000 /
                                           videoSequence->frameRate);
                info.pts = pts;
                info.flags = AVCODEC_BUFFER_FLAGS_NONE;
                memcpy(OH_AVBuffer_GetAddr(reinterpret_cast<OH_AVBuffer *>(buffer)),
                       frames[currentIndex - 2 + startFrame]->fileBytes->data(),
                       frames[currentIndex - 2 + startFrame]->fileBytes->length());
                std::unique_lock<std::mutex> listLock(listMutex);
                pendingFrames.push_back(info.pts);
                listLock.unlock();
            } else {
                info.size = 0;
                info.offset = 0;
                info.pts = 0;
                info.flags = AVCODEC_BUFFER_FLAGS_EOS;
                std::unique_lock<std::mutex> listLock(listMutex);
                pendingFrames.push_back(info.pts);
                listLock.unlock();
            }
            ret = OH_AVBuffer_SetBufferAttr(reinterpret_cast<OH_AVBuffer *>(buffer), &info);
            if (ret != AV_ERR_OK) {
                LOGE("OH_AVBuffer_SetBufferAttr failed!");
                break;
            }
            LOGI("---------------OH_VideoDecoder_PushInputBuffer--index:%d, length:%d, currentIndex:%d, pts:%ld", index,
                 info.size, currentIndex, pts);
            ret = OH_VideoDecoder_PushInputBuffer(videoCodec, index);
            if (ret != AV_ERR_OK) {
                LOGE("OH_VideoDecoder_PushInputBuffer failed!");
                break;
            }
            if (currentIndex >= (headers.size() + frames.size())) {
                break;
            }
            lock.lock();
            codecUserData->inputBufferInfoQueue.pop();
            lock.unlock();

            std::unique_lock<std::mutex> outLock(codecUserData->outputMutex);
            codecUserData->outputCondition.wait(outLock, [] {
                LOGI("--------------- OutputFunc wait, size:%d", codecUserData->outputBufferInfoQueue.size());
                return codecUserData->outputBufferInfoQueue.size() > 0 || pendingFrames.size() <= 3;
            });
            if (codecUserData->outputBufferInfoQueue.size() > 0) {
                uint32_t indexOut = codecUserData->outputBufferInfoQueue.front().bufferIndex;
                CodecBufferInfo bufferInfo = codecUserData->outputBufferInfoQueue.front();
                outLock.unlock();
                OH_AVCodecBufferAttr attr = {0, 0, 0, AVCODEC_BUFFER_FLAGS_NONE};
                int ret = OH_AVBuffer_GetBufferAttr(bufferInfo.buffer, &attr);
                ret = OH_VideoDecoder_FreeOutputBuffer(videoCodec, indexOut);
                if (ret != AV_ERR_OK) {
                    LOGE("OH_VideoDecoder_FreeOutputBuffer failed!, ret:%d", ret);
                    break;
                } else {
                    std::unique_lock<std::mutex> listLock(listMutex);
                    pendingFrames.remove(attr.pts);
                    listLock.unlock();
                    LOGI("---------------OH_VideoDecoder_FreeOutputBuffer---success---currentIndex:%d， timestamp:%d, size:%d",
                         currentIndex++, attr.pts, attr.size);
                }
                outLock.lock();
                codecUserData->outputBufferInfoQueue.pop();
                outLock.unlock();
                if (bufferInfo.attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
                    LOGI("---------------OH_VideoDecoder_FreeOutputBuffer---end-");
                    break;
                }
            }
            currentIndex++;
        }

        OH_VideoDecoder_Destroy(videoCodec);
        delete codecUserData;
    }
}


static napi_value OnUpdateDensity(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    double value;
    napi_get_value_double(env, args[0], &value);
    screenDensity = static_cast<float>(value);
    return nullptr;
}

static napi_value AddImageFromEncoded(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    size_t strSize;
    char srcBuf[2048];
    napi_get_value_string_utf8(env, args[0], srcBuf, sizeof(srcBuf), &strSize);
    std::string name = srcBuf;
    size_t length;
    void *data;
    auto code = napi_get_arraybuffer_info(env, args[1], &data, &length);
    if (code != napi_ok) {
        return nullptr;
    }
    auto tgfxData = tgfx::Data::MakeWithCopy(data, length);
    if (appHost == nullptr) {
        appHost = CreateAppHost();
    }
    appHost->addImage(name, tgfx::Image::MakeFromEncoded(tgfxData));
    return nullptr;
}

static void Draw(int index) {
    //   if (window == nullptr || appHost == nullptr || appHost->width() <= 0 || appHost->height() <= 0) {
    //     return;
    //   }
    //   auto device = window->getDevice();
    //   auto context = device->lockContext();
    //   if (context == nullptr) {
    //     printf("Fail to lock context from the Device.\n");
    //     return;
    //   }
    //   auto surface = window->getSurface(context);
    //   if (surface == nullptr) {
    //     device->unlock();
    //     return;
    //   }
    //   auto canvas = surface->getCanvas();
    //   canvas->clear();
    //   canvas->save();
    //   auto numDrawers = drawers::Drawer::Count() - 1;
    //   index = (index % numDrawers) + 1;
    //   auto drawer = drawers::Drawer::GetByName("GridBackground");
    //   drawer->draw(canvas, appHost.get());
    //   drawer = drawers::Drawer::GetByIndex(index);
    //   drawer->draw(canvas, appHost.get());
    //   canvas->restore();
    //   surface->flush();
    //   context->submit();
    //   window->present(context);
    //   device->unlock();
    if (index) {
    }

    auto device = window->getDevice();
    auto context = device->lockContext();
    if (context == nullptr) {
        printf("Fail to lock context from the Device.\n");
        return;
    }
    auto surface = window->getSurface(context);
    if (surface == nullptr) {
        device->unlock();
        return;
    }
    auto canvas = surface->getCanvas();
    canvas->clear();
    canvas->save();
    auto numDrawers = drawers::Drawer::Count() - 1;
    index = (index % numDrawers) + 1;
    auto drawer = drawers::Drawer::GetByName("GridBackground");
    drawer->draw(canvas, appHost.get());
    drawer = drawers::Drawer::GetByIndex(index);
    drawer->draw(canvas, appHost.get());
    canvas->restore();
    surface->flush();
    context->submit();
    window->present(context);
    device->unlock();
}

static napi_value OnDraw(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    double value;
    napi_get_value_double(env, args[0], &value);
    Draw(static_cast<int>(value));
    return nullptr;
}

static std::shared_ptr<drawers::AppHost> CreateAppHost() {
    auto appHost = std::make_shared<drawers::AppHost>();
    static const std::string FallbackFontFileNames[] = {"/system/fonts/HarmonyOS_Sans.ttf",
                                                        "/system/fonts/HarmonyOS_Sans_SC.ttf",
                                                        "/system/fonts/HarmonyOS_Sans_TC.ttf"};
    for (auto &fileName : FallbackFontFileNames) {
        auto typeface = tgfx::Typeface::MakeFromPath(fileName);
        if (typeface != nullptr) {
            appHost->addTypeface("default", std::move(typeface));
            break;
        }
    }
    auto typeface = tgfx::Typeface::MakeFromPath("/system/fonts/HMOSColorEmojiCompat.ttf");
    if (typeface != nullptr) {
        appHost->addTypeface("emoji", std::move(typeface));
    }
    return appHost;
}

static void UpdateSize(OH_NativeXComponent *component, void *nativeWindow) {
    uint64_t width;
    uint64_t height;
    int32_t ret = OH_NativeXComponent_GetXComponentSize(component, nativeWindow, &width, &height);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }
    if (appHost == nullptr) {
        appHost = CreateAppHost();
    }
    appHost->updateScreen(static_cast<int>(width), static_cast<int>(height), screenDensity);
    if (window != nullptr) {
        window->invalidSize();
    }
}

static void OnSurfaceChangedCB(OH_NativeXComponent *component, void *nativeWindow) {
    UpdateSize(component, nativeWindow);
    if (component && nativeWindow) {
    }
    pagSurface->updateSize();
}

static void OnSurfaceDestroyedCB(OH_NativeXComponent *, void *) {
    window = nullptr;
    delete pagPlayer;
}

static void DispatchTouchEventCB(OH_NativeXComponent *, void *) {
    int test = 0;
    if (test == 0) {
    }
}

static void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *nativeWindow) {
    UpdateSize(component, nativeWindow);
    //   window = tgfx::EGLWindow::MakeFrom(reinterpret_cast<EGLNativeWindowType>(nativeWindow));
    //   if (window == nullptr) {
    //     printf("OnSurfaceCreatedCB() Invalid surface specified.\n");
    //     return;
    //   }
    //   Draw(0);
    if (component && nativeWindow) {
    }
    bool videoTest = false;
    if (videoTest) {
        if (1) {
            pagVideoSequenceDecodeTest_1();
        } else {
            pagVideoSequenceDecodeTest();
            VideoDecodeTest();
        }
        return;
    }

    std::string filePath = "/data/storage/el1/bundle/hello2d/resources/resfile/data_video.pag";
    auto pagFile = pag::PAGFile::Load(filePath);
    if (pagFile == nullptr) {
        return;
    }

    auto drawable = pag::GPUDrawable::FromWindow(reinterpret_cast<NativeWindow *>(nativeWindow));
    if (drawable == nullptr) {
        return;
    }
    pagSurface = pag::PAGSurface::MakeFrom(std::static_pointer_cast<pag::Drawable>(drawable));
    if (pagSurface == nullptr) {
        return;
    }
    pagPlayer = new pag::PAGPlayer();
    pagPlayer->setComposition(pagFile);
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setProgress(0.3);
    bool status = pagPlayer->flush();
    if (status) {
    }
    // Draw(0);
}

static void RegisterCallback(napi_env env, napi_value exports) {
    napi_status status;
    napi_value exportInstance = nullptr;
    OH_NativeXComponent *nativeXComponent = nullptr;
    status = napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status != napi_ok) {
        return;
    }
    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        return;
    }
    static OH_NativeXComponent_Callback callback;
    callback.OnSurfaceCreated = OnSurfaceCreatedCB;
    callback.OnSurfaceChanged = OnSurfaceChangedCB;
    callback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    callback.DispatchTouchEvent = DispatchTouchEventCB;
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &callback);
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        {"draw", nullptr, OnDraw, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"updateDensity", nullptr, OnUpdateDensity, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"addImageFromEncoded", nullptr, AddImageFromEncoded, nullptr, nullptr, nullptr, napi_default, nullptr}};
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    RegisterCallback(env, exports);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {1, 0, nullptr, Init, "hello2d", ((void *)0), {0}};

extern "C" __attribute__((constructor)) void RegisterHello2dModule(void) { napi_module_register(&demoModule); }