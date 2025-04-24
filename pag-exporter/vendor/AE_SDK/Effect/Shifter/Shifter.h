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

#ifndef SHIFTER_H
#define SHIFTER_H

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"
#include "AEGP_SuiteHandler.h"
#include "AEFX_SuiteHelper.h"

#ifdef AE_OS_WIN
	#include <Windows.h>
#endif

#define	NAME "Shifter"
#define DESCRIPTION	"Blend in a shifted copy of the image.\nCopyright 1994-2023 Adobe Inc."

#define	MAJOR_VERSION		2
#define	MINOR_VERSION		1
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_DEVELOP
#define	BUILD_VERSION		1

enum {
	SHIFT_INPUT = 0,	// default input layer 
	SHIFT_DISPLACE,		// offset point control
	SHIFT_BLEND,		// blend slider
	SHIFT_USE_TRANSFORM_IN_LEGACY_RENDER_PATH,
	SHIFT_NUM_PARAMS
};

enum {
	DISPLACE_DISK_ID = 1,
	BLEND_DISK_ID,
	USE_TRANSFORM_DISK_ID,
};
#define SHIFT_DISPLACE_X_DFLT	(10L)
#define	SHIFT_DISPLACE_Y_DFLT	(10L)

#define	SHIFT_BLEND_MIN		0.0f
#define	SHIFT_BLEND_MAX		100.0f
#define	SHIFT_BLEND_DFLT	50.0f 

#define RESTRICT_BOUNDS		0
#define SLIDER_PRECISION	2

typedef struct {
	PF_Fixed	x_offFi;
	PF_Fixed	y_offFi;
	PF_Fixed	blend_valFi;
	PF_ProgPtr	ref;
	PF_SampPB	samp_pb;
	PF_InData	in_data;
	PF_Boolean	no_opB;
} ShiftInfo;

extern "C" {

	DllExport
	PF_Err 
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extraP);

}

#endif // SHIFTER_H