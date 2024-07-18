#include "napi/native_api.h"

#include "pag/pag.h"
#include "pag/file.h"

#include <cstdint>
#include <multimedia/player_framework/native_avcodec_videodecoder.h>
#include <multimedia/player_framework/native_avcapability.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include <multimedia/player_framework/native_avformat.h>
#include <multimedia/player_framework/native_avbuffer.h>
#include <native_buffer/native_buffer.h>
#include <queue>
#include <thread>
#include <fstream>


#include "hilog/log.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200  // 全局domain宏，标识业务领域
#define LOG_TAG "MY_TAG"   // 全局tag宏，标识模块日志tag

#define SAMPLE_LOG(func, fmt, args...)                 \
    do {                                                       \
        (void)func(LOG_APP, "{%{public}s():%{public}d} " fmt, __FUNCTION__, __LINE__, ##args);   \
    } while (0)

#define LOGF(fmt, ...) SAMPLE_LOG(OH_LOG_FATAL, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) SAMPLE_LOG(OH_LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) SAMPLE_LOG(OH_LOG_WARN,  fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) SAMPLE_LOG(OH_LOG_INFO,  fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) SAMPLE_LOG(OH_LOG_DEBUG, fmt, ##__VA_ARGS__)

using namespace pag;
static OH_AVCodec *avCodec = nullptr;
static bool bStarted = false;

struct CodecBufferInfo {
    uint32_t bufferIndex = 0;
    uintptr_t *buffer = nullptr;
    uint8_t *bufferAddr = nullptr;
    OH_AVCodecBufferAttr attr = {0, 0, 0, AVCODEC_BUFFER_FLAGS_NONE};

    CodecBufferInfo(uint8_t *addr) : bufferAddr(addr){};
    CodecBufferInfo(uint8_t *addr, int32_t bufferSize)
        : bufferAddr(addr), attr({0, bufferSize, 0, AVCODEC_BUFFER_FLAGS_NONE}){};
    CodecBufferInfo(uint32_t argBufferIndex, OH_AVMemory *argBuffer, OH_AVCodecBufferAttr argAttr)
        : bufferIndex(argBufferIndex), buffer(reinterpret_cast<uintptr_t *>(argBuffer)), attr(argAttr){};
    CodecBufferInfo(uint32_t argBufferIndex, OH_AVMemory *argBuffer)
        : bufferIndex(argBufferIndex), buffer(reinterpret_cast<uintptr_t *>(argBuffer)){};
    CodecBufferInfo(uint32_t argBufferIndex, OH_AVBuffer *argBuffer)
        : bufferIndex(argBufferIndex), buffer(reinterpret_cast<uintptr_t *>(argBuffer)) {
        OH_AVBuffer_GetBufferAttr(argBuffer, &attr);
    };
};

class CodecUserData {
public:

    uint32_t inputFrameCount_ = 0;
    std::mutex inputMutex_;
    std::condition_variable inputCond_;
    std::queue<CodecBufferInfo> inputBufferInfoQueue_;

    uint32_t outputFrameCount_ = 0;
    std::mutex outputMutex_;
    std::condition_variable outputCond_;
    std::queue<CodecBufferInfo> outputBufferInfoQueue_;

    void ClearQueue() {
        {
            std::unique_lock<std::mutex> lock(inputMutex_);
            auto emptyQueue = std::queue<CodecBufferInfo>();
            inputBufferInfoQueue_.swap(emptyQueue);
        }
        {
            std::unique_lock<std::mutex> lock(outputMutex_);
            auto emptyQueue = std::queue<CodecBufferInfo>();
            outputBufferInfoQueue_.swap(emptyQueue);
        }
    }
};

static CodecUserData* codecUserData = nullptr;

// 解码异常回调OH_AVCodecOnError实现
static void OnError(OH_AVCodec *codec, int32_t errorCode, void *userData) {
    (void)codec;
    (void)errorCode;
    (void)userData;
}

// 解码数据流变化回调OH_AVCodecOnStreamChanged实现
static void OnStreamChanged(OH_AVCodec *codec, OH_AVFormat *format, void *userData) {
    (void)codec;
    (void)format;
    (void)userData;
}

static void OnNeedInputBuffer(OH_AVCodec *codec, uint32_t index, OH_AVBuffer *buffer, void *userData) {
    if (userData == nullptr) {
        return;
    }
    (void)codec;
    LOGI("---------------OnNeedInputBuffer--index:%{public}d", index);
    CodecUserData *codecUserData = static_cast<CodecUserData *>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->inputMutex_);
    codecUserData->inputBufferInfoQueue_.emplace(index, buffer);
    codecUserData->inputCond_.notify_all();
}

// 解码输出回调OH_AVCodecOnNewOutputBuffer实现
static void OnNewOutputBuffer(OH_AVCodec *codec, uint32_t index, OH_AVBuffer *buffer, void *userData) {
    if (userData == nullptr) {
        return;
    }
    (void)codec;
    LOGI("---------------OnNewOutputBuffer--index:%{public}d", index);
    CodecUserData *codecUserData = static_cast<CodecUserData *>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->outputMutex_);
    codecUserData->outputBufferInfoQueue_.emplace(index, buffer);
    codecUserData->outputCond_.notify_all();
}

static void OutputFunc() {
   LOGI("--------------- OutputFunc ---------------------");
   std::string outPath = "/storage/media/100/local/files/Docs/Documents/test.yuv";
   auto file = fopen(outPath.c_str(), "wb");
   size_t currentIndex = 0;
   while (true) {
       std::unique_lock<std::mutex> lock(codecUserData->outputMutex_);
       codecUserData->outputCond_.wait(lock, [] {
          return codecUserData->outputBufferInfoQueue_.size() > 0 || !bStarted;
       });
       if (!bStarted) {
           break;
       }
       uint32_t index = codecUserData->outputBufferInfoQueue_.front().bufferIndex;
       CodecBufferInfo bufferInfo = codecUserData->outputBufferInfoQueue_.front();
       lock.unlock();
       int ret = OH_VideoDecoder_FreeOutputBuffer(avCodec, index);
       if (ret != AV_ERR_OK) {
              OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_FreeOutputBuffer failed!, ret:%{public}d", ret);
              break;
       } else {
             LOGI("---------------OH_VideoDecoder_FreeOutputBuffer---success---currentIndex:%{public}d， timestamp:%{public}d, size:%{public}d", currentIndex ++, bufferInfo.attr.pts, bufferInfo.attr.size);
             if (bufferInfo.attr.size > 0) {
                fwrite(OH_AVBuffer_GetAddr(reinterpret_cast<OH_AVBuffer *>(bufferInfo.buffer)), 1, bufferInfo.attr.size, file);
             } else {
                fclose(file);
                break;
             }
       }
       lock.lock();
       codecUserData->outputBufferInfoQueue_.pop();
       lock.unlock();
   }
}

static void pagVideoSequenceDecodeTest() {
    std::string filePath = "/data/storage/el1/bundle/entry/resources/resfile/particle_video.pag";
    auto file = pag::File::Load(filePath);
    if (file && file->compositions.size() > 0) {
        VideoComposition *videoComposition = (VideoComposition*)(file->compositions[0]);
        VideoSequence *videoSequence = videoComposition->sequences[0];
        auto headers = videoSequence->headers;
        auto frames = videoSequence->frames;
        
        avCodec = OH_VideoDecoder_CreateByMime("video/avc");
        if (avCodec == nullptr) {
            OH_LOG_ERROR(LOG_APP,"create hardware decoder failed!");
            return;
        }
        
//         OH_AVCapability *capability = OH_AVCodec_GetCapabilityByCategory(OH_AVCODEC_MIMETYPE_VIDEO_AVC, false, SOFTWARE);
//         const char *name = OH_AVCapability_GetName(capability);
//         avCodec = OH_VideoDecoder_CreateByName(name);

        OH_AVFormat *format = OH_AVFormat_Create();
        LOGI("---------------videoSequence->width:%{public}d, height:%{public}d", videoSequence->width, videoSequence->height);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_WIDTH, videoSequence->width);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_HEIGHT, videoSequence->height);
        OH_AVFormat_SetDoubleValue(format, OH_MD_KEY_FRAME_RATE, videoSequence->frameRate);
        OH_AVFormat_SetIntValue(format, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_NV12);
        int ret = OH_VideoDecoder_Configure(avCodec, format);
        if (ret != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_Configure failed!");
            OH_AVFormat_Destroy(format);
            format = nullptr;
            return;
        }
        OH_AVFormat_Destroy(format);
        format = nullptr;

        codecUserData = new CodecUserData();
        ret = OH_VideoDecoder_RegisterCallback(avCodec, { OnError, OnStreamChanged, OnNeedInputBuffer, OnNewOutputBuffer},
                                         codecUserData);
        if (ret != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_RegisterCallback failed!");
            delete codecUserData;
            return;
        }
        ret = OH_VideoDecoder_Prepare(avCodec);
        if (ret != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_Prepare failed!");
            delete codecUserData;
            return;
        }
        ret = OH_VideoDecoder_Start(avCodec);
        if (ret != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_Start failed!");
            delete codecUserData;
            return;
        }
        bStarted = true;
        std::thread work(OutputFunc);
        size_t currentIndex = 0;
        while(true) {
           std::unique_lock<std::mutex> lock(codecUserData->inputMutex_);
           codecUserData->inputCond_.wait(lock, []() {
              return codecUserData->inputBufferInfoQueue_.size() > 0 || !bStarted;
           });
           if (!bStarted) {
               break;
           }
           
           CodecBufferInfo codecBufferInfo = codecUserData->inputBufferInfoQueue_.front();
           int index = codecBufferInfo.bufferIndex;
           auto buffer = codecBufferInfo.buffer;
           lock.unlock();

           OH_AVCodecBufferAttr info;
           if (currentIndex < 2) {
              info.size = headers[currentIndex]->length();
              info.offset = 0;
              info.pts = 0;
              info.flags = AVCODEC_BUFFER_FLAGS_NONE;
              memcpy( OH_AVBuffer_GetAddr(reinterpret_cast<OH_AVBuffer *>(buffer)), headers[currentIndex]->data(), 
                                            headers[currentIndex]->length());

          } else if (currentIndex < (headers.size() + frames.size())) {
              info.size = frames[currentIndex - 2]->fileBytes->length();
              info.offset = 0;
              info.pts = frames[currentIndex - 2]->frame * 1000000 / videoSequence->frameRate;
              info.flags = frames[currentIndex - 2]->isKeyframe ? AVCODEC_BUFFER_FLAGS_SYNC_FRAME : AVCODEC_BUFFER_FLAGS_NONE;
              memcpy( OH_AVBuffer_GetAddr(reinterpret_cast<OH_AVBuffer *>(buffer)), frames[currentIndex - 2]->fileBytes->data(), 
                                            frames[currentIndex - 2]->fileBytes->length());
          } else {
              info.size = 0;
              info.offset = 0;
              info.pts = 0;
              info.flags = AVCODEC_BUFFER_FLAGS_EOS;
          }
          ret = OH_AVBuffer_SetBufferAttr(reinterpret_cast<OH_AVBuffer *>(buffer), &info);
          if (ret != AV_ERR_OK) {
              OH_LOG_ERROR(LOG_APP,"OH_AVBuffer_SetBufferAttr failed!");
              break;
          }
          LOGI("---------------OH_VideoDecoder_PushInputBuffer--index:%{public}d, length:%{public}d, currentIndex:%{public}d", index, info.size, currentIndex);
          ret = OH_VideoDecoder_PushInputBuffer(avCodec, index);
          if (ret != AV_ERR_OK) {
              OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_PushInputBuffer failed!");
              break;
          }

          if (currentIndex >= (headers.size() + frames.size())) {
             break;
          } else {
             currentIndex ++;
          }
          lock.lock();
          codecUserData->inputBufferInfoQueue_.pop();
          lock.unlock();
        }

        work.join();
        bStarted = false;
        ret = OH_VideoDecoder_Destroy(avCodec);   
        LOGI("---------------OH_VideoDecoder_Destroy--ret:%{public}d", ret);
        delete codecUserData;
    }
}

static void PAGDrawTest()
{
    std::string filePath = "/data/storage/el1/bundle/entry/resources/resfile/particle_video.pag";
    auto pagFile = PAGFile::Load(filePath);
    if (pagFile == nullptr) {
        return;
    }
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    if (pagSurface == nullptr) {
        return;
    }
    auto pagPlayer = new PAGPlayer();
    pagPlayer->setComposition(pagFile);
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setProgress(0);
    auto status = pagPlayer->flush();
    if (status) {
        printf("-------- pag flush success !!!");
    } else {
        printf("-------- pag flush failed !!!");
    }
    
    delete pagPlayer;
}


static napi_value Add(napi_env env, napi_callback_info info)
{
    pagVideoSequenceDecodeTest();
    size_t argc = 2;
    napi_value args[2] = {nullptr};

    napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);
    
    auto version = pag::PAG::SDKVersion();
//     LOGE("--------test--------!");
//     OH_LOG_ERROR(LOG_APP,"OH_AVBuffer_SetBufferAttr failed!");
//     PAGDrawTest();

    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);

    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);

    double value0;
    napi_get_value_double(env, args[0], &value0);

    double value1;
    napi_get_value_double(env, args[1], &value1);

    napi_value sum;
    napi_create_double(env, value0 + value1, &sum);

    return sum;

}


EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "add", nullptr, Add, nullptr, nullptr, nullptr, napi_default, nullptr }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
