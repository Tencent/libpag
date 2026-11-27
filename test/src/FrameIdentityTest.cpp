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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions and
//  limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <limits>
#include "platform/android/CodecTimeline.h"
#include "rendering/video/FrameRead.h"
#include "utils/TestUtils.h"

namespace pag {
namespace {
/** Provides controlled delivery status and acquisition targets without tgfx private APIs. */
class FrameIdentityBuffer : public tgfx::ImageBuffer {
 public:
  FrameIdentityBuffer(uint8_t identity, int64_t targetTimestamp,
                      tgfx::ImageBufferStatus status = tgfx::ImageBufferStatus::Ready)
      : targetTimestamp(targetTimestamp), pixels({identity, 0, 0, 255}), deliveryState(status) {
  }

  int width() const override {
    return 1;
  }

  int height() const override {
    return 1;
  }

  bool isAlphaOnly() const override {
    return false;
  }

  bool expired() const override {
    return invalidated || deliveryState == tgfx::ImageBufferStatus::Expired;
  }

  tgfx::ImageBufferStatus status() const override {
    return deliveryState;
  }

  const int64_t targetTimestamp;

  const std::shared_ptr<tgfx::ColorSpace>& colorSpace() const override {
    return space;
  }

  const std::array<uint8_t, 4>& rgba() const {
    return pixels;
  }

  void expire() {
    deliveryState = tgfx::ImageBufferStatus::Expired;
  }

  void invalidate() {
    invalidated = true;
  }

 protected:
  std::shared_ptr<tgfx::TextureView> onMakeTexture(tgfx::Context*, bool) const override {
    return nullptr;
  }

 private:
  const std::array<uint8_t, 4> pixels;
  tgfx::ImageBufferStatus deliveryState;
  bool invalidated = false;
  std::shared_ptr<tgfx::ColorSpace> space = nullptr;
};

static void BindFrameRead(FrameRead* frame, const std::shared_ptr<FrameIdentityBuffer>& buffer) {
  frame->buffer = buffer;
  frame->verifiedTargetTimestamp = buffer->targetTimestamp;
}

static void ExpectBufferIdentity(const std::shared_ptr<FrameIdentityBuffer>& buffer,
                                 uint8_t identity) {
  ASSERT_NE(buffer, nullptr);
  ASSERT_EQ(buffer->status(), tgfx::ImageBufferStatus::Ready);
  EXPECT_FALSE(buffer->expired());
  EXPECT_FALSE(buffer->isAlphaOnly());
  EXPECT_EQ(buffer->width(), 1);
  EXPECT_EQ(buffer->height(), 1);
  EXPECT_EQ(buffer->rgba()[0], identity);
  EXPECT_EQ(buffer->rgba()[3], 255);
}
}  // namespace

PAG_TEST(FrameIdentityTest, EndOfStreamWithImageMustBeDeliveredBeforeEnding) {
  EXPECT_TRUE(CodecTimeline::hasImage(false, 0));
  EXPECT_TRUE(CodecTimeline::hasImage(true, 128));
  EXPECT_FALSE(CodecTimeline::hasImage(true, 0));
}

PAG_TEST(FrameIdentityTest, CodecTimelinePreservesReorderedBusinessTimes) {
  CodecTimeline timeline(3000);
  ASSERT_TRUE(timeline.isValid());
  int64_t encoded = 0;
  int64_t decoded = 0;
  EXPECT_TRUE(timeline.encode(3000, &encoded));
  EXPECT_EQ(encoded, 3000);
  ASSERT_TRUE(timeline.restart());
  for (auto time : {2000, 0, 1000, 3000}) {
    ASSERT_TRUE(timeline.encode(time, &encoded));
    EXPECT_GT(encoded, 3000);
    ASSERT_TRUE(timeline.decode(encoded, &decoded));
    EXPECT_EQ(decoded, time);
  }
  EXPECT_FALSE(timeline.decode(3000, &decoded));
  EXPECT_FALSE(timeline.encode(-1, &encoded));
  EXPECT_FALSE(timeline.encode(3001, &encoded));
}

PAG_TEST(FrameIdentityTest, CodecTimelineRejectsOverflowAndFailedGeneration) {
  CodecTimeline timeline(std::numeric_limits<int64_t>::max() / 1000 - 1);
  ASSERT_TRUE(timeline.isValid());
  auto previousGeneration = timeline.generation->load();
  EXPECT_FALSE(timeline.restart());
  EXPECT_NE(timeline.generation->load(), previousGeneration);
  int64_t encoded = 0;
  EXPECT_FALSE(timeline.encode(0, &encoded));
  CodecTimeline invalid(std::numeric_limits<int64_t>::max());
  EXPECT_FALSE(invalid.isValid());
  CodecTimeline valid(1000);
  auto generation = valid.generation->load();
  valid.invalidate();
  EXPECT_FALSE(valid.isValid());
  EXPECT_NE(valid.generation->load(), generation);
  EXPECT_FALSE(valid.encode(0, &encoded));
}

PAG_TEST(FrameIdentityTest, FlushExpiresOldCredentialBeforeAnyNewOutput) {
  CodecTimeline timeline(1000);
  FrameRead request;
  request.sourceGeneration = timeline.generation;
  request.generation = 0;
  request.options->targetTimestamp = 0;
  auto buffer = std::make_shared<FrameIdentityBuffer>(1, 0);
  BindFrameRead(&request, buffer);
  ExpectBufferIdentity(buffer, 1);
  ASSERT_TRUE(request.matchesTarget());
  ASSERT_TRUE(timeline.restart());
  EXPECT_FALSE(request.isCurrent());
  EXPECT_FALSE(request.matchesTarget());
  request.options->targetTimestamp = -1;
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, LateSameBusinessTimestampCannotMatchNewGeneration) {
  CodecTimeline timeline(1000);
  int64_t current = 0;
  ASSERT_TRUE(timeline.restart());
  ASSERT_TRUE(timeline.encode(0, &current));
  FrameRead request;
  request.sourceGeneration = timeline.generation;
  request.generation = timeline.generation->load();
  request.options->targetTimestamp = current * 1000;
  auto previous = std::make_shared<FrameIdentityBuffer>(1, 0);
  BindFrameRead(&request, previous);
  ExpectBufferIdentity(previous, 1);
  EXPECT_EQ(request.buffer->status(), tgfx::ImageBufferStatus::Ready);
  EXPECT_FALSE(request.matchesTarget());
  auto buffer = std::make_shared<FrameIdentityBuffer>(2, current * 1000);
  BindFrameRead(&request, buffer);
  ExpectBufferIdentity(buffer, 2);
  EXPECT_TRUE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, CallerRejectsLatePreviousFrameAndAcceptsNextRead) {
  FrameRead request;
  request.options->targetTimestamp = 1000;
  auto previous = std::make_shared<FrameIdentityBuffer>(1, 0);
  BindFrameRead(&request, previous);
  ExpectBufferIdentity(previous, 1);
  EXPECT_EQ(request.buffer->status(), tgfx::ImageBufferStatus::Ready);
  EXPECT_FALSE(request.matchesTarget());
  previous->expire();
  auto buffer = std::make_shared<FrameIdentityBuffer>(2, 1000);
  BindFrameRead(&request, buffer);
  ExpectBufferIdentity(buffer, 2);
  EXPECT_TRUE(request.matchesTarget());
  EXPECT_TRUE(previous->expired());
}

PAG_TEST(FrameIdentityTest, CallerRejectsOvershotAndUnknownIdentity) {
  FrameRead request;
  request.options->targetTimestamp = 1000;
  auto overshot = std::make_shared<FrameIdentityBuffer>(2, 2000);
  BindFrameRead(&request, overshot);
  ExpectBufferIdentity(overshot, 2);
  EXPECT_EQ(request.buffer->status(), tgfx::ImageBufferStatus::Ready);
  EXPECT_FALSE(request.matchesTarget());
  auto unknown = std::make_shared<FrameIdentityBuffer>(3, -1);
  BindFrameRead(&request, unknown);
  ExpectBufferIdentity(unknown, 3);
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, VerifiedTargetDoesNotValidateAnExpiredBuffer) {
  FrameRead request;
  request.options->targetTimestamp = 0;
  auto previous = std::make_shared<FrameIdentityBuffer>(1, 0);
  BindFrameRead(&request, previous);
  ExpectBufferIdentity(previous, 1);
  EXPECT_TRUE(request.matchesTarget());
  previous->expire();
  auto next = std::make_shared<FrameIdentityBuffer>(2, 1000);
  ExpectBufferIdentity(next, 2);
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, PendingAndEmptyReadCannotMatchTarget) {
  FrameRead request;
  request.options->targetTimestamp = 0;
  EXPECT_FALSE(request.matchesTarget());
  auto buffer = std::make_shared<FrameIdentityBuffer>(0, 0, tgfx::ImageBufferStatus::Pending);
  BindFrameRead(&request, buffer);
  EXPECT_EQ(request.verifiedTargetTimestamp, request.options->targetTimestamp);
  EXPECT_EQ(request.buffer->status(), tgfx::ImageBufferStatus::Pending);
  EXPECT_FALSE(request.matchesTarget());
  buffer->expire();
  EXPECT_EQ(request.buffer->status(), tgfx::ImageBufferStatus::Expired);
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, RetryableDeliveryCannotValidateKnownOrUnknownTarget) {
  for (auto status : {tgfx::ImageBufferStatus::Timeout, tgfx::ImageBufferStatus::Failed}) {
    FrameRead request;
    request.options->targetTimestamp = 1000;
    auto buffer = std::make_shared<FrameIdentityBuffer>(1, 1000, status);
    BindFrameRead(&request, buffer);
    EXPECT_FALSE(buffer->expired());
    EXPECT_EQ(buffer->status(), status);
    EXPECT_FALSE(request.hasValidBuffer());
    EXPECT_FALSE(request.matchesTarget());
    request.options->targetTimestamp = -1;
    EXPECT_FALSE(request.matchesTarget());
  }
}

PAG_TEST(FrameIdentityTest, ReadyBufferCannotBeRetargetedAfterAcquisition) {
  FrameRead request;
  request.options->targetTimestamp = 1000;
  auto buffer = std::make_shared<FrameIdentityBuffer>(1, 1000);
  BindFrameRead(&request, buffer);
  ASSERT_TRUE(request.matchesTarget());
  request.options->targetTimestamp = 2000;
  EXPECT_EQ(request.verifiedTargetTimestamp, 1000);
  EXPECT_TRUE(request.hasValidBuffer());
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, MissingBufferCannotMatchKnownTarget) {
  FrameRead request;
  request.options->targetTimestamp = 0;
  EXPECT_EQ(request.buffer, nullptr);
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, NegativeTargetStillRejectsStaleGeneration) {
  CodecTimeline timeline(1000);
  FrameRead request;
  request.sourceGeneration = timeline.generation;
  request.generation = timeline.generation->load();
  request.options->targetTimestamp = -1;
  EXPECT_TRUE(request.isCurrent());
  EXPECT_FALSE(request.matchesTarget());
  BindFrameRead(&request, std::make_shared<FrameIdentityBuffer>(1, 0));
  EXPECT_TRUE(request.matchesTarget());
  ASSERT_TRUE(timeline.restart());
  EXPECT_FALSE(request.isCurrent());
  EXPECT_FALSE(request.matchesTarget());
  auto buffer = std::make_shared<FrameIdentityBuffer>(1, 0);
  BindFrameRead(&request, buffer);
  ExpectBufferIdentity(buffer, 1);
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, UnknownTargetStillRequiresDeliveredPixelsWhenBufferExists) {
  FrameRead request;
  EXPECT_TRUE(request.matchesTarget());
  auto pending = std::make_shared<FrameIdentityBuffer>(0, -1, tgfx::ImageBufferStatus::Pending);
  BindFrameRead(&request, pending);
  EXPECT_FALSE(request.hasValidBuffer());
  EXPECT_FALSE(request.matchesTarget());
  pending->expire();
  EXPECT_FALSE(request.matchesTarget());
  auto delivered = std::make_shared<FrameIdentityBuffer>(2, -1);
  BindFrameRead(&request, delivered);
  ExpectBufferIdentity(delivered, 2);
  EXPECT_TRUE(request.hasValidBuffer());
  EXPECT_TRUE(request.matchesTarget());
  delivered->invalidate();
  EXPECT_EQ(delivered->status(), tgfx::ImageBufferStatus::Ready);
  EXPECT_FALSE(request.hasValidBuffer());
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, KnownTargetRejectsInvalidatedReadyBuffer) {
  FrameRead request;
  request.options->targetTimestamp = 1000;
  auto delivered = std::make_shared<FrameIdentityBuffer>(2, 1000);
  BindFrameRead(&request, delivered);
  ASSERT_TRUE(request.matchesTarget());
  delivered->invalidate();
  EXPECT_EQ(delivered->status(), tgfx::ImageBufferStatus::Ready);
  EXPECT_FALSE(request.matchesTarget());
}

PAG_TEST(FrameIdentityTest, CodecTimelineRejectsExpiredOutputAndInvalidArguments) {
  CodecTimeline timeline(1000);
  int64_t first = 0;
  int64_t second = 0;
  int64_t decoded = -1;
  ASSERT_TRUE(timeline.encode(0, &first));
  ASSERT_TRUE(timeline.restart());
  ASSERT_TRUE(timeline.encode(0, &second));
  EXPECT_FALSE(timeline.decode(first, &decoded));
  EXPECT_FALSE(timeline.encode(0, nullptr));
  EXPECT_FALSE(timeline.decode(second, nullptr));
  ASSERT_TRUE(timeline.restart());
  EXPECT_FALSE(timeline.decode(first, &decoded));
  EXPECT_FALSE(timeline.decode(second, &decoded));
  int64_t current = 0;
  ASSERT_TRUE(timeline.encode(0, &current));
  ASSERT_TRUE(timeline.decode(current, &decoded));
  EXPECT_EQ(decoded, 0);
  EXPECT_FALSE(timeline.decode(current - 1, &decoded));
  EXPECT_FALSE(timeline.decode(current + 1001, &decoded));
  timeline.invalidate();
  EXPECT_FALSE(timeline.decode(current, &decoded));
}
}  // namespace pag
