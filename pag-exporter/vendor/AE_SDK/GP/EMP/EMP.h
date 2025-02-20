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



#pragma once

#ifndef EMP_H
#define EMP_H

#include "AEConfig.h"
#include "A.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "AE_Hook.h"
#include "Entry.h"

#ifdef AE_OS_WIN
	#include <stdio.h>
	#include <windows.h>
#endif

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	0

#define	NAME				"EMP"
#define DESCRIPTION			"External Monitor Preview"

#include "AE_Hook.h"

extern "C" {
DllExport PF_Err 
EntryPointFunc(
	A_long 			major_version,	/* >> */
 	A_long 			minor_version,	/* >> */		
	AE_FileSpecH 	file_specH,		/* >> */		
	AE_FileSpecH 	res_specH,		/* >> */		
	AE_Hooks 		*hooksP);		/* <> */
}
#ifdef AE_OS_WIN
	BOOL APIENTRY LibMain(HANDLE hInstance, DWORD fdwReason, LPVOID lpReserved);
#endif
		
#endif //EMP_H