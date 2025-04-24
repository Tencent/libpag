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


#ifndef PORTABLE_TABLE_H
#define PORTABLE_TABLE_H

#pragma once


#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "A.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "Portable_Strings.h"
#include "String_Utils.h"
#include "AEFX_SuiteHelper.h"
#include "AEGP_SuiteHandler.h"

//	BOTH suite handlers?! Yes. 

//	Normally, a plug-in should rely on one or the other of 
//	our suite handling utility models. Since this sample is
//	about reacting to different hosts, we'll use both.


#define	MAJOR_VERSION	3
#define	MINOR_VERSION	3
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1

enum {
	PORTABLE_INPUT = 0,	// default input layer 
	PORTABLE_SLIDER,
	PORTABLE_NUM_PARAMS
};

enum {
	PORTABLE_DISK_ID = 1
};

typedef struct {
	A_FpShort	slider_value;	
} PortableRenderInfo;

#define	PORTABLE_MIN		0.0	
#define	PORTABLE_MAX		200.0
#define	PORTABLE_BIG_MAX	200.0
#define	PORTABLE_DFLT		10.0
#define SLIDER_PRECISION	1
#define DISPLAY_FLAGS		PF_ValueDisplayFlag_PERCENT

extern "C" {

	DllExport
	PF_Err 
	EffectMain(
					PF_Cmd			cmd,
					PF_InData		*in_data,
					PF_OutData		*out_data,
					PF_ParamDef		*params[],
					PF_LayerDef		*output,
					void			*extra);

}
#endif // PORTABLE_TABLE_H