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
/*
	HistoGrid.cpp
	A simple example of how the UI can request asynchronous rendered upstream frames
	for lightweight processing in AE 13.5 and later.  This effect calculates
	a sampled 10x10 color grid from the upstream frame,  calcs and displays a preview
	of that color grid.  In render, a higher quality grid is calculated and used to
	modify the output image creating a blend of a color grid + the original image.
 
	BACKGROUND
 
	As of 13.5, UI and Render thread introduction means lightweight histograms must
	asynchronously request the frame needed inside of DRAW.  This replaces the pattern of
	calculation of histograms inside of render, storing in sequence data, and then
	passing back REFRESH_UI outflag.  That method no longer works in 13.5 because
	the render sequence data is not in the same database as the UI and must not be written to.
 
	This example shows:
		1. how to use the new RenderAsyncManager inside of PF_Event_DRAW to manage
			asynchronous frame requests for CUSTOM UI
		2. how to request the "upstream" frame of the effect for lightweight preview processing
			for display in CUSTOM UI.
			Note how the requested render size is downsampled to keep preview render fast.
			Note how code shared by preview and by render is factored into a common function
			that can be called by either without introducing UI dependencies in render
			or vice versa.
		3. Storing a cached preview to prevent flicker in asynchronous requests
 
	The emphasis is on "lightweight". Heavy processing on a frame for preview on AE UI thread
	can interfere with user interactivity and ram preview playback in 13.5 onward.
	Heavier processing would need to be done on another thread.
 
	Search for "EXAMPLE" in comments for locations modified in this source code
	to make that work.
	
	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			new sample starting from ColorGrid						seth		1/15/2015
				converted arb data to none-persist sequence data color caching
	1.1			Added new entry point									zal			9/18/2017
	1.2			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

*/


#include "HistoGrid.h"

AEGP_PluginID    G_my_plugin_id                         = 0;

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

	out_data->out_flags  |= 	PF_OutFlag_CUSTOM_UI			    |			//  still needed.  EXAMPLE
                                PF_OutFlag_USE_OUTPUT_EXTENT	    |
                                PF_OutFlag_PIX_INDEPENDENT		    |
                                PF_OutFlag_DEEP_COLOR_AWARE;
							
	out_data->out_flags2 |= 	PF_OutFlag2_FLOAT_COLOR_AWARE	    |
                                PF_OutFlag2_SUPPORTS_SMART_RENDER   |
                                PF_OutFlag2_CUSTOM_UI_ASYNC_MANAGER |           // flag for AsyncManager usage in PF_Event_DRAW.   EXAMPLE
                                PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	
	if (in_data->appl_id != 'PrMr') {
		// This is only needed for the AEGP suites, which Premiere Pro doesn't support
		
		AEFX_SuiteScoper<AEGP_UtilitySuite3>   u_suite(in_data,	kAEGPUtilitySuite,             kAEGPUtilitySuiteVersion3);
		ERR(u_suite->AEGP_RegisterWithAEGP(NULL, "HistoGrid", &G_my_plugin_id));
	}

	return err;
}

static PF_Err
HistoGrid_ColorizePixelFloat(
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
HistoGrid_ColorizePixel16(
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
HistoGrid_ColorizePixel8(
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

	// EXAMPLE. the NULL is being used to reserve an area in the custom UI for drawing the preview
	// An example of using an ARB for this for storing persistant data is in the ColorGrid example
	def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
	def.ui_width = UI_GRID_WIDTH;
	def.ui_height = UI_GRID_HEIGHT;
	PF_ADD_NULL("Preview", COLOR_GRID_UI); // "UI");

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

// EXAMPLE setting up transient sequence data for caching of computed preview data
static PF_Err
SequenceSetup(
			  PF_InData		*in_data,
			  PF_OutData		*out_data)
{
	PF_Err			err = PF_Err_NONE;
	CA_SeqData		**seqH;
	CA_SeqData		*seqP;
	
	if (in_data->sequence_data) {
		//CA_SeqData		**seqH = (CA_SeqData**)in_data->sequence_data;
		PF_DISPOSE_HANDLE(in_data->sequence_data);
		out_data->sequence_data = NULL;
	}
	
	seqH = (CA_SeqData**)PF_NEW_HANDLE(sizeof(CA_SeqData));
	
	if (seqH) {
		memset( *seqH, 0, sizeof(CA_SeqData));
		seqP = (CA_SeqData*)PF_LOCK_HANDLE(seqH);
		
		seqP->magic = CA_MAGIC;
		seqP->validB = FALSE;		// EXAMPLE cached preview is not yet valid
		
		if (!err) {
			in_data->sequence_data = out_data->sequence_data = (PF_Handle)seqH;
		}
	
		PF_UNLOCK_HANDLE(seqH);
		
	} else {
		err = PF_Err_OUT_OF_MEMORY;
	}
	
	if (err) {
		PF_SPRINTF(out_data->return_msg, "SequenceSetup failure in HistoGrid Effect");
		out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
	}
	
	return err;
	
}


static PF_Err
SequenceResetup(
				PF_InData		*in_data,
				PF_OutData		*out_data,
				PF_ParamDef		*params[])
{
	return SequenceSetup(in_data, out_data);
}




static PF_Err
SequenceSetdown(
				PF_InData		*in_data,
				PF_OutData		*out_data,
				PF_ParamDef		*params[],
				PF_LayerDef		*output)
{
	PF_Err			err = PF_Err_NONE;
	
	if (in_data->sequence_data) {
		PF_DISPOSE_HANDLE(in_data->sequence_data);
		in_data->sequence_data = out_data->sequence_data = NULL;
	}
	return err;
	
}

static PF_Err
SequenceFlatten(
				PF_InData		*in_data,
				PF_OutData		*out_data,
				PF_ParamDef		*params[],
				PF_LayerDef		*output)
{
	PF_Err		err = A_Err_NONE;
	
	if (in_data->sequence_data)
	{
		out_data->sequence_data = in_data->sequence_data;
	}
	
	return err;
}

inline void AssignGridCellColor( PF_Pixel8* in, PF_PixelFloat* box_colorP) {
	box_colorP->alpha =  (PF_FpShort)in->alpha/PF_MAX_CHAN8;
	box_colorP->red =  (PF_FpShort)in->red/PF_MAX_CHAN8;
	box_colorP->green =  (PF_FpShort)in->green/PF_MAX_CHAN8;
	box_colorP->blue =  (PF_FpShort)in->blue/PF_MAX_CHAN8;
};

inline void AssignGridCellColor( PF_Pixel16* in, PF_PixelFloat* box_colorP) {
	box_colorP->alpha =  (PF_FpShort)in->alpha/PF_MAX_CHAN16;
	box_colorP->red =  (PF_FpShort)in->red/PF_MAX_CHAN16;
	box_colorP->green =  (PF_FpShort)in->green/PF_MAX_CHAN16;
	box_colorP->blue =  (PF_FpShort)in->blue/PF_MAX_CHAN16;
};

inline void AssignGridCellColor( PF_PixelFloat* in, PF_PixelFloat* box_colorP) { *box_colorP = *in; }

// for each grid cell get the central cell color for preview or render usage
template <typename PIXFORMAT>
static void ComputeGridCellColors (
										PF_InData		*in_data,
										CA_ColorCache	*color_cacheP,
										PF_EffectWorld  *input )
{
	PF_PixelFloat* box_colorP = NULL;
	A_long cell_width = input->width / BOXES_ACROSS;	// ignoring remainder for example
	A_long cell_height = input->height / BOXES_DOWN;
	A_long x = 0, y = 0;
	
	if (cell_width > 0 && cell_height > 0) {
		for (A_long row_boxL=0; row_boxL < BOXES_DOWN; row_boxL++) {
			y = row_boxL * cell_height + cell_height / 2;
			
			for (A_long col_boxL=0; col_boxL < BOXES_ACROSS; col_boxL++) {

				// input
				x = col_boxL * cell_width + cell_width / 2;
				PIXFORMAT       *in = reinterpret_cast<PIXFORMAT*>((char*)input->data + input->rowbytes * y) + x;
				
				//output
				box_colorP	= &color_cacheP->colorA[row_boxL * (BOXES_ACROSS) + col_boxL];
				AssignGridCellColor( in, box_colorP);
				
			}
			
			// might need interrupt here
		}
	}
}

static void ClearColorCache(CA_ColorCache* color_cacheP) {
	PF_PixelFloat black = {0.0,0.0,0.0,0.0};
	for (int32_t i=0; i < CACHE_ELEMENTS; i++) {
		color_cacheP->colorA[i] = black;
	}
}

// EXAMPLE
// This calculates color grid values on an input frame and is used in both
// render and custom UI preview.  To keep this operation lightweight on the UI,  we'll pass a highly downsampled frame.
// On the UI thread the result is cached in sequence data to prevent flicker if we don't have a completed computation yet
PF_Err ComputeColorGridFromFrame( PF_InData		*in_data, PF_EffectWorld* worldP,  CA_ColorCache*	color_cacheP)
{
	PF_Err err = A_Err_NONE;
	AEFX_SuiteScoper<AEGP_WorldSuite3>               w_suite(in_data,	kAEGPWorldSuite,             kAEGPWorldSuiteVersion3);
	AEFX_SuiteScoper<AEGP_MemorySuite1>				m_suite(in_data,	kAEGPMemorySuite,				kAEGPMemorySuiteVersion1);
	AEFX_SuiteScoper<PF_WorldSuite2>              pf_suite(in_data,	kPFWorldSuite,             kPFWorldSuiteVersion2);
	
	// clear the preview
	ClearColorCache(color_cacheP);
	
	if (!worldP) {
		// success checkout but no frame means blank preview, we just reset colors above, leave blank
	} else {
		// calculate the frame grid colors
		PF_PixelFormat pixformat;
		pf_suite->PF_GetPixelFormat( worldP, &pixformat);
		
		switch (pixformat) {
				
			case PF_PixelFormat_ARGB128:
				ComputeGridCellColors<PF_PixelFloat>(  in_data,  color_cacheP, worldP );
				break;
				
			case PF_PixelFormat_ARGB64:
				ComputeGridCellColors<PF_Pixel16>( in_data,  color_cacheP, worldP );
				break;
				
			case PF_PixelFormat_ARGB32:
				ComputeGridCellColors<PF_Pixel8>( in_data,  color_cacheP, worldP );
				break;
				
			default:
				err = PF_Err_BAD_CALLBACK_PARAM;	// unexpected bitdepth
				break;
		}


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
	PF_Point		origin			= {0,0};
	A_u_short		iSu				= 0;
	PF_Rect			current_rectR	= {0,0,0,0};
	A_long			box_acrossL		= 0,
					box_downL		= 0,
					progress_baseL	= 0,
					progress_finalL	= BOXES_PER_GRID;
	

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	

	origin.h = (A_short)in_data->pre_effect_source_origin_x;
	origin.v = (A_short)in_data->pre_effect_source_origin_y;
	
	// EXAMPLE compute the colors we will use for render. high quality
	PF_EffectWorld *input_worldP = &params[CG_INPUT]->u.ld;
	
	CA_ColorCache color_cache;
	ERR( ComputeColorGridFromFrame( in_data, input_worldP, &color_cache));
	current_colorP = reinterpret_cast<PF_PixelFloat*>(&color_cache);

	/* 
		This section uses the pre-effect extent hint, since it wants
		to only be applied to the source layer material, and NOT to any
		resized effect area. Example: User applies "Resizer" to a layer
		before using HistoGrid. The effect makes the output area larger
		than the source footage. HistoGrid will look at the pre-effect
		extent width and height to determine what the relative coordinates
		are for the source material inside the params[0] (the layer).
	*/

	for(iSu = 0; !err && iSu < BOXES_PER_GRID; ++iSu){
		if(box_acrossL == BOXES_ACROSS)	{
			box_downL++;
			box_acrossL = 0;
		}
		HistoGrid_Get_Box_In_Grid(	&origin,
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
											input_worldP,
											&current_rectR,
											reinterpret_cast<void*>(&temp8),
											HistoGrid_ColorizePixel8,
											output);

		current_colorP++;
		box_acrossL++;
	}

	return err;
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err err = PF_Err_NONE;
	
	PF_RenderRequest req = extra->input->output_request;
	PF_CheckoutResult in_result;
	

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
	
	CA_ColorCache		*color_cacheP			= NULL;
	PF_PixelFloat	*current_colorP = NULL;



	ERR((extra->cb->checkout_layer_pixels(	in_data->effect_ref, CG_INPUT, &input_worldP)));
	
	ERR(extra->cb->checkout_output(	in_data->effect_ref, &output_worldP));
	
	// EXAMPLE compute the colors we will use for render..high quality
	CA_ColorCache color_cache;
	color_cacheP = &color_cache;
	ERR( ComputeColorGridFromFrame( in_data, input_worldP,  &color_cache));
	current_colorP = reinterpret_cast<PF_PixelFloat*>(color_cacheP);
		
	// actually render
	
	if (!err){
		for(iSu = 0; !err && iSu < BOXES_PER_GRID; ++iSu){
			if(box_acrossL == BOXES_ACROSS)	{
				box_downL++;
				box_acrossL = 0;
			}
			HistoGrid_Get_Box_In_Grid(	&origin,
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
																			HistoGrid_ColorizePixelFloat,
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
																			HistoGrid_ColorizePixel16,
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
																			HistoGrid_ColorizePixel8,
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
	
	if (in_data->sequence_data == NULL &&
		(extra->e_type == PF_Event_DO_CLICK ||
		 extra->e_type == PF_Event_DRAW)) {
			err = SequenceResetup(in_data, out_data, params);
			if (!err) in_data->sequence_data = out_data->sequence_data;
		}
	
	switch (extra->e_type) 	{

	case PF_Event_DRAW:
		ERR(DrawEvent(in_data, out_data, params, output, extra, params[1]->u.cd.value));
		break;
	
	default:
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
		"HistoGrid", // Name
		"ADBE HistoGrid", // Match Name
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
				
		case PF_Cmd_SEQUENCE_SETUP:
			err = SequenceSetup(in_data, out_data);
			break;
		case PF_Cmd_SEQUENCE_SETDOWN:
			err = SequenceSetdown(in_data, out_data, params, output);
			break;
		case PF_Cmd_SEQUENCE_RESETUP:
			err = SequenceResetup(in_data, out_data, params);
			break;
		case PF_Cmd_SEQUENCE_FLATTEN:
			err = SequenceFlatten(in_data, out_data, params, output);
			break;

		case PF_Cmd_RENDER:
			err = Render(	in_data, out_data, params, output);
			break;

		case PF_Cmd_EVENT:
			err = HandleEvent(	in_data, out_data, params, output, reinterpret_cast<PF_EventExtra*>(extra));
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


