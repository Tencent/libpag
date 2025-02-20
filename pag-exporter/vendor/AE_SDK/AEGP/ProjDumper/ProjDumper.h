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
#include "entry.h"

#ifdef AE_OS_WIN
	#include <windows.h>
	#include <stdio.h>
	#include <string.h>
#elif defined AE_OS_MAC
	#include <wchar.h>
#endif

#include "AE_GeneralPlug.h"
#include "AE_Effect.h"
#include "A.h"
#include "AE_EffectUI.h"
#include "SPSuites.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AEGP_SuiteHandler.h"
#include "AE_Macros.h"

#ifdef AE_OS_WIN
	#define PROXY_FOOTAGE_PATH	L"C:\\proxy.jpg"
	#define LAYERED_PATH		L"C:\\noel_clown_nose.psd"
#elif defined AE_OS_MAC
	#define PROXY_FOOTAGE_PATH	L"proxy.jpg"
	#define LAYERED_PATH		L"noel_clown_nose.psd"
#endif

#define ERROR_MISSING_FOOTAGE	"Footage not found! Make sure you've copied proxy.jpg from the ProjDumper folder and noel_clown_nose.psd from the Projector folder to the root of your primary hard disk."
#define DUMP_SUCCEEDED_WIN		"Project dumped to C:\\Windows\\Temp\\"
#define DUMP_SUCCEEDED_MAC		"Project dumped to root of primary hard drive."
#define DUMP_FAILED				"Project dump failed."

// This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;
