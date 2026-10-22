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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGXOptimizer.h"

namespace pagx {
using Clock = std::chrono::steady_clock;

static double Median(std::vector<double>* values) {
  if (values->empty()) {
    return 0;
  }
  auto mid = values->begin() + static_cast<int>(values->size()) / 2;
  std::nth_element(values->begin(), mid, values->end());
  return *mid;
}

static void CollectPagxFiles(const std::string& dir, std::vector<std::string>* files) {
  std::error_code ec;
  for (auto it = std::filesystem::recursive_directory_iterator(dir, ec); it != end(it);
       it.increment(ec)) {
    if (ec) {
      break;
    }
    if (it->is_regular_file() && it->path().extension() == ".pagx") {
      files->push_back(it->path().string());
    }
  }
  std::sort(files->begin(), files->end());
}

struct PerfRow {
  std::string path;
  size_t xmlBytes = 0;
  size_t nodes = 0;
  double importMs = 0;
  double layoutMs = 0;
  size_t optNodes = 0;
  size_t optXmlBytes = 0;
  double optImportMs = 0;
  double optLayoutMs = 0;
};

// The import time covers file read + XML parse + node tree build; the layout time covers
// constraint solving and text shaping. Layout always runs on a freshly imported document
// because it mutates node state.
static void MeasureFile(const std::string& path, int iterations, PerfRow* row) {
  row->path = path;
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (in) {
    row->xmlBytes = static_cast<size_t>(in.tellg());
  }
  std::vector<double> importMs;
  std::vector<double> layoutMs;
  for (int i = 0; i < iterations; i++) {
    auto importStart = Clock::now();
    auto doc = PAGXImporter::FromFile(path);
    auto importEnd = Clock::now();
    if (doc == nullptr) {
      return;
    }
    importMs.push_back(std::chrono::duration<double, std::milli>(importEnd - importStart).count());
    row->nodes = doc->nodes.size();
    auto layoutStart = Clock::now();
    doc->applyLayout();
    auto layoutEnd = Clock::now();
    layoutMs.push_back(std::chrono::duration<double, std::milli>(layoutEnd - layoutStart).count());
  }
  row->importMs = Median(&importMs);
  row->layoutMs = Median(&layoutMs);
}

// Measures the same file after PAGXOptimizer::Optimize by round-tripping the optimized
// document through XML, so the optimized numbers come from a normal import + layout path.
static void MeasureOptimized(const std::string& path, PerfRow* row) {
  auto doc = PAGXImporter::FromFile(path);
  if (doc == nullptr) {
    return;
  }
  PAGXOptimizer::Optimize(doc.get());
  row->optNodes = doc->nodes.size();
  auto xml = PAGXExporter::ToXML(*doc);
  row->optXmlBytes = xml.size();
  auto tempPath = path + ".perf_tmp";
  std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
  out.write(xml.data(), static_cast<std::streamsize>(xml.size()));
  out.close();
  std::vector<double> importMs;
  std::vector<double> layoutMs;
  for (int i = 0; i < 3; i++) {
    auto importStart = Clock::now();
    auto optimized = PAGXImporter::FromFile(tempPath);
    auto importEnd = Clock::now();
    if (optimized == nullptr) {
      break;
    }
    importMs.push_back(std::chrono::duration<double, std::milli>(importEnd - importStart).count());
    auto layoutStart = Clock::now();
    optimized->applyLayout();
    auto layoutEnd = Clock::now();
    layoutMs.push_back(std::chrono::duration<double, std::milli>(layoutEnd - layoutStart).count());
  }
  row->optImportMs = Median(&importMs);
  row->optLayoutMs = Median(&layoutMs);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
}

PAGX_TEST(PAGXPerfTest, LoadBaseline) {
  const char* inputEnv = std::getenv("PAGX_PERF_INPUT");
  if (inputEnv == nullptr) {
    GTEST_SKIP() << "set PAGX_PERF_INPUT (file or directory) to run the perf baseline";
    return;
  }
  const char* iterationsEnv = std::getenv("PAGX_PERF_ITERATIONS");
  auto iterations = iterationsEnv == nullptr ? 5 : std::max(1, std::atoi(iterationsEnv));
  const char* outputEnv = std::getenv("PAGX_PERF_OUTPUT");

  std::vector<std::string> files;
  std::error_code ec;
  if (std::filesystem::is_regular_file(inputEnv)) {
    files.push_back(inputEnv);
  } else {
    CollectPagxFiles(inputEnv, &files);
  }
  ASSERT_TRUE(!files.empty()) << "no .pagx files under " << inputEnv;

  // The importer prints warnings for missing fonts and malformed files; redirect stderr so the
  // timing loop is not dominated by console writes.
  std::streambuf* oldErr = std::cerr.rdbuf();
  std::ostringstream sink;
  std::cerr.rdbuf(sink.rdbuf());

  std::vector<PerfRow> rows;
  for (auto& path : files) {
    PerfRow row = {};
    MeasureFile(path, iterations, &row);
    MeasureOptimized(path, &row);
    rows.push_back(row);
  }
  std::cerr.rdbuf(oldErr);

  std::ostringstream table;
  table << "path\txml_bytes\tnodes\timport_ms\tlayout_ms\topt_nodes\topt_xml_bytes\topt_import_"
           "ms\topt_layout_ms\n";
  double importTotal = 0;
  double layoutTotal = 0;
  size_t xmlTotal = 0;
  size_t optXmlTotal = 0;
  size_t shrinkSamples = 0;
  for (auto& row : rows) {
    table << row.path << "\t" << row.xmlBytes << "\t" << row.nodes << "\t" << row.importMs << "\t"
          << row.layoutMs << "\t" << row.optNodes << "\t" << row.optXmlBytes << "\t"
          << row.optImportMs << "\t" << row.optLayoutMs << "\n";
    importTotal += row.importMs;
    layoutTotal += row.layoutMs;
    xmlTotal += row.xmlBytes;
    optXmlTotal += row.optXmlBytes;
    if (row.optXmlBytes > 0 && row.optXmlBytes < row.xmlBytes) {
      shrinkSamples++;
    }
  }
  table << "TOTAL(" << rows.size() << ")\t" << xmlTotal << "\t-\t" << importTotal << "\t"
        << layoutTotal << "\t-\t" << optXmlTotal << "\t-\t-\n";
  table << "optimized-smaller-ratio\t" << shrinkSamples << "/" << rows.size() << "\n";
  std::cout << table.str();

  if (outputEnv != nullptr) {
    std::ofstream out(outputEnv, std::ios::binary | std::ios::trunc);
    out << table.str();
  }
}
}  // namespace pagx
