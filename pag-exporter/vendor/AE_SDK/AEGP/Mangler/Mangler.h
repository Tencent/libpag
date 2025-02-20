/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Inc. and its suppliers, if                    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Inc. and its                    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

#include "AEConfig.h"

#ifdef AE_OS_WIN
	#include <windows.h>
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
#endif

#include "entry.h"
#include "AE_GeneralPlug.h"
#include "A.h"
#include "AE_EffectUI.h"
#include "AEGP_Utils.h"
#include "AEGP_SuiteHandler.h"
#include "Mangler_Strings.h"
#include "String_Utils.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"

typedef struct {
	A_char 				marker_text[AEGP_MAX_MARKER_NAME_SIZE];
	AEGP_ItemH			compH;
	AEGP_StreamType		stream_type;	// Selected Stream	
} ManglerOptions;

// This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;
