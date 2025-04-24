/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2015-2023 Adobe Inc.                                  */
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
#ifndef HistoGrid_H
#define HistoGrid_H

#include "AEConfig.h"
#include "A.h"
#include "AE_Effect.h"
#include "AE_EffectSuites.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AEGP_SuiteHandler.h"
#include "AE_EffectUI.h"
#include "AE_Macros.h"
#include "AE_AdvEffectSuites.h"
#include "Param_Utils.h"
#include "AEFX_SuiteHelper.h"
#include "entry.h"
#include "Smart_Utils.h"

#ifdef AE_OS_WIN
	#include <stdlib.h>
	#include <string.h>
#endif


#define	NAME "HistoGrid"
#define DESCRIPTION "\rCopyright 2015-2023 Adobe Inc.\rCustom ECW UI sample."
#define	MAJOR_VERSION		1
#define	MINOR_VERSION		2
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_DEVELOP
#define	BUILD_VERSION		1

#define COLOR_GRID_MAGIC	'BANG'

#define UI_GRID_WIDTH		203
#define UI_GRID_HEIGHT		UI_GRID_WIDTH
#define	UI_GUTTER_TOP		10
#define UI_GUTTER_LEFT		10
#define COLOR_GRID_UI		2323
#define ARB_REFCON			(void*)0xDEADBEEF

#define	HISTOGRID_ARB_MAX_PRINT_SIZE		512


enum {
	CG_INPUT = 0,	// default input layer
	CG_GRID_UI,		// EXAMPLE placeholder for area we are going to draw preview into
	CG_NUM_PARAMS
};


// defines per DaveS.

#define BOXES_ACROSS			10
#define BOXES_DOWN				10
#define BOXES_PER_GRID			(BOXES_ACROSS*BOXES_DOWN)	//Ones based
#define CACHE_ELEMENTS		BOXES_PER_GRID


typedef struct {
	PF_PixelFloat	colorA[CACHE_ELEMENTS];
} CA_ColorCache;

typedef struct {
	PF_Boolean no_opB;
} CG_RenderInfo;

#define CA_MAGIC	'CAsd'

// EXAMPLE. storing preview info in sequence data to prevent flicker
// Note this is only used on the UI thread
typedef struct CA_SeqData {
	int32_t 		magic;
	
	CA_ColorCache	cached_color_preview;
	PF_Boolean		validB;				// whether cached color data for DRAW is valid, otherwise draw blank
	
} CA_SeqData;

extern AEGP_PluginID    G_my_plugin_id;


extern "C" {

	DllExport 
	PF_Err
	EffectMain(
		PF_Cmd				cmd,
		PF_InData			*in_data,
		PF_OutData			*out_data,
		PF_ParamDef			*params[],
		PF_LayerDef			*output, 
		void				*extra);

}

PF_Err
ComputeColorGridFromFrame(
	PF_InData		*in_data,
	PF_EffectWorld* worldP,
	int32_t			skip_pix,
	CA_ColorCache*		color_cacheP);

PF_Err
Deactivate(
		   PF_InData		*in_data,
		   PF_OutData		*out_data,
		   PF_ParamDef		*params[],
		   PF_LayerDef		*output,
		   PF_EventExtra	*extra);


// Custom ECW Events 

void
PFToDrawbotRect(
	const PF_Rect		*in_rectR,
	DRAWBOT_RectF32		*out_rectR);

DRAWBOT_ColorRGBA
QDtoDRAWBOTColor(
	const PF_App_Color	*c);

PF_Err	
AcquireBackgroundColor(	
					   PF_InData			*in_data,
					   PF_OutData			*out_data,
					   PF_ParamDef			*params[],
					   PF_EventExtra		*event_extra,
					   DRAWBOT_ColorRGBA	*drawbot_color);

PF_Boolean
HistoGrid_PointInRect(
	const PF_Point			*point, 
	const PF_Rect				*rectR);

PF_Err
HistoGrid_Get_Box_In_Grid(
	const PF_Point		*origin,		/*>>*/
	A_long				grid_width,		/*>>*/
	A_long				grid_height,	/*>>*/
	A_long				box_across,		/*>>*/
	A_long				box_down,		/*>>*/
	PF_Rect				*box_in_rect);	/*<<*/

PF_Err
HistoGrid_Replace_Color(
	PF_Pixel			*box_color);

PF_Err
HistoGrid_GetNewColor(
	PF_InData		*in_data,
	A_long			hposL,
	A_long			vposL,
	CA_ColorCache		*color_cacheP);

PF_Err ComputeColorGridFromFrame(
	PF_InData		*in_data,
	PF_EffectWorld* worldP,
	CA_ColorCache*	color_cacheP);

PF_Err
DrawEvent(	
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output,
	PF_EventExtra		*event_extra,
	PF_Pixel			some_color);

// Arb Data Handling functions


#endif // HistoGrid_H
