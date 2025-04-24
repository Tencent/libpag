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

/*	Resizer.cpp	

	This sample shows how to resize the output buffer to prevent
	clipping the edges of an effect (or just to scale the source).
	The resizing occurs during the response to PF_Cmd_FRAME_SETUP.

	Revision history

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			(from the mists of time)								dmw		
	1.1			added versioning, link to correct DLL					bbb			9/1/99
	1.2			Added cute about box									bbb			5/3/00
	1.3			Added deep color support, replaced						bbb			6/20/00
				function macros with suite usage.

	2.0			Moved to C++ to use AEGP_SuiteHandler.					bbb			11/6/00
	2.1			Latest greatest											bbb			8/12/04
	2.2			Added new entry point									zal			9/18/2017
	2.3			Remove deprecated 'register' keyword					cb			12/18/2020
	2.4			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

*/

#include "Resizer.h"

static 
PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// Premiere Pro/Elements doesn't support ANSICallbacksSuite1
	if (in_data->appl_id != 'PrMr') {
		suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
												"%s v%d.%d\r%s",
												STR(StrID_Name), 
												MAJOR_VERSION, 
												MINOR_VERSION, 
												STR(StrID_Description));
	}

			return err;
}

static 
PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	out_data->out_flags = 	PF_OutFlag_DEEP_COLOR_AWARE				 |
							PF_OutFlag_I_EXPAND_BUFFER				 |
							PF_OutFlag_I_HAVE_EXTERNAL_DEPENDENCIES;

	out_data->out_flags2 =  PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS |
							PF_OutFlag2_I_USE_3D_CAMERA				 |
							PF_OutFlag2_I_USE_3D_LIGHTS				 |
							PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	
	return PF_Err_NONE;
}

static 
PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);

	PF_ADD_SLIDER(	STR(StrID_Name), 
					RESIZE_AMOUNT_MIN, 
					RESIZE_AMOUNT_MAX, 
					RESIZE_AMOUNT_MIN, 
					RESIZE_AMOUNT_MAX, 
					RESIZE_AMOUNT_DFLT,
					AMOUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_COLOR(	STR(StrID_Color_Param_Name), 
					128,
					255,
					255,
					COLOR_DISK_ID);
	
	AEFX_CLR_STRUCT(def);

	def.u.bd.value = FALSE;		// value for legacy projects which did not have this param

	PF_ADD_CHECKBOX(	STR(StrID_Checkbox_Param_Name), 
						STR(StrID_Checkbox_Description), 
						TRUE, // value for new applications, and when reset
						PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS, 
						DOWNSAMPLE_DISK_ID);

	// Don't expose 3D capabilities in PPro, since they are unsupported
	if (in_data->appl_id != 'PrMr'){
		PF_ADD_CHECKBOX(	STR(StrID_3D_Param_Name), 
							STR(StrID_3D_Param_Description), 
							FALSE,
							0, 
							THREED_DISK_ID);

		out_data->num_params = RESIZE_NUM_PARAMS;
	} else {
		out_data->num_params = RESIZE_NUM_PARAMS - 1;
	}
	
	return err;
}

static 
PF_Err 
FrameSetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	// Output buffer resizing may only occur during PF_Cmd_FRAME_SETUP. 

	PF_FpLong		border_x	=	0, 
					border_y	=	0,	
					border 		=	params[RESIZE_AMOUNT]->u.sd.value;
	
	if (params[RESIZE_DOWNSAMPLE]->u.bd.value) {
		// shrink the border to accomodate decreased resolutions.
		 
		border_x = border * (static_cast<PF_FpLong>(in_data->downsample_x.num) / in_data->downsample_x.den);
		border_y = border * (static_cast<PF_FpLong>(in_data->downsample_y.num) / in_data->downsample_y.den);
	} else 	{
		border_x = border_y = border;
	}
	
	// add 2 times the border width and height to the input width and
	// height to get the output size.

	out_data->width = 2 * static_cast<A_long>(border_x) + params[0]->u.ld.width;
	out_data->height = 2 * static_cast<A_long>(border_y) + params[0]->u.ld.height;

	// The origin of the input buffer corresponds to the (border_x, 
	// border_y) pixel in the output buffer.

	out_data->origin.h = static_cast<A_short>(border_x);
	out_data->origin.v = static_cast<A_short>(border_y);

	return PF_Err_NONE;
}

static 
PF_Err 
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*outputP )
{
	PF_Err		err		= PF_Err_NONE;

	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	A_Matrix4			matrix;
	A_Time				comp_timeT		=	{0,1};

	AEGP_LayerH			effect_layerH	=	NULL, 
						camera_layerH	=	NULL;

	A_FpLong			focal_lengthL	=	0;
	AEGP_CompH			compH			=	NULL;
	AEGP_ItemH			comp_itemH		=	NULL;
	A_long				compwL			=	0, 
						comphL			=	0;

	PF_Point			originPt		=	{0,0};
	PF_Rect				iter_rectR		=	{0,0,0,0};
	PF_Pixel			color			=	{0,0,0,0};
	PF_Pixel16			big_color		=	{0,0,0,0};		
	PF_Rect				src_rectR		=	{0,0,0,0},
						dst_rectR		=	{0,0,0,0};

	PF_FpLong			border_x		=	0,
						border_y		=	0,
						border			=	params[RESIZE_AMOUNT]->u.sd.value;

	PF_Boolean	deepB 			=	PF_WORLD_IS_DEEP(outputP);

	originPt.h 	= static_cast<short>(in_data->output_origin_x);
	originPt.v 	= static_cast<short>(in_data->output_origin_y);
	
	iter_rectR.right 	= 1;
	iter_rectR.bottom 	= (short)outputP->height;
	
	color	= params[RESIZE_COLOR]->u.cd.value;
	
	if (in_data->appl_id != 'PrMr' &&
		params[RESIZE_USE_3D]->u.bd.value) { // if we're paying attention to the camera
		ERR(suites.PFInterfaceSuite1()->AEGP_ConvertEffectToCompTime(in_data->effect_ref,
																	in_data->current_time,
																	in_data->time_scale,
																	&comp_timeT));

		ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectCamera(in_data->effect_ref,
															&comp_timeT,
															&camera_layerH));
		
		if (!err && camera_layerH){
			ERR(suites.LayerSuite5()->AEGP_GetLayerToWorldXform(	camera_layerH,
																&comp_timeT,
																&matrix));
			if (!err){
				AEGP_StreamVal	stream_val;
				AEFX_CLR_STRUCT(stream_val);

				ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue( 	camera_layerH, 
																		AEGP_LayerStream_ZOOM,
																		AEGP_LTimeMode_CompTime,
																		&comp_timeT,
																		FALSE,
																		&stream_val,
																		NULL));
				
				if (!err) {
					focal_lengthL = stream_val.one_d;
					ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(	in_data->effect_ref, &effect_layerH));
				}
				ERR(suites.LayerSuite5()->AEGP_GetLayerParentComp(effect_layerH, &compH));
				ERR(suites.CompSuite4()->AEGP_GetItemFromComp(compH, &comp_itemH));
				ERR(suites.ItemSuite6()->AEGP_GetItemDimensions(comp_itemH, &compwL, &comphL));
				if (!err) {
					A_long num_layersL	=	0L;
					ERR(suites.LayerSuite5()->AEGP_GetCompNumLayers(compH, &num_layersL));
					if (!err && num_layersL) {
						AEGP_LayerH	layerH		=	NULL;
						AEGP_ObjectType	type	=	AEGP_ObjectType_NONE;
						
						for (A_long iL = 0L; !err && (iL < num_layersL) && (type != AEGP_ObjectType_LIGHT); ++iL){
							ERR(suites.LayerSuite5()->AEGP_GetCompLayerByIndex(compH, iL, &layerH));
							if (layerH)	{
								ERR(suites.LayerSuite5()->AEGP_GetLayerObjectType(layerH, &type));
								if (type == AEGP_ObjectType_LIGHT) {
									AEGP_LightType	light_type = AEGP_LightType_AMBIENT;
									ERR(suites.LightSuite2()->AEGP_GetLightType(layerH, &light_type));

									//	At this point, you have the camera, the transform, the layer to which the 
									//	effect is applied, and details about the camera. Have fun!

								}
							}
						}
					}
				}
			}
		}
	}

	/*	The suite functions can't automatically detect the requested
		bit depth. Call different functions based on the bit depth of
		the OUTPUT world.
	*/

	// First draw the border color around the edges where the output has been resized.
	// Premiere Pro/Elements doesn't support FillMatteSuite2,
	// but it does support many of the callbacks in utils
	if (!err){
		if(deepB){
			big_color.red		=	CONVERT8TO16(color.red);
			big_color.green		=	CONVERT8TO16(color.green);
			big_color.blue		=	CONVERT8TO16(color.blue);
			big_color.alpha		=	CONVERT8TO16(color.alpha);
			
			ERR(suites.FillMatteSuite2()->fill16(in_data->effect_ref,
												&big_color,
												NULL,
												outputP));
		} else if (in_data->appl_id != 'PrMr') {
			ERR(suites.FillMatteSuite2()->fill(	in_data->effect_ref,
												&color,
												NULL,
												outputP));
		} else {
			// For PPro, since effects operate on the sequence rectangle, not the clip rectangle, we need to take care to color the proper area
			if (params[RESIZE_DOWNSAMPLE]->u.bd.value) {
				// shrink the border to accomodate decreased resolutions.
				border_x = border * (static_cast<PF_FpLong>(in_data->downsample_x.num) / in_data->downsample_x.den);
				border_y = border * (static_cast<PF_FpLong>(in_data->downsample_y.num) / in_data->downsample_y.den);
			}
			else 	{
				border_x = border_y = border;
			}

			PF_Rect	border_rectR = { 0, 0, 0, 0 };
			border_rectR.left = in_data->pre_effect_source_origin_x;
			border_rectR.top = in_data->pre_effect_source_origin_y;
			border_rectR.right = in_data->pre_effect_source_origin_x + in_data->width + 2 * static_cast<A_long>(border_x);
			border_rectR.bottom = in_data->pre_effect_source_origin_y + in_data->height + 2 * static_cast<A_long>(border_y);

			ERR(PF_FILL(&color,
						&border_rectR,
						outputP));
		}
	}		 

	// Now, copy the input frame
	if (!err){
		dst_rectR.left = originPt.h;
		dst_rectR.top = originPt.v;
		dst_rectR.right = static_cast<short>(originPt.h + params[0]->u.ld.width);
		dst_rectR.bottom = static_cast<short>(originPt.v + params[0]->u.ld.height);

		/*	The suite functions do not automatically detect the requested
			output quality. Call different functions based on the current
			quality state.
		*/

		// Premiere Pro/Elements doesn't support WorldTransformSuite1,
		// but it does support many of the callbacks in utils
		if (PF_Quality_HI == in_data->quality && in_data->appl_id != 'PrMr')	{	
			ERR(suites.WorldTransformSuite1()->copy_hq(	in_data->effect_ref,
														&params[RESIZE_INPUT]->u.ld,
														outputP,
														NULL,
														&dst_rectR));
		} else if (in_data->appl_id != 'PrMr')	{
			ERR(suites.WorldTransformSuite1()->copy(in_data->effect_ref,
													&params[RESIZE_INPUT]->u.ld,
													outputP,
													NULL,
													&dst_rectR));
		} else {
			// For PPro, since effects operate on the sequence rectangle, not the clip rectangle, we need to take care to place the video in the proper area
			if (params[RESIZE_DOWNSAMPLE]->u.bd.value) {
				// shrink the border to accomodate decreased resolutions.
				border_x = border * (static_cast<PF_FpLong>(in_data->downsample_x.num) / in_data->downsample_x.den);
				border_y = border * (static_cast<PF_FpLong>(in_data->downsample_y.num) / in_data->downsample_y.den);
			}
			else 	{
				border_x = border_y = border;
			}

			src_rectR.left = in_data->pre_effect_source_origin_x;
			src_rectR.top = in_data->pre_effect_source_origin_y;
			src_rectR.right = in_data->pre_effect_source_origin_x + in_data->width;
			src_rectR.bottom = in_data->pre_effect_source_origin_y + in_data->height;

			dst_rectR.left = in_data->pre_effect_source_origin_x + static_cast<A_long>(border_x);
			dst_rectR.top = in_data->pre_effect_source_origin_y + static_cast<A_long>(border_y);
			dst_rectR.right = in_data->pre_effect_source_origin_x + in_data->width + static_cast<A_long>(border_x);
			dst_rectR.bottom = in_data->pre_effect_source_origin_y + in_data->height + static_cast<A_long>(border_y);

			ERR(PF_COPY(&params[0]->u.ld,
						outputP,
						&src_rectR,
						&dst_rectR));
		}
	}
	return err;
}

static PF_Err		
DescribeDependencies(	
	PF_InData		*in_data,	
	PF_OutData		*out_data,	
	PF_ParamDef		*params[],	
	void			*extra)	
{
	PF_Err						err		= PF_Err_NONE;
	PF_ExtDependenciesExtra		*extraP = (PF_ExtDependenciesExtra*)extra;
	PF_Handle					msgH	= NULL;
	A_Boolean	something_is_missingB	= FALSE;

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	switch (extraP->check_type) {

		case PF_DepCheckType_ALL_DEPENDENCIES:

			msgH = suites.HandleSuite1()->host_new_handle(strlen(STR(StrID_DependString1)) + 1);
			suites.ANSICallbacksSuite1()->strcpy(reinterpret_cast<char*>(DH(msgH)),STR(StrID_DependString1));
			break;

		case PF_DepCheckType_MISSING_DEPENDENCIES:
			
			if (something_is_missingB)	{
				msgH = suites.HandleSuite1()->host_new_handle(strlen(STR(StrID_DependString2)) + 1);
				suites.ANSICallbacksSuite1()->strcpy(reinterpret_cast<char*>(DH(msgH)),STR(StrID_DependString2));
			}
			break;

		default:
			msgH = suites.HandleSuite1()->host_new_handle(strlen(STR(StrID_NONE)) + 1);
			suites.ANSICallbacksSuite1()->strcpy(reinterpret_cast<char*>(DH(msgH)),STR(StrID_NONE));
			break;

	}
	extraP->dependencies_strH = msgH;
	return err;
}


static PF_Err
QueryDynamicFlags(	
	PF_InData		*in_data,	
	PF_OutData		*out_data,	
	PF_ParamDef		*params[],	
	void			*extra)	
{
	PF_Err 	err 	= PF_Err_NONE,
			err2 	= PF_Err_NONE;

	PF_ParamDef def;

	AEFX_CLR_STRUCT(def);
	
	//	The parameter array passed with PF_Cmd_QUERY_DYNAMIC_FLAGS
	//	contains invalid values; use PF_CHECKOUT_PARAM() to obtain
	//	valid values.

	 ERR(PF_CHECKOUT_PARAM(	in_data, 
							RESIZE_USE_3D, 
							in_data->current_time,
							in_data->time_step,
							in_data->time_scale,
							&def));

	if (!err) {
		if (def.u.bd.value)	{
			out_data->out_flags2 |= PF_OutFlag2_I_USE_3D_LIGHTS;
			out_data->out_flags2 |= PF_OutFlag2_I_USE_3D_CAMERA;
		} else {
			out_data->out_flags2 &= ~PF_OutFlag2_I_USE_3D_LIGHTS;
			out_data->out_flags2 &= ~PF_OutFlag2_I_USE_3D_CAMERA;
		}
	}

	ERR2(PF_CHECKIN_PARAM(in_data, &def));

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
		"Resizer", // Name
		"ADBE Resizer", // Match Name
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
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	PF_Err		err = PF_Err_NONE;
	
	try{
		switch (cmd) {
			case PF_Cmd_ABOUT:
				err = About(in_data,out_data,params,output);
				break;
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(in_data,out_data,params,output);
				break;
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(in_data,out_data,params,output);
				break;
			case PF_Cmd_FRAME_SETUP:
				err = FrameSetup(in_data,out_data,params,output);
				break;
			case PF_Cmd_RENDER:
				err = Render(in_data,out_data,params,output);
				break;
			case PF_Cmd_GET_EXTERNAL_DEPENDENCIES:
				err = DescribeDependencies(in_data,out_data,params,extra);
				break;
			case PF_Cmd_QUERY_DYNAMIC_FLAGS:
				err = QueryDynamicFlags(in_data,out_data,params,extra);
				break;
		}
	}
	catch(const PF_Err &thrown_err) {
		err = thrown_err;
	}
	return err;
}

