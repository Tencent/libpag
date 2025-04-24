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
	Shifter.cpp
	
	Demonstrates sub-pixel sampling and use of the iteration suites.

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			(from the mists of time)	
	1.1			Added new entry point									zal			9/18/2017
	1.2			Remove deprecated 'register' keyword					cb			12/18/2020
	2.1			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

*/

#include "Shifter.h"


static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg, 
											"%s, v%d.%d\r%s",
											NAME, 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											DESCRIPTION);

	return PF_Err_NONE;
}


static PF_Err 
GlobalSetup (PF_InData		*in_data,
			PF_OutData		*out_data,
			PF_ParamDef		*params[],
			PF_LayerDef		*output )
{
	out_data->my_version	= PF_VERSION(	MAJOR_VERSION, 
											MINOR_VERSION,
											BUG_VERSION, 
											STAGE_VERSION, 
											BUILD_VERSION);

	out_data->out_flags     |= 	PF_OutFlag_DEEP_COLOR_AWARE         |
                                PF_OutFlag_USE_OUTPUT_EXTENT;
	
	out_data->out_flags2	|=	PF_OutFlag2_SUPPORTS_SMART_RENDER	|
								PF_OutFlag2_FLOAT_COLOR_AWARE		|
								PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	return PF_Err_NONE;
}


static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err	err = PF_Err_NONE;
	
	PF_ParamDef		def;
	AEFX_CLR_STRUCT(def);	
	
	PF_ADD_POINT(	"Offset", 
					SHIFT_DISPLACE_X_DFLT, 
					SHIFT_DISPLACE_Y_DFLT, 
					RESTRICT_BOUNDS,
					DISPLACE_DISK_ID);
	
	AEFX_CLR_STRUCT(def);	

	// For old time's sake, let's build this slider the old fashioned way:
	
	def.param_type = PF_Param_FIX_SLIDER;
	PF_STRCPY(def.name, "Opacity");
	def.flags = 0;
	def.u.fd.value_str[0] = '\0';
	def.u.fd.value_desc[0] = '\0';
	def.u.fd.valid_min = def.u.fd.slider_min = 0;
	def.u.fd.valid_max = def.u.fd.slider_max = A_Fixed_ONE;
	def.u.fd.value = def.u.fd.dephault = A_Fixed_HALF;
	def.u.fd.precision = 1;	def.u.fd.display_flags = PF_ValueDisplayFlag_PERCENT;
	if ((err = PF_ADD_PARAM(in_data, -1, &def))) return err;

	PF_ADD_CHECKBOXX(	"Use Xform in Legacy Render Path",
						TRUE,
						0,
						USE_TRANSFORM_DISK_ID);
	
	out_data->num_params = SHIFT_NUM_PARAMS; 
	
	return err;
}

static PF_Err 
ShiftImage8 (
	void 		*refcon, 
	A_long 		xL, 
	A_long 		yL, 
	PF_Pixel 	*inP, 
	PF_Pixel 	*outP)
{
	ShiftInfo	*siP		= reinterpret_cast<ShiftInfo*>(refcon);
	PF_Err				err			= PF_Err_NONE;
	PF_Fixed			new_xFi		= 0, 
						new_yFi		= 0;

    // Resample original image at offset point.

    new_xFi = INT2FIX(xL) + siP->x_offFi;
    new_yFi = INT2FIX(yL) + siP->y_offFi;
    
    // Can also use subpixel_sample in AEGP suites like 16 and 32 bit depth
    ERR(siP->in_data.utils->subpixel_sample(siP->in_data.effect_ref,
                                            new_xFi,
                                            new_yFi,
                                            &siP->samp_pb,
                                            outP));
	return err;
}
 
static PF_Err 
ShiftImage16 (
	void 		*refcon, 
	A_long 		xL, 
	A_long 		yL, 
	PF_Pixel16 	*inP, 
	PF_Pixel16 	*outP)
{
	ShiftInfo	*siP		= (ShiftInfo*)refcon;
	PF_InData			*in_data	= &(siP->in_data);
	PF_Err				err			= PF_Err_NONE;
	PF_Fixed			new_xFi 		= 0, 
						new_yFi 		= 0;
						
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// Resample original image at offset point.

	new_xFi = INT2FIX(xL) + siP->x_offFi;
	new_yFi = INT2FIX(yL) + siP->y_offFi;

	ERR(suites.Sampling16Suite1()->subpixel_sample16(in_data->effect_ref,
                                                     new_xFi,
                                                     new_yFi,
                                                     &siP->samp_pb,
                                                     outP));
	return err;
}

static PF_Err 
ShiftImageFloat (
	void			*refcon, 
	A_long 			xL, 
	A_long 			yL, 
	PF_PixelFloat 	*inP, 
	PF_PixelFloat 	*outP)
{
	ShiftInfo	*siP		= (ShiftInfo*)refcon;
	PF_Err				err			= PF_Err_NONE;
	PF_Fixed			new_xFi 		= 0, 
						new_yFi 		= 0;
						
	AEGP_SuiteHandler suites(siP->in_data.pica_basicP);

	// Resample original image at offset point.

	new_xFi = INT2FIX(xL) + siP->x_offFi;
	new_yFi = INT2FIX(yL) + siP->y_offFi;

	ERR(suites.SamplingFloatSuite1()->subpixel_sample_float(siP->in_data.effect_ref,
															new_xFi, 
															new_yFi, 
															&siP->samp_pb, 
															outP));
	return err;
}

static PF_Err 
Render (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err			err 		=	PF_Err_NONE;
	ShiftInfo		si;
	PF_Fixed		blendFi		=	params[SHIFT_BLEND]->u.fd.value;
	A_long			linesL		=	0;
	PF_EffectWorld	*inputP		=	&params[SHIFT_INPUT]->u.ld;

	/*	We convert the image width and height to Fixed values
		then divide by two, to calculate the Fixed point offset.
		Shifting left 15 bits effectively does this conversion.
	*/

	AEFX_CLR_STRUCT(si);
	
	si.x_offFi = ((long)inputP->width << 15) -	// 1/2 width as Fixed 
		params[SHIFT_DISPLACE]->u.td.x_value;

	si.y_offFi = ((long)inputP->height << 15) -	// 1/2 height as Fixed 
		params[SHIFT_DISPLACE]->u.td.y_value;

	si.ref = in_data->effect_ref;
	si.samp_pb.src = inputP;
	si.in_data = *in_data;

	// If using the transform callback (which is much more performant in Premiere)
	// Note: We're aware that this path produces different rendering results than the SmartFX render code,
	// and that's why we present the option to the user as a checkbox.
	if (params[SHIFT_USE_TRANSFORM_IN_LEGACY_RENDER_PATH]->u.bd.value) {

		// Initialize our output to transparent black
		PF_Pixel transparent_black = {0, 0, 0, 0};
		ERR(PF_FILL(&transparent_black, &output->extent_hint, output));

		// Now blend the shifted image in
		// If 0% blend, it is transparent so just skip it
		if (!err && blendFi > 0) {		

			PF_CompositeMode composite_mode;
			AEFX_CLR_STRUCT(composite_mode);
			composite_mode.opacity = static_cast<int>(FIX_2_FLOAT(params[SHIFT_BLEND]->u.fd.value) * 255);
			composite_mode.xfer = PF_Xfer_COPY;

			// Build the matrix to do the shift
			PF_FloatMatrix float_matrix;
			AEFX_CLR_STRUCT(float_matrix.mat);
			float_matrix.mat[0][0] = float_matrix.mat[1][1] = float_matrix.mat[2][2] = 1;
			float_matrix.mat[2][0] = FIX2INT(params[SHIFT_DISPLACE]->u.td.x_value); // This is the x translation
			float_matrix.mat[2][1] = FIX2INT(params[SHIFT_DISPLACE]->u.td.y_value); // This is the y translation

			ERR(in_data->utils->transform_world(in_data->effect_ref,
												in_data->quality,
												PF_MF_Alpha_STRAIGHT,
												in_data->field,
												inputP,
												&composite_mode,
												NULL,
												&float_matrix,
												1,
												TRUE,
												&output->extent_hint,
												output));
		}
	} else {

		// This is the old code that demonstrates iterate and subpixel_sample
		if ((1L << 16) == blendFi) {

			// If 100% blend, simply copy the source.
			ERR(PF_COPY(inputP, 
						output, 
						NULL, 
						NULL));
		} else {
			
			AEGP_SuiteHandler	suites(in_data->pica_basicP);		

			linesL = output->extent_hint.bottom - output->extent_hint.top;

			// iterate() checks for user interruptions.

			ERR(suites.Iterate8Suite2()->iterate(	in_data,
													0, 
													linesL, 
													inputP, 
													&output->extent_hint,
													(void*)&si, 
													ShiftImage8, 
													output));

			ERR(PF_BLEND(	output, 
							inputP, 
							blendFi, 
							output));
		}

	}

	return err;
}

static PF_Err 
RespondtoAEGP ( 	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void*			extraP)
{
	PF_Err			err = PF_Err_NONE;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg, 
											"%s",	
											reinterpret_cast<A_char*>(extraP));

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
						
	AEGP_SuiteHandler 	suites(in_data->pica_basicP);
	PF_EffectWorld		*input_worldP	= NULL, 
						*output_worldP  = NULL;
	PF_WorldSuite2		*wsP			= NULL;
	PF_PixelFormat		format			= PF_PixelFormat_INVALID;

	PF_Point			origin;

	ShiftInfo	*infoP = reinterpret_cast<ShiftInfo*>(suites.HandleSuite1()->host_lock_handle(reinterpret_cast<PF_Handle>(extra->input->pre_render_data)));

	if (infoP){
		if (!infoP->no_opB){
			// checkout input & output buffers.
			ERR((extra->cb->checkout_layer_pixels(	in_data->effect_ref, SHIFT_INPUT, &input_worldP)));

			ERR(extra->cb->checkout_output(	in_data->effect_ref, &output_worldP));
			
			if (!err && output_worldP){
				ERR(AEFX_AcquireSuite(	in_data, 
										out_data, 
										kPFWorldSuite, 
										kPFWorldSuiteVersion2, 
										"Couldn't load suite.",
										(void**)&wsP));

				infoP->ref 			= in_data->effect_ref;
				infoP->samp_pb.src 	= input_worldP;
				infoP->in_data 		= *in_data;

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
																			&output_worldP->extent_hint,
																			&origin,
																			(void*)(infoP),
																			ShiftImageFloat,
																			output_worldP));
							break;
							
						case PF_PixelFormat_ARGB64:
					
							ERR(suites.Iterate16Suite2()->iterate_origin(	in_data,
																			0,
																			output_worldP->height,
																			input_worldP,
																			&output_worldP->extent_hint,
																			&origin,
																			(void*)(infoP),
																			ShiftImage16,
																			output_worldP));
							break;
							
						case PF_PixelFormat_ARGB32:
							 
							ERR(suites.Iterate8Suite2()->iterate_origin(	in_data,
																			0,
																			output_worldP->height,
																			input_worldP,
																			&output_worldP->extent_hint,
																			&origin,
																			(void*)(infoP),
																			ShiftImage8,
																			output_worldP));
							break;

						default:
							err = PF_Err_BAD_CALLBACK_PARAM;
							break;
					}
				}
				//	Note: same blend, no matter bit depth.
				
				ERR(suites.WorldTransformSuite1()->blend(	in_data->effect_ref,
															input_worldP,
															output_worldP,
															infoP->blend_valFi,
															output_worldP));
			}
		} else {
			// copy input buffer;
			ERR(PF_COPY(input_worldP, output_worldP, NULL, NULL));
		}

		suites.HandleSuite1()->host_unlock_handle(reinterpret_cast<PF_Handle>(extra->input->pre_render_data));

	} else {
		err = PF_Err_BAD_CALLBACK_PARAM;
	}
	ERR2(AEFX_ReleaseSuite(	in_data,
							out_data,
							kPFWorldSuite, 
							kPFWorldSuiteVersion2, 
							"Couldn't release suite."));
	return err;
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err err = PF_Err_NONE;
	PF_ParamDef blend_param, displace_param;
	PF_RenderRequest req = extra->input->output_request;
	PF_CheckoutResult in_result;

	AEGP_SuiteHandler suites(in_data->pica_basicP);

	PF_Handle	infoH	=	suites.HandleSuite1()->host_new_handle(sizeof(ShiftInfo));
	
	if (infoH){
		
		ShiftInfo	*infoP = reinterpret_cast<ShiftInfo*>(suites.HandleSuite1()->host_lock_handle(infoH));
		
		if (infoP){
			extra->output->pre_render_data = infoH;
			
			AEFX_CLR_STRUCT(blend_param);
			
			ERR(PF_CHECKOUT_PARAM(	in_data, 
									SHIFT_DISPLACE, 
									in_data->current_time, 
									in_data->time_step, 
									in_data->time_scale, 
									&displace_param));

			ERR(PF_CHECKOUT_PARAM(	in_data, 
									SHIFT_BLEND, 
									in_data->current_time, 
									in_data->time_step, 
									in_data->time_scale, 
									&blend_param));

			if (!err){
				req.preserve_rgb_of_zero_alpha = TRUE;	//	Hey, we care about zero alpha
				req.field = PF_Field_FRAME;				//	We want checkout_layer to provide a complete frame for sampling

				ERR(extra->cb->checkout_layer(	in_data->effect_ref,
												SHIFT_INPUT,
												SHIFT_INPUT,
												&req,
												in_data->current_time,
												in_data->time_step,
												in_data->time_scale,
												&in_result));

				if (!err){
					AEFX_CLR_STRUCT(*infoP);
					infoP->blend_valFi 	= blend_param.u.fd.value;

					PF_Fixed 	widthFi	= INT2FIX(ABS(in_result.max_result_rect.right - in_result.max_result_rect.left)),
								heightFi = INT2FIX(ABS(in_result.max_result_rect.bottom - in_result.max_result_rect.top));

					infoP->x_offFi 		= (widthFi / 2) - displace_param.u.td.x_value;
					infoP->y_offFi 		= (heightFi / 2) - displace_param.u.td.y_value;

					UnionLRect(&in_result.result_rect, 		&extra->output->result_rect);
					UnionLRect(&in_result.max_result_rect, 	&extra->output->max_result_rect);	

					//	Notice something missing, namely the PF_CHECKIN_PARAM to balance
					//	the old-fashioned PF_CHECKOUT_PARAM, above? 
					
					//	For SmartFX, AE automagically checks in any params checked out 
					//	during PF_Cmd_SMART_PRE_RENDER, new or old-fashioned.
				}
			}

			suites.HandleSuite1()->host_unlock_handle(infoH);
		} else {
			err = PF_Err_OUT_OF_MEMORY;
		}		
	} else {
		err = PF_Err_OUT_OF_MEMORY;
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
		"Shifter", // Name
		"ADBE Shifter", // Match Name
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
	void			*extraP)
{
	PF_Err		err = PF_Err_NONE;
	
	try
	{
		switch (cmd) {
			case PF_Cmd_ABOUT:
				err = About(in_data, 
							out_data, 
							params, 
							output);
				break;
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(	in_data, 
									out_data,
									params, 
									output);
				break;
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(	in_data, 
									out_data, 
									params, 
									output);
				break;
			case PF_Cmd_RENDER:
				err = Render(	in_data, 
								out_data, 
								params, 
								output);
				break;

			case PF_Cmd_SMART_PRE_RENDER:
				err = PreRender(in_data, 
								out_data, 
								reinterpret_cast<PF_PreRenderExtra*>(extraP));
				break;
				
			case PF_Cmd_SMART_RENDER:
				err = SmartRender(	in_data, 
									out_data, 
									reinterpret_cast<PF_SmartRenderExtra*>(extraP));
				break;

			case PF_Cmd_COMPLETELY_GENERAL:
				err = RespondtoAEGP(in_data, 
									out_data, 
									params, 
									output, 
									extraP);
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}

