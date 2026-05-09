// Copyright (C) 2026 Tencent. All rights reserved.
//
// Runtime discovery of spec/samples/*.pagx fixtures for Phase 12's
// parameterised render tests. Keeping enumeration out of the test body
// lets RenderEquivalenceTest and RenderCrossCheckTest share the same
// sample list + hasText hint, and gains new samples the instant someone
// drops a .pagx in spec/samples/ without test-code changes.

#pragma once

#include <string>
#include <vector>

namespace pagx::test {

struct PAGXSample {
  // Short name used as both the gtest parameter and the Baseline::Compare
  // key suffix (e.g. "rectangle" → "PAGRenderTest_Render/rectangle").
  std::string name;

  // Absolute filesystem path resolved via pag::ProjectPath::Absolute.
  std::string absolutePath;

  // Lightweight hint: true if the raw XML contains a `<Text` or `<TextBox`
  // element. Used by the OutlineAll variant of RenderEquivalenceTest to
  // skip text-free samples. The detection is a naive substring scan — it
  // intentionally accepts false positives over complex XML parsing
  // (Phase 12's OutlineAll is SKIP-gated anyway while the Baker
  // OutlineAll path is pending).
  bool hasText = false;

  // Source directory slug: "spec" for spec/samples/, or one of
  // "pagx_to_html" / "cli" / "layout" / "text" for the matching
  // resources/* subdirectories. Only populated by EnumerateComparisonSamples;
  // EnumerateSpecSamples / EnumerateFirstBatchSamples leave it as "spec"
  // for backward compatibility with existing gate tests.
  std::string section = "spec";
};

// Scans spec/samples/*.pagx (non-recursive) and returns one PAGXSample per
// file. Order is alphabetical so gtest parameterised reports are stable.
// Returns empty vector on a missing spec/samples dir — callers (test
// fixtures) should treat that as a setup error.
std::vector<PAGXSample> EnumerateSpecSamples();

// Subset for the Phase 12 PR gate: returns only samples that have been
// observed to pass PAGX → Bake → Encode → Decode → Inflate end-to-end on
// the reference machine (Apple M4 Pro; see Phase 12.0 probe). Today this
// returns the full EnumerateSpecSamples() list because the Phase 11.5
// CompositionRef forward-reference fix promoted all 48 fixtures into the
// OK set. Kept as a separate entry point so future "known-broken" samples
// can be exempted from the hard gate without losing them from the
// exploratory list.
std::vector<PAGXSample> EnumerateFirstBatchSamples();

// Union of spec/samples/ + resources/{pagx_to_html,cli,layout,text}/ for
// the visual comparison report (tools/render_compare.py). Every returned
// PAGXSample has `section` populated. Samples are grouped by section in
// the output order spec → pagx_to_html → cli → layout → text; within a
// section order is alphabetical. Never gated — purely for visual review.
std::vector<PAGXSample> EnumerateComparisonSamples();

}  // namespace pagx::test
