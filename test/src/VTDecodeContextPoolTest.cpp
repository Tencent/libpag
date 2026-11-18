/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#if defined(__APPLE__)

#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
#include "gtest/gtest.h"
#include "platform/cocoa/private/HardwareDecodeContext.h"

namespace pag {
namespace {
// Shared state between producer threads (standing in for decode threads of several decoders) and
// the single delivery thread (standing in for VideoToolbox emitting a bounded late callback).
struct GraveyardStressState {
  std::mutex queueMutex = {};
  std::vector<VTDecodeOutputContext*> pendingDelivery = {};
  std::atomic<bool> producersDone = {false};
  std::atomic<size_t> peakGraveyard = {0};
};

// Plays VideoToolbox: drains the queue and delivers each parked context's (empty) buffer, mimicking
// a late-but-bounded callback. Exits only after observing, under the same lock, an empty queue and
// producersDone, so no parked context is left undelivered.
void RunDeliveryThread(GraveyardStressState* state) {
  while (true) {
    std::vector<VTDecodeOutputContext*> batch;
    bool done = false;
    {
      std::lock_guard<std::mutex> lock(state->queueMutex);
      batch.swap(state->pendingDelivery);
      done = state->producersDone.load(std::memory_order_acquire);
    }
    for (auto* context : batch) {
      context->deliverBuffer(nullptr);
    }
    if (batch.empty()) {
      if (done) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(20));
    }
  }
}

void RecordPeak(GraveyardStressState* state, size_t value) {
  size_t previous = state->peakGraveyard.load(std::memory_order_relaxed);
  while (value > previous &&
         !state->peakGraveyard.compare_exchange_weak(previous, value, std::memory_order_relaxed)) {
  }
}

// One decode thread: runs many short-lived pools through synchronous, late, and failed decodes,
// reclaiming after each cycle (which also sweeps the shared graveyard) and draining on pool
// destruction (which parks still-pending contexts in the shared graveyard).
void RunProducerThread(GraveyardStressState* state, unsigned seed) {
  std::mt19937 rng(seed);
  for (int poolIndex = 0; poolIndex < 150; ++poolIndex) {
    VTDecodeContextPool pool;
    for (int cycle = 0; cycle < 40; ++cycle) {
      auto* context = pool.create();
      switch (rng() % 3) {
        case 0:
          // Synchronous callback: delivered before the decode thread consumes it.
          context->deliverBuffer(nullptr);
          context->takeBuffer();
          break;
        case 1:
          // Late callback: consumed now, delivered later by the delivery thread.
          context->takeBuffer();
          {
            std::lock_guard<std::mutex> lock(state->queueMutex);
            state->pendingDelivery.push_back(context);
          }
          break;
        default:
          // Failed decode: VideoToolbox will not call back, so the context is abandoned.
          context->takeBuffer();
          context->abandon();
          break;
      }
      pool.reclaimResolved();
      RecordPeak(state, VTDecodeGraveyardSize());
    }
  }
}

// Sweeps the shared graveyard, the only public trigger being a pool reclaim.
void SweepGraveyard() {
  VTDecodeContextPool pool;
  pool.reclaimResolved();
}
}  // namespace

/**
 * Hammers the process-wide graveyard from several concurrent decoders while one thread delivers the
 * late callbacks, then asserts it drains back to its baseline once every bounded callback has
 * landed. A use-after-free or double-free of a context under the concurrent deliver-vs-reclaim race
 * would crash this test; a leak would leave the graveyard above baseline. It verifies the premise
 * that the graveyard only ever parks pending bounded-delay frames and self-drains without unbounded
 * accumulation.
 */
TEST(VTDecodeContextPoolTest, GraveyardSelfDrainsUnderStress) {
  SweepGraveyard();
  const size_t baseline = VTDecodeGraveyardSize();

  GraveyardStressState state;
  const unsigned producerCount = 4;

  std::thread deliveryThread(RunDeliveryThread, &state);
  std::vector<std::thread> producers;
  for (unsigned index = 0; index < producerCount; ++index) {
    producers.emplace_back(RunProducerThread, &state, 20261117u + index);
  }
  for (auto& producer : producers) {
    producer.join();
  }
  state.producersDone.store(true, std::memory_order_release);
  deliveryThread.join();

  // Every late callback has now landed; a final sweep must return the graveyard to its baseline.
  SweepGraveyard();
  EXPECT_EQ(VTDecodeGraveyardSize(), baseline);

  // At any instant a producer can park at most one pool's worth (40) of still-pending late
  // contexts before the delivery thread sweeps them, so across producerCount producers the peak
  // stays well under producerCount * 40; a graveyard that stopped reclaiming would blow past it.
  EXPECT_LT(state.peakGraveyard.load(), baseline + static_cast<size_t>(producerCount) * 40);
}
}  // namespace pag

#endif  // defined(__APPLE__)
