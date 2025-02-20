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

#ifndef SDK_BACKWARDS_H
#define SDK_BACKWARDS_H

#include "AEConfig.h"

#ifdef AE_OS_WIN
	#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "A.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"


#define	NAME "SDK_Backwards"
#define DESCRIPTION	"A sample audio manipulation plug-in.\n Copyright 2007-2023 Adobe Inc."

#define	MAJOR_VERSION		1
#define	MINOR_VERSION		5
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_DEVELOP
#define	BUILD_VERSION		0

enum {
	AR_INPUT = 0,		// default input layer
	AR_FREQ,			// frequency slider
	AR_LEVEL,			// level slider
	AR_NUM_PARAMS
};

enum {
	FREQ_DISK_ID = 1,
	LEVEL_DISK_ID
};
 // Settings for tone rate

#define	AR_FREQ_MIN							0.0f
#define	AR_FREQ_MAX							4000.0f
#define	AR_FREQ_VALID_MAX					20020.0f
#define	AR_FREQ_DFLT						1000.0f
#define	AR_FREQ_CURVE_TOLER					AEFX_AUDIO_DEFAULT_CURVE_TOLERANCE
#define AR_FREQ_DISPLAY_PRECISION			1
#define	AR_FREQ_DISPLAY_FLAGS				0

 // Settings for tone level

#define	AR_LEVEL_MIN						0.0f
#define	AR_LEVEL_MAX						1.0f
#define	AR_LEVEL_VALID_MAX					1.0f
#define	AR_LEVEL_DFLT						0.5f
#define	AR_LEVEL_CURVE_TOLER				AEFX_AUDIO_DEFAULT_CURVE_TOLERANCE
#define AR_LEVEL_DISPLAY_PRECISION			1
#define	AR_LEVEL_DISPLAY_FLAGS				1

//  MacOS
#define 	SND_RATE_22K			0x56ee8ba3			/* FIXED POINT!	*/
#define 	SND_RATE_11K			0x2b7745d1
#define 	SND_RATE_7K			0x1cfa2e8b
#define 	SND_RATE_5K			0x15bba2e8
#define 	SND_RATE_DEFAULT		SND_RATE_11K

// Rest of World...
#define 	SND_RATE_8000			0x1f400000
#define 	SND_RATE_11025			0x2b110000
#define 	SND_RATE_16000			0x3e800000
#define 	SND_RATE_22050			0x56220020
#define 	SND_RATE_32001			0x7d000000
#define 	SND_RATE_44100			0xac440000

extern "C" {

	DllExport
	PF_Err 
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output);

}

static A_long 
RoundDouble(
	PF_FpLong 	x);

#endif // SDK_BACKWARDS_H