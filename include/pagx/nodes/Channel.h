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

#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * KeyValue is the value carried by a Channel's keyframes. Its std::variant alternatives are
 * ordered to match ChannelValueType, so the active alternative index equals the corresponding
 * ChannelValueType value.
 */
using KeyValue = std::variant<float, bool, int, std::string, ImageRef, Color>;

/**
 * Discriminator for the value type carried by a Channel's keyframes. Aligned with the order of
 * KeyValue's std::variant alternatives.
 */
enum class ChannelValueType : uint8_t {
  Float = 0,
  Bool = 1,
  Int = 2,
  String = 3,
  ImageRef = 4,
  Color = 5,
};

/**
 * Channel is the abstract base for a single animated channel within an AnimationObject. Concrete
 * subclasses are TypedChannel<T> specializations, each binding a KeyValue alternative to a set of
 * keyframes. The base exposes type-erased evaluation so callers can sample a Channel without
 * knowing its concrete value type.
 */
class Channel : public Node {
 public:
  /**
   * The name of the property this Channel drives on the target node (for example a transform or
   * color channel). Interpreted by the runtime when applying evaluated values to the layer tree.
   */
  std::string name = {};

  /**
   * Returns the value type of this Channel's keyframes. Each TypedChannel<T> specialization
   * reports a distinct ChannelValueType so callers can dispatch on the concrete type without
   * dynamic_cast or RTTI.
   */
  virtual ChannelValueType valueType() const = 0;

  /**
   * Evaluates the channel value at the given Frame index. Useful for unit tests and any caller
   * operating in frame-discrete time.
   * @param frame the frame index to sample.
   * @return the interpolated value at the given frame.
   */
  KeyValue evaluateAt(Frame frame) const {
    return onEvaluateAtFrame(frame);
  }

  /**
   * Evaluates the channel value at the given time in microseconds, using the supplied frameRate
   * to convert microseconds to a continuous frame position. Implementations perform interpolation
   * in double precision to avoid losing precision when the timeline is driven by microsecond
   * deltas.
   * @param microseconds the time to sample, in microseconds.
   * @param frameRate the frame rate used to map microseconds to a frame position.
   * @return the interpolated value at the given time.
   */
  KeyValue evaluateAt(int64_t microseconds, float frameRate) const {
    return onEvaluateAtMicros(microseconds, frameRate);
  }

  NodeType nodeType() const override {
    return NodeType::Channel;
  }

 protected:
  Channel() = default;

 private:
  virtual KeyValue onEvaluateAtFrame(Frame frame) const = 0;
  virtual KeyValue onEvaluateAtMicros(int64_t microseconds, float frameRate) const = 0;

  friend class PAGXDocument;
};

/**
 * TypedChannel binds a concrete KeyValue alternative T to an ordered list of keyframes. Each
 * specialization reports a distinct ChannelValueType and provides interpolation for its value
 * type through the runtime KeyframeEvaluator.
 */
template <typename T>
class TypedChannel : public Channel {
 public:
  /**
   * The keyframes driving this channel, ordered by time. Interpolation between keyframes follows
   * each keyframe's KeyframeInterpolationType.
   */
  std::vector<Keyframe<T>> keyframes = {};

  ChannelValueType valueType() const override;

 private:
  // Defined in src/pagx/animation/Channel.cpp with explicit instantiations for each KeyValue
  // alternative (float, bool, int, std::string, ImageRef, Color). The implementation runs through
  // the runtime KeyframeEvaluator helper so that bezier easing and Color/float lerp logic stay
  // out of this public header.
  KeyValue onEvaluateAtFrame(Frame frame) const override;
  KeyValue onEvaluateAtMicros(int64_t microseconds, float frameRate) const override;

  friend class PAGXDocument;
};

}  // namespace pagx
