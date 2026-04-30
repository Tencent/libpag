// Copyright (C) 2026 Tencent. All rights reserved.
//
// Internal alias of the public DiagnosticCode / Diagnostic types. Files under
// src/pagx/pag/** use the short names (ErrorCode / Diagnostic), keeping the
// implementation source consistent while public headers expose the full
// pagx::DiagnosticCode name (see §G.2 dual-naming rule).
#pragma once

#include "pagx/Diagnostic.h"

namespace pagx::pag {

using ErrorCode = pagx::DiagnosticCode;
using Diagnostic = pagx::Diagnostic;

}  // namespace pagx::pag
