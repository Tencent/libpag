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
 * kVTDecodeFrame_EnableAsynchronousDecompression nor kVTDecodeFrame_EnableTemporalProcessing is set
 * (this decoder passes decodeFlags = 0), the output callback is invoked before
 * VTDecompressionSessionDecodeFrame returns. The late-delivery window this class guards against
 * comes from a different path: a frame the session held back is emitted by
 * VTDecompressionSessionFinishDelayedFrames, and issue #3780 measured eight such late deliveries on
 * affected devices. Because decodeFlags = 0 does not opt into kVTDecodeFrame_EnableTemporalProcessing,
 * VideoToolbox may not delay such a frame indefinitely, so a pending callback is bounded, not
 * unbounded. A late callback writing the CVPixelBufferRef into a stack slot of the already-returned
 * decode frame was the original crash, so the buffer lives in this heap context instead. The
 * infoFlags out-parameter stays a plain stack variable because VideoToolbox fills it synchronously
 * before the decode call returns, unlike the refcon buffer that the callback writes.
 *
 * The decoder owns every context through VTDecodeContextPool and never lets a callback delete one. A
 * context is reclaimed once resolved(); a context still pending at teardown is moved to a shared
 * graveyard rather than deleted or waited on, so a late callback can still land safely and a later
 * sweep reclaims it.
 */
class VTDecodeOutputContext {
 public:
  ~VTDecodeOutputContext();

  /**
   * Called by the VideoToolbox output callback to hand over the delivered pixel buffer, whose
   * ownership transfers into this context. Only records the buffer and marks the callback done; the
   * owning pool or graveyard, not the callback, releases the context.
   */
  void deliverBuffer(CVPixelBufferRef buffer);

  /**
   * Called by the decode thread once VTDecompressionSessionDecodeFrame returns. Marks the context
   * consumed and returns the buffer already delivered by a synchronous callback, or nullptr when no
   * callback has run yet. Ownership of a returned buffer transfers to the caller.
   */
  CVPixelBufferRef takeBuffer();

  /**
   * Called by the decode thread when VTDecompressionSessionDecodeFrame reported an error, so
   * VideoToolbox will not invoke the callback for this frame. The context then needs no callback to
   * become resolved() and is reclaimed on the next sweep instead of lingering forever.
   */
  void abandon();

  /**
   * Returns true once the decode thread is done and either the callback has run or no callback will
   * come, so the owning pool or graveyard may delete it.
   */
  bool resolved();

 private:
  std::mutex locker = {};
  CVPixelBufferRef outputPixelBuffer = nullptr;
  bool callbackDone = false;
  bool consumerDone = false;
  bool callbackNotExpected = false;
};

/**
 * VTDecodeContextPool owns every VTDecodeOutputContext a decoder creates. The decode thread reclaims
 * resolved contexts after each decode; at session teardown it drains the pool, deleting resolved
 * contexts and handing any still-pending one to a process-wide graveyard. The graveyard lets a
 * bounded late callback land on a context whose decoder is already gone, without blocking teardown
 * on VideoToolbox; the next decoder's reclaim sweep deletes it once it resolves. Only frames the
 * session genuinely dropped would never resolve, and those are abandon()ed on the decode error path
 * rather than reaching the graveyard, so it stays self-draining.
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
   * Deletes this pool's resolved contexts and sweeps the shared graveyard, bounding both to the
   * frames still in flight.
   */
  void reclaimResolved();

  /**
   * Deletes this pool's resolved contexts and moves any still-pending one to the shared graveyard.
   * Call at session teardown, after the session is invalidated, so pending callbacks land in the
   * graveyard instead of on a freed context.
   */
  void drain();

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

/**
 * Diagnostic hook returning the number of contexts currently parked in the shared graveyard.
 * Exposed so tests can assert the graveyard self-drains; production code does not rely on it.
 */
size_t VTDecodeGraveyardSize();
}  // namespace pag
