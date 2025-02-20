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

#include "Projector.h"

static AEGP_PluginID	S_my_id				= 0;

static SPBasicSuite		*S_sp_basic_suiteP 	= 0;


static A_Err DeathHook(	
	AEGP_GlobalRefcon 	unused1 ,
	AEGP_DeathRefcon 	unused2)
{
	A_Err 					err		= A_Err_NONE,
							err2	= A_Err_NONE;
	A_long 					countL	= 0L,
							sizeL	= 0L;
	A_char					infoZ[AEGP_MAX_ABOUT_STRING_SIZE] = {'\0'};
	AEGP_SuiteHandler		suites(S_sp_basic_suiteP);
	
	try {
		ERR(suites.MemorySuite1()->AEGP_GetMemStats(S_my_id, &countL, &sizeL));

		// If we still have memory allocated, complain about it.
		
		if (!err && (countL || sizeL)) {	
			suites.ANSICallbacksSuite1()->sprintf(	infoZ,
													"Projector leaked %d allocations, for a total of %dk.",
													countL, 
													sizeL);

			ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, reinterpret_cast<const char*>(&infoZ)));
		}
			
		suites.ANSICallbacksSuite1()->sprintf(	infoZ,
												"Projector made %d allocations, for a total of %d kbytes.",
												countL, 
												sizeL);
	}
	catch (A_Err &thrown_err){
		err =	thrown_err;
	} 							
	return err;
}
						
static A_Err
VerifyFileImportable(
	const char *		file_fullpathZ,		// >>
	AE_FIM_Refcon		refcon,				// >> the client defines this and it is stored with import callbacks
	A_Boolean			*importablePB)		// << 
{
	*importablePB = TRUE;	// Do a lot more checking than this in your plug-in! 
	return A_Err_NONE;
}



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


static A_Err
CreateProject(
	AE_FIM_ImportOptions	options)
{
	A_Err 					err		= A_Err_NONE, 
							err2	= A_Err_NONE;
	
	AEGP_SuiteHandler		suites(S_sp_basic_suiteP);
	
	// Wow, look at all the stuff we'll be working with!
		
	AEGP_ProjectH		projH			= NULL;

	AEGP_FootageH		footageH		= NULL, 
						proxy_footageH	= NULL,
						layer1H			= NULL,
						layer2H			= NULL;

	AEGP_ItemH			footage_itemH	= NULL,
						layer1_itemH	= NULL,
						layer2_itemH	= NULL,
						root_itemH		= NULL, 
						new_folderH		= NULL, 
						active_itemH	= NULL, 
						solid_itemH 	= NULL,
						new_comp_itemH	= NULL,
						dest_itemH		= NULL,
						added_comp_itemH = NULL;

	AEGP_LayerH			added_layerH	= NULL,
						added_cameraH	= NULL,
						added_lightH	= NULL,
						added_solidH 	= NULL;

	AEGP_CompH			compH			= NULL,
						new_compH		= NULL,
						added_compH		= NULL;
	
	AEGP_ItemType		item_type		= AEGP_ItemType_NONE;

	AEGP_InstalledEffectKey	current_key	= AEGP_InstalledEffectKey_NONE;
						
	AEGP_ColorVal		solid_color		= {0.0, 0.0, 0.0, 0.0};

	A_char				nameZ[AEGP_MAX_PROJ_NAME_SIZE] = {'\0'},
						match_nameZ[AEGP_MAX_EFFECT_MATCH_NAME_SIZE] = {'\0'};

	AEGP_MemHandle		pathZ;

	A_long				widthL			=	0,
						heightL			=	0,
						num_paramsL		=	0,
						num_effectsL	=	0,
						num_segsL		= 	0;

	A_FloatPoint 		centerFPt		=	{0,0};

	A_FpLong			thirty_five_mmF 	= 102.05,
						focalF				= 99.21,		// 54.54 fov
						framerate 			= 23.976;
	AEGP_StreamValue	stream_value,
						value,
						vertex_val;
	AEGP_RQItemRefH		refH 				= NULL;	
	AEGP_OutputModuleRefH	omH 			= NULL;

	AEGP_StreamRefH 	zoom_streamH		= NULL,
						streamH				= NULL;

	AEGP_EffectRefH		effectH				= NULL;
	AEGP_StreamRefH 	mask_streamH		= NULL;

	A_Ratio 			pix_aspect_ratioRt	= {1,1},
						new_parRt			= {235,100};
	
	A_Boolean			is_add_validB		= FALSE,
						foundB				= FALSE;
	
	A_Time				timeT				= {0,1},
						current_timeT		= {0,1},
						comp_timeT			= {0,0},
						start_timeT 		= {0,1};
						
	AEGP_DownsampleFactor	dsf 			= {3,3};

	A_Boolean 			vid_activeB 		= FALSE;	

		

	AEGP_MaskVertex		v_one, 
						v_two, 
						v_three, 
						v_four;
						
	AEGP_FootageInterp	interp;
	A_FloatRect			masked_boundsR		= {0,0,0,0};
	
	A_UTF16Char			folder_name[256],
						psd_path[256],
						source_footage_path[256],
						proxy_footage_path[256];

	AEFX_CLR_STRUCT(interp);
	AEFX_CLR_STRUCT(value);
	AEFX_CLR_STRUCT(stream_value);
	AEFX_CLR_STRUCT(vertex_val);

	// First, make a folder to contain everything.
	
	ERR(suites.ProjSuite5()->AEGP_GetProjectByIndex(0, &projH));
	ERR(suites.ProjSuite5()->AEGP_GetProjectRootFolder(projH, &root_itemH));
	ERR(suites.ProjSuite5()->AEGP_GetProjectName(projH, nameZ));
	ERR(suites.ProjSuite5()->AEGP_GetProjectPath(projH, &pathZ));
	ERR(suites.MemorySuite1()->AEGP_FreeMemHandle(pathZ));

	copyConvertStringLiteralIntoUTF16(FOLDER_NAME, folder_name);

	ERR(suites.ItemSuite8()->AEGP_CreateNewFolder(folder_name,
												  root_itemH,
												  &new_folderH));
	
	// If folder creation worked, let's make a composition.
	
	if (!err && *new_folderH) {
		A_Ratio comp_parRt	=	{9,10};
		A_Time	durationT	=	{52,1};
		A_Ratio fps			=	{2997, 100};
		
		err = suites.CompSuite4()->AEGP_CreateComp(	new_folderH,
													"boingy",
													320,
													240,
													&comp_parRt,
													&durationT,
													&fps,															
													&new_compH);
		
		ERR(suites.CompSuite4()->AEGP_CreateComp(	new_folderH,
													"boogaloo",
													320,
													240,
													&comp_parRt,
													&durationT,
													&fps,															
													&added_compH));
		
		
		
		//	If adding the comp worked, let's add some footage, mess with 
		//	its interpretation, add a proxy for it, and set its quality.
	}					

	ERR(suites.CompSuite4()->AEGP_SetCompDownsampleFactor(new_compH, &dsf));
	ERR(suites.CompSuite4()->AEGP_SetCompFrameRate(new_compH, &framerate));
	ERR(suites.CompSuite4()->AEGP_SetCompPixelAspectRatio(new_compH, &new_parRt));
	
	ERR(suites.CompSuite4()->AEGP_GetItemFromComp(new_compH, &new_comp_itemH));
	ERR(suites.ItemSuite8()->AEGP_GetItemDimensions(new_comp_itemH, &widthL, &heightL));
	ERR(suites.ItemSuite8()->AEGP_GetItemPixelAspectRatio(new_comp_itemH, &pix_aspect_ratioRt));

	if (!err) {
		centerFPt.x = widthL / 2;
		centerFPt.y = heightL / 2;
		
		err = suites.CompSuite4()->AEGP_CreateCameraInComp(	"A basic camera", 
															centerFPt, 
															new_compH,
															&added_cameraH); 

	}
	ERR(suites.CameraSuite2()->AEGP_SetCameraFilmSize(	added_cameraH,
														AEGP_FilmSizeUnits_HORIZONTAL,
														&thirty_five_mmF));

	ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(	S_my_id, 
														added_cameraH,
														AEGP_LayerStream_ZOOM, 
														&zoom_streamH));	

	if (!err){
		stream_value.streamH = zoom_streamH;	

		stream_value.val.one_d = widthL * focalF * RATIO2FLOAT(pix_aspect_ratioRt) / thirty_five_mmF;

		ERR(suites.StreamSuite2()->AEGP_SetStreamValue( S_my_id, 
														zoom_streamH, 	
														&stream_value));
													
		ERR2(suites.StreamSuite2()->AEGP_DisposeStreamValue(&stream_value));
	}

	ERR(suites.CompSuite4()->AEGP_CreateLightInComp("some light",
													centerFPt,
													new_compH,
													&added_lightH));
	
	if (!err){
		A_Time	in_pointT = {3,1},
				durationT = {5,1};
		
		err = suites.LayerSuite5()->AEGP_SetLayerInPointAndDuration(added_lightH,
																	AEGP_LTimeMode_LayerTime,
																	&in_pointT,
																	&durationT);
																	
		in_pointT.value = 2;
		durationT.value = 8;
					
		err = suites.LayerSuite5()->AEGP_SetLayerInPointAndDuration(added_lightH,
																	AEGP_LTimeMode_LayerTime,
																	&in_pointT,
																	&durationT);
		
	}																	

	ERR(suites.LightSuite2()->AEGP_SetLightType(added_lightH, AEGP_LightType_AMBIENT));

	//	WARNING: If you don't fix these hard-coded paths (in
	//	Projector.h), you WILL get errors while running this
	//	plug-in. Don't blame us; change them!
	
	/*	
		#ifdef AE_OS_WIN
		#define PSD_PATH "c:\\noel_clown_nose.psd"
		#define SRC_FOOTAGE_PATH "c:\\noel_clown_nose.psd"
		#define PROXY_FOOTAGE_PATH "c:\\proxy.jpg"
		#define RENDER_PATH "c:\\stinky.avi"
		#else
		#define PSD_PATH "/noel_clown_nose.psd"
		#define SRC_FOOTAGE_PATH "/copy_to_root.jpg"
		#define PROXY_FOOTAGE_PATH "/proxy.jpg"
		#define RENDER_PATH "/stinky.mov"
		#endif
	*/

	AEGP_FootageLayerKey	key1, key2;
	
	AEFX_CLR_STRUCT(key1);
	AEFX_CLR_STRUCT(key2);
	
	key1.layer_idL =
	key2.layer_idL = AEGP_LayerID_UNKNOWN;
	
	key1.layer_indexL = 0;
	key2.layer_indexL = 1;
	
	copyConvertStringLiteralIntoUTF16(PSD_PATH, psd_path);
	
	err = suites.FootageSuite5()->AEGP_NewFootage(	S_my_id, 
												  psd_path,
												  &key1,
												  NULL,
												  FALSE, 
												  NULL, 
												  &layer1H);
	
	ERR(suites.FootageSuite5()->AEGP_AddFootageToProject(layer1H, new_folderH, &layer1_itemH));
	
	ERR(suites.FootageSuite5()->AEGP_NewFootage(S_my_id, 
												psd_path, 
												&key2,
												NULL,
												FALSE, 
												NULL, 
												&layer2H));
	
	ERR(suites.FootageSuite5()->AEGP_AddFootageToProject(layer2H, new_folderH, &layer2_itemH));
	
	
	copyConvertStringLiteralIntoUTF16(SRC_FOOTAGE_PATH, source_footage_path);
	
	ERR(suites.FootageSuite5()->AEGP_NewFootage(S_my_id, 
												source_footage_path,
												NULL,
												NULL,
												FALSE, 
												NULL, 
												&footageH));

	if (!err)	{
		ERR(suites.FootageSuite5()->AEGP_AddFootageToProject(footageH, new_folderH, &footage_itemH));
		
		ERR(suites.FootageSuite5()->AEGP_GetFootageInterpretation(footage_itemH, false, &interp));
		
		interp.il.order	=	FIEL_Order_LOWER_FIRST;
		interp.il.type	=	FIEL_Type_INTERLACED;

		ERR(suites.FootageSuite5()->AEGP_SetFootageInterpretation(footage_itemH, false, &interp));
		
	} else	{ // Couldn't add footage to project
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, "Failed to add footage."));
	}
	
	
	if (footage_itemH) 	{
		copyConvertStringLiteralIntoUTF16(PROXY_FOOTAGE_PATH, proxy_footage_path);		
		
		ERR(suites.FootageSuite5()->AEGP_NewFootage(	S_my_id, 
														proxy_footage_path, 
														NULL,
														NULL,
														FALSE, 
														NULL, 
														&proxy_footageH));

		ERR(suites.FootageSuite5()->AEGP_SetItemProxyFootage(proxy_footageH, footage_itemH));
		ERR(suites.ItemSuite8()->AEGP_SetItemUseProxy(footage_itemH, TRUE));
		ERR(suites.ItemSuite8()->AEGP_SetItemComment(footage_itemH, "arrgh"));

		/*	If a composition is currently selected, add the footage to it. 
			Otherwise, add to our new composition.
		*/

		ERR(suites.ItemSuite8()->AEGP_GetActiveItem(&active_itemH));

		if (active_itemH) {
			ERR(suites.ItemSuite8()->AEGP_GetItemType(active_itemH, &item_type));
		}

		if (!err){
			if (AEGP_ItemType_COMP == item_type){
				dest_itemH = active_itemH;
			} else {
				dest_itemH = new_comp_itemH;
			}
		}
		
		ERR(suites.CompSuite4()->AEGP_GetCompFromItem(dest_itemH, &compH));
		ERR(suites.CompSuite4()->AEGP_GetCompDisplayStartTime(compH, &start_timeT));
		ERR(suites.LayerSuite5()->AEGP_IsAddLayerValid(footage_itemH, compH, &is_add_validB));

		if (is_add_validB) {
			ERR(suites.LayerSuite5()->AEGP_AddLayer(footage_itemH, compH, &added_layerH));
			ERR(suites.CompSuite4()->AEGP_GetItemFromComp(added_compH, &added_comp_itemH));
			ERR(suites.LayerSuite5()->AEGP_AddLayer(added_comp_itemH, compH, &added_layerH));
		}

		if (added_layerH) {
			ERR(suites.LayerSuite5()->AEGP_SetLayerParent(added_layerH, added_cameraH));
		}					

		ERR(suites.LayerSuite5()->AEGP_GetLayerCurrentTime(added_layerH, AEGP_LTimeMode_LayerTime, &current_timeT));
		ERR(suites.LayerSuite5()->AEGP_ConvertLayerToCompTime(added_layerH, &current_timeT, &comp_timeT));
		ERR(suites.LayerSuite5()->AEGP_IsVideoActive(added_layerH, 
													AEGP_LTimeMode_LayerTime, 
													&current_timeT, 
													&vid_activeB));

		ERR(suites.LayerSuite5()->AEGP_GetLayerMaskedBounds(added_layerH, 
															AEGP_LTimeMode_LayerTime, 
															&current_timeT, 
															&masked_boundsR));
		
		//	 If that worked, let's apply the Levels effect (if it's available), 
		//	 and mess with its settings. 
					
		if (added_layerH) {
			ERR(suites.EffectSuite2()->AEGP_GetNumInstalledEffects(&num_effectsL));
		}

		if (!err && num_effectsL) {
			for (AEGP_InstalledEffectKey i = 0; !err && (i < num_effectsL) && (TRUE != foundB); i++) {

				ERR(suites.EffectSuite2()->AEGP_GetNextInstalledEffect(	current_key, &current_key));
				ERR(suites.EffectSuite2()->AEGP_GetEffectMatchName(	current_key, match_nameZ));

				if (!err && 
					(!strcmp(match_nameZ, "ADBE Easy Levels") ||
					 !strcmp(match_nameZ, "ADBE Pro Levels"))) {

					foundB						=	TRUE;
					
					err = suites.EffectSuite2()->AEGP_ApplyEffect(	S_my_id,
																	added_layerH,
																	current_key,
																	&effectH);

					ERR(suites.StreamSuite2()->AEGP_GetEffectNumParamStreams(effectH, &num_paramsL));
					ERR(suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(	S_my_id,
																				effectH,
																				INPUT_WHITE_PARAM,
																				&streamH));
				
					if (streamH) {
						ERR(suites.StreamSuite2()->AEGP_GetStreamName(streamH, TRUE, nameZ));
					}

					if (!strcmp(nameZ, "Input Black")) {
						ERR(suites.StreamSuite2()->AEGP_GetNewStreamValue(	S_my_id,
																			streamH,
																			AEGP_LTimeMode_LayerTime,
																			&timeT,
																			TRUE,
																			&value));
						if (!err) {
							value.val.one_d = .62745098;
							
							ERR(suites.StreamSuite2()->AEGP_SetStreamValue(	S_my_id, streamH, &value));
						}
						ERR2(suites.StreamSuite2()->AEGP_DisposeStreamValue(&value));
					}

					ERR(suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(	S_my_id,
																				effectH,
																				INPUT_BLACK_PARAM,
																				&streamH));

					if (streamH) {
						ERR(suites.StreamSuite2()->AEGP_GetStreamName(streamH,
																		TRUE,
																		nameZ));
					}

					if (!strcmp(nameZ, "Input White")) {
						ERR(suites.StreamSuite2()->AEGP_GetNewStreamValue(	S_my_id,
																			streamH,
																			AEGP_LTimeMode_LayerTime,
																			&timeT,
																			TRUE,
																			&value));

						if (!err) {
							value.val.one_d = .92156862745;
							
							ERR(suites.StreamSuite2()->AEGP_SetStreamValue(	S_my_id, streamH, &value));
						}
						ERR(suites.StreamSuite2()->AEGP_DisposeStreamValue(&value));
					}
				}
			}
			
			if (effectH) {
				ERR(suites.EffectSuite2()->AEGP_DisposeEffect(effectH));
			}
		}
	}
	
	// Add a solid

	AEGP_ColorVal scratch = {1.0, 1.0, 1.0, .87654321};

	ERR(suites.CompSuite4()->AEGP_CreateSolidInComp(	"new_solid",
														123,
														112,
														&scratch,
														new_compH,
														NULL,				// NULL			-->	"use the pref for still durations".
																			// 0			-->	"use the length of the comp"
																			// A valid time -->	"use this valid time".
														&added_solidH));
			
	ERR(suites.LayerSuite5()->AEGP_GetLayerSourceItem( 	added_solidH,
														&solid_itemH));
	
	ERR(suites.FootageSuite5()->AEGP_SetSolidFootageDimensions(	solid_itemH,
																false,
																456,
																123));

	ERR(suites.FootageSuite5()->AEGP_GetSolidFootageColor(solid_itemH, false, &solid_color));

	if (!err) {
		solid_color.redF 	= 1.0;
		solid_color.greenF 	= 0.0;
		solid_color.blueF	= 1.0;
		
		ERR(suites.FootageSuite5()->AEGP_SetSolidFootageColor(solid_itemH, false, &solid_color));
	}
	
	// Add a mask to the solid
	
	if (!err) {
		AEGP_MaskRefH	new_maskH 		= NULL;
		A_long			indexL			= 0L;

		ERR(suites.MaskSuite4()->AEGP_CreateNewMask(added_solidH, &new_maskH, &indexL));
		
		if (new_maskH) {
			ERR(suites.StreamSuite2()->AEGP_GetNewMaskStream(	S_my_id,
																new_maskH,
																AEGP_MaskStream_OUTLINE,
																&mask_streamH));
		
			A_Boolean rotoB = FALSE;
			
			ERR(suites.MaskSuite4()->AEGP_GetMaskIsRotoBezier(	new_maskH,
																&rotoB));
																
			rotoB = !rotoB;
			
			ERR(suites.MaskSuite4()->AEGP_SetMaskIsRotoBezier(	new_maskH, rotoB));
		}
		ERR(suites.StreamSuite2()->AEGP_GetNewStreamValue(	S_my_id,
															mask_streamH,
															AEGP_LTimeMode_CompTime,
															&timeT,
															TRUE,
															&vertex_val));

		ERR(suites.MaskOutlineSuite2()->AEGP_CreateVertex(	vertex_val.val.mask, 0));
		ERR(suites.MaskOutlineSuite2()->AEGP_CreateVertex(	vertex_val.val.mask, 1));
		ERR(suites.MaskOutlineSuite2()->AEGP_CreateVertex(	vertex_val.val.mask, 2));
		ERR(suites.MaskOutlineSuite2()->AEGP_CreateVertex(	vertex_val.val.mask, 3));

		ERR(suites.MaskOutlineSuite2()->AEGP_GetMaskOutlineVertexInfo(vertex_val.val.mask, 0, &v_one));
		
		}
		
	if (!err) {
		v_one.x = 10;
		v_one.y = 10;
		ERR(suites.MaskOutlineSuite2()->AEGP_SetMaskOutlineVertexInfo(vertex_val.val.mask, 0, &v_one));
	}
	
	ERR(suites.MaskOutlineSuite2()->AEGP_GetMaskOutlineVertexInfo(vertex_val.val.mask, 1, &v_two));

	if (!err) {
		v_two.x = 50;
		v_two.y = 10;
		ERR(suites.MaskOutlineSuite2()->AEGP_SetMaskOutlineVertexInfo(vertex_val.val.mask, 1, &v_two));
	}

	ERR(suites.MaskOutlineSuite2()->AEGP_GetMaskOutlineVertexInfo(vertex_val.val.mask, 2, &v_three));

	if (!err) {
		v_three.x = 50;
		v_three.y = 50;
		ERR(suites.MaskOutlineSuite2()->AEGP_SetMaskOutlineVertexInfo( vertex_val.val.mask, 2, &v_three));
	}

	ERR(suites.MaskOutlineSuite2()->AEGP_GetMaskOutlineVertexInfo( 	vertex_val.val.mask,
																	3,
																	&v_four));
	if (!err) {
		v_four.x = 10;
		v_four.y = 50;
		ERR(suites.MaskOutlineSuite2()->AEGP_SetMaskOutlineVertexInfo(vertex_val.val.mask, 3, &v_four));
	}

	ERR(suites.MaskOutlineSuite2()->AEGP_GetMaskOutlineNumSegments(	vertex_val.val.mask, &num_segsL));
	ERR(suites.MaskOutlineSuite2()->AEGP_SetMaskOutlineOpen( vertex_val.val.mask, FALSE));
	ERR(suites.StreamSuite2()->AEGP_SetStreamValue(S_my_id, mask_streamH, &vertex_val));
	
	ERR2(suites.StreamSuite2()->AEGP_DisposeStreamValue(&vertex_val));

	if (zoom_streamH){
		ERR2(suites.StreamSuite2()->AEGP_DisposeStream(zoom_streamH));
	}

	if (streamH){
		ERR2(suites.StreamSuite2()->AEGP_DisposeStream(streamH));
	}

	
	ERR(suites.LayerSuite5()->AEGP_SetLayerQuality(added_layerH, AEGP_LayerQual_BEST));
	
	// Add some items to the render queue
	
	for (A_long iL = 0; !err && iL < 6; ++iL){
		ERR(suites.RenderQueueSuite1()->AEGP_AddCompToRenderQueue(	compH, "Yesod:output.mov"));
	}
	
	ERR(suites.RQItemSuite2()->AEGP_GetNextRQItem(NULL, &refH));
	
	if (refH) {
		ERR(suites.RQItemSuite2()->AEGP_SetComment(refH, "Added by Projector sample!"));
	}
	
	ERR(suites.OutputModuleSuite4()->AEGP_AddDefaultOutputModule(refH, &omH));
	
	
	/* 	The following code works; it's just annoying. Uncomment at your leisure.

	If all that worked, let's render!	
			
	if (!err){
		//	Set to AEGP_RenderQueueState_RENDERING because adding to render
			queue is illegal during rendering.
		
		
		AEGP_RenderQueueState current_state  = AEGP_RenderQueueState_RENDERING;

		err = suites.RenderQueueSuite1()->AEGP_GetRenderQueueState(&current_state);

		if (!err && (AEGP_RenderQueueState_STOPPED == current_state)){
			err = suites.RenderQueueSuite1()->AEGP_AddCompToRenderQueue(compH, RENDER_PATH);

			ERR(suites.RenderQueueSuite1()->AEGP_SetRenderQueueState(AEGP_RenderQueueState_RENDERING));
		}
	}*/
	
	return err;
}

static A_Err 
ImportFile(
	const	char 				*file_fullpathZ,	// >>
	AE_FIM_ImportOptions		imp_options,		// >>	opaque structure; in the future  suite could be expanded with 
													//		query functions
	AE_FIM_SpecialAction		action,				// >>	is it a special kind of import situation?
	AEGP_ItemH					itemH,				// >>	meaning varies depending on AE_FIM_SpecialAction
													//		both for no special action and drag'n'drop it is
													//		a folder where imported item should go
	AE_FIM_Refcon				refcon)				// >>	the client defines this and it is stored with import callbacks
{
	A_Err err = A_Err_NONE;

	try {
		if (AE_FIM_SpecialAction_DRAG_N_DROP_FILE == action) {
			err = CreateProject(imp_options);// handle drag n drop
		} else{
			err = CreateProject(imp_options);
		}
	} catch (A_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}

static A_Err
RegisterFileType()
{
	A_Err					err		=	A_Err_NONE;

	AEIO_FileKind			kind;
	AEIO_FileKind			ext;
	AE_FIM_ImportFlavorRef	my_ref	=	AE_FIM_ImportFlavorRef_NONE;

	AEGP_SuiteHandler	suites(S_sp_basic_suiteP);
	
	AEFX_CLR_STRUCT(kind);
	AEFX_CLR_STRUCT(ext);

#ifdef AE_OS_WIN
	memcpy((char *)&kind.mac.type, "sdk", 3);
#else
	kind.mac.type =   	'SDK_';
#endif
	kind.mac.creator	= AEIO_ANY_CREATOR;						

	ext.ext.extension[0] 	= 's';
	ext.ext.extension[1] 	= 'd';
	ext.ext.extension[2] 	= 'k';
	ext.ext.pad 			= '.';				

	
	ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavor("SDK Project",	&my_ref));

	ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavorFileTypes(	my_ref,
																1,
																&kind,
																1,
																&ext));

	if (!err && (AE_FIM_ImportFlavorRef_NONE != my_ref)){
		AE_FIM_ImportCallbacks ae_fim_icbs;

		AEFX_CLR_STRUCT (ae_fim_icbs);
		
		ae_fim_icbs.import_cb	=	reinterpret_cast<AE_FIM_ImportFileCB>(ImportFile);
		ae_fim_icbs.verify_cb	=	reinterpret_cast<AE_FIM_VerifyImportableCB>(VerifyFileImportable);

		ERR(suites.FIMSuite3()->AEGP_RegisterImportFlavorImportCallbacks(my_ref, AE_FIM_ImportFlag_COMP, &ae_fim_icbs));
	}
	return err;
}

A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,		/* >> */
	A_long				 	major_versionL,		/* >> */		
	A_long					minor_versionL,		/* >> */		
	AEGP_PluginID			aegp_plugin_id,		/* >> */
	AEGP_GlobalRefcon		*global_refconP)	/* << */
{
	A_Err 				err		=	A_Err_NONE;
	S_sp_basic_suiteP			=	pica_basicP;
	S_my_id						=	aegp_plugin_id;	

	AEGP_SuiteHandler	suites(S_sp_basic_suiteP);

	ERR(suites.RegisterSuite5()->AEGP_RegisterDeathHook(aegp_plugin_id, DeathHook, NULL));
	ERR(RegisterFileType());
	#ifdef DEBUG
		ERR(suites.MemorySuite1()->AEGP_SetMemReportingOn(TRUE));
	#endif
	return err;
}
