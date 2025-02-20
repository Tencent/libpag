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

/*	Transformer.cpp	

	This sample exercises some of After Effects' image processing
	callback functions.

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Seemed like a good idea at the time.					bbb			2/27/2002
	2.0			lots of changes? let's bump the major.					bbb			2/20/2003
	2.0.1		Add a check before disposing an EffectWorld				zal			11/25/2014
	2.1			Added new entry point									zal			9/18/2017
	2.2			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

*/

#include "Transformer.h"


static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s",
											STR(StrID_Name), 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											STR(StrID_Description));
	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	/*	We do very little here. If we'd allocated
		anything, we'd need to create a response
		to PF_Cmd_GLOBAL_SETDOWN, but since we 
		don't, we won't.
	*/
		
	out_data->my_version 	= 	PF_VERSION(	MAJOR_VERSION, 
											MINOR_VERSION,
											BUG_VERSION, 
											STAGE_VERSION, 
											BUILD_VERSION);

	out_data->out_flags 	= 	PF_OutFlag_DEEP_COLOR_AWARE		|
								PF_OutFlag_PIX_INDEPENDENT		|
								PF_OutFlag_USE_OUTPUT_EXTENT;

	out_data->out_flags2 	=	PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG	|
								PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	
	return PF_Err_NONE;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);

	PF_ADD_FIXED(	STR(StrID_ColorBlendSlider_Name),
					XFORM_AMOUNT_MIN,
					XFORM_AMOUNT_MAX,
					XFORM_AMOUNT_MIN,
					XFORM_AMOUNT_MAX,
					XFORM_COLOR_BLEND_DFLT,					
					0,
					PF_ValueDisplayFlag_PERCENT,
					0,
					COLORBLEND_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_COLOR(	STR(StrID_Color_Param_Name), 
					128,
					255,
					255,
					COLOR_DISK_ID);
	
	AEFX_CLR_STRUCT(def);
	
	def.flags 	= 	PF_ParamFlag_START_COLLAPSED;
	
	PF_ADD_TOPIC(STR(StrID_Topic_Name), TOPIC_DISK_ID);
	
	AEFX_CLR_STRUCT(def);

	PF_ADD_FIXED(	STR(StrID_LayerBlendSlider_Name),
					XFORM_AMOUNT_MIN,
					XFORM_AMOUNT_MAX,
					XFORM_AMOUNT_MIN,
					XFORM_AMOUNT_MAX,
					XFORM_AMOUNT_MAX,
					0,
					PF_ValueDisplayFlag_PERCENT,
					0,
					LAYERBLEND_DISK_ID);

	AEFX_CLR_STRUCT(def);
	
	PF_ADD_LAYER(	STR(StrID_Layer_Param_Name),
					PF_LayerDefault_MYSELF,
					LAYER_DISK_ID);
					
	AEFX_CLR_STRUCT(def);
	
	PF_END_TOPIC(END_TOPIC_DISK_ID);
	
	out_data->num_params = XFORM_NUM_PARAMS;

	return err;
}

static PF_Err
Copy8BitPixelTo16BitPixel(
	const PF_Pixel	*eightP,
	PF_Pixel16		*sixteenP)
{
	PF_Err	err	=	PF_Err_NONE;
	
	if (!eightP || !sixteenP)	{
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	
	if (!err) {
		sixteenP->red		=	CONVERT8TO16(eightP->red);
		sixteenP->green		=	CONVERT8TO16(eightP->green);
		sixteenP->blue		=	CONVERT8TO16(eightP->blue);
		sixteenP->alpha		=	CONVERT8TO16(eightP->alpha);
	}
	return err;
}

static PF_Err 
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	PF_Err				err		= 	PF_Err_NONE,	
						err2	=	PF_Err_NONE;	
						
	PF_Pixel16			bigpix	=	{0,0,0,0};

	A_long				widthL	=	output->width,
						heightL	=	output->height;
	
	PF_NewWorldFlags	flags	=	PF_NewWorldFlag_CLEAR_PIXELS;
	
	PF_EffectWorld		color_world;
	
	PF_Boolean			deepB			=	PF_WORLD_IS_DEEP(output),
						two_sourcesB	=	FALSE;
	
	PF_ParamDef			temp_param;
	
	AEFX_CLR_STRUCT(temp_param);	
	AEFX_CLR_STRUCT(color_world);	
	
	/*	We're going to blend the input with the color. If 
		the user has picked an additional layer, we'll blend 
		it in too, giving it a 'special' look along the way.	
	*/
	
	if (PF_Quality_HI == in_data->quality) {
		ERR(suites.WorldTransformSuite1()->copy_hq(	in_data->effect_ref,
													&params[XFORM_INPUT]->u.ld,
													output,
													&params[XFORM_INPUT]->u.ld.extent_hint,
													&output->extent_hint));
	} else {
		ERR(suites.WorldTransformSuite1()->copy(	in_data->effect_ref,
													&params[XFORM_INPUT]->u.ld,
													output,
													&params[XFORM_INPUT]->u.ld.extent_hint,
													&output->extent_hint));
	}	
	
	ERR(PF_ABORT(in_data));

	/*	Make an offscreen world. If the user has selected an
		extra input layer, we'll size to that (so we can use
		the blend callback, which requires input PF_Worlds of 
		the same dimensions), of the appropriate pixel depth.	
	*/
	
	if (deepB) {
		flags	|=	PF_NewWorldFlag_DEEP_PIXELS;
	}
	
	ERR(PF_CHECKOUT_PARAM(	in_data,
							XFORM_LAYER,
							in_data->current_time,
							in_data->time_step,
							in_data->time_scale,
							&temp_param));
	ERR(PF_ABORT(in_data));
	
	if (!err) {
		if (temp_param.u.ld.data) {
			two_sourcesB	=	TRUE;
			widthL			=	temp_param.u.ld.width;
			heightL			=	temp_param.u.ld.height;		
		} else {
			widthL	=	output->width;
			heightL	=	output->height;
		}

		ERR(suites.WorldSuite1()->new_world(	in_data->effect_ref,
												widthL,
												heightL,
												flags,
												&color_world));

		ERR(PF_ABORT(in_data));
	
		if (deepB) {
			ERR(Copy8BitPixelTo16BitPixel(&params[XFORM_COLOR]->u.cd.value, &bigpix));

			ERR(suites.FillMatteSuite2()->fill16(in_data->effect_ref,
												&bigpix,
												NULL,
												&color_world));
		} else {
			ERR(suites.FillMatteSuite2()->fill(	in_data->effect_ref,
												&params[XFORM_COLOR]->u.cd.value,
												NULL,
												&color_world));
		}

		ERR(PF_ABORT(in_data));


		if (two_sourcesB) {

			ERR(suites.WorldTransformSuite1()->blend(in_data->effect_ref,
													&temp_param.u.ld,
													&color_world,
													params[XFORM_LAYERBLEND]->u.fd.value,
													&color_world));
		} else 	{
			ERR(suites.WorldTransformSuite1()->blend(in_data->effect_ref,
													&params[XFORM_INPUT]->u.ld,
													&color_world,
													params[XFORM_COLORBLEND]->u.fd.value,
													output));
		}

		ERR(PF_ABORT(in_data));

		if (!err) {		

			//	Blend the color layer () with the output	

			PF_CompositeMode	mode;
			
			AEFX_CLR_STRUCT(mode);
			
			mode.xfer		=	PF_Xfer_DIFFERENCE;
			mode.opacity	=	static_cast<A_u_char>(FIX2INT_ROUND(params[XFORM_LAYERBLEND]->u.fd.value / PF_MAX_CHAN8));
			mode.rgb_only	=	TRUE;

			PF_FpLong			opacityF	= FIX_2_FLOAT(params[XFORM_LAYERBLEND]->u.fd.value);

			mode.opacity	= static_cast<A_u_char>((opacityF 	* PF_MAX_CHAN8) 	+ 0.5);
			mode.opacitySu	= static_cast<A_u_short>((opacityF 	* PF_MAX_CHAN16) 	+ 0.5);

				if (mode.opacitySu 	> PF_MAX_CHAN16) {
				mode.opacitySu 	= PF_MAX_CHAN16;
			}

			ERR(suites.WorldTransformSuite1()->transfer_rect(in_data->effect_ref,
															in_data->quality,
															PF_MF_Alpha_STRAIGHT,
															in_data->field,
															&color_world.extent_hint,
															&color_world,
															&mode,
															NULL,
															(A_long)suites.ANSICallbacksSuite1()->fabs(output->extent_hint.right 	- color_world.extent_hint.right ) / 2,
															(A_long)suites.ANSICallbacksSuite1()->fabs(output->extent_hint.bottom - color_world.extent_hint.bottom ) / 2,
															output));
		}
	}		
	
	ERR(PF_ABORT(in_data));

	if (color_world.data){			// if there's a data pointer, we allocated something
		ERR2(suites.WorldSuite1()->dispose_world(in_data->effect_ref, &color_world));
	}

	// Always check in, no matter what the error condition!
	ERR2(PF_CHECKIN_PARAM(in_data, &temp_param));
													
	return err;
	
}


extern "C" DllExport
PF_Err PluginDataEntryFunction2(
	PF_PluginDataPtr inPtr,
	PF_PluginDataCB2 inPluginDataCallBackPtr,
	SPBasicSuite* inSPBasicSuitePtr,
	const char* inHostName,
	const char* inHostVersion)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;

	result = PF_REGISTER_EFFECT_EXT2(
		inPtr,
		inPluginDataCallBackPtr,
		"Transformer", // Name
		"ADBE Transformer", // Match Name
		"Sample Plug-ins", // Category
		AE_RESERVED_INFO, // Reserved Info
		"EffectMain",	// Entry point
		"https://www.adobe.com");	// support URL

	return result;
}


PF_Err
EffectMain(
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try	{
		switch (cmd) {
			case PF_Cmd_ABOUT:

				ERR(About(	in_data,
							out_data,
							params,
							output));
				break;

			case PF_Cmd_GLOBAL_SETUP:

				ERR(GlobalSetup(in_data,
								out_data,
								params,
								output));
				break;

			case PF_Cmd_PARAMS_SETUP:

				ERR(ParamsSetup(in_data,
								out_data,
								params,
								output));
				break;

			case PF_Cmd_RENDER:

				ERR(Render(in_data,
							out_data,
							params,
							output));
				break;
		}
	} catch(PF_Err &thrown_err) {
		err = thrown_err;
	}
	return err;
}
