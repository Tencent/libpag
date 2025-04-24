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

#ifndef CCU_H
#define CCCU_H

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "Param_Utils.h"
#include "AEFX_SuiteHelper.h"
#include "String_Utils.h"
#include "CCU_Strings.h"
#include <math.h>

#define	MAJOR_VERSION	3
#define	MINOR_VERSION	3
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	0

#define OVAL_PTS		64
#define CCU_SLOP		16

enum 
{
	CCU_INPUT = 0,
	CCU_X_RADIUS,
	CCU_Y_RADIUS,
	CCU_PT,
	CCU_NUM_PARAMS
};

enum 
{
	X_SLIDER_DISK_ID = 1,
	Y_SLIDER_DISK_ID,
	CENTER_DISK_ID
};

enum 
{
	CCU_None = 0,
	CCU_Handles
};

#define	CCU_RADIUS_BIG_MAX	4000
#define	CCU_RADIUS_MAX		250
#define	CCU_RADIUS_MIN 		0
#define	CCU_RADIUS_DFLT		50

#define CCU_PTX_DFLT   		50L
#define CCU_PTY_DFLT   		50L

#define SLIDER_PRECISION	1
#define DISPLAY_FLAGS		0
#define RESTRICT_BOUNDS		0


extern "C" {

	DllExport 
	PF_Err
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extraPV);

}

typedef void (*FrameFunc)(	
	PF_InData*, 
	PF_EventExtra*, 
	PF_Point*, 
	PF_Point*, 
	PF_FixedPoint*);	

PF_Err 
HandleEvent (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extraP);

// Prototypes for custom ECW event handling

void 
FixedFrameFromParams (
					  PF_InData		*in_data,
					  PF_ParamDef	*params[],
					  PF_FixedRect	*frameFiRP);

void 
FrameFromParams (
				 PF_InData			*in_data,
				 PF_ParamDef		*params[],
				 PF_Rect			*frameRP);

void 
Source2FrameRect (
				  PF_InData		*in_data,
				  PF_EventExtra	*event_extraP,
				  PF_FixedRect	*fx_frameRP,
				  PF_FixedPoint	*bounding_boxFiPtAP);

void	
CompFrame2Layer(
				PF_InData		*in_data,	
				PF_EventExtra	*event_extraP,		
				PF_Point		*framePt,		
				PF_Point		*lyrPt,		
				PF_FixedPoint	*fix_lyrFiPt);

void 
Layer2CompFrame (
				 PF_InData		*in_data,	
				 PF_EventExtra	*event_extraP,		
				 PF_Point		*layerPtP,		
				 PF_Point		*framePtP,		
				 PF_FixedPoint	*fix_frameFiPtP);

void	
LayerFrame2Layer(
				 PF_InData		*in_data,	
				 PF_EventExtra	*event_extraP,		
				 PF_Point		*framePtP,	
				 PF_Point		*lyrPtP,	
				 PF_FixedPoint	*fix_lyrPtP);

void 
Layer2LayerFrame (
				  PF_InData		*in_data,	
				  PF_EventExtra	*event_extraP,		
				  PF_Point		*layerPtP,		
				  PF_Point		*framePtP,		
				  PF_FixedPoint	*fix_frameFiPtP);

void 
DrawHandles (
			 PF_InData		*in_data,
			 PF_OutData		*out_data,
			 PF_ParamDef	*params[],
			 PF_EventExtra	*event_extraP);

PF_Err 
DrawEvent (
		   PF_InData		*in_data,
		   PF_OutData		*out_data,
		   PF_ParamDef		*params[],
		   PF_LayerDef		*output,
		   PF_EventExtra	*event_extraP);

PF_Boolean 
DoClickHandles (
				PF_InData		*in_data,
				PF_Rect			*frameRP,					
				PF_ParamDef		*params[],
				FrameFunc		frame_func,
				PF_EventExtra	*event_extraP);

PF_Boolean	
DoDragHandles(
			  PF_InData		*in_data,
			  PF_OutData	*out_data,
			  PF_ParamDef	*params[],
			  FrameFunc		frame_func,
			  PF_EventExtra	*event_extraP);

PF_Err 
DoClick (
		 PF_InData		*in_data,
		 PF_OutData		*out_data,
		 PF_ParamDef	*params[],
		 PF_LayerDef	*output,
		 PF_EventExtra	*event_extraP);

PF_Err	
DoDrag(
	   PF_InData		*in_data,
	   PF_OutData		*out_data,
	   PF_ParamDef		*params[],
	   PF_LayerDef		*output,
	   PF_EventExtra	*event_extraP);

#endif // CCU_H
