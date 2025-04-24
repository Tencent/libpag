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

#include "Custom_ECW_UI.h"
#include "AEFX_SuiteHelper.h"
#include <cmath>


// Function to convert and copy string literals to A_UTF16Char.
// On Win: Pass the input directly to the output
// On Mac: All conversion happens through the CFString format
static void
copyConvertStringLiteralIntoUTF16(
								  const wchar_t* inputString,
								  A_UTF16Char* destination)
{
#ifdef AE_OS_MAC
	int length = wcslen(inputString);
	CFRange	range = {0, 256};
	range.length = length;
	CFStringRef inputStringCFSR = CFStringCreateWithBytes(kCFAllocatorDefault,
														  reinterpret_cast<const UInt8 *>(inputString),
														  length * sizeof(wchar_t),
														  kCFStringEncodingUTF32LE,
														  false);
	CFStringGetBytes(inputStringCFSR,
					 range,
					 kCFStringEncodingUTF16,
					 0,
					 false,
					 reinterpret_cast<UInt8 *>(destination),
					 length * (sizeof (A_UTF16Char)),
					 NULL);
	destination[length] = 0; // Set NULL-terminator, since CFString calls don't set it
	CFRelease(inputStringCFSR);
#elif defined AE_OS_WIN
	size_t length = wcslen(inputString);
	wcscpy_s(reinterpret_cast<wchar_t*>(destination), length + 1, inputString);
#endif
}


static PF_Err
DrawEvent(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extra)
{
	PF_Err					err		=	PF_Err_NONE, err2 = PF_Err_NONE;
	
	DRAWBOT_DrawRef			drawing_ref = NULL;
	DRAWBOT_SurfaceRef		surface_ref = NULL;
	DRAWBOT_SupplierRef		supplier_ref = NULL;
	DRAWBOT_BrushRef		brush_ref = NULL;
	DRAWBOT_BrushRef		string_brush_ref = NULL;
	DRAWBOT_PathRef			path_ref = NULL;
	DRAWBOT_FontRef			font_ref = NULL;

	DRAWBOT_Suites			drawbotSuites;
	DRAWBOT_ColorRGBA		drawbot_color;
	DRAWBOT_RectF32			rectR;
	float					default_font_sizeF = 0.0;

	// Acquire all the Drawbot suites in one go; it should be matched with release routine.
	// You can also use C++ style AEFX_DrawbotSuitesScoper which doesn't need release routine.
	ERR(AEFX_AcquireDrawbotSuites(in_data, out_data, &drawbotSuites));
	
	PF_EffectCustomUISuite1	*effectCustomUISuiteP;

	ERR(AEFX_AcquireSuite(in_data,
							out_data,
							kPFEffectCustomUISuite,
							kPFEffectCustomUISuiteVersion1,
							NULL,
							(void**)&effectCustomUISuiteP));

	if (!err && effectCustomUISuiteP) {
		// Get the drawing reference by passing context to this new api
		ERR((*effectCustomUISuiteP->PF_GetDrawingReference)(event_extra->contextH, &drawing_ref));

		AEFX_ReleaseSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, NULL);
	}

	// Get the Drawbot supplier from drawing reference; it shouldn't be released like pen or brush (see below)
	ERR(drawbotSuites.drawbot_suiteP->GetSupplier(drawing_ref, &supplier_ref));
	
	// Get the Drawbot surface from drawing reference; it shouldn't be released like pen or brush (see below)
	ERR(drawbotSuites.drawbot_suiteP->GetSurface(drawing_ref, &surface_ref));

	// Premiere Pro/Elements does not support a standard parameter type
	// with custom UI (bug #1235407), so we can't use the color values.
	// Use an static grey value instead.
	if (in_data->appl_id != 'PrMr')
	{
		drawbot_color.red = static_cast<float>(params[ECW_UI_COLOR]->u.cd.value.red) / PF_MAX_CHAN8;
		drawbot_color.green = static_cast<float>(params[ECW_UI_COLOR]->u.cd.value.green) / PF_MAX_CHAN8;
		drawbot_color.blue = static_cast<float>(params[ECW_UI_COLOR]->u.cd.value.blue) / PF_MAX_CHAN8;
	}
	else
	{
        // PPro only code. Won't affect Thread-Safety in the AE world
		static float gray = 0;
		drawbot_color.red = fmod(gray, 1);
		drawbot_color.green = fmod(gray, 1);
		drawbot_color.blue = fmod(gray, 1);
		gray += 0.01f;
	}
	drawbot_color.alpha = 1.0;

	if (PF_EA_CONTROL == event_extra->effect_win.area) {
		
		// Create a new path. It should be matched with release routine.
		// You can also use C++ style DRAWBOT_PathP that releases automatically at the end of scope.
		ERR(drawbotSuites.supplier_suiteP->NewPath(supplier_ref, &path_ref));

		// Create a new brush taking color as input; it should be matched with release routine.
		// You can also use C++ style DRAWBOT_BrushP which doesn't require release routine.
		ERR(drawbotSuites.supplier_suiteP->NewBrush(supplier_ref, &drawbot_color, &brush_ref));

		rectR.left = event_extra->effect_win.current_frame.left + 0.5;	// Center of the pixel in new drawing model is (0.5, 0.5)
		rectR.top = event_extra->effect_win.current_frame.top + 0.5;
		rectR.width = static_cast<float>(	event_extra->effect_win.current_frame.right -
											event_extra->effect_win.current_frame.left);
		rectR.height = static_cast<float>(	event_extra->effect_win.current_frame.bottom -
											event_extra->effect_win.current_frame.top);

		// Add the rectangle to path
		ERR(drawbotSuites.path_suiteP->AddRect(path_ref, &rectR));

		// Fill the path with the brush created
		ERR(drawbotSuites.surface_suiteP->FillPath(surface_ref, brush_ref, path_ref, kDRAWBOT_FillType_Default));

		// Get the default font size.
		ERR(drawbotSuites.supplier_suiteP->GetDefaultFontSize(supplier_ref, &default_font_sizeF));

		// Create default font with default size.  Note that you can provide a different font size.
		ERR(drawbotSuites.supplier_suiteP->NewDefaultFont(supplier_ref, default_font_sizeF, &font_ref));

		DRAWBOT_UTF16Char	unicode_string[256];

		copyConvertStringLiteralIntoUTF16(CUSTOM_UI_STRING, unicode_string);
		
		// Draw string with white color
		drawbot_color.red = drawbot_color.green = drawbot_color.blue = drawbot_color.alpha = 1.0;

		ERR(drawbotSuites.supplier_suiteP->NewBrush(supplier_ref, &drawbot_color, &string_brush_ref));

		DRAWBOT_PointF32			text_origin;
		
		text_origin.x = event_extra->effect_win.current_frame.left + 5.0;
		text_origin.y = event_extra->effect_win.current_frame.top + 50.0;

		ERR(drawbotSuites.surface_suiteP->DrawString(	surface_ref, 
														string_brush_ref, 
														font_ref, 
														&unicode_string[0], 
														&text_origin,
														kDRAWBOT_TextAlignment_Default,
														kDRAWBOT_TextTruncation_None,
														0.0f));

		if (string_brush_ref) {
			ERR2(drawbotSuites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(string_brush_ref)));
		}

		if (font_ref) {
			ERR2(drawbotSuites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(font_ref)));
		}

		// Release/destroy the brush. Otherwise, it will lead to memory leak.
		if (brush_ref) {
			ERR2(drawbotSuites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(brush_ref)));
		}
		
		// Release/destroy the path. Otherwise, it will lead to memory leak.
		if (path_ref) {
			ERR2(drawbotSuites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path_ref)));
		}
	}

	// Release the earlier acquired Drawbot suites
	ERR2(AEFX_ReleaseDrawbotSuites(in_data, out_data));
 
	if (!err){
		event_extra->evt_out_flags = PF_EO_HANDLED_EVENT;	
	}

	return err;
}

static PF_Err 
DoDrag(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extra)
{
	PF_Err 			err			= PF_Err_NONE;
	PF_ContextH		contextH	= event_extra->contextH;
	PF_Point		mouse_down;
	
	if (PF_Window_EFFECT == (*contextH)->w_type){
		if (PF_EA_CONTROL == event_extra->effect_win.area) {
			mouse_down = event_extra->u.do_click.screen_point;
			event_extra->u.do_click.continue_refcon[1] = mouse_down.h;
			params[ECW_UI_COLOR]->u.cd.value.red	= (unsigned char)((mouse_down.h << 8) / UI_BOX_WIDTH);
			params[ECW_UI_COLOR]->u.cd.value.blue	= (unsigned char)((mouse_down.v << 8) / UI_BOX_HEIGHT);
			params[ECW_UI_COLOR]->u.cd.value.green	= (unsigned char)(params[ECW_UI_COLOR]->u.cd.value.red + params[ECW_UI_COLOR]->u.cd.value.blue);
			params[ECW_UI_COLOR]->u.cd.value.alpha	= (unsigned char)0xFF;
			params[ECW_UI_COLOR]->uu.change_flags	= PF_ChangeFlag_CHANGED_VALUE;
		}
	}
	return err;
}

static PF_Err 
DoClick(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extra)
{
	PF_Err	err		=	PF_Err_NONE;

	AEGP_SuiteHandler		suites(in_data->pica_basicP);
	PF_ExtendedSuiteTool	tool = PF_ExtendedSuiteTool_MAGNIFY;
	
	// Premiere Pro/Elements does not support this suite
	if (in_data->appl_id != 'PrMr')
	{
		ERR(suites.HelperSuite2()->PF_SetCurrentExtendedTool(tool));
	}
	else
	{
		event_extra->u.adjust_cursor.set_cursor = PF_Cursor_MAGNIFY;
	}

	if (!err){
		event_extra->u.do_click.send_drag 	= TRUE;
		event_extra->evt_out_flags 			= PF_EO_HANDLED_EVENT;
	}
	return err;
}

static PF_Err 
ChangeCursor(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extra)
{
	if (PF_Mod_SHIFT_KEY & event_extra->u.adjust_cursor.modifiers)	{
		event_extra->u.adjust_cursor.set_cursor = PF_Cursor_EYEDROPPER;
	} else {
		if (PF_Mod_CMD_CTRL_KEY & event_extra->u.adjust_cursor.modifiers) {
			event_extra->u.adjust_cursor.set_cursor = PF_Cursor_CROSSHAIRS;
		}
	}
	return PF_Err_NONE;
}

PF_Err 
HandleEvent(	
				PF_InData		*in_data,
				PF_OutData		*out_data,
				PF_ParamDef		*params[],
				PF_LayerDef		*output,
				PF_EventExtra	*extra)
{
	PF_Err		err		= PF_Err_NONE;
	
	switch (extra->e_type) {
		case PF_Event_DO_CLICK:
			err =	DoClick(in_data, 
							out_data, 
							params, 
							output, 
							extra);
			break;
			
		case PF_Event_DRAG:
			err =	DoDrag(	in_data, 
							out_data, 
							params, 
							output, 
							extra);
			break;
			
		case PF_Event_DRAW:
			err =	DrawEvent(	in_data, 
								out_data, 
								params, 
								output, 
								extra);
			break;
			
		case PF_Event_ADJUST_CURSOR:
			err	=	ChangeCursor(	in_data, 
									out_data, 
									params, 
									output, 
									extra);
			
			break;
		default:
			break;
	}
	return err;
}

