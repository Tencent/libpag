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
/*
	ColorGrid.cpp
	
	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			created by												eks			10/1/1999
	2.0	 		various bugfixes...										bbb			1/4/2003
	3.0			updated to C++											bbb			8/21/2003
	3.1			handrails, training wheels								bbb			3/15/2004
	3.2			Added new entry point									zal			9/18/2017
	3.3			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

*/


#include "ColorGrid.h"

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err					err	= PF_Err_NONE;


	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg, 
								"%s, v%d.%d\r%s",
								NAME, 
								MAJOR_VERSION, 
								MINOR_VERSION, 
								DESCRIPTION);
	return err;
}

static PF_Err 
GlobalSetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err	err				= PF_Err_NONE;

	out_data->my_version	= PF_VERSION(	MAJOR_VERSION, 
											MINOR_VERSION,
											BUG_VERSION, 
											STAGE_VERSION, 
											BUILD_VERSION);

	out_data->out_flags  |= PF_OutFlag_CUSTOM_UI			    |
							PF_OutFlag_USE_OUTPUT_EXTENT	    |
							PF_OutFlag_PIX_INDEPENDENT		    |
							PF_OutFlag_DEEP_COLOR_AWARE;
							
    out_data->out_flags2 |= PF_OutFlag2_FLOAT_COLOR_AWARE	    |
							PF_OutFlag2_SUPPORTS_SMART_RENDER   |
                            PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

	return err;
}

static PF_Err 
ColorGrid_ColorizePixelFloat(
	void			*refcon,
	A_long			x, 
	A_long			y, 
	PF_PixelFloat	*inP, 
	PF_PixelFloat	*outP)
{
	PF_Err		err = PF_Err_NONE;
	PF_PixelFloat	*current_color = reinterpret_cast<PF_PixelFloat*>(refcon);
	
	outP->red	= (current_color->red 	+ inP->red) 	/ 2;
	outP->green	= (current_color->green + inP->green)	/ 2;
	outP->blue	= (current_color->blue 	+ inP->blue) 	/ 2;
	outP->alpha	= inP->alpha;
	
	return err;
}

static PF_Err 
ColorGrid_ColorizePixel16(
						void			*refcon,
						A_long			x, 
						A_long			y, 
						PF_Pixel16		*inP, 
						PF_Pixel16		*outP)
{
	PF_Err		err = PF_Err_NONE;
	PF_Pixel16	*current_color = reinterpret_cast<PF_Pixel16*>(refcon);
	
	outP->red	= (current_color->red 	+ inP->red) 	/ 2;
	outP->green	= (current_color->green + inP->green)	/ 2;
	outP->blue	= (current_color->blue 	+ inP->blue) 	/ 2;
	outP->alpha	= inP->alpha;
	
	return err;
}

static PF_Err 
ColorGrid_ColorizePixel8(
						void			*refcon,
						A_long			x, 
						A_long			y, 
						PF_Pixel8		*inP, 
						PF_Pixel8		*outP)
{
	PF_Err		err = PF_Err_NONE;
	PF_Pixel8	*current_color = reinterpret_cast<PF_Pixel8*>(refcon);
	
	outP->red	= (current_color->red 	+ inP->red) 	/ 2;
	outP->green	= (current_color->green + inP->green)	/ 2;
	outP->blue	= (current_color->blue 	+ inP->blue) 	/ 2;
	outP->alpha	= inP->alpha;
	
	return err;
}



static PF_Err 
ParamsSetup (
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output)
{
	PF_Err			err = PF_Err_NONE;
	PF_ParamDef		def;	

	AEFX_CLR_STRUCT(def);

	ERR(CreateDefaultArb(	in_data, 
							out_data, 
							&def.u.arb_d.dephault));

	PF_ADD_ARBITRARY2(	"Color Grid", 
						UI_GRID_WIDTH,
						UI_GRID_HEIGHT,
						0,
						PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL,
						def.u.arb_d.dephault, 
						COLOR_GRID_UI,
						ARB_REFCON);

	if (!err) {
		PF_CustomUIInfo			ci;
		
		ci.events				= PF_CustomEFlag_EFFECT;
 		
		ci.comp_ui_width		= ci.comp_ui_height = 0;
		ci.comp_ui_alignment	= PF_UIAlignment_NONE;
		
		ci.layer_ui_width		= 
		ci.layer_ui_height		= 0;
		ci.layer_ui_alignment	= PF_UIAlignment_NONE;
		
		ci.preview_ui_width		= 
		ci.preview_ui_height	= 0;
		ci.layer_ui_alignment	= PF_UIAlignment_NONE;

		err = (*(in_data->inter.register_ui))(in_data->effect_ref, &ci);
		out_data->num_params = CG_NUM_PARAMS;
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
	PF_Err			err				= PF_Err_NONE;	
	PF_PixelFloat	*current_colorP	= NULL;
	PF_Handle		arbH			= params[CG_GRID_UI]->u.arb_d.value;
	CG_ArbData		*arbP			= NULL;
	PF_Point		origin			= {0,0};
	A_u_short		iSu				= 0;
	PF_Rect			current_rectR	= {0,0,0,0};
	A_long			box_acrossL		= 0,
					box_downL		= 0,
					progress_baseL	= 0,
					progress_finalL	= BOXES_PER_GRID;
	

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	arbP = reinterpret_cast<CG_ArbData*>(suites.HandleSuite1()->host_lock_handle(arbH));
	current_colorP = reinterpret_cast<PF_PixelFloat*>(arbP);

	origin.h = (A_short)in_data->pre_effect_source_origin_x;
	origin.v = (A_short)in_data->pre_effect_source_origin_y;

	/* 
		This section uses the pre-effect extent hint, since it wants
		to only be applied to the source layer material, and NOT to any
		resized effect area. Example: User applies "Resizer" to a layer
		before using ColorGrid. The effect makes the output area larger
		than the source footage. ColorGrid will look at the pre-effect
		extent width and height to determine what the relative coordinates
		are for the source material inside the params[0] (the layer).
	*/

	for(iSu = 0; !err && iSu < BOXES_PER_GRID; ++iSu){
		if(box_acrossL == BOXES_ACROSS)	{
			box_downL++;
			box_acrossL = 0;
		}
		ColorGrid_Get_Box_In_Grid(	&origin,
									in_data->width * in_data->downsample_x.num / in_data->downsample_x.den,
									in_data->height * in_data->downsample_y.num / in_data->downsample_y.den,
									box_acrossL,
									box_downL,
									&current_rectR);

		PF_Pixel8 temp8;
		
		temp8.red		= static_cast<A_u_char>(current_colorP->red		* PF_MAX_CHAN8);
		temp8.green		= static_cast<A_u_char>(current_colorP->green	* PF_MAX_CHAN8);
		temp8.blue		= static_cast<A_u_char>(current_colorP->blue	* PF_MAX_CHAN8);

		progress_baseL++;

		suites.Iterate8Suite2()->iterate(	in_data,
											progress_baseL,
											progress_finalL,
											&params[CG_INPUT]->u.ld,
											&current_rectR,
											reinterpret_cast<void*>(&temp8),
											ColorGrid_ColorizePixel8,
											output);

		current_colorP++;
		box_acrossL++;
	}
	PF_UNLOCK_HANDLE(arbH);
	return err;
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err err = PF_Err_NONE;

	PF_ParamDef arb_param;
	
	PF_RenderRequest req = extra->input->output_request;
	PF_CheckoutResult in_result;
	
	AEFX_CLR_STRUCT(arb_param);
	
	ERR(PF_CHECKOUT_PARAM(	in_data, 
							CG_GRID_UI, 
							in_data->current_time, 
							in_data->time_step, 
							in_data->time_scale, 
							&arb_param));

	ERR(extra->cb->checkout_layer(	in_data->effect_ref,
									CG_INPUT,
									CG_INPUT,
									&req,
									in_data->current_time,
									in_data->time_step,
									in_data->time_scale,
									&in_result));
	
	if (!err){
		UnionLRect(&in_result.result_rect, 		&extra->output->result_rect);
		UnionLRect(&in_result.max_result_rect, 	&extra->output->max_result_rect);	
	}
	ERR(PF_CHECKIN_PARAM(in_data, &arb_param));
	return err;
}

static PF_Err
SmartRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_SmartRenderExtra		*extra)
{
	PF_Err				err		= PF_Err_NONE,
						err2	= PF_Err_NONE;
	
	PF_EffectWorld		*input_worldP	= NULL, 
						*output_worldP  = NULL;
	PF_WorldSuite2		*wsP			= NULL;
	PF_PixelFormat		format			= PF_PixelFormat_INVALID;
	PF_Point			origin			= {0,0};
	A_u_short			iSu				= 0;
	PF_Rect				current_rectR	= {0,0,0,0};
	A_long				box_acrossL		= 0,
						box_downL		= 0;

	
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	CG_ArbData		*arbP			= NULL; 
	PF_PixelFloat	*current_colorP = NULL;

	PF_ParamDef arb_param;
	AEFX_CLR_STRUCT(arb_param);
	
	ERR(PF_CHECKOUT_PARAM(	in_data, 
							CG_GRID_UI, 
							in_data->current_time, 
							in_data->time_step, 
							in_data->time_scale, 
							&arb_param));
	
	if (!err){
		arbP = reinterpret_cast<CG_ArbData*>(*arb_param.u.arb_d.value);
		if (arbP){
			current_colorP = reinterpret_cast<PF_PixelFloat*>(arbP);
		}
	}
	ERR((extra->cb->checkout_layer_pixels(	in_data->effect_ref, CG_INPUT, &input_worldP)));
	
	ERR(extra->cb->checkout_output(	in_data->effect_ref, &output_worldP));
	
	if (!err){
		for(iSu = 0; !err && iSu < BOXES_PER_GRID; ++iSu){
			if(box_acrossL == BOXES_ACROSS)	{
				box_downL++;
				box_acrossL = 0;
			}
			ColorGrid_Get_Box_In_Grid(	&origin,
										in_data->width * in_data->downsample_x.num / in_data->downsample_x.den,
										in_data->height * in_data->downsample_y.num / in_data->downsample_y.den,
										box_acrossL,
										box_downL,
										&current_rectR);

			if (!err && output_worldP){
				ERR(AEFX_AcquireSuite(	in_data, 
										out_data, 
										kPFWorldSuite, 
										kPFWorldSuiteVersion2, 
										"Couldn't load suite.",
										(void**)&wsP));
				
				ERR(wsP->PF_GetPixelFormat(input_worldP, &format));
				
				origin.h = (A_short)(in_data->output_origin_x);
				origin.v = (A_short)(in_data->output_origin_y);
				
				if (!err){
					switch (format) {
						
						case PF_PixelFormat_ARGB128:
							
							ERR(suites.IterateFloatSuite2()->iterate_origin(in_data,
																			0,
																			output_worldP->height,
																			input_worldP,
																			&current_rectR,
																			&origin,
																			reinterpret_cast<void*>(current_colorP),
																			ColorGrid_ColorizePixelFloat,
																			output_worldP));
							break;
							
						case PF_PixelFormat_ARGB64:
							
							PF_Pixel16 temp16;
							
							temp16.red		= static_cast<A_u_short>(current_colorP->red	* PF_MAX_CHAN16);
							temp16.green	= static_cast<A_u_short>(current_colorP->green * PF_MAX_CHAN16);
							temp16.blue		= static_cast<A_u_short>(current_colorP->blue	* PF_MAX_CHAN16);
							temp16.alpha	= static_cast<A_u_short>(current_colorP->red	* PF_MAX_CHAN16);
							
							ERR(suites.Iterate16Suite2()->iterate_origin(	in_data,
																			0,
																			output_worldP->height,
																			input_worldP,
																			&current_rectR,
																			&origin,
																			reinterpret_cast<void*>(&temp16),
																			ColorGrid_ColorizePixel16,
																			output_worldP));
							break;
							
						case PF_PixelFormat_ARGB32:
							
							PF_Pixel8 temp8;
							
							temp8.red		= static_cast<A_u_char>(current_colorP->red		* PF_MAX_CHAN8);
							temp8.green		= static_cast<A_u_char>(current_colorP->green	* PF_MAX_CHAN8);
							temp8.blue		= static_cast<A_u_char>(current_colorP->blue	* PF_MAX_CHAN8);
							
							ERR(suites.Iterate8Suite2()->iterate_origin(	in_data,
																			0,
																			output_worldP->height,
																			input_worldP,
																			&current_rectR,
																			&origin,
																			reinterpret_cast<void*>(&temp8),
																			ColorGrid_ColorizePixel8,
																			output_worldP));
							break;
							
						default:
							err = PF_Err_BAD_CALLBACK_PARAM;
							break;
					}
				}
			}
			current_colorP++;
			box_acrossL++;
		}
	}
	ERR2(AEFX_ReleaseSuite(	in_data,
							out_data,
							kPFWorldSuite, 
							kPFWorldSuiteVersion2, 
							"Couldn't release suite."));
	ERR2(PF_CHECKIN_PARAM(in_data, &arb_param));
	ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, CG_INPUT));
	return err;
}

static PF_Err 
HandleEvent(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*extra)
{

	PF_Err			err 	= PF_Err_NONE;
	
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	switch (extra->e_type) 	{

	case PF_Event_DO_CLICK:
		ERR(DoClick(in_data, out_data, params, output, extra));
		// Premiere Pro/Elements does not support this suite
		if (in_data->appl_id != 'PrMr')
		{
			ERR(suites.AdvAppSuite2()->PF_InfoDrawText3("ColorGrid - Do Click Event","Adobe Inc", NULL));
		}
		break;
	
	case PF_Event_DRAG:
		// Premiere Pro/Elements does not support this suite
		if (in_data->appl_id != 'PrMr')
		{
			ERR(suites.AdvAppSuite2()->PF_InfoDrawText3("ColorGrid - Drag Event","Adobe Inc", NULL));
		}
		break;
	
	case PF_Event_DRAW:
		ERR(DrawEvent(in_data, out_data, params, output, extra, params[1]->u.cd.value));
		// Premiere Pro/Elements does not support this suite
		if (in_data->appl_id != 'PrMr')
		{
			//	don't draw info palette *during* a draw event, it will mess up 
			//	the drawing and cause schmutz
			// ERR(suites.AdvAppSuite2()->PF_InfoDrawText3("ColorGrid - Draw Event","Adobe Inc", NULL));
		}
		break;
	
	case PF_Event_ADJUST_CURSOR:
		ERR(ChangeCursor(in_data, out_data, params, output, extra));
		// Premiere Pro/Elements does not support this suite
		if (in_data->appl_id != 'PrMr')
		{
			ERR(suites.AdvAppSuite2()->PF_InfoDrawText3("ColorGrid - Change Cursor Event","Adobe Inc", NULL));
		}
		break;

	default:
		break;
	
	}
	return err;
}

static PF_Err 
HandleArbitrary(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output,
	PF_ArbParamsExtra	*extra)
{
	PF_Err 	err 	= PF_Err_NONE;
	void 	*srcP	= NULL,
		 	*dstP	= NULL;

	switch (extra->which_function) {
	
	case PF_Arbitrary_NEW_FUNC:
		if (extra->u.new_func_params.refconPV != ARB_REFCON) {
			err = PF_Err_INTERNAL_STRUCT_DAMAGED;
		} else {
			err = CreateDefaultArb(	in_data,
									out_data,
									extra->u.new_func_params.arbPH);
		}
		break;

	case PF_Arbitrary_DISPOSE_FUNC:
		if (extra->u.dispose_func_params.refconPV != ARB_REFCON) {
			err = PF_Err_INTERNAL_STRUCT_DAMAGED;
		} else {
			PF_DISPOSE_HANDLE(extra->u.dispose_func_params.arbH);
		}
		break;

	case PF_Arbitrary_COPY_FUNC:
		if(extra->u.copy_func_params.refconPV == ARB_REFCON) {
			ERR(CreateDefaultArb(	in_data,
									out_data,
									extra->u.copy_func_params.dst_arbPH));

			ERR(Arb_Copy(	in_data,
							out_data,
							&extra->u.copy_func_params.src_arbH,
							extra->u.copy_func_params.dst_arbPH));
		}
		break;
	case PF_Arbitrary_FLAT_SIZE_FUNC:
		*(extra->u.flat_size_func_params.flat_data_sizePLu) = sizeof(CG_ArbData);
		break;

	case PF_Arbitrary_FLATTEN_FUNC:

		if(extra->u.flatten_func_params.buf_sizeLu == sizeof(CG_ArbData)){
			srcP = (CG_ArbData*)PF_LOCK_HANDLE(extra->u.flatten_func_params.arbH);
			dstP = extra->u.flatten_func_params.flat_dataPV;
			if (srcP){
				memcpy(dstP,srcP,sizeof(CG_ArbData));
			}
			PF_UNLOCK_HANDLE(extra->u.flatten_func_params.arbH);
		}
		break;

	case PF_Arbitrary_UNFLATTEN_FUNC:
		if(extra->u.unflatten_func_params.buf_sizeLu == sizeof(CG_ArbData)){		
			PF_Handle	handle = PF_NEW_HANDLE(sizeof(CG_ArbData));
			dstP = (CG_ArbData*)PF_LOCK_HANDLE(handle);
			srcP = (void*)extra->u.unflatten_func_params.flat_dataPV;
			if (srcP){
				memcpy(dstP,srcP,sizeof(CG_ArbData));
			}
			*(extra->u.unflatten_func_params.arbPH) = handle;
			PF_UNLOCK_HANDLE(handle);
		}
		break;

	case PF_Arbitrary_INTERP_FUNC:
		if(extra->u.interp_func_params.refconPV == ARB_REFCON) {
			ERR(CreateDefaultArb(	in_data,
									out_data,
									extra->u.interp_func_params.interpPH));
			
			ERR(Arb_Interpolate(	in_data,
									out_data,
									extra->u.interp_func_params.tF,
									&extra->u.interp_func_params.left_arbH,
									&extra->u.interp_func_params.right_arbH,
									extra->u.interp_func_params.interpPH));
		}
		break;

	case PF_Arbitrary_COMPARE_FUNC:
		ERR(Arb_Compare(	in_data,
							out_data,
							&extra->u.compare_func_params.a_arbH,
							&extra->u.compare_func_params.b_arbH,
							extra->u.compare_func_params.compareP));
		break;

	case PF_Arbitrary_PRINT_SIZE_FUNC:
		if (extra->u.print_size_func_params.refconPV == ARB_REFCON) {
			*extra->u.print_size_func_params.print_sizePLu = COLORGRID_ARB_MAX_PRINT_SIZE;		
		} else {
			err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
		}
		break;

	case PF_Arbitrary_PRINT_FUNC:

		if (extra->u.print_func_params.refconPV == ARB_REFCON) {
			ERR(Arb_Print(in_data,
							out_data,
							extra->u.print_func_params.print_flags,
							extra->u.print_func_params.arbH,
							extra->u.print_func_params.print_sizeLu,
							extra->u.print_func_params.print_bufferPC));
		} else {
			err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
		}
		break;

	case PF_Arbitrary_SCAN_FUNC:
		if (extra->u.scan_func_params.refconPV == ARB_REFCON) {
			ERR(Arb_Scan(	in_data,
							out_data,
							extra->u.scan_func_params.refconPV,
							extra->u.scan_func_params.bufPC,
							extra->u.scan_func_params.bytes_to_scanLu,
							extra->u.scan_func_params.arbPH));					
		} else {
			err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
		}
		break;
	}
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
		"ColorGrid", // Name
		"ADBE ColorGrid", // Match Name
		"Sample Plug-ins", // Category
		AE_RESERVED_INFO, // Reserved Info
		"EffectMain",	// Entry point
		"https://www.adobe.com");	// support URL

	return result;
}


PF_Err
EffectMain(
	PF_Cmd				cmd,
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output,
	void				*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {

		case PF_Cmd_ABOUT:
			err = About(in_data, out_data, params, output);
			break;

		case PF_Cmd_GLOBAL_SETUP:
			err = GlobalSetup(	in_data, out_data, params, output);
			break;

		case PF_Cmd_PARAMS_SETUP:
			err = ParamsSetup(	in_data, out_data, params, output);
			break;

		case PF_Cmd_RENDER:
			err = Render(	in_data, out_data, params, output);
			break;

		case PF_Cmd_EVENT:
			err = HandleEvent(	in_data, out_data, params, output, reinterpret_cast<PF_EventExtra*>(extra));
			break;

		case PF_Cmd_ARBITRARY_CALLBACK:
			err = HandleArbitrary(	in_data, out_data, params, output, reinterpret_cast<PF_ArbParamsExtra*>(extra));
			break;
			
		case  PF_Cmd_SMART_PRE_RENDER:
			err = PreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra*>(extra));
			break;

		case  PF_Cmd_SMART_RENDER:
			err = SmartRender(	in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra));
			break;
		}
	} catch (PF_Err &thrown_err) {
		err = thrown_err;
	}
	return err;
}


