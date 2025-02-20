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

#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "AEConfig.h"
#ifdef AE_OS_WIN
	#include <windows.h>
	#include <wingdi.h>
	#include "stdlib.h"
#endif

#define PF_DEEP_COLOR_AWARE 1

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "AE_EffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AE_AdvEffectSuites.h"
#include "AEFX_SuiteHelper.h"
#include "Smart_Utils.h"
#include "Param_Utils.h"
#include "String_Utils.h"
#include "Supervisor_Strings.h"
#include "AEGP_SuiteHandler.h"

#define	MAJOR_VERSION		5
#define	MINOR_VERSION		10
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_DEVELOP
#define	BUILD_VERSION		1

typedef enum {
	MODE_BASIC = 1,
	MODE_ADVANCED,
	MODE_SIZE = MODE_ADVANCED
} current_mode;

typedef enum {
	FLAVOR_CHOCOLATE = 1,
	FLAVOR_FILLER1,
	FLAVOR_STRAWBERRY,
	FLAVOR_FILLER2,
	FLAVOR_SHERBET,
	FLAVOR_SIZE = FLAVOR_SHERBET
} current_flavor;

typedef struct {
	PF_Boolean		initializedB;
	AEGP_PluginID	my_id;
} my_global_data, *my_global_dataP, **my_global_dataH;

typedef struct {
	PF_State		state;
	PF_Boolean		advanced_modeB;
} my_sequence_data, *my_sequence_dataP, **my_sequence_dataH;

typedef struct {
	PF_PixelFloat	color;
} prerender_stuff, *pre_render_stuffP, **pre_render_stuffH;

enum {
	SUPER_INPUT = 0,
	SUPER_MODE,
	SUPER_FLAVOR,
	SUPER_COLOR,
	SUPER_SLIDER,
	SUPER_CHECKBOX,
	SUPER_NUM_PARAMS
};

enum {
	SUPER_MODE_DISK_ID = 1,
	SUPER_FLAVOR_DISK_ID,
	SUPER_GROUP_START_DISK_ID,
	SUPER_COLOR_DISK_ID,
	SUPER_SLIDER_DISK_ID,
	SUPER_CHECKBOX_DISK_ID,
	SUPER_GROUP_END_DISK_ID
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
#endif  // SUPERVISOR_H
