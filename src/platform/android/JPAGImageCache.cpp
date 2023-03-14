#include <android/bitmap.h>
#include <jni.h>
#include "JNIHelper.h"
#include "JPAGHandle.h"
#include "PAGImageCache.h"

namespace pag {
static jfieldID PAGImageCache_nativeHandle;
}

using namespace pag;

std::shared_ptr<PAGImageCache> get(JNIEnv* env, jobject thiz) {
  if (env == nullptr || thiz == nullptr) {
    return nullptr;
  }
  auto pagSurface = reinterpret_cast<JPAGHandle<PAGImageCache>*>(
      env->GetLongField(thiz, PAGImageCache_nativeHandle));
  if (pagSurface == nullptr) {
    return nullptr;
  }
  return pagSurface->get();
}

extern "C" {
PAG_API jboolean Java_org_libpag_PAGCacheManager_00024PAGImageCache_saveBitmap(JNIEnv* env,
                                                                               jobject thiz,
                                                                               jint frame,
                                                                               jobject bitmap) {
  auto cache = get(env, thiz);
  if (cache == nullptr) {
    return false;
  }
  unsigned char* newBitmapPixels;
  int ret;
  if ((ret = AndroidBitmap_lockPixels(env, bitmap, (void**)&newBitmapPixels)) < 0) {
    LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    return false;
  }
  auto res = cache->savePixels(frame, newBitmapPixels);
  if ((ret = AndroidBitmap_unlockPixels(env, bitmap)) != ANDROID_BITMAP_RESULT_SUCCESS) {
    LOGE("AndroidBitmap_unlockPixels() failed ! error=%d", ret);
  }
  return res;
}

PAG_API jboolean Java_org_libpag_PAGCacheManager_00024PAGImageCache_inflateBitmap(JNIEnv* env,
                                                                                  jobject thiz,
                                                                                  jint frame,
                                                                                  jobject bitmap) {
  auto cache = get(env, thiz);
  if (cache == nullptr) {
    return false;
  }

  unsigned char* newBitmapPixels;
  int ret;
  if ((ret = AndroidBitmap_lockPixels(env, bitmap, (void**)&newBitmapPixels)) < 0) {
    LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    return false;
  }
  auto res = cache->inflatePixels(frame, newBitmapPixels);
  if ((ret = AndroidBitmap_unlockPixels(env, bitmap)) != ANDROID_BITMAP_RESULT_SUCCESS) {
    LOGE("AndroidBitmap_unlockPixels() failed ! error=%d", ret);
  }
  return res;
}

PAG_API jboolean Java_org_libpag_PAGCacheManager_00024PAGImageCache_isCached(JNIEnv* env,
                                                                             jobject thiz,
                                                                             jint frame) {
  auto cache = get(env, thiz);
  if (cache == nullptr) {
    return false;
  }
  return cache->isCached(frame);
}
PAG_API jboolean Java_org_libpag_PAGCacheManager_00024PAGImageCache_isAllCached(JNIEnv* env,
                                                                                jobject thiz) {
  auto cache = get(env, thiz);
  if (cache == nullptr) {
    return false;
  }
  return cache->isAllCached();
}
PAG_API void Java_org_libpag_PAGCacheManager_00024PAGImageCache_release(JNIEnv* env, jobject thiz) {
  auto cache = get(env, thiz);
  if (cache == nullptr) {
    return;
  }
  cache->release();
}

PAG_API jlong Java_org_libpag_PAGCacheManager_00024PAGImageCache_SetupCache(JNIEnv* env, jclass,
                                                                            jstring path,
                                                                            jint width, jint height,
                                                                            jint frame_count,
                                                                            jboolean need_init) {
  auto cache = PAGImageCache::Make(pag::SafeConvertToStdString(env, path), width, height,
                                   frame_count, need_init);
  if (cache == nullptr) {
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGHandle<PAGImageCache>(cache));
}

PAG_API void Java_org_libpag_PAGCacheManager_00024PAGImageCache_nativeInit(JNIEnv* env,
                                                                           jclass clazz) {
  PAGImageCache_nativeHandle = env->GetFieldID(clazz, "nativeContext", "J");
}
}