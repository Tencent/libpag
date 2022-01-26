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

#include <thread>
#include "HitTestCase.h"
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
PAG_TEST_CASE(MultiThreadCase)

/**
 * swiftshader在多线程同时创建 EGLContextExt 时候会失败，需加锁
 */
void mockPAGView() {
  auto file = PAGFile::Load(DEFAULT_PAG_PATH);
  ASSERT_NE(file, nullptr) << "pag path is:" << DEFAULT_PAG_PATH << std::endl;
  auto surface = PAGSurface::MakeOffscreen(file->width(), file->height());
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  surface->drawable->getDevice();
  ASSERT_NE(surface, nullptr);
  player->setComposition(file);
  int num = 20;
  for (int i = 0; i < num; i++) {
    long time = file->duration() * i / num;
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

void mockAsyncFlush(int num = 30) {
  ASSERT_NE(PAGCpuTest::TestPAGSurface, nullptr);
  for (int i = 0; i < num; i++) {
    PAGCpuTest::TestPAGFile->setCurrentTime(PAGCpuTest::TestPAGFile->duration() * i / num);
    PAGCpuTest::TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  std::cout << "\nmockAsyncFlush finish" << std::endl;
}

/**
 * 用例描述: 测试在A线程调用flush，B线程同时进行编辑操作
 */
PAG_TEST_F(MultiThreadCase, AsyncFlush) {
  auto mockThread = std::thread(mockAsyncFlush, 10);
  std::shared_ptr<PAGComposition> pagCom =
      std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  ASSERT_NE(pagCom, nullptr);

  for (int i = 0; i < 10; i++) {
    pagCom->swapLayerAt(2, 3);
    PAGCpuTest::TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto layer = pagCom->getLayerAt(2);
    PAGCpuTest::TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pagCom->removeLayerAt(2);
    PAGCpuTest::TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pagCom->addLayerAt(layer, 2);
    PAGCpuTest::TestPAGPlayer->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  std::cout << "\nAsyncFlush edit" << std::endl;
  mockThread.join();
}

/**
 * 用例描述: 测试在A线程调用flush，B线程进行freeCache
 */
PAG_TEST_F(MultiThreadCase, AsyncFlushAndFreeCache) {
  ASSERT_NE(TestPAGFile, nullptr);
  auto mockThread = std::thread(mockAsyncFlush, 200);
  for (int i = 0; i < 200; i++) {
    PAGCpuTest::TestPAGSurface->freeCache();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  std::cout << "\nAsyncFlush edit" << std::endl;
  mockThread.join();
}

/**
 * 用例描述: 多线程HitTest相关，因为flush线程会不停刷新位置，所以另外一个线程来测试hit的准确性没有意义，此处更多是看
 * 多线程会不会引起死锁之类的问题，正确性是HitTest接口本身来保证
 */
PAG_TEST_F(MultiThreadCase, HitTestPoint) {
  auto mockThread = std::thread(mockAsyncFlush, 30);
  for (int i = 0; i < 15; i++) {
    TestPAGPlayer->hitTestPoint(TestPAGFile, 720.0 * i / 15, 1080.0 * i / 15, true);
  }
  mockThread.join();
}

/**
 * 用例描述: GetLayersUnderPoint功能测试
 */
PAG_TEST_F(MultiThreadCase, GetLayersUnderPoint) {
  auto mockThread = std::thread(mockAsyncFlush, 30);
  for (int i = 0; i < 30; i++) {
    TestPAGFile->getLayersUnderPoint(720.0 * i / 15, 1080.0 * i / 15);
  }
  mockThread.join();
}

PAG_TEST_CASE(MultiThreadCase_BitmapSequenceHitTest)

/**
 * 用例描述: 图片序列帧的hitTest"
 */
PAG_TEST_F(MultiThreadCase_BitmapSequenceHitTest, BitmapSequenceHitTestPoint) {
  auto mockThread = std::thread(mockAsyncFlush, 30);
  for (int i = 0; i < 30; i++) {
    TestPAGPlayer->hitTestPoint(TestPAGFile, 720.0 * i / 15, 1080.0 * i / 15, true);
  }
  mockThread.join();
}

PAG_TEST_CASE(MultiThreadCase_VideoSequenceHitTest)

/**
 * 用例描述: 视频序列帧的hitTest
 */
PAG_TEST_F(MultiThreadCase_VideoSequenceHitTest, VideoSequenceHitTestPoint) {
  auto mockThread = std::thread(mockAsyncFlush, 30);
  for (int i = 0; i < 30; i++) {
    TestPAGPlayer->hitTestPoint(TestPAGFile, 720.0 * i / 15, 1080.0 * i / 15, true);
  }
  mockThread.join();
}
}  // namespace pag
