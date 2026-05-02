// Copyright (C) 2026 Tencent. All rights reserved.
//
// Offscreen render helper for PAGX/PAG v2 tests. Takes a tgfx::Layer tree (as
// produced by either pagx::LayerBuilder::Build or pagx::pag::LayerInflater::
// Inflate) and renders it into a tgfx::Surface the caller can either pixmap-
// read for byte-level comparisons or feed directly into Baseline::Compare for
// webp-backed baseline checks.
//
// Implementation mirrors test/src/PAGXTest.cpp:264-270 so any baseline
// captured against the LayerBuilder path is reusable for the PAG v2 round-
// tripped path; Phase 12's RenderEquivalenceTest relies on that invariant.
#pragma once

#include <memory>

namespace tgfx {
class Context;
class Layer;
class Surface;
}  // namespace tgfx

namespace pagx::test {

/**
 * Renders a tgfx::Layer tree to an offscreen tgfx::Surface.
 *
 * @param context       Live tgfx::Context (typically `pag::PAGXTest::context`,
 *                      which the fixture acquires via GLDevice::Make +
 *                      lockContext in SetUp).
 * @param layer         Root of the layer tree to render. A nullptr layer
 *                      yields nullptr (helper never draws an empty root).
 * @param canvasWidth   Surface width in pixels. Must be > 0.
 * @param canvasHeight  Surface height in pixels. Must be > 0.
 * @param scale         Uniform scale applied via a wrapper container layer.
 *                      Mirrors the MinCanvasEdge up-scaling the LayerBuilder
 *                      render path does for tiny sources.
 *
 * Returns nullptr on any failure path (invalid dims / Surface::Make fails /
 * null layer) so callers can ASSERT on a non-null result without having to
 * enumerate failure modes. The returned Surface is a fresh, independently
 * ownable object — safe to pixmap-read, Baseline::Compare, or discard.
 */
std::shared_ptr<tgfx::Surface> RenderLayerToSurface(tgfx::Context* context,
                                                    std::shared_ptr<tgfx::Layer> layer,
                                                    int canvasWidth, int canvasHeight,
                                                    float scale = 1.0f);

}  // namespace pagx::test
