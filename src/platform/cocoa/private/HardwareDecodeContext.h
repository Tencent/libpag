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

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>
#include <mutex>
#include <vector>

namespace pag {
/**
 * VTDecodeOutputContext carries the pixel buffer produced by a single
 * VTDecompressionSessionDecodeFrame call across the decode thread and VideoToolbox's output
 * callback.
 *
 * VideoToolbox's VTDecompressionSession.h documents that when neither
 * kVTDecodeFrame_EnableAsynchronousDecompression nor kVTDecodeFrame_EnableTemporalProcessing is
 * set (this decoder passes decodeFlags = 0), the output callback is invoked before
 * VTDecompressionSessionDecodeFrame returns. The late-delivery window this class guards against
 * comes from a different path: VTDecompressionSessionWaitForAsynchronousFrames /
 * FinishDelayedFrames may emit frames the session held back, and issue #3780 measured eight such
 * late deliveries on affected devices. A late callback writing the CVPixelBufferRef into a stack
 * slot of the already-returned decode frame was the original crash, so the buffer lives in this
 * heap context instead. The infoFlags out-parameter stays a plain stack variable because
 * VideoToolbox fills it synchronously before the decode call returns, unlike the refcon buffer
 * that the callback writes.
 *
 * The decoder owns every context through VTDecodeContextPool and never lets a callback delete one.
 * A context is reclaimed once both sides are done (the common synchronous frame); delayed or
 * dropped frames that never resolve are reclaimed after the teardown barrier
 * (VTDecompressionSessionWaitForAsynchronousFrames) guarantees no further callback can arrive.
 */
class VTDecodeOutputContext {
 public:
  ~VTDecodeOutputContext();

  /**
   * Called by the VideoToolbox output callback to hand over the delivered pixel buffer, whose
   * ownership transfers into this context. Only records the buffer and marks the callback done; the
   * owning pool, not the callback, releases the context.
   */
  void deliverBuffer(CVPixelBufferRef buffer);

  /**
   * Called by the decode thread once VTDecompressionSessionDecodeFrame returns. Marks the context
   * consumed and returns the buffer already delivered by a synchronous callback, or nullptr when no
   * callback has run yet. Ownership of a returned buffer transfers to the caller.
   */
  CVPixelBufferRef takeBuffer();

  /**
   * Returns true once both the callback and the decode thread have finished with this context, so
   * the owning pool may delete it.
   */
  bool resolved();

 private:
  std::mutex locker = {};
  CVPixelBufferRef outputPixelBuffer = nullptr;
  bool callbackDone = false;
  bool consumerDone = false;
};

/**
 * VTDecodeContextPool owns every VTDecodeOutputContext a decoder creates. The decode thread
 * reclaims resolved contexts after each decode, and reclaims all of them once the session teardown
 * barrier guarantees no further callback can arrive.
 */
class VTDecodeContextPool {
 public:
  ~VTDecodeContextPool();

  /**
   * Creates a context owned by this pool, to be passed as the sourceFrameRefCon of one
   * VTDecompressionSessionDecodeFrame call.
   */
  VTDecodeOutputContext* create();

  /**
   * Deletes the contexts whose callback and decode thread are both done, bounding the pool to the
   * frames still in flight.
   */
  void reclaimResolved();

  /**
   * Deletes every context. Call only after the session barrier so no VideoToolbox callback can
   * still reference a context.
   */
  void reclaimAll();

 private:
  std::vector<VTDecodeOutputContext*> contexts = {};
};

/**
 * VideoToolbox decompression output callback shared by the iOS and macOS decoders. Defined once in
 * HardwareDecodeContext.mm so the two decoders cannot drift into an ODR violation.
 */
void DidDecompress(void* decompressionOutputRefCon, void* sourceFrameRefCon, OSStatus status,
                   VTDecodeInfoFlags infoFlags, CVImageBufferRef pixelBuffer,
                   CMTime presentationTimeStamp, CMTime presentationDuration);
}  // namespace pag
