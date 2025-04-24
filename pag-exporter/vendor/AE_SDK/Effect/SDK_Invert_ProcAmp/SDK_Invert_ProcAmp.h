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
#ifndef SDK_Invert_ProcAmp_H
#define SDK_Invert_ProcAmp_H

#include "SDK_Invert_ProcAmp_Kernel.cl.h"
#include "AEConfig.h"
#include "entry.h"
#include "AEFX_SuiteHelper.h"
#include "PrSDKAESupport.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectGPUSuites.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "String_Utils.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"


#if _WIN32
#include <CL/cl.h>
#define HAS_METAL 0
#else
#include <OpenCL/cl.h>
#define HAS_METAL 1
#include <Metal/Metal.h>
#include "SDK_Invert_ProcAmp_Kernel.metal.h"
#endif
#include <math.h>

#ifdef AE_OS_WIN
	#include <Windows.h>
#endif

#define DESCRIPTION	"\nCopyright 2018-2023 Adobe Inc.\rSample Invert ProcAmp effect."

#define NAME			"SDK_Invert_ProcAmp"
#define	MAJOR_VERSION	1
#define	MINOR_VERSION	1
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


enum {
	SDK_INVERT_PROCAMP_INPUT=0,
	SDK_INVERT_PROCAMP_BRIGHTNESS,
	SDK_INVERT_PROCAMP_CONTRAST,
	SDK_INVERT_PROCAMP_HUE,
	SDK_INVERT_PROCAMP_SATURATION,
	SDK_INVERT_PROCAMP_NUM_PARAMS
};


#define	BRIGHTNESS_MIN_VALUE		-100
#define	BRIGHTNESS_MAX_VALUE		100
#define	BRIGHTNESS_MIN_SLIDER		-100
#define	BRIGHTNESS_MAX_SLIDER		100
#define	BRIGHTNESS_DFLT				0

#define	CONTRAST_MIN_VALUE			0
#define	CONTRAST_MAX_VALUE			200
#define	CONTRAST_MIN_SLIDER			0
#define	CONTRAST_MAX_SLIDER			200
#define	CONTRAST_DFLT				100

#define	HUE_DFLT					0

#define	SATURATION_MIN_VALUE		0
#define	SATURATION_MAX_VALUE		200
#define	SATURATION_MIN_SLIDER		0
#define	SATURATION_MAX_SLIDER		200
#define	SATURATION_DFLT				100


extern "C" {

	DllExport 
	PF_Err
	EffectMain (	
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extra);

}

#if HAS_METAL
	/*
	 ** Plugins must not rely on a host autorelease pool.
	 ** Create a pool if autorelease is used, or Cocoa convention calls, such as Metal, might internally autorelease.
	 */
	struct ScopedAutoreleasePool
	{
		ScopedAutoreleasePool()
		:  mPool([[NSAutoreleasePool alloc] init])
		{
		}
	
		~ScopedAutoreleasePool()
		{
			[mPool release];
		}
	
		NSAutoreleasePool *mPool;
	};
#endif 

typedef struct
{
	float mBrightness;
	float mContrast;
	float mHueCosSaturation;
	float mHueSinSaturation;
} InvertProcAmpParams;

#endif // SDK_Invert_ProcAmp_H
