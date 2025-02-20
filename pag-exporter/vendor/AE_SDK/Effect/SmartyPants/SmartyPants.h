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

#ifndef SmartyPants_H
#define SmartyPants_H

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
#include "Smart_Utils.h"
#include "AEGP_SuiteHandler.h"
#include "AEFX_SuiteHelper.h"
#include "SmartyPants_Strings.h"

#ifdef AE_OS_WIN
	#include <Windows.h>
#endif	


/* Versioning information */

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	4
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1

/* Parameter defaults */

#define	SMARTY_AMOUNT_MIN		0f
#define	SMARTY_AMOUNT_MAX		1f
#define	SMARTY_COLOR_BLEND_DFLT	.5f

#define SMARTY_INPUT		0
#define	SMARTY_CHANNEL		1
#define SMARTY_BLEND		2
#define	SMARTY_NUM_PARAMS	3
#define	SMARTY_BLEND_MIN	INT2FIX(0)
#define	SMARTY_BLEND_MAX	(100.0)
#define	SMARTY_BLEND_DFLT	INT2FIX(0)


enum {
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

typedef PF_Err (*PF_RGBtoYIQFunc)(	PF_ProgPtr		effect_ref,		/* reference from in_data */
									PF_Pixel		*rgb,
									PF_YIQ_Pixel	yiq);

typedef PF_Err (*PF_YIQtoRGBFunc)(	PF_ProgPtr		effect_ref,		/* reference from in_data */
									PF_YIQ_Pixel	yiq,
									PF_Pixel		*rgb);

typedef PF_Err (*PF_RGBtoHLSFunc)(	PF_ProgPtr		effect_ref,		/* reference from in_data */
									PF_Pixel		*rgb,
									PF_HLS_Pixel	hls);

typedef PF_Err (*PF_HLStoRGBFunc)(	PF_ProgPtr		effect_ref,		/* reference from in_data */
									PF_HLS_Pixel	hls,
									PF_Pixel		*rgb);

typedef struct {
	A_long		channel;
	PF_Pixel	last, last_to;
	A_short		blend;
	A_Boolean	cache_good;
	A_char		padding;
	A_long		tab[3][3][256];
} SmartyPantsData;

typedef struct {
	SmartyPantsData		*id;
	PF_RGBtoYIQFunc		RGBtoYIQcb;
	PF_YIQtoRGBFunc		YIQtoRGBcb;
	PF_RGBtoHLSFunc		RGBtoHLScb;
	PF_HLStoRGBFunc		HLStoRGBcb;
	PF_InData			*in_data; 
	PF_LayerDef			*outputP;
} SmartyPantsDataPlus;

enum {
	Channel_RGB = 1,
	Channel_RED,
	Channel_GREEN,
	Channel_BLUE,
	PADDING1,
	Channel_HLS,
	Channel_HUE,
	Channel_LIGHTNESS,
	Channel_SATURATION,
	PADDING2,
	Channel_YIQ,
	Channel_LUMINANCE,
	Channel_IN_PHASE_CHROMINANCE,
	Channel_QUADRATURE_CHROMINANCE,
	PADDING3,
	Channel_ALPHA
};

#endif // SmartyPants_H