/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/



/** AEGP_SuiteHandler.h

	DEPRECATED:
		This way of doing things is out of date.  See AEFX_SuiteHandlerTemplate.h for the
		new way of doing things.

		-kjw 2/28/2014

	A very helpful class that manages demand loading and automatic, exception-safe freeing
	of AEGP suites.

	USAGE INSTRUCTIONS:

		The accompanying file, AEGP_SuiteHandler.cpp, is designed to be compiled right into
		the client application or plug-in.

		You'll get a link error.

		This is because AEGP_SuiteHandler.cpp lacks a definition for the MissingSuiteError()
		method. You must provide one to define the error handling behaviour of the class.
		This function may or may not display an error message etc. but it must end
		by throwing an exception. It cannot return.

		Other than that, usage is pretty trivial. Construct with a pointer to the PICA
		basic suite, and then call the public method to obtain lazily loaded pointers
		to other AEGP suites. Upon desctruction, all loaded suites are freed (so this class
		is really handy for writing exception-safe AEGP code.)

		NOTE!!!  If you need to upgrade a suite, DO NOT SIMPLY UPDATE THE VERSION NUMBER.
		You should:
			1.  Add a new member to the Suites structure for that suite.
			2.  Add the boiler plate macro to release the suite in ReleaseAllSuites (AEGP_SUITE_RELEASE_BOILERPLATE).
			3.  Add the boiler plate macro to define the suite. (AEGP_SUITE_ACCESS_BOILERPLATE)

		If you have any questions, ask me. -jja 5/7/2004

		If you'll be using ADM suites, #define I_NEED_ADM_SUPPORT before #including AEGP_SuiteHandler.h.

				-bbb 9/16/2004
**/

#ifndef _H_AEGP_SUITEHANDLER
#define _H_AEGP_SUITEHANDLER

#include <AE_GeneralPlug.h>
#include <AE_EffectSuites.h>
#include <AE_AdvEffectSuites.h>
#include <AE_EffectCBSuites.h>
#include <AE_EffectSuitesHelper.h>
#include <adobesdk/DrawbotSuite.h>
#include <SPSuites.h>

#ifdef	I_NEED_ADM_SUPPORT
#include <ADMBasic.h>
#include <ADMDialog.h>
#include <ADMDialogGroup.h>
#include <ADMItem.h>
#include <ADMList.h>
#include <ADMEntry.h>
#include <ADMNotifier.h>
#endif

// Suite registration and handling object
class AEGP_SuiteHandler {

private:
	// forbid copy construct
	AEGP_SuiteHandler(const AEGP_SuiteHandler&) {}
	AEGP_SuiteHandler& operator=(const AEGP_SuiteHandler&) { return *this; }

	// basic suite pointer
	const SPBasicSuite				*i_pica_basicP;

	// Suites we can register. These are mutable because they are demand loaded using a const object.

	struct Suites {
		AEGP_KeyframeSuite5			*keyframe_suite5P;
		AEGP_KeyframeSuite4			*keyframe_suite4P;
		AEGP_StreamSuite3			*stream_suite3P;
		AEGP_StreamSuite4			*stream_suite4P;
		AEGP_StreamSuite5			*stream_suite5P;
		AEGP_StreamSuite6			*stream_suite6P;
		AEGP_MarkerSuite1			*marker_suite1P;
		AEGP_MarkerSuite2			*marker_suite2P;
		AEGP_MarkerSuite3			*marker_suite3P;
		AEGP_CompSuite4				*comp_suite4P;
		AEGP_CompSuite5				*comp_suite5P;
		AEGP_CompSuite6				*comp_suite6P;
		AEGP_CompSuite7				*comp_suite7P;
		AEGP_CompSuite8				*comp_suite8P;
		AEGP_CompSuite9				*comp_suite9P;
		AEGP_CompSuite10			*comp_suite10P;
		AEGP_CompSuite11			*comp_suite11P;
		AEGP_LayerSuite3			*layer_suite3P;
		AEGP_LayerSuite4			*layer_suite4P;
		AEGP_StreamSuite2			*stream_suite2P;
		AEGP_DynamicStreamSuite2	*dynamic_stream_suite2P;
		AEGP_DynamicStreamSuite3	*dynamic_stream_suite3P;
		AEGP_DynamicStreamSuite4	*dynamic_stream_suite4P;
		AEGP_KeyframeSuite3			*keyframe_suite3P;
		AEGP_CanvasSuite5			*canvas_suite5P;
		AEGP_CanvasSuite6			*canvas_suite6P;
		AEGP_CanvasSuite7			*canvas_suite7P;
		AEGP_CanvasSuite8			*canvas_suite8P;
		AEGP_CameraSuite2			*camera_suite2P;
		AEGP_RegisterSuite5			*register_suite5P;
		AEGP_MemorySuite1			*memory_suite1P;
		AEGP_ItemViewSuite1			*item_view_suite1P;
		AEGP_ItemSuite9				*item_suite9P;
		AEGP_ItemSuite8				*item_suite8P;
		AEGP_ItemSuite7				*item_suite7P;
		AEGP_ItemSuite6				*item_suite6P;
		AEGP_ItemSuite5				*item_suite5P;
		AEGP_ItemSuite1				*item_suite1P;
		AEGP_LightSuite1			*light_suite1P;
		AEGP_LightSuite2			*light_suite2P;
		AEGP_EffectSuite1			*effect_suite1P;
		AEGP_EffectSuite2			*effect_suite2P;
		AEGP_EffectSuite3			*effect_suite3P;
		AEGP_EffectSuite4			*effect_suite4P;
		AEGP_MaskSuite4				*mask_suite4P;
		AEGP_MaskOutlineSuite1		*mask_outline_suite1P;
		AEGP_MaskOutlineSuite2		*mask_outline_suite2P;
		AEGP_MaskOutlineSuite3		*mask_outline_suite3P;
		AEGP_CommandSuite1			*command_suite1P;
		AEGP_UtilitySuite3			*utility_suite3P;
		AEGP_RenderSuite1			*render_suite1P;
		AEGP_RenderSuite2			*render_suite2P;
		AEGP_RenderSuite3			*render_suite3P;
		AEGP_RenderSuite4			*render_suite4P;
		AEGP_RenderSuite5			*render_suite5P;
		PF_ANSICallbacksSuite1		*ansi_callbacks_suite1P;
		PF_HandleSuite1				*handle_suite1P;
		PF_FillMatteSuite2			*fill_matte_suite2P;
		PF_WorldTransformSuite1		*world_transform_suite1P;
		AEGP_QueryXformSuite2		*query_xform_suite2P;
		AEGP_CompositeSuite2		*composite_suite2P;
		PF_WorldSuite1				*world_suite1P;
		AEGP_PFInterfaceSuite1		*pf_interface_suite1P;
		AEGP_MathSuite1			*math_suite1P;
		PF_AdvTimeSuite4			*adv_time_suite4P;
		PF_PathQuerySuite1			*path_query_suite1P;
		PF_PathDataSuite1			*path_data_suite1P;
		PF_ParamUtilsSuite3			*param_utils_suite3P;
		PFAppSuite4					*app_suite4P;
		PFAppSuite5					*app_suite5P;
		PFAppSuite6					*app_suite6P;
		PF_AdvAppSuite2				*adv_app_suite2P;
		AEGP_IOInSuite4				*io_in_suite4P;
		AEGP_IOOutSuite4			*io_out_suite4P;
		AEGP_PersistentDataSuite3	*persistent_data_suite3P;
		AEGP_PersistentDataSuite4	*persistent_data_suite4P;
		AEGP_RenderQueueSuite1		*render_queue_suite1P;
		AEGP_RQItemSuite2			*rq_item_suite2P;
		AEGP_OutputModuleSuite4		*output_module_suite4P;
		AEGP_FIMSuite3				*fim_suite3P;
		PF_Sampling8Suite1			*sampling_8_suite1P;
		PF_Sampling16Suite1			*sampling_16_suite1P;
		PF_Iterate8Suite1			*iterate_8_suite1P;
		PF_iterate16Suite1			*iterate_16_suite1P;
		PF_iterateFloatSuite1		*iterate_float_suite1P;
		PF_Iterate8Suite2			*iterate_8_suite2P;
		PF_iterate16Suite2			*iterate_16_suite2P;
		PF_iterateFloatSuite2		*iterate_float_suite2P;
		AEGP_CollectionSuite2		*collection_suite2P;
		AEGP_TextDocumentSuite1		*text_document_suite1P;
		AEGP_SoundDataSuite1		*sound_data_suite1P;
		AEGP_IterateSuite1			*aegp_iterate_suite1P;
		AEGP_IterateSuite2			*aegp_iterate_suite2P;
		AEGP_TextLayerSuite1		*text_layer_suite1P;
		AEGP_ArtisanUtilSuite1		*artisan_util_suite1P;
		AEGP_WorldSuite2			*aegp_world_suite_2P;
		AEGP_WorldSuite3			*aegp_world_suite_3P;
		AEGP_RenderOptionsSuite1	*render_options_suite_1P;
		AEGP_LayerRenderOptionsSuite1	*layer_render_options_suite_1P;
		AEGP_LayerRenderOptionsSuite2	*layer_render_options_suite_2P;
		AEGP_RenderAsyncManagerSuite1		*async_manager_suite_1P;
		AEGP_TrackerSuite1			*tracker_suite_1P;
		AEGP_TrackerUtilitySuite1	*tracker_utility_suite_1P;
		PF_HelperSuite2				*helper_suite_2P;
		AEGP_LayerSuite5			*layer_suite_5P;
		AEGP_LayerSuite6			*layer_suite_6P;
		AEGP_LayerSuite7			*layer_suite_7P;
		AEGP_LayerSuite8			*layer_suite_8P;
		AEGP_LayerSuite9			*layer_suite_9P;

#ifdef	I_NEED_ADM_SUPPORT
		ADMBasicSuite8				*adm_basic_suite_8P;
		ADMDialogSuite8				*adm_dialog_suite_8P;
		ADMDialogGroupSuite3		*adm_dialog_group_suite_3P;
		ADMItemSuite8				*adm_item_suite_8P;
		ADMListSuite3				*adm_list_suite_3P;
		ADMEntrySuite5				*adm_entry_suite_5P;
		ADMNotifierSuite2			*adm_notifier_suite_2P;
#endif
		AEGP_LayerSuite1			*layer_suite_1P;
		AEGP_MaskSuite1				*mask_suite_1P;
		AEGP_MaskSuite5				*mask_suite_5P;
		AEGP_MaskSuite6				*mask_suite_6P;
		AEGP_StreamSuite1			*stream_suite_1P;
		AEGP_CompSuite1				*comp_suite_1P;
		AEGP_CollectionSuite1		*collection_suite_1P;
		AEGP_KeyframeSuite1			*keyframe_suite_1P;
		PF_AdvAppSuite1				*adv_app_suite_1P;
		AEGP_UtilitySuite1			*utility_suite_1P;
		AEGP_RenderOptionsSuite2	*render_options_suite_2P;
		AEGP_ProjSuite5				*proj_suite_5P;
		AEGP_ProjSuite6				*proj_suite_6P;
		AEGP_FootageSuite5			*footage_suite_5P;
		AEGP_RQItemSuite3			*rq_item_suite_3P;
		AEGP_UtilitySuite4			*utility_suite_4P;
		AEGP_ColorSettingsSuite4	*color_settings_suite_4P;
		AEGP_ColorSettingsSuite3	*color_settings_suite_3P;
		AEGP_ColorSettingsSuite2	*color_settings_suite_2P;
		AEGP_ColorSettingsSuite1	*color_settings_suite_1P;
		PF_AdvItemSuite1			*adv_item_suite_1P;
		AEGP_RenderOptionsSuite3	*render_options_suite_3P;
		PF_ColorParamSuite1			*color_param_suite_1P;
		PF_SamplingFloatSuite1		*sampling_float_suite_1P;
		AEGP_UtilitySuite5			*utility_suite_5P;
		AEGP_UtilitySuite6			*utility_suite_6P;
		PF_EffectCustomUISuite1		*custom_ui_suite1P;
		PF_EffectCustomUISuite2		*custom_ui_suite2P;
		PF_EffectCustomUIOverlayThemeSuite1	*custom_ui_theme_suite1P;

		//Drawbot Suites
		DRAWBOT_DrawbotSuiteCurrent		*drawing_suite_currentP;
		DRAWBOT_SupplierSuiteCurrent	*drawbot_supplier_suite_currentP;
		DRAWBOT_SurfaceSuiteCurrent		*drawbot_surface_suite_currentP;
		DRAWBOT_PathSuiteCurrent		*drawbot_path_suite_currentP;

		SPSuitesSuite 					*suites_suite_2P;
	};

	mutable Suites i_suites;

	// private methods
	// I had to make this inline by moving the definition from the .cpp file
	// CW mach-o target was freaking otherwise when seeing the call to this
	// function in inlined public suite accessors below

	void *LoadSuite(const A_char *nameZ, A_long versionL) const
	{
		const void *suiteP;
		A_long err = i_pica_basicP->AcquireSuite(nameZ, versionL, &suiteP);

		if (err || !suiteP) {
			MissingSuiteError();
		}

		return (void*)suiteP;
	}

	void ReleaseSuite(const A_char *nameZ, A_long versionL);
	void ReleaseAllSuites()
	{
		#define	AEGP_SUITE_RELEASE_BOILERPLATE(MEMBER_NAME, kSUITE_NAME, kVERSION_NAME)		\
			if (i_suites.MEMBER_NAME) {														\
				ReleaseSuite(kSUITE_NAME, kVERSION_NAME);									\
			}

		AEGP_SUITE_RELEASE_BOILERPLATE(marker_suite1P, kAEGPMarkerSuite, kAEGPMarkerSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(marker_suite2P, kAEGPMarkerSuite, kAEGPMarkerSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(marker_suite3P, kAEGPMarkerSuite, kAEGPMarkerSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_suite3P, kAEGPLayerSuite, kAEGPLayerSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_suite4P, kAEGPLayerSuite, kAEGPLayerSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(stream_suite6P, kAEGPStreamSuite, kAEGPStreamSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(stream_suite5P, kAEGPStreamSuite, kAEGPStreamSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(stream_suite4P, kAEGPStreamSuite, kAEGPStreamSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(stream_suite3P, kAEGPStreamSuite, kAEGPStreamSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(stream_suite2P, kAEGPStreamSuite, kAEGPStreamSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(stream_suite_1P, kAEGPStreamSuite, kAEGPStreamSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(dynamic_stream_suite2P, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(dynamic_stream_suite3P, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(dynamic_stream_suite4P, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(keyframe_suite5P, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(keyframe_suite4P, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(keyframe_suite3P, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(keyframe_suite_1P, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite4P, kAEGPCompSuite, kAEGPCompSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite5P, kAEGPCompSuite, kAEGPCompSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite6P, kAEGPCompSuite, kAEGPCompSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite7P, kAEGPCompSuite, kAEGPCompSuiteVersion7);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite8P, kAEGPCompSuite, kAEGPCompSuiteVersion8);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite9P, kAEGPCompSuite, kAEGPCompSuiteVersion9);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite10P, kAEGPCompSuite, kAEGPCompSuiteVersion10);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite11P, kAEGPCompSuite, kAEGPCompSuiteVersion11);
		AEGP_SUITE_RELEASE_BOILERPLATE(canvas_suite5P, kAEGPCanvasSuite, kAEGPCanvasSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(canvas_suite6P, kAEGPCanvasSuite, kAEGPCanvasSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(canvas_suite7P, kAEGPCanvasSuite, kAEGPCanvasSuiteVersion7);
		AEGP_SUITE_RELEASE_BOILERPLATE(canvas_suite8P, kAEGPCanvasSuite, kAEGPCanvasSuiteVersion8);
		AEGP_SUITE_RELEASE_BOILERPLATE(camera_suite2P, kAEGPCameraSuite, kAEGPCameraSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(register_suite5P, kAEGPRegisterSuite, kAEGPRegisterSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(item_view_suite1P, kAEGPItemViewSuite, kAEGPItemViewSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(item_suite8P, kAEGPItemSuite, kAEGPItemSuiteVersion8);
		AEGP_SUITE_RELEASE_BOILERPLATE(item_suite7P, kAEGPItemSuite, kAEGPItemSuiteVersion7);
		AEGP_SUITE_RELEASE_BOILERPLATE(item_suite6P, kAEGPItemSuite, kAEGPItemSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(item_suite5P, kAEGPItemSuite, kAEGPItemSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(item_suite1P, kAEGPItemSuite, kAEGPItemSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_interface_suite1P, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(math_suite1P, kAEGPMathSuite, kAEGPMathSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(adv_time_suite4P, kPFAdvTimeSuite, kPFAdvTimeSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(path_query_suite1P, kPFPathQuerySuite, kPFPathQuerySuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(memory_suite1P, kAEGPMemorySuite, kAEGPMemorySuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(path_data_suite1P, kPFPathDataSuite, kPFPathDataSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(param_utils_suite3P, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(app_suite4P, kPFAppSuite, kPFAppSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(app_suite5P, kPFAppSuite, kPFAppSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(app_suite6P, kPFAppSuite, kPFAppSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(adv_app_suite2P, kPFAdvAppSuite, kPFAdvAppSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(light_suite1P, kAEGPLightSuite, kAEGPLightSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(light_suite2P, kAEGPLightSuite, kAEGPLightSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(effect_suite1P, kAEGPEffectSuite, kAEGPEffectSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(effect_suite2P, kAEGPEffectSuite, kAEGPEffectSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(effect_suite3P, kAEGPEffectSuite, kAEGPEffectSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(effect_suite4P, kAEGPEffectSuite, kAEGPEffectSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(mask_suite4P, kAEGPMaskSuite, kAEGPMaskSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(mask_outline_suite1P, kAEGPMaskOutlineSuite, kAEGPMaskOutlineSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(mask_outline_suite2P, kAEGPMaskOutlineSuite, kAEGPMaskOutlineSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(mask_outline_suite3P, kAEGPMaskOutlineSuite, kAEGPMaskOutlineSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(command_suite1P, kAEGPCommandSuite, kAEGPCommandSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(utility_suite3P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_suite1P, kAEGPRenderSuite, kAEGPRenderSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_suite2P, kAEGPRenderSuite, kAEGPRenderSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_suite3P, kAEGPRenderSuite, kAEGPRenderSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_suite4P, kAEGPRenderSuite, kAEGPRenderSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_suite5P, kAEGPRenderSuite, kAEGPRenderSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(ansi_callbacks_suite1P, kPFANSISuite, kPFANSISuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(handle_suite1P, kPFHandleSuite, kPFHandleSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(fill_matte_suite2P, kPFFillMatteSuite, kPFFillMatteSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(world_transform_suite1P, kPFWorldTransformSuite, kPFWorldTransformSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(query_xform_suite2P, kAEGPQueryXformSuite, kAEGPQueryXformSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(composite_suite2P, kAEGPCompositeSuite, kAEGPCompositeSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(world_suite1P, kPFWorldSuite, kPFWorldSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(io_in_suite4P, kAEGPIOInSuite, kAEGPIOInSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(io_out_suite4P, kAEGPIOOutSuite, kAEGPIOOutSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_queue_suite1P, kAEGPRenderQueueSuite, kAEGPRenderQueueSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(rq_item_suite2P, kAEGPRQItemSuite, kAEGPRQItemSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(output_module_suite4P, kAEGPOutputModuleSuite, kAEGPOutputModuleSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(fim_suite3P, kAEGPFIMSuite, kAEGPFIMSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(math_suite1P, kAEGPMathSuite, kAEGPMathSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(adv_time_suite4P, kPFAdvTimeSuite, kPFAdvTimeSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(sampling_8_suite1P, kPFSampling8Suite, kPFSampling8SuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(sampling_16_suite1P, kPFSampling16Suite, kPFSampling16SuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(iterate_8_suite1P, kPFIterate8Suite, kPFIterate8SuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(iterate_16_suite1P, kPFIterate16Suite, kPFIterate16SuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(iterate_float_suite1P, kPFIterateFloatSuite, kPFIterateFloatSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(iterate_8_suite2P, kPFIterate8Suite, kPFIterate8SuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(iterate_16_suite2P, kPFIterate16Suite, kPFIterate16SuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(iterate_float_suite2P, kPFIterateFloatSuite, kPFIterateFloatSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(collection_suite2P, kAEGPCollectionSuite, kAEGPCollectionSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(text_document_suite1P, kAEGPTextDocumentSuite, kAEGPTextDocumentSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(sound_data_suite1P, kAEGPSoundDataSuite, kAEGPSoundDataVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(text_layer_suite1P, kAEGPTextLayerSuite, kAEGPTextLayerSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(artisan_util_suite1P, kAEGPArtisanUtilSuite, kAEGPArtisanUtilSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(aegp_world_suite_2P, kAEGPWorldSuite, kAEGPWorldSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(aegp_world_suite_3P, kAEGPWorldSuite, kAEGPWorldSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_options_suite_1P, kAEGPRenderOptionsSuite, kAEGPRenderOptionsSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(tracker_suite_1P, kAEGPTrackerSuite, kAEGPTrackerSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(tracker_utility_suite_1P, kAEGPTrackerUtilitySuite, kAEGPTrackerUtilitySuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(helper_suite_2P, kPFHelperSuite2, kPFHelperSuite2Version2);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_suite_5P, kAEGPLayerSuite, kAEGPLayerSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_suite_6P, kAEGPLayerSuite, kAEGPLayerSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_suite_7P, kAEGPLayerSuite, kAEGPLayerSuiteVersion7);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_suite_8P, kAEGPLayerSuite, kAEGPLayerSuiteVersion8);
		AEGP_SUITE_RELEASE_BOILERPLATE(adv_item_suite_1P, kPFAdvItemSuite, kPFAdvItemSuiteVersion1);
#ifdef	I_NEED_ADM_SUPPORT
		AEGP_SUITE_RELEASE_BOILERPLATE(adm_basic_suite_8P, kADMBasicSuite, kADMBasicSuiteVersion8);
		AEGP_SUITE_RELEASE_BOILERPLATE(adm_dialog_suite_8P, kADMDialogSuite, kADMDialogSuiteVersion8);
		AEGP_SUITE_RELEASE_BOILERPLATE(adm_dialog_group_suite_3P, kADMDialogGroupSuite, kADMDialogGroupSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(adm_item_suite_8P, kADMItemSuite, kADMItemSuiteVersion8);
		AEGP_SUITE_RELEASE_BOILERPLATE(adm_list_suite_3P, kADMListSuite, kADMListSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(adm_entry_suite_5P, kADMEntrySuite, kADMEntrySuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(adm_notifier_suite_2P, kADMNotifierSuite, kADMNotifierSuiteVersion2);
#endif
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_suite_1P, kAEGPLayerSuite, kAEGPLayerSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(mask_suite_1P, kAEGPMaskSuite, kAEGPMaskSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(mask_suite_5P, kAEGPMaskSuite, kAEGPMaskSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(mask_suite_6P, kAEGPMaskSuite, kAEGPMaskSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite_1P, kAEGPCompSuite, kAEGPCompSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(collection_suite_1P, kAEGPCollectionSuite, kAEGPCollectionSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(adv_app_suite_1P, kPFAdvAppSuite, kPFAdvAppSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(utility_suite_1P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_options_suite_2P, kAEGPRenderOptionsSuite, kAEGPRenderOptionsSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_render_options_suite_1P, kAEGPLayerRenderOptionsSuite, kAEGPLayerRenderOptionsSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_render_options_suite_2P, kAEGPLayerRenderOptionsSuite, kAEGPLayerRenderOptionsSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(async_manager_suite_1P, kAEGPRenderAsyncManagerSuite, kAEGPRenderAsyncManagerSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(proj_suite_5P, kAEGPProjSuite, kAEGPProjSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(proj_suite_6P, kAEGPProjSuite, kAEGPProjSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(footage_suite_5P, kAEGPFootageSuite, kAEGPFootageSuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(rq_item_suite_3P, kAEGPRQItemSuite, kAEGPRQItemSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(utility_suite_4P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(persistent_data_suite4P, kAEGPPersistentDataSuite, kAEGPPersistentDataSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(persistent_data_suite3P, kAEGPPersistentDataSuite, kAEGPPersistentDataSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(color_settings_suite_4P, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion4);
		AEGP_SUITE_RELEASE_BOILERPLATE(color_settings_suite_3P, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion3);
		AEGP_SUITE_RELEASE_BOILERPLATE(color_settings_suite_2P, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(color_settings_suite_1P, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(color_param_suite_1P, kPFColorParamSuite, kPFColorParamSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(sampling_float_suite_1P, kPFSamplingFloatSuite, kPFSamplingFloatSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(utility_suite_5P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion5);
		AEGP_SUITE_RELEASE_BOILERPLATE(utility_suite_6P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(custom_ui_suite1P, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(custom_ui_suite2P, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion2);
		AEGP_SUITE_RELEASE_BOILERPLATE(custom_ui_theme_suite1P, kPFEffectCustomUIOverlayThemeSuite, kPFEffectCustomUIOverlayThemeSuiteVersion1);
		AEGP_SUITE_RELEASE_BOILERPLATE(drawing_suite_currentP, kDRAWBOT_DrawSuite, kDRAWBOT_DrawSuite_VersionCurrent);
		AEGP_SUITE_RELEASE_BOILERPLATE(drawbot_supplier_suite_currentP, kDRAWBOT_SupplierSuite, kDRAWBOT_SupplierSuite_VersionCurrent);
		AEGP_SUITE_RELEASE_BOILERPLATE(drawbot_surface_suite_currentP, kDRAWBOT_SurfaceSuite, kDRAWBOT_SurfaceSuite_VersionCurrent);
		AEGP_SUITE_RELEASE_BOILERPLATE(drawbot_path_suite_currentP, kDRAWBOT_PathSuite, kDRAWBOT_PathSuite_VersionCurrent);
		AEGP_SUITE_RELEASE_BOILERPLATE(suites_suite_2P, kSPSuitesSuite, kSPSuitesSuiteVersion);
}

	// Here is the error handling function which must be defined.
	// It must exit by throwing an exception, it cannot return.
	void MissingSuiteError() const;

public:
	// To construct, pass pica_basicP
	AEGP_SuiteHandler(const SPBasicSuite *pica_basicP);
	~AEGP_SuiteHandler();

	const SPBasicSuite *Pica()	const	{ return i_pica_basicP; }

	#define	AEGP_SUITE_ACCESS_BOILERPLATE(SUITE_NAME, VERSION_NUMBER, SUITE_PREFIX, MEMBER_NAME, kSUITE_NAME, kVERSION_NAME)	\
		SUITE_PREFIX##SUITE_NAME##VERSION_NUMBER *SUITE_NAME##VERSION_NUMBER() const											\
	{																															\
		if (i_suites.MEMBER_NAME == NULL) {																						\
			i_suites.MEMBER_NAME = (SUITE_PREFIX##SUITE_NAME##VERSION_NUMBER*)													\
										LoadSuite(kSUITE_NAME, kVERSION_NAME);													\
		}																														\
		return i_suites.MEMBER_NAME;																							\
	}

	AEGP_SUITE_ACCESS_BOILERPLATE(MarkerSuite, 1, AEGP_, marker_suite1P, kAEGPMarkerSuite, kAEGPMarkerSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(MarkerSuite, 2, AEGP_, marker_suite2P, kAEGPMarkerSuite, kAEGPMarkerSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(MarkerSuite, 3, AEGP_, marker_suite3P, kAEGPMarkerSuite, kAEGPMarkerSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, 3, AEGP_, layer_suite3P, kAEGPLayerSuite, kAEGPLayerSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, 4, AEGP_, layer_suite4P, kAEGPLayerSuite, kAEGPLayerSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(StreamSuite, 6, AEGP_, stream_suite6P, kAEGPStreamSuite, kAEGPStreamSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(StreamSuite, 5, AEGP_, stream_suite5P, kAEGPStreamSuite, kAEGPStreamSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(StreamSuite, 4, AEGP_, stream_suite4P, kAEGPStreamSuite, kAEGPStreamSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(StreamSuite, 3, AEGP_, stream_suite3P, kAEGPStreamSuite, kAEGPStreamSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(StreamSuite, 2, AEGP_, stream_suite2P, kAEGPStreamSuite, kAEGPStreamSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(StreamSuite, 1, AEGP_, stream_suite_1P, kAEGPStreamSuite, kAEGPStreamSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(DynamicStreamSuite, 2, AEGP_, dynamic_stream_suite2P, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(DynamicStreamSuite, 3, AEGP_, dynamic_stream_suite3P, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(DynamicStreamSuite, 4, AEGP_, dynamic_stream_suite4P, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(KeyframeSuite, 5, AEGP_, keyframe_suite5P, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(KeyframeSuite, 4, AEGP_, keyframe_suite4P, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(KeyframeSuite, 3, AEGP_, keyframe_suite3P, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(KeyframeSuite, 1, AEGP_, keyframe_suite_1P, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 4, AEGP_, comp_suite4P, kAEGPCompSuite, kAEGPCompSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 5, AEGP_, comp_suite5P, kAEGPCompSuite, kAEGPCompSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 6, AEGP_, comp_suite6P, kAEGPCompSuite, kAEGPCompSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 7, AEGP_, comp_suite7P, kAEGPCompSuite, kAEGPCompSuiteVersion7);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 8, AEGP_, comp_suite8P, kAEGPCompSuite, kAEGPCompSuiteVersion8);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 9, AEGP_, comp_suite9P, kAEGPCompSuite, kAEGPCompSuiteVersion9);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 10, AEGP_, comp_suite10P, kAEGPCompSuite, kAEGPCompSuiteVersion10);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 11, AEGP_, comp_suite11P, kAEGPCompSuite, kAEGPCompSuiteVersion11);
	AEGP_SUITE_ACCESS_BOILERPLATE(CanvasSuite, 5, AEGP_, canvas_suite5P, kAEGPCanvasSuite, kAEGPCanvasSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(CanvasSuite, 6, AEGP_, canvas_suite6P, kAEGPCanvasSuite, kAEGPCanvasSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(CanvasSuite, 7, AEGP_, canvas_suite7P, kAEGPCanvasSuite, kAEGPCanvasSuiteVersion7);
	AEGP_SUITE_ACCESS_BOILERPLATE(CanvasSuite, 8, AEGP_, canvas_suite8P, kAEGPCanvasSuite, kAEGPCanvasSuiteVersion8);
	AEGP_SUITE_ACCESS_BOILERPLATE(CameraSuite, 2, AEGP_, camera_suite2P, kAEGPCameraSuite, kAEGPCameraSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(RegisterSuite, 5, AEGP_, register_suite5P, kAEGPRegisterSuite, kAEGPRegisterSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(MemorySuite, 1, AEGP_, memory_suite1P, kAEGPMemorySuite, kAEGPMemorySuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemViewSuite, 1, AEGP_, item_view_suite1P, kAEGPItemViewSuite, kAEGPItemViewSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemSuite, 9, AEGP_, item_suite9P, kAEGPItemSuite, kAEGPItemSuiteVersion9);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemSuite, 8, AEGP_, item_suite8P, kAEGPItemSuite, kAEGPItemSuiteVersion8);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemSuite, 7, AEGP_, item_suite7P, kAEGPItemSuite, kAEGPItemSuiteVersion7);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemSuite, 6, AEGP_, item_suite6P, kAEGPItemSuite, kAEGPItemSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemSuite, 5, AEGP_, item_suite5P, kAEGPItemSuite, kAEGPItemSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemSuite, 1, AEGP_, item_suite1P, kAEGPItemSuite, kAEGPItemSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFInterfaceSuite, 1, AEGP_, pf_interface_suite1P, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(MathSuite, 1, AEGP_, math_suite1P, kAEGPMathSuite, kAEGPMathSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(AdvTimeSuite, 4, PF_, adv_time_suite4P, kPFAdvTimeSuite, kPFAdvTimeSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(PathQuerySuite, 1, PF_, path_query_suite1P, kPFPathQuerySuite, kPFPathQuerySuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(PathDataSuite, 1, PF_, path_data_suite1P, kPFPathDataSuite, kPFPathDataSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(ParamUtilsSuite, 3, PF_, param_utils_suite3P, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(AppSuite, 4, PF, app_suite4P, kPFAppSuite, kPFAppSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(AppSuite, 5, PF, app_suite5P, kPFAppSuite, kPFAppSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(AppSuite, 6, PF, app_suite6P, kPFAppSuite, kPFAppSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(AdvAppSuite, 2, PF_, adv_app_suite2P, kPFAdvAppSuite, kPFAdvAppSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(LightSuite, 1, AEGP_, light_suite1P, kAEGPLightSuite, kAEGPLightSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(LightSuite, 2, AEGP_, light_suite2P, kAEGPLightSuite, kAEGPLightSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(EffectSuite, 1, AEGP_, effect_suite1P, kAEGPEffectSuite, kAEGPEffectSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(EffectSuite, 2, AEGP_, effect_suite2P, kAEGPEffectSuite, kAEGPEffectSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(EffectSuite, 3, AEGP_, effect_suite3P, kAEGPEffectSuite, kAEGPEffectSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(EffectSuite, 4, AEGP_, effect_suite4P, kAEGPEffectSuite, kAEGPEffectSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(MaskSuite, 4, AEGP_, mask_suite4P, kAEGPMaskSuite, kAEGPMaskSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(MaskOutlineSuite, 1, AEGP_, mask_outline_suite1P, kAEGPMaskOutlineSuite, kAEGPMaskOutlineSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(MaskOutlineSuite, 2, AEGP_, mask_outline_suite2P, kAEGPMaskOutlineSuite, kAEGPMaskOutlineSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(MaskOutlineSuite, 3, AEGP_, mask_outline_suite3P, kAEGPMaskOutlineSuite, kAEGPMaskOutlineSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(CommandSuite, 1, AEGP_, command_suite1P, kAEGPCommandSuite, kAEGPCommandSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(UtilitySuite, 3, AEGP_, utility_suite3P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderSuite, 1, AEGP_, render_suite1P, kAEGPRenderSuite, kAEGPRenderSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderSuite, 2, AEGP_, render_suite2P, kAEGPRenderSuite, kAEGPRenderSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderSuite, 3, AEGP_, render_suite3P, kAEGPRenderSuite, kAEGPRenderSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderSuite, 4, AEGP_, render_suite4P, kAEGPRenderSuite, kAEGPRenderSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderSuite, 5, AEGP_, render_suite5P, kAEGPRenderSuite, kAEGPRenderSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(ANSICallbacksSuite, 1, PF_, ansi_callbacks_suite1P, kPFANSISuite, kPFANSISuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(HandleSuite, 1, PF_, handle_suite1P, kPFHandleSuite, kPFHandleSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(FillMatteSuite, 2, PF_, fill_matte_suite2P, kPFFillMatteSuite, kPFFillMatteSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(WorldTransformSuite, 1, PF_, world_transform_suite1P, kPFWorldTransformSuite, kPFWorldTransformSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(QueryXformSuite, 2, AEGP_, query_xform_suite2P, kAEGPQueryXformSuite, kAEGPQueryXformSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompositeSuite, 2, AEGP_, composite_suite2P, kAEGPCompositeSuite, kAEGPCompositeSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(WorldSuite, 1, PF_, world_suite1P, kPFWorldSuite, kPFWorldSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(IOInSuite, 4, AEGP_, io_in_suite4P, kAEGPIOInSuite, kAEGPIOInSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(IOOutSuite, 4, AEGP_, io_out_suite4P, kAEGPIOOutSuite, kAEGPIOOutSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderQueueSuite, 1, AEGP_, render_queue_suite1P, kAEGPRenderQueueSuite, kAEGPRenderQueueSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(RQItemSuite, 2, AEGP_, rq_item_suite2P, kAEGPRQItemSuite, kAEGPRQItemSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(OutputModuleSuite, 4, AEGP_, output_module_suite4P, kAEGPOutputModuleSuite, kAEGPOutputModuleSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(FIMSuite, 3, AEGP_, fim_suite3P, kAEGPFIMSuite, kAEGPFIMSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(Sampling8Suite, 1, PF_, sampling_8_suite1P, kPFSampling8Suite, kPFSampling8SuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(Sampling16Suite, 1, PF_, sampling_16_suite1P, kPFSampling16Suite, kPFSampling16SuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(Iterate8Suite, 1, PF_, iterate_8_suite1P, kPFIterate8Suite, kPFIterate8SuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(Iterate16Suite, 1, PF_, iterate_16_suite1P, kPFIterate16Suite, kPFIterate16SuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(IterateFloatSuite, 1, PF_, iterate_float_suite1P, kPFIterateFloatSuite, kPFIterateFloatSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(Iterate8Suite, 2, PF_, iterate_8_suite2P, kPFIterate8Suite, kPFIterate8SuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(Iterate16Suite, 2, PF_, iterate_16_suite2P, kPFIterate16Suite, kPFIterate16SuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(IterateFloatSuite, 2, PF_, iterate_float_suite2P, kPFIterateFloatSuite, kPFIterateFloatSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(CollectionSuite, 2, AEGP_, collection_suite2P, kAEGPCollectionSuite, kAEGPCollectionSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(TextDocumentSuite, 1, AEGP_, text_document_suite1P, kAEGPTextDocumentSuite, kAEGPTextDocumentSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(SoundDataSuite, 1, AEGP_, sound_data_suite1P, kAEGPSoundDataSuite, kAEGPSoundDataVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(IterateSuite, 1, AEGP_, aegp_iterate_suite1P, kAEGPIterateSuite, kAEGPIterateSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(IterateSuite, 2, AEGP_, aegp_iterate_suite2P, kAEGPIterateSuite, kAEGPIterateSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(TextLayerSuite, 1, AEGP_, text_layer_suite1P, kAEGPTextLayerSuite, kAEGPTextLayerSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(ArtisanUtilSuite, 1, AEGP_, artisan_util_suite1P, kAEGPArtisanUtilSuite, kAEGPArtisanUtilSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(WorldSuite, 2, AEGP_, aegp_world_suite_2P, kAEGPWorldSuite, kAEGPWorldSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(WorldSuite, 3, AEGP_, aegp_world_suite_3P, kAEGPWorldSuite, kAEGPWorldSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderOptionsSuite, 1, AEGP_, render_options_suite_1P, kAEGPRenderOptionsSuite, kAEGPRenderOptionsSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(TrackerSuite, 1, AEGP_, tracker_suite_1P, kAEGPTrackerSuite, kAEGPTrackerSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(TrackerUtilitySuite, 1, AEGP_, tracker_utility_suite_1P, kAEGPTrackerUtilitySuite, kAEGPTrackerUtilitySuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(HelperSuite, 2, PF_, helper_suite_2P, kPFHelperSuite2, kPFHelperSuite2Version2);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, 5, AEGP_, layer_suite_5P, kAEGPLayerSuite, kAEGPLayerSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, 6, AEGP_, layer_suite_6P, kAEGPLayerSuite, kAEGPLayerSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, 7, AEGP_, layer_suite_7P, kAEGPLayerSuite, kAEGPLayerSuiteVersion7);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, 8, AEGP_, layer_suite_8P, kAEGPLayerSuite, kAEGPLayerSuiteVersion8);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, 9, AEGP_, layer_suite_9P, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
#ifdef	I_NEED_ADM_SUPPORT
	AEGP_SUITE_ACCESS_BOILERPLATE(BasicSuite, 8, ADM, adm_basic_suite_8P, kADMBasicSuite, kADMBasicSuiteVersion8);
	AEGP_SUITE_ACCESS_BOILERPLATE(DialogSuite, 8, ADM, adm_dialog_suite_8P, kADMDialogSuite, kADMDialogSuiteVersion8);
	AEGP_SUITE_ACCESS_BOILERPLATE(DialogGroupSuite, 3, ADM, adm_dialog_group_suite_3P, kADMDialogGroupSuite, kADMDialogGroupSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemSuite, 8, ADM, adm_item_suite_8P, kADMItemSuite, kADMItemSuiteVersion8);
	AEGP_SUITE_ACCESS_BOILERPLATE(ListSuite, 3, ADM, adm_list_suite_3P, kADMListSuite, kADMListSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(EntrySuite, 5, ADM, adm_entry_suite_5P, kADMEntrySuite, kADMEntrySuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(NotifierSuite, 2, ADM, adm_notifier_suite_2P, kADMNotifierSuite, kADMNotifierSuiteVersion2);
#endif
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, 1, AEGP_, layer_suite_1P, kAEGPLayerSuite, kAEGPLayerSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(AdvItemSuite, 1, PF_, adv_item_suite_1P, kPFAdvItemSuite, kPFAdvItemSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(MaskSuite, 1, AEGP_, mask_suite_1P, kAEGPMaskSuite, kAEGPMaskSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(MaskSuite, 5, AEGP_, mask_suite_5P, kAEGPMaskSuite, kAEGPMaskSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(MaskSuite, 6, AEGP_, mask_suite_6P, kAEGPMaskSuite, kAEGPMaskSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, 1, AEGP_, comp_suite_1P, kAEGPCompSuite, kAEGPCompSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(CollectionSuite, 1, AEGP_, collection_suite_1P, kAEGPCollectionSuite, kAEGPCollectionSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(AdvAppSuite, 1, PF_, adv_app_suite_1P, kPFAdvAppSuite, kPFAdvAppSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(UtilitySuite, 1, AEGP_, utility_suite_1P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderOptionsSuite, 2, AEGP_, render_options_suite_2P, kAEGPRenderOptionsSuite, kAEGPRenderOptionsSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderOptionsSuite, 3, AEGP_, render_options_suite_3P, kAEGPRenderOptionsSuite, kAEGPRenderOptionsSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerRenderOptionsSuite, 1, AEGP_, layer_render_options_suite_1P, kAEGPLayerRenderOptionsSuite, kAEGPLayerRenderOptionsSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerRenderOptionsSuite, 2, AEGP_, layer_render_options_suite_2P, kAEGPLayerRenderOptionsSuite, kAEGPLayerRenderOptionsSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderAsyncManagerSuite, 1, AEGP_, async_manager_suite_1P, kAEGPRenderAsyncManagerSuite, kAEGPRenderAsyncManagerSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(ProjSuite, 5, AEGP_, proj_suite_5P, kAEGPProjSuite, kAEGPProjSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(ProjSuite, 6, AEGP_, proj_suite_6P, kAEGPProjSuite, kAEGPProjSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(FootageSuite, 5, AEGP_, footage_suite_5P, kAEGPFootageSuite, kAEGPFootageSuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(RQItemSuite, 3, AEGP_, rq_item_suite_3P, kAEGPRQItemSuite, kAEGPRQItemSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(UtilitySuite, 4, AEGP_, utility_suite_4P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(ColorSettingsSuite, 4, AEGP_, color_settings_suite_4P, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(ColorSettingsSuite, 3, AEGP_, color_settings_suite_3P, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(ColorSettingsSuite, 2, AEGP_, color_settings_suite_2P, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(ColorSettingsSuite, 1, AEGP_, color_settings_suite_1P, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(ColorParamSuite, 1, PF_, color_param_suite_1P, kPFColorParamSuite, kPFColorParamSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(PersistentDataSuite, 4, AEGP_, persistent_data_suite4P, kAEGPPersistentDataSuite, kAEGPPersistentDataSuiteVersion4);
	AEGP_SUITE_ACCESS_BOILERPLATE(PersistentDataSuite, 3, AEGP_, persistent_data_suite3P, kAEGPPersistentDataSuite, kAEGPPersistentDataSuiteVersion3);
	AEGP_SUITE_ACCESS_BOILERPLATE(SamplingFloatSuite, 1, PF_, sampling_float_suite_1P, kPFSamplingFloatSuite, kPFSamplingFloatSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(UtilitySuite, 5, AEGP_, utility_suite_5P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion5);
	AEGP_SUITE_ACCESS_BOILERPLATE(UtilitySuite, 6, AEGP_, utility_suite_6P, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(EffectCustomUISuite, 1, PF_, custom_ui_suite1P, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(EffectCustomUISuite, 2, PF_, custom_ui_suite2P, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion2);
	AEGP_SUITE_ACCESS_BOILERPLATE(EffectCustomUIOverlayThemeSuite, 1, PF_, custom_ui_theme_suite1P, kPFEffectCustomUIOverlayThemeSuite, kPFEffectCustomUIOverlayThemeSuiteVersion1);
	AEGP_SUITE_ACCESS_BOILERPLATE(DrawbotSuite, Current, DRAWBOT_, drawing_suite_currentP, kDRAWBOT_DrawSuite, kDRAWBOT_DrawSuite_VersionCurrent);
	AEGP_SUITE_ACCESS_BOILERPLATE(SupplierSuite, Current, DRAWBOT_, drawbot_supplier_suite_currentP, kDRAWBOT_SupplierSuite, kDRAWBOT_SupplierSuite_VersionCurrent);
	AEGP_SUITE_ACCESS_BOILERPLATE(SurfaceSuite, Current, DRAWBOT_, drawbot_surface_suite_currentP, kDRAWBOT_SurfaceSuite, kDRAWBOT_SurfaceSuite_VersionCurrent);
	AEGP_SUITE_ACCESS_BOILERPLATE(PathSuite, Current, DRAWBOT_, drawbot_path_suite_currentP, kDRAWBOT_PathSuite, kDRAWBOT_PathSuite_VersionCurrent);

	AEGP_SUITE_ACCESS_BOILERPLATE(SuitesSuite, , SP, suites_suite_2P, kSPSuitesSuite, kSPSuitesSuiteVersion);
};

#endif
