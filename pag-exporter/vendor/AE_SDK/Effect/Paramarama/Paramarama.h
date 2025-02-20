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

#ifndef PARAMARAMA_H
#define PARAMARAMA_H

#define PF_DEEP_COLOR_AWARE 1	

#include "AEConfig.h"
#include "entry.h"
#ifdef AE_OS_WIN
	#include "string.h"
#endif
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"

#include "Paramarama_Strings.h"

/* Versioning information */

#define	MAJOR_VERSION	2
#define	MINOR_VERSION	1
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


/* Parameter defaults */

#define	PARAMARAMA_AMOUNT_MIN	0
#define	PARAMARAMA_AMOUNT_MAX	100
#define	PARAMARAMA_AMOUNT_DFLT	93

#define DEFAULT_RED				111
#define DEFAULT_GREEN			222
#define DEFAULT_BLUE			33

#define DEFAULT_FLOAT_VAL		27.62
#define DEFAULT_ANGLE_VAL		(0L << 16)
#define FLOAT_MIN				2.387
#define FLOAT_MAX				987.653

#define DEFAULT_POINT_VALS		50.0

#define NULL_WIDTH				100
#define NULL_HEIGHT				10

enum 
{
	PARAMARAMA_INPUT = 0,
	PARAMARAMA_AMOUNT,
	PARAMARAMA_COLOR,
	PARAMARAMA_FLOAT_VAL,
	PARAMARAMA_ANGLE,
	PARAMARAMA_POPUP,
	PARAMARAMA_DOWNSAMPLE,
	PARAMARAMA_3D_POINT,
	PARAMARAMA_BUTTON,
	PARAMARAMA_NUM_PARAMS
};

enum 
{
	AMOUNT_DISK_ID = 1,
	COLOR_DISK_ID,
	FLOAT_DISK_ID,
	DOWNSAMPLE_DISK_ID,
	ANGLE_DISK_ID,
	NULL_DISK_ID,
	POPUP_DISK_ID,
	THREE_D_POINT_DISK_ID,
	BUTTON_DISK_ID
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
		
#endif // PARAMARAMA_H