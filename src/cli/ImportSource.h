// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#pragma once
#include "cli/ImportPlan.h"

namespace pagx::cli {
// Converts one prepared source without any browser/prepass or CLI dispatch.
// snapshot is the already acquired HTML file; baseDir refers to the original source,
// never the temporary snapshot location. Images are loaded before temporary inputs expire.
ImportResult ImportPreparedSource(const ImportItem& item, const std::string& snapshot,
                                  const std::string& baseDir, const ImportRequest& options,
                                  const std::vector<std::string>& fonts = {});
}  // namespace pagx::cli
