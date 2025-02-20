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

#ifndef PathMaster_H
#define PathMaster_H

#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "String_Utils.h"
#include "AEGP_SuiteHandler.h"
#include "AEFX_SuiteHelper.h"
#include "PF_Masks.h"
#ifdef AE_OS_WIN
	#include "string.h"
#endif
typedef struct {
	PF_Pixel				color;
	PF_Pixel16				deep_color;
	A_u_char				opacityCu;
	A_u_short				opacitySu;
	PF_Boolean				invertB;
} BlendInfo;

typedef struct {
	A_Boolean	flatB;
	A_char		string[PF_MAX_EFFECT_MSG_LEN + 1];
	A_Fixed		fixed_valF;
} Flat_Seq_Data;

typedef struct {
	A_Boolean	flatB;
	A_char		*stringP;
	A_Fixed		fixed_valF;
} Unflat_Seq_Data;


#define	OPACITY_VALID_MIN		0
#define	OPACITY_VALID_MAX		1
#define	OPACITY_DFLT			1
#define OPACITY_PRECISION		0

#define	FEATHER_MIN				0
#define	FEATHER_MAX				100
#define	FEATHER_VALID_MIN		0
#define	FEATHER_VALID_MAX		100
#define	FEATHER_DFLT			100

#define	MAJOR_VERSION	4
#define	MINOR_VERSION	3
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1

enum {
	PATHMASTER_INPUT = 0,
	PATHMASTER_PATH,
	PATHMASTER_COLOR,
	PATHMASTER_X_FEATHER,
	PATHMASTER_Y_FEATHER,
	PATHMASTER_INVERT,
	PATHMASTER_OPACITY,
	PATHMASTER_NUM_PARAMS
};

enum {
	PATH_DISK_ID = 1,
	COLOR_DISK_ID,
	X_FEATHER_DISK_ID,
	Y_FEATHER_DISK_ID,
	DOWNSAMPLE_DISK_ID,
	OPACITY_DISK_ID
};

typedef enum {
	StrID_NONE, 
	StrID_Name,
	StrID_Description,
	StrID_PathName,
	StrID_ColorName, 
	
	StrID_CheckName,
	StrID_CheckDetail,	
	
	StrID_Err_LoadSuite,
	StrID_Err_FreeSuite, 
	
	StrID_OpacityName,
	StrID_X_Feather,
	StrID_Y_Feather,
	StrID_API_Vers_Warning,
	StrID_Really_Long_String,
	
	StrID_NUMTYPES
} StrIDType;

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
#endif // PathMaster_H