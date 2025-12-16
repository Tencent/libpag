/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"
#include "entry.h"

namespace exporter {

class AECommand {
 public:
  static A_Err OnUpdateMenu(AEGP_GlobalRefcon globalRefcon, AEGP_UpdateMenuRefcon menuRefcon,
                            AEGP_WindowType windowType);
  static A_Err OnClickConfig(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                             AEGP_Command command, AEGP_HookPriority hookPriority,
                             A_Boolean alreadyHandled, A_Boolean* handled);
  static A_Err OnClickPanel(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                            AEGP_Command command, AEGP_HookPriority hookPriority,
                            A_Boolean alreadyHandled, A_Boolean* handled);

  static A_Err OnClickExporter(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                               AEGP_Command command, AEGP_HookPriority hookPriority,
                               A_Boolean alreadyHandled, A_Boolean* handled);
  static A_Err OnClickPreview(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                              AEGP_Command command, AEGP_HookPriority hookPriority,
                              A_Boolean alreadyHandled, A_Boolean* handled);
  static AEGP_Command PAGExporterCMD;
  static A_long idleCount;
  static AEGP_Command PAGConfigCMD;
  static AEGP_Command PAGPanelCMD;
  static AEGP_Command PAGPreviewCMD;
};

}  // namespace exporter
