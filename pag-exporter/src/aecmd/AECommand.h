/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef AECOMMAND_H
#define AECOMMAND_H
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"


static AEGP_Command PAGExporterCMD = 0L;
static A_long idleCount = 0L;
static AEGP_Command PAGConfigCMD = 0L;
static AEGP_Command PAGPanelCMD = 0L;
static AEGP_Command PAGPreviewCMD = 0L;

A_Err OnUpdateMenu(AEGP_GlobalRefcon globalRefcon, AEGP_UpdateMenuRefcon menuRefcon,
                          AEGP_WindowType windowType);
A_Err OnClickConfig(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                           AEGP_Command command, AEGP_HookPriority hookPriority,
                           A_Boolean alreadyHandled, A_Boolean* handled);
#endif  //AECOMMAND_H
