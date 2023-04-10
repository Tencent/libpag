/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "tgfx/gpu/opengl/eagl/EAGLDevice.h"
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES3/glext.h>
#include "EAGLProcGetter.h"

namespace tgfx {
static std::mutex deviceLocker = {};
static std::vector<EAGLDevice*> deviceList = {};
static std::vector<EAGLDevice*> delayPurgeList = {};
static std::atomic_bool appInBackground = {true};

void ApplicationWillResignActive() {
  // 此时需要 applicationInBackground 先为 true，确保回调过程没有没有产生新的 GL 操作。
  appInBackground = true;
  std::lock_guard<std::mutex> autoLock(deviceLocker);
  for (auto& device : deviceList) {
    device->finish();
  }
}

void ApplicationDidBecomeActive() {
  appInBackground = false;
  std::vector<EAGLDevice*> delayList = {};
  deviceLocker.lock();
  std::swap(delayList, delayPurgeList);
  deviceLocker.unlock();
  for (auto& device : delayList) {
    delete device;
  }
}

void* GLDevice::CurrentNativeHandle() {
  return [EAGLContext currentContext];
}

std::shared_ptr<GLDevice> GLDevice::Current() {
  if (appInBackground) {
    return nullptr;
  }
  return EAGLDevice::Wrap([EAGLContext currentContext], true);
}

std::shared_ptr<GLDevice> GLDevice::Make(void* sharedContext) {
  if (appInBackground) {
    return nullptr;
  }
  EAGLContext* eaglContext = nil;
  EAGLContext* eaglShareContext = reinterpret_cast<EAGLContext*>(sharedContext);
  if (eaglShareContext != nil) {
    eaglContext = [[EAGLContext alloc] initWithAPI:[eaglShareContext API]
                                        sharegroup:[eaglShareContext sharegroup]];
  } else {
    eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (eaglContext == nil) {
      eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    }
  }
  if (eaglContext == nil) {
    return nullptr;
  }
  auto device = EAGLDevice::Wrap(eaglContext, false);
  [eaglContext release];
  return device;
}

std::shared_ptr<EAGLDevice> EAGLDevice::MakeAdopted(EAGLContext* eaglContext) {
  if (appInBackground) {
    return nullptr;
  }
  return EAGLDevice::Wrap(eaglContext, true);
}

std::shared_ptr<EAGLDevice> EAGLDevice::Wrap(EAGLContext* eaglContext, bool isAdopted) {
  if (eaglContext == nil) {
    return nullptr;
  }
  auto glDevice = GLDevice::Get(eaglContext);
  if (glDevice) {
    return std::static_pointer_cast<EAGLDevice>(glDevice);
  }
  auto oldEAGLContext = [[EAGLContext currentContext] retain];
  if (oldEAGLContext != eaglContext) {
    auto result = [EAGLContext setCurrentContext:eaglContext];
    if (!result) {
      [oldEAGLContext release];
      return nullptr;
    }
  }
  auto device = std::shared_ptr<EAGLDevice>(new EAGLDevice(eaglContext),
                                            EAGLDevice::NotifyReferenceReachedZero);
  device->isAdopted = isAdopted;
  device->weakThis = device;
  if (oldEAGLContext != eaglContext) {
    [EAGLContext setCurrentContext:oldEAGLContext];
  }
  [oldEAGLContext release];
  return device;
}

void EAGLDevice::NotifyReferenceReachedZero(EAGLDevice* device) {
  if (!appInBackground) {
    delete device;
    return;
  }
  std::lock_guard<std::mutex> autoLock(deviceLocker);
  delayPurgeList.push_back(device);
}

EAGLDevice::EAGLDevice(EAGLContext* eaglContext)
    : GLDevice(eaglContext), _eaglContext(eaglContext) {
  [_eaglContext retain];
  std::lock_guard<std::mutex> autoLock(deviceLocker);
  auto index = deviceList.size();
  deviceList.push_back(this);
  cacheArrayIndex = index;
}

EAGLDevice::~EAGLDevice() {
  {
    std::lock_guard<std::mutex> autoLock(deviceLocker);
    auto tail = *(deviceList.end() - 1);
    auto index = cacheArrayIndex;
    deviceList[index] = tail;
    tail->cacheArrayIndex = index;
    deviceList.pop_back();
  }
  releaseAll();
  if (textureCache != nil) {
    CFRelease(textureCache);
    textureCache = nil;
  }
  [_eaglContext release];
}

bool EAGLDevice::sharableWith(void* nativeContext) const {
  if (nativeContext == nullptr) {
    return false;
  }
  auto shareContext = static_cast<EAGLContext*>(nativeContext);
  return [_eaglContext sharegroup] == [shareContext sharegroup];
}

EAGLContext* EAGLDevice::eaglContext() const {
  return _eaglContext;
}

CVOpenGLESTextureCacheRef EAGLDevice::getTextureCache() {
  if (!textureCache) {
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attrs, kCVOpenGLESTextureCacheMaximumTextureAgeKey,
                         [NSNumber numberWithFloat:0]);
    CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, attrs, _eaglContext, NULL, &textureCache);
    CFRelease(attrs);
  }
  return textureCache;
}

void EAGLDevice::releaseTexture(CVOpenGLESTextureRef texture) {
  if (texture == nil || textureCache == nil) {
    return;
  }
  CFRelease(texture);
  CVOpenGLESTextureCacheFlush(textureCache, 0);
}

bool EAGLDevice::onMakeCurrent() {
  return makeCurrent();
}

void EAGLDevice::onClearCurrent() {
  clearCurrent();
}

bool EAGLDevice::makeCurrent(bool force) {
  if (!force && appInBackground) {
    return false;
  }
  oldContext = [[EAGLContext currentContext] retain];
  if (oldContext == _eaglContext) {
    return true;
  }
  if (![EAGLContext setCurrentContext:_eaglContext]) {
    [oldContext release];
    oldContext = nil;
    return false;
  }
  return true;
}

void EAGLDevice::clearCurrent() {
  if (oldContext == _eaglContext) {
    [oldContext release];
    oldContext = nil;
    return;
  }
  [EAGLContext setCurrentContext:nil];
  if (oldContext) {
    // 可能失败。
    [EAGLContext setCurrentContext:oldContext];
  }
  [oldContext release];
  oldContext = nil;
}

void EAGLDevice::finish() {
  std::lock_guard<std::mutex> autoLock(locker);
  if (makeCurrent(true)) {
    glFinish();
    clearCurrent();
  }
}
}  // namespace tgfx

@interface PAGAppMonitor : NSObject
@end

@implementation PAGAppMonitor
+ (void)applicationWillResignActive:(NSNotification*)notification {
  tgfx::ApplicationWillResignActive();
}

+ (void)applicationDidBecomeActive:(NSNotification*)notification {
  tgfx::ApplicationDidBecomeActive();
}
@end

static bool RegisterNotifications() {
  [[NSNotificationCenter defaultCenter] addObserver:[PAGAppMonitor class]
                                           selector:@selector(applicationWillResignActive:)
                                               name:UIApplicationWillResignActiveNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:[PAGAppMonitor class]
                                           selector:@selector(applicationDidBecomeActive:)
                                               name:UIApplicationDidBecomeActiveNotification
                                             object:nil];
  return true;
}

static bool registered = RegisterNotifications();
