/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <thread>
#include "base/utils/Log.h"
#include "base/utils/TimeUtil.h"
#include "utils/TestUtils.h"

namespace pag {
/**
 * swiftshader在多线程同时创建 EGLContextExt 时候会失败，需加锁
 */
void mockPAGView() {
  PAG_SETUP(surface, player, file);
  int num = 20;
  Frame totalFrames = TimeToFrame(file->duration(), file->frameRate());
  for (int i = 0; i < num; i++) {
    long time = file->duration() * i / totalFrames;
    file->setCurrentTime(time);
    player->flush();
    MakeSnapshot(surface);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

/**
 * 用例描述: 使用多个PAGPlayer模拟多个PAGView同时渲染
 */
PAG_TEST(SimpleMultiThreadCase, MultiPAGView) {
  std::vector<std::thread> threads;
  int num = 10;
  for (int i = 0; i < num; i++) {
    threads.emplace_back(std::thread(mockPAGView));
  }
  for (auto& mock : threads) {
    if (mock.joinable()) mock.join();
  }
}

static std::shared_ptr<PAGSurface> TestPAGSurface = nullptr;
static std::shared_ptr<PAGPlayer> TestPAGPlayer = nullptr;
static std::shared_ptr<PAGFile> TestPAGFile = nullptr;

void SetupPAG() {
  TestPAGFile = LoadPAGFile("resources/apitest/test.pag");
  ASSERT_TRUE(TestPAGFile != nullptr);
  TestPAGSurface = OffscreenSurface::Make(TestPAGFile->width(), TestPAGFile->height());
  ASSERT_TRUE(TestPAGSurface != nullptr);
  TestPAGPlayer = std::make_shared<PAGPlayer>();
  TestPAGPlayer->setSurface(TestPAGSurface);
  TestPAGPlayer->setComposition(TestPAGFile);
}

void TearDownPAG() {
  TestPAGFile = nullptr;
  TestPAGSurface = nullptr;
  TestPAGPlayer = nullptr;
}

void mockAsyncFlush(int num = 30) {
  Frame totalFrames = TimeToFrame(TestPAGFile->duration(), TestPAGFile->frameRate());
  for (int i = 0; i < num; i++) {
    TestPAGFile->setCurrentTime(TestPAGFile->duration() * i / totalFrames);
    TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LOGI("mockAsyncFlush finish");
}

/**
 * 用例描述: 测试在A线程调用flush，B线程同时进行编辑操作
 */
PAG_TEST(MultiThreadCase, AsyncFlush) {
  SetupPAG();
  auto mockThread = std::thread(mockAsyncFlush, 10);
  std::shared_ptr<PAGComposition> pagCom =
      std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  ASSERT_NE(pagCom, nullptr);

  for (int i = 0; i < 10; i++) {
    pagCom->swapLayerAt(2, 3);
    TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto layer = pagCom->getLayerAt(2);
    TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pagCom->removeLayerAt(2);
    TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pagCom->addLayerAt(layer, 2);
    TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LOGI("AsyncFlush edit");
  mockThread.join();
  TearDownPAG();
}

/**
 * 用例描述: 测试在A线程调用flush，B线程进行freeCache
 */
PAG_TEST(MultiThreadCase, AsyncFlushAndFreeCache) {
  SetupPAG();
  auto mockThread = std::thread(mockAsyncFlush, 200);
  for (int i = 0; i < 200; i++) {
    TestPAGSurface->freeCache();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  LOGI("AsyncFlush edit");
  mockThread.join();
  TearDownPAG();
}

/**
 * 用例描述:
 * 多线程HitTest相关，因为flush线程会不停刷新位置，所以另外一个线程来测试hit的准确性没有意义，此处更多是看
 * 多线程会不会引起死锁之类的问题，正确性是HitTest接口本身来保证
 */
PAG_TEST(MultiThreadCase, HitTestPoint) {
  SetupPAG();
  auto mockThread = std::thread(mockAsyncFlush, 30);
  for (int i = 0; i < 15; i++) {
    TestPAGPlayer->hitTestPoint(TestPAGFile, 720.0 * i / 15, 1080.0 * i / 15, true);
  }
  mockThread.join();
  TearDownPAG();
}

/**
 * 用例描述: GetLayersUnderPoint功能测试
 */
PAG_TEST(MultiThreadCase, GetLayersUnderPoint) {
  SetupPAG();
  auto mockThread = std::thread(mockAsyncFlush, 30);
  for (int i = 0; i < 30; i++) {
    TestPAGFile->getLayersUnderPoint(720.0 * i / 15, 1080.0 * i / 15);
  }
  mockThread.join();
  TearDownPAG();
}

/**
 * 用例描述: 图片序列帧的hitTest"
 */
PAG_TEST(MultiThreadCase_BitmapSequenceHitTest, BitmapSequenceHitTestPoint) {
  SetupPAG();
  auto mockThread = std::thread(mockAsyncFlush, 30);
  for (int i = 0; i < 30; i++) {
    TestPAGPlayer->hitTestPoint(TestPAGFile, 720.0 * i / 15, 1080.0 * i / 15, true);
  }
  mockThread.join();
  TearDownPAG();
}

/**
 * 用例描述: 视频序列帧的hitTest
 */
PAG_TEST(MultiThreadCase_VideoSequenceHitTest, VideoSequenceHitTestPoint) {
  SetupPAG();
  auto mockThread = std::thread(mockAsyncFlush, 30);
  for (int i = 0; i < 30; i++) {
    TestPAGPlayer->hitTestPoint(TestPAGFile, 720.0 * i / 15, 1080.0 * i / 15, true);
  }
  mockThread.join();
  TearDownPAG();
}
}  // namespace pag
