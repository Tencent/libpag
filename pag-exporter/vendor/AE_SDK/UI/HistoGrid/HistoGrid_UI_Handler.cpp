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

#include "HistoGrid.h"

void	PFToDrawbotRect(const PF_Rect		*in_rectR,
						DRAWBOT_RectF32		*out_rectR)
{
	out_rectR->left = in_rectR->left + 0.5;
	out_rectR->top = in_rectR->top + 0.5;
	out_rectR->width = (float)(in_rectR->right - in_rectR->left);
	out_rectR->height = (float)(in_rectR->bottom - in_rectR->top);
}


#define		kMaxShortColor		65535

DRAWBOT_ColorRGBA QDtoDRAWBOTColor(const PF_App_Color *c)
{
	const float inv_sixty_five_kF = 1.0f / (float)(kMaxShortColor);
	
	DRAWBOT_ColorRGBA	color;
	
	color.red = c->red * inv_sixty_five_kF;
	color.green = c->green * inv_sixty_five_kF;
	color.blue = c->blue	* inv_sixty_five_kF;
	color.alpha = 1.0f;
	
	return color;
}


PF_Err	
AcquireBackgroundColor(	
					   PF_InData			*in_data,
					   PF_OutData			*out_data,
					   PF_ParamDef			*params[],
					   PF_EventExtra		*event_extra,
					   DRAWBOT_ColorRGBA	*drawbot_color)
{

	PF_Err			err		= PF_Err_NONE, err2	= PF_Err_NONE;

	PFAppSuite4		*app_suiteP = NULL;
	PF_App_Color	local_color = {0,0,0};

	ERR(AEFX_AcquireSuite(	in_data,
		out_data,
		kPFAppSuite,
		kPFAppSuiteVersion4,
		NULL,
		(void**)&app_suiteP));

	if (app_suiteP) {
		ERR(app_suiteP->PF_AppGetBgColor(&local_color));
		if (!err && drawbot_color) {
			*drawbot_color = QDtoDRAWBOTColor(&local_color);
		}
	}
	
	ERR2(AEFX_ReleaseSuite(	in_data,
		out_data,
		kPFAppSuite,
		kPFAppSuiteVersion4,
		NULL));
	
	return err;
}


PF_Boolean
HistoGrid_PointInRect(
	const PF_Point			*point,			/*>>*/ 
	const PF_Rect			*rectR)			/*>>*/
{
	PF_Boolean		result = FALSE;

	if(	point->h > rectR->left		&&
		point->h <= rectR->right		&&
		point->v > rectR->top		&&
		point->v <= rectR->bottom) {	
		
		result = TRUE; 
	}

	return result;
}


PF_Err
HistoGrid_Get_Box_In_Grid(
	const PF_Point		*origin,		/*>>*/
	A_long				grid_width,		/*>>*/
	A_long				grid_height,	/*>>*/
	A_long				box_across,		/*>>*/
	A_long				box_down,		/*>>*/
	PF_Rect				*box_in_rect)	/*<<*/
{
	PF_Err			err			= PF_Err_NONE;
	A_short			box_width	= 0,
					box_height	= 0;

	// Given the grid h+w and the box coord (0,0 through BOXES_ACROSS,BOXES_DOWN)
	// return the rect of the box

	box_width	= (A_short)(grid_width 	/ BOXES_ACROSS);
	box_height	= (A_short)(grid_height	/ BOXES_DOWN);

	box_in_rect->left	= (A_short)(box_width 	* box_across) 	+ origin->h;
	box_in_rect->top	= (A_short)(box_height 	* box_down) 	+ origin->v;
	box_in_rect->right	= (A_short)box_in_rect->left 			+ box_width;
	box_in_rect->bottom	= (A_short)box_in_rect->top 			+ box_height;

	return err;
}


PF_Err
Deactivate(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*extra)
{
	PF_Err 				err 	= PF_Err_NONE;

	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	// Premiere Pro/Elements does not support this suite
	if (in_data->appl_id != 'PrMr')
	{
		ERR(suites.AdvAppSuite2()->PF_InfoDrawText("HistoGrid - Deactivate Event","Adobe Inc"));
	}

	return err;
}



// EXAMPLE: This requests the upstream input frame for lightweight preview purposes, but highly downsampled for speed
// The frame may not be immediately available.  PF_Event_DRAW will get triggered again when async render completes
static PF_Err
RequestAsyncFrameForPreview(
				 PF_InData		*in_data,
				 PF_OutData		*out_data,
				 PF_ParamDef		*params[],
				 PF_EventExtra		*event_extra,
				AEGP_FrameReceiptH  *frame_receiptPH )	// << the resulting receipt if we have one yet
{
	PF_Err	err = PF_Err_NONE;
	
	// get needed suites
	AEFX_SuiteScoper<AEGP_EffectSuite3>               e_suite(in_data,	kAEGPEffectSuite,             kAEGPEffectSuiteVersion3);
	AEFX_SuiteScoper<AEGP_LayerRenderOptionsSuite1>	lro_suite(in_data,	kAEGPLayerRenderOptionsSuite, kAEGPLayerRenderOptionsSuiteVersion1);
	AEFX_SuiteScoper<AEGP_RenderOptionsSuite1>       ro_suite(in_data,	kAEGPRenderOptionsSuite,      kAEGPRenderOptionsSuiteVersion1);
	AEFX_SuiteScoper<AEGP_PFInterfaceSuite1>         pf_suite(in_data, kAEGPPFInterfaceSuite,        kAEGPPFInterfaceSuiteVersion1);
	AEFX_SuiteScoper<PF_EffectCustomUISuite2>		pfcu_suite(in_data, kPFEffectCustomUISuite,		  kPFEffectCustomUISuiteVersion2);
	AEFX_SuiteScoper<AEGP_RenderAsyncManagerSuite1> ram_suite(in_data,	kAEGPRenderAsyncManagerSuite, kAEGPRenderAsyncManagerSuiteVersion1);
	
	// objects
	AEGP_EffectRefH effectH = NULL;
	AEGP_LayerRenderOptionsH layer_ropsH = NULL;

	// get render options description of upstream input frame to effect
	ERR(pf_suite->AEGP_GetNewEffectForEffect(G_my_plugin_id, in_data->effect_ref, &effectH));
	ERR(lro_suite->AEGP_NewFromUpstreamOfEffect(G_my_plugin_id, effectH, &layer_ropsH));
	
	// make the preview render request fast by downsampling a lot.  This could be more intelligent
	#define DOWNSAMPLE_FACTOR 16
	ERR(lro_suite->AEGP_SetDownsampleFactor(layer_ropsH, DOWNSAMPLE_FACTOR, DOWNSAMPLE_FACTOR ));
	
	// ask the async manager to checkout the desired frame. if this isn't in cache,
	// this will fail but the async manager will schedule an asynchronous render.
	// when that render completes, PF_Event_DRAW will be called again
	
	PF_AsyncManagerP async_mgrP = NULL;	// no dispose needed
	ERR(pfcu_suite->PF_GetContextAsyncManager(in_data, event_extra, &async_mgrP));
	
	#define PURPOSE1 1		// unique ID for effect helps hints auto cancelation of rendering by async manager when using multiple render requests
	ERR((ram_suite->AEGP_CheckoutOrRender_LayerFrame_AsyncManager( async_mgrP,  PURPOSE1, layer_ropsH,  frame_receiptPH)));
	
	// cleanup
	if (effectH)		{ e_suite->AEGP_DisposeEffect(effectH); }
	if (layer_ropsH)	{ lro_suite->AEGP_Dispose(layer_ropsH); }
	
	return err;
}



PF_Err
DrawEvent(
		PF_InData			*in_data,
		PF_OutData			*out_data,
		PF_ParamDef			*params[],
		PF_LayerDef			*output,
		PF_EventExtra		*event_extra,
		PF_Pixel			some_color)
{
	PF_Err					err			= PF_Err_NONE,
							err2		= PF_Err_NONE;

	// Setup the arb data handling
	CA_ColorCache			*arbP;
	PF_Point				origin;
	PF_Rect					box_rectR;
	A_long					box_across	= 0,
							box_down	= 0;
	float					grid_width	= 0,
							grid_height = 0;
	int						i			= 0;
	PF_PixelFloat			*pixP		= NULL;	
	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	DRAWBOT_DrawRef			drawing_ref = NULL;
	DRAWBOT_SurfaceRef		surface_ref = NULL;
	DRAWBOT_SupplierRef		supplier_ref = NULL;
	DRAWBOT_PathRef			path_ref = NULL;
	DRAWBOT_BrushRef		brush_ref = NULL;
	DRAWBOT_Suites			drawbotSuites;
	DRAWBOT_RectF32			onscreen_rectR;
	DRAWBOT_ColorRGBA		background_color;
	DRAWBOT_ColorRGBA		pixel_colr[CACHE_ELEMENTS];
	
	// Acquire all the drawbot suites in one go; it should be matched with release routine.
	// You can also use C++ style AEFX_DrawbotSuitesScoper which doesn't need release routine.
	ERR(AEFX_AcquireDrawbotSuites(in_data, out_data, &drawbotSuites));
	
	// Get the drawing reference by passing context to this new api
	PF_EffectCustomUISuite1	*effectCustomUISuiteP;
	err = AEFX_AcquireSuite(in_data,
							out_data,
							kPFEffectCustomUISuite,
							kPFEffectCustomUISuiteVersion1,
							NULL,
							(void**)&effectCustomUISuiteP);
	if (!err && effectCustomUISuiteP) {
		err = (*effectCustomUISuiteP->PF_GetDrawingReference)(event_extra->contextH, &drawing_ref);
		AEFX_ReleaseSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, NULL);
	}

	// Get the drawbot supplier from drawing reference; it shouldn't be released like pen or brush (see below)
	ERR(drawbotSuites.drawbot_suiteP->GetSupplier(drawing_ref, &supplier_ref));
	
	// Get the drawbot surface from drawing reference; it shouldn't be released like pen or brush (see below)
	ERR(drawbotSuites.drawbot_suiteP->GetSurface(drawing_ref, &surface_ref));
	
	ERR(AcquireBackgroundColor(	in_data,
								out_data,
								params,
								event_extra,
								&background_color));
	
	if (!err && event_extra->effect_win.area == PF_EA_CONTROL) {
		// Use to fill background with AE's BG color
		onscreen_rectR.left 	= (float)event_extra->effect_win.current_frame.left;
		onscreen_rectR.top		= (float)event_extra->effect_win.current_frame.top;
		onscreen_rectR.width 	= (float)(event_extra->effect_win.current_frame.right - event_extra->effect_win.current_frame.left);
		onscreen_rectR.height 	= (float)(event_extra->effect_win.current_frame.bottom + 1 - event_extra->effect_win.current_frame.top);

		origin.v	= (short)onscreen_rectR.top;
		origin.h	= (short)onscreen_rectR.left;

		// Calculate the space taken up by the grid
		// Allow the width to scale horizontally, but not too much
		grid_width = onscreen_rectR.width;
		grid_height = onscreen_rectR.height;
		if (grid_width < UI_GRID_WIDTH / 1.5) {
			grid_width = UI_GRID_WIDTH / 1.5f;
		} else if (grid_width > UI_GRID_WIDTH * 1.5) {
			grid_width = UI_GRID_WIDTH * 1.5f;
		}
	}
	
	ERR(drawbotSuites.surface_suiteP->PaintRect(surface_ref, &background_color, &onscreen_rectR));
	
	// EXAMPLE:  If this is the effect pane, then request the upstream frame for preview purposes
	// and do any lightweight preview computation on it.
	// If the have a frame, update our sequence data color cache with the new computation
	// If the frame is not be immediately available, we draw our cached (or blank) state
	// PF_Event_DRAW will get called again later when the frame render completes and then we'll try again

	CA_SeqData* seqP = reinterpret_cast<CA_SeqData*>(PF_LOCK_HANDLE(in_data->sequence_data));
	if (!err && seqP && (*event_extra->contextH)->w_type == PF_Window_EFFECT) {

		AEGP_FrameReceiptH frame_receiptH = NULL;
		ERR(RequestAsyncFrameForPreview(in_data, out_data, params, event_extra, &frame_receiptH));
		
		if (!err && frame_receiptH != NULL) {

			PF_Err err = PF_Err_NONE;
			AEFX_SuiteScoper<AEGP_WorldSuite3>               w_suite(in_data,	kAEGPWorldSuite,             kAEGPWorldSuiteVersion3);
			AEFX_SuiteScoper<AEGP_MemorySuite1>				m_suite(in_data,	kAEGPMemorySuite,				kAEGPMemorySuiteVersion1);
			AEFX_SuiteScoper<AEGP_RenderSuite4>              r_suite(in_data,	kAEGPRenderSuite,             kAEGPRenderSuiteVersion4);

			AEGP_WorldH worldH = NULL;
			ERR( r_suite->AEGP_GetReceiptWorld(frame_receiptH, &worldH));

			if (!err && worldH) {	// receipt could be valid but empty
				CA_ColorCache	new_color_preview;
				PF_EffectWorld world;
				ERR(w_suite->AEGP_FillOutPFEffectWorld(worldH, &world));
				ERR(ComputeColorGridFromFrame(in_data, &world,  &new_color_preview));
				if (!err) {
					// we got an updated color cache, refresh our sequence data cache
						seqP->validB = TRUE;
						seqP->cached_color_preview = new_color_preview;
				}
			}

			r_suite->AEGP_CheckinFrame(frame_receiptH);
		}
		
		// The below will draw the grid with the color we have.
		// Might be fresh computed, previously cached (to reduce flicker), or just blank because it isn't valid yet
		// (on sequence data setup)
	}

	// Draw the color grid using the cached color preview (if valid)
	arbP = (seqP && seqP->validB) ? &seqP->cached_color_preview : NULL;

	if(arbP) {
		pixP = reinterpret_cast<PF_PixelFloat*>(arbP);	

		for(i = 0; i < CACHE_ELEMENTS; i++){
			pixel_colr[i].red = pixP->red;
			pixel_colr[i].green = pixP->green;
			pixel_colr[i].blue = pixP->blue;
			pixel_colr[i].alpha = 1.0;
			pixP++;
		}
		
		for(i = 0; i < BOXES_PER_GRID && !err; i++){
			// Fill in our grid
			
			ERR(HistoGrid_Get_Box_In_Grid(&origin,
										  static_cast<A_long>(grid_width),
										  static_cast<A_long>(grid_height),
										  box_across,
										  box_down,
										  &box_rectR));
			
			// Create a new path; it should be matched with release routine.
			// You can also use C++ style DRAWBOT_PenP which doesn't need release routine.
			ERR(drawbotSuites.supplier_suiteP->NewPath(supplier_ref, &path_ref));
			
			DRAWBOT_RectF32		rectR;
			PFToDrawbotRect(&box_rectR, &rectR);
			
			// Add a rect to the path
			ERR(drawbotSuites.path_suiteP->AddRect(path_ref, &rectR));
			
			// Create a new brush taking color as input; it should be matched with release routine.
			// You can also use C++ style DRAWBOT_BrushP which doesn't need release routine.
			ERR(drawbotSuites.supplier_suiteP->NewBrush(supplier_ref, &(pixel_colr[i]), &brush_ref));
			
			// Fill the path with the brush.
			ERR(drawbotSuites.surface_suiteP->FillPath(surface_ref, brush_ref, path_ref, kDRAWBOT_FillType_Default));
			
			// Release/destroy the brush. It will lead to memory leak otherwise. 
			if (brush_ref) {
				ERR2(drawbotSuites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)brush_ref));
			}
			
			// Release/destroy the path. It will lead to memory leak otherwise.
			if (path_ref) {
				ERR2(drawbotSuites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path_ref));
			}
			
			box_across++;
			
			if(box_across == BOXES_ACROSS)	{
				box_across = 0;
				box_down++;
			}
		}
	}
	PF_UNLOCK_HANDLE(in_data->sequence_data);

	DRAWBOT_ColorRGBA	black_color;
	
	black_color.red = black_color.green = black_color.blue = 0.0;
	black_color.alpha = 1.0;

	DRAWBOT_PenRef	pen_ref = NULL;
	
	ERR(drawbotSuites.supplier_suiteP->NewPen(supplier_ref, &black_color, 1.0, &pen_ref));
	
	// Draw the grid - outline
	for(box_down = 0; box_down < BOXES_DOWN && !err; ++box_down) {
		for(box_across = 0; box_across < BOXES_ACROSS && !err; ++box_across) {
			
			ERR(HistoGrid_Get_Box_In_Grid(	&origin,
											static_cast<A_long>(grid_width),
											static_cast<A_long>(grid_height),
											box_across,
											box_down,
											&box_rectR));
			
			ERR(drawbotSuites.supplier_suiteP->NewPath(supplier_ref, &path_ref));
			
			DRAWBOT_RectF32		rectR;
			PFToDrawbotRect(&box_rectR, &rectR);

			ERR(drawbotSuites.path_suiteP->AddRect(path_ref, &rectR));
			
			ERR(drawbotSuites.surface_suiteP->StrokePath(surface_ref, pen_ref, path_ref));
			
			if (path_ref) {
				ERR2(drawbotSuites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path_ref));
			}
		}
	}
	
	if (pen_ref) {
		ERR2(drawbotSuites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)pen_ref));
	}
	
	// Release the earlier acquired drawbot suites
	ERR2(AEFX_ReleaseDrawbotSuites(in_data, out_data));

	event_extra->evt_out_flags	= PF_EO_HANDLED_EVENT;

	return err;
}



