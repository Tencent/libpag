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

#ifndef CONVOLUTRIX_H
#define CONVOLUTRIX_H

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AEGP_SuiteHandler.h"
#include "Convolutrix_Strings.h"

#ifdef AE_OS_WIN
	#include <cstdlib>
	#include <string.h>
#endif

#include <math.h>

/* Versioning information */

#define	MAJOR_VERSION	3
#define	MINOR_VERSION	2
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


/* Parameter defaults */

#define	CONVO_AMOUNT_MIN	0.0		
#define	CONVO_AMOUNT_MAX	100
#define	CONVO_AMOUNT_DFLT	50

#define	CURVE_TOLERANCE		0.05f

enum 
{
	CONVO_INPUT = 0,
	CONVO_AMOUNT,
	CONVO_BLEND_GROUP_START,
	CONVO_BLEND_COLOR_AMOUNT,
	CONVO_COLOR,
	CONVO_BLEND_GROUP_END,
	CONVO_NUM_PARAMS
};

enum 
{
	AMOUNT_DISK_ID = 1,
	COLOR_DISK_ID,
	CONVO_AMOUNT_ID,
	CONVO_BLEND_GROUP_START_ID,
	CONVO_BLEND_AMOUNT_ID,
	CONVO_COLOR_ID,
	CONVO_BLEND_GROUP_END_ID,
	SIZE_DISK_ID
};

#define KERNEL_SIZE 3


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

#endif // CONVOLUTRIX_H