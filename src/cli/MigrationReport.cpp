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
//  license is distributed on an "as is" basis, without WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "cli/MigrationReport.h"
#include <cstdio>
#include <iostream>
#include "pag/file.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ViewModel.h"
#include "pagx/nodes/ViewModelProperty.h"

namespace pagx::cli {

static std::string WarningCodeName(PAGExportWarning code) {
  switch (code) {
    case PAGExportWarning::MissingFont:
      return "MissingFont";
    case PAGExportWarning::MissingImage:
      return "MissingImage";
    case PAGExportWarning::MissingSubDocument:
      return "MissingSubDocument";
    case PAGExportWarning::UnsupportedURL:
      return "UnsupportedURL";
    case PAGExportWarning::AnimatedContentSkipped:
      return "AnimatedContentSkipped";
    case PAGExportWarning::LayoutChannelDropped:
      return "LayoutChannelDropped";
    case PAGExportWarning::MixedEmbeddedText:
      return "MixedEmbeddedText";
    case PAGExportWarning::EmbeddedTextBound:
      return "EmbeddedTextBound";
    case PAGExportWarning::InvalidAnimationTiming:
      return "InvalidAnimationTiming";
    case PAGExportWarning::NonFiniteValue:
      return "NonFiniteValue";
    case PAGExportWarning::ImageScaledDown:
      return "ImageScaledDown";
    case PAGExportWarning::UnsupportedColorSource:
      return "UnsupportedColorSource";
    case PAGExportWarning::SubDocumentExpansionLimit:
      return "SubDocumentExpansionLimit";
    case PAGExportWarning::UnresolvedMaskReference:
      return "UnresolvedMaskReference";
    case PAGExportWarning::UnresolvedDisplacementMapReference:
      return "UnresolvedDisplacementMapReference";
  }
  return "Unknown";
}

static std::string FormatBytes(uint64_t bytes) {
  char buffer[32] = {};
  if (bytes >= 1024 * 1024) {
    snprintf(buffer, sizeof(buffer), "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  } else if (bytes >= 1024) {
    snprintf(buffer, sizeof(buffer), "%.2f KB", static_cast<double>(bytes) / 1024.0);
  } else {
    snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
  }
  return buffer;
}

static std::string PropertyTypeName(ViewModelPropertyType type) {
  switch (type) {
    case ViewModelPropertyType::Number:
      return "number";
    case ViewModelPropertyType::String:
      return "string";
    case ViewModelPropertyType::Boolean:
      return "boolean";
    case ViewModelPropertyType::Color:
      return "color";
    case ViewModelPropertyType::Image:
      return "image";
    case ViewModelPropertyType::ViewModel:
      return "viewModel";
    case ViewModelPropertyType::Enum:
      return "enum";
    case ViewModelPropertyType::Trigger:
      return "trigger";
  }
  return "unknown";
}

std::string BuildMigrationReport(const MigrationReportInput& input) {
  std::string report = "# Legacy PAG Migration Report\n\n";
  // Migration level.
  auto issueCount = input.issues != nullptr ? input.issues->size() : 0u;
  auto level = issueCount == 0 ? std::string("fully-migrated") : std::string("partial");
  report += "- Migration level: **" + level + "** (" + std::to_string(issueCount) + " issue(s))\n";
  if (input.legacyFile != nullptr) {
    auto durationSeconds =
        input.legacyFile->frameRate() > 0
            ? static_cast<double>(input.legacyFile->duration()) / input.legacyFile->frameRate()
            : 0.0;
    char durationText[32] = {};
    snprintf(durationText, sizeof(durationText), "%.2fs", durationSeconds);
    report += "- Input: " + input.inputPath + " (" + FormatBytes(input.inputBytes) + ")\n";
    report += "- Output: " + input.outputPath + " (" + FormatBytes(input.outputBytes) + ")\n";
    report += "- Size ratio: " +
              (input.inputBytes > 0 ? std::to_string(static_cast<double>(input.outputBytes) /
                                                     static_cast<double>(input.inputBytes))
                                    : std::string("n/a")) +
              "x\n";
    report += "\n## Legacy feature inventory\n\n";
    report += "| Feature | Value |\n|---|---|\n";
    report += "| Dimensions | " + std::to_string(input.legacyFile->width()) + " x " +
              std::to_string(input.legacyFile->height()) + " |\n";
    report += "| Duration | " + std::string(durationText) + " (" +
              std::to_string(input.legacyFile->duration()) + " frames @ " +
              std::to_string(input.legacyFile->frameRate()) + " fps) |\n";
    report += "| Editable texts | " + std::to_string(input.legacyFile->numTexts()) + " |\n";
    report += "| Editable images | " + std::to_string(input.legacyFile->numImages()) + " |\n";
    report += "| Video compositions | " + std::to_string(input.legacyFile->numVideos()) + " |\n";
  }
  if (issueCount > 0) {
    report += "\n## Unsupported / degraded details\n\n";
    report += "| Code | Scope | Message |\n|---|---|---|\n";
    for (auto& issue : *input.issues) {
      report +=
          "| " + WarningCodeName(issue.code) + " | " + issue.scope + " | " + issue.message + " |\n";
    }
  }
  if (input.document != nullptr && input.document->viewModel != nullptr) {
    report += "\n## ViewModel property mapping (spec 7.2)\n\n";
    report += "| Property | Type | Default |\n|---|---|---|\n";
    for (auto* property : input.document->viewModel->properties) {
      if (property == nullptr) {
        continue;
      }
      std::string defaultText =
          property->propertyType == ViewModelPropertyType::String
              ? property->defaultString
              : (property->propertyType == ViewModelPropertyType::Image ? std::string("<image>")
                                                                        : std::string("-"));
      if (defaultText.size() > 40) {
        defaultText = defaultText.substr(0, 37) + "...";
      }
      report += "| " + property->name + " | " + PropertyTypeName(property->propertyType) + " | " +
                defaultText + " |\n";
    }
  }
  return report;
}

void PrintMigrationSummary(const MigrationReportInput& input) {
  auto issueCount = input.issues != nullptr ? input.issues->size() : 0u;
  std::cerr << "pagx import: migrated " << input.inputPath << " -> " << input.outputPath << " ("
            << FormatBytes(input.inputBytes) << " -> " << FormatBytes(input.outputBytes) << ", "
            << issueCount << " issue(s))\n";
}

}  // namespace pagx::cli
