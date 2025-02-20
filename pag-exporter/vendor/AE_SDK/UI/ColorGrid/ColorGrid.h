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
#ifndef ColorGrid_H
#define ColorGrid_H

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
#include "AEFX_ArbParseHelper.h"
#include "Param_Utils.h"
#include "AEFX_SuiteHelper.h"
#include "entry.h"
#include "Smart_Utils.h"

#ifdef AE_OS_WIN
	#include <stdlib.h>
	#include <string.h>
#endif


#define	NAME "ColorGrid"
#define DESCRIPTION "\rCopyright 2007-2023 Adobe Inc.\rArbitrary data and Custom UI sample."
#define	MAJOR_VERSION		3
#define	MINOR_VERSION		3
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_DEVELOP
#define	BUILD_VERSION		1

#define COLOR_GRID_MAGIC	'BANG'

#define UI_GRID_WIDTH		203
#define UI_GRID_HEIGHT		UI_GRID_WIDTH
#define	UI_GUTTER_TOP		10
#define UI_GUTTER_LEFT		10
#define COLOR_GRID_UI		2323
#define ARB_REFCON			(void*)0xDEADBEEFDEADBEEF

#define	COLORGRID_ARB_MAX_PRINT_SIZE		512


enum {
	CG_INPUT = 0,	// default input layer 
	CG_GRID_UI,		
	CG_NUM_PARAMS
};

// defines per DaveS.

#define BOXES_ACROSS			3
#define BOXES_DOWN				3
#define BOXES_PER_GRID			(BOXES_ACROSS*BOXES_DOWN)	//Ones based
#define CG_ARBDATA_ELEMENTS		BOXES_PER_GRID				


typedef struct {
	PF_PixelFloat	colorA[CG_ARBDATA_ELEMENTS];
} CG_ArbData;

typedef struct {
	PF_Boolean no_opB;
} CG_RenderInfo;


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
CreateDefaultArb(	
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ArbitraryH		*dephault);

PF_Err
Deactivate(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*extra);

PF_Err
Arb_Print_Size();

PF_Err
Arb_Print(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ArbPrintFlags	print_flags,
	PF_ArbitraryH		arbH,
	A_u_long			print_sizeLu,
	A_char				*print_bufferPC);
		
PF_Err
Arb_Scan(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	void 				*refconPV,
	const char			*bufPC,		
	unsigned long		bytes_to_scanLu,
	PF_ArbitraryH		*arbPH);

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
ColorGrid_PointInRect(
	const PF_Point			*point, 
	const PF_Rect				*rectR);

PF_Err
ColorGrid_Get_Box_In_Grid(
	const PF_Point		*origin,		/*>>*/
	A_long				grid_width,		/*>>*/
	A_long				grid_height,	/*>>*/
	A_long				box_across,		/*>>*/
	A_long				box_down,		/*>>*/
	PF_Rect				*box_in_rect);	/*<<*/

PF_Err
ColorGrid_Replace_Color(
	PF_Pixel			*box_color);

PF_Err
ColorGrid_GetNewColor(
	PF_InData		*in_data,
	A_long			hposL,
	A_long			vposL,
	CG_ArbData		*arbP);

PF_Err 
DoClick(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output,
	PF_EventExtra		*event_extra);

PF_Err 
DrawEvent(	
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output,
	PF_EventExtra		*event_extra,
	PF_Pixel			some_color);

PF_Err 
ChangeCursor(	
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output,
	PF_EventExtra		*event_extra);

// Arb Data Handling functions


PF_Err	
CreateDefaultArb(	
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ArbitraryH		*dephault);

PF_Err
Arb_Copy(	
	PF_InData				*in_data,
	PF_OutData				*out_data,
	const PF_ArbitraryH		*srcP,
	PF_ArbitraryH			*dstP);

PF_Err
Arb_Interpolate(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	double					itrp_amtF,
	const PF_ArbitraryH		*l_arbP,
	const PF_ArbitraryH		*r_arbP,
	PF_ArbitraryH			*result_arbP);

PF_Err
Arb_Compare(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	const PF_ArbitraryH		*a_arbP,
	const PF_ArbitraryH		*b_arbP,
	PF_ArbCompareResult		*resultP);

void
ColorGrid_InterpPixel(
	PF_FpLong		intrp_amtF,
	PF_PixelFloat	*lpix,
	PF_PixelFloat	*rpix,
	PF_PixelFloat	*headP);

#endif // ColorGrid_H