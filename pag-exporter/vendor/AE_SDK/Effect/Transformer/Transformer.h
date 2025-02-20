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

#ifndef TRANSFORMER_H
#define TRANSFORMER_H

/*	Ensures AE_Effect.h provides us with 16bpc pixels */

#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"

#include "Transformer_Strings.h"

#ifdef AE_OS_WIN
	#include <Windows.h>
#endif	

/* Versioning information */

#define	MAJOR_VERSION	2
#define	MINOR_VERSION	2
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


/* Parameter defaults */

#define	XFORM_AMOUNT_MIN		0
#define	XFORM_AMOUNT_MAX		1
#define	XFORM_COLOR_BLEND_DFLT	.5

enum 
{
	XFORM_INPUT = 0,
	XFORM_COLORBLEND,
	XFORM_COLOR,
	XFORM_GROUP_BEGIN,
	XFORM_LAYERBLEND,
	XFORM_LAYER,
	XFORM_GROUP_END,
	XFORM_NUM_PARAMS
};

enum 
{
	COLORBLEND_DISK_ID = 1,
	COLOR_DISK_ID,
	TOPIC_DISK_ID,
	LAYERBLEND_DISK_ID,
	LAYER_DISK_ID,
	END_TOPIC_DISK_ID
};


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

#endif // TRANSFORMER_H