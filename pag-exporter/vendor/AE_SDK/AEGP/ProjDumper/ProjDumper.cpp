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
ProjDumper.cpp

Revision History

Version		Change											Engineer	Date
=======		======											========	======
1.0 		created											bbb
1.1			refactor, fix an err checking time remapping	zal			6/23/2016

*/

#include "ProjDumper.h"

static AEGP_Command	S_dump_proj_cmd = 0,
						S_other_cmd = 0;

static AEGP_PluginID	S_my_id				= 0;

static SPBasicSuite		*sP		=	0;

static A_Err
PrintAndDisposeStream(
	AEGP_StreamRefH		streamH,
	A_char				*format_stringZ,
	A_char				*indent_stringZ,
	A_char				*stream_nameZ,
	FILE 				*out)
{
	A_Err 				err = A_Err_NONE, err2;
	AEGP_StreamType		stream_type;
	A_long				num_kfsL;
	A_Time				sampleT = { 0, 100 };
	AEGP_StreamValue	val;
	AEGP_StreamValue	*sample_valP = &val;
	A_char				unitsAC[AEGP_MAX_STREAM_UNITS_SIZE + 1];
	
	AEGP_SuiteHandler	suites(sP);
	
	AEFX_CLR_STRUCT(val);

	ERR(suites.StreamSuite2()->AEGP_GetStreamName(streamH, FALSE, stream_nameZ));
	
	if (!err && stream_nameZ) {		// don't bother to print blank name...
		fprintf(out, format_stringZ, indent_stringZ, stream_nameZ);

		ERR(suites.StreamSuite2()->AEGP_GetStreamType(streamH, &stream_type));

		ERR(suites.StreamSuite2()->AEGP_GetStreamUnitsText(streamH, FALSE, unitsAC));
		ERR(suites.KeyframeSuite3()->AEGP_GetStreamNumKFs(streamH, &num_kfsL));
		
		if (!err && num_kfsL) {
			A_Boolean pre_expressionB	=	TRUE;
			
			if (AEGP_StreamType_NO_DATA != stream_type)	{
				ERR(suites.StreamSuite2()->AEGP_GetNewStreamValue(S_my_id, 
																	streamH, 
																	AEGP_LTimeMode_LayerTime, 
																	&sampleT, 
																	pre_expressionB, 
																	sample_valP));
			}
			if (!err) {
				switch (stream_type) 
				{
					case AEGP_StreamType_TwoD_SPATIAL:
					case AEGP_StreamType_TwoD:
						fprintf(out, "\t\tX %1.2f %s Y %1.2f %s", sample_valP->val.two_d.x, unitsAC, sample_valP->val.two_d.y, unitsAC);
						break;		
					case AEGP_StreamType_OneD:
						fprintf(out, "\t\t%1.2f %s", sample_valP->val.one_d, unitsAC);
						break;		
					case AEGP_StreamType_COLOR:				
						fprintf(out, "\t\tR %1.2f G %1.2f B %1.2f", sample_valP->val.color.redF, sample_valP->val.color.greenF, sample_valP->val.color.blueF);
						break;		
					case AEGP_StreamType_ARB:
						fprintf(out, "arb data");
						break;		
					case AEGP_StreamType_LAYER_ID:
						fprintf(out, "\t\tID%d", sample_valP->val.layer_id);
						break;		
					case AEGP_StreamType_MASK_ID:
						fprintf(out, "\t\tID%d", sample_valP->val.mask_id);
						break;		

				}
				if (AEGP_StreamType_NO_DATA != stream_type)	{
					ERR(suites.StreamSuite2()->AEGP_DisposeStreamValue(sample_valP));
				}
			}
		}		

		if (!err) {
			fprintf(out, "\n");	
		}
	}

	ERR2(suites.StreamSuite2()->AEGP_DisposeStream(streamH));

	return err;
}


static A_Err
DumpEffects(
FILE 				*out,
AEGP_LayerH			layerH,
AEGP_StreamRefH		streamH)
{
	A_Err 			err = A_Err_NONE,
					err2 = A_Err_NONE;
	A_long			num_effectsL, num_paramsL;

	A_char			effect_nameAC[AEGP_MAX_ITEM_NAME_SIZE] = { '\0' },
					stream_nameAC[AEGP_MAX_ITEM_NAME_SIZE] = { '\0' },
					indent_stringAC[128];

	AEGP_EffectRefH		effectH = NULL;
	AEGP_SuiteHandler	suites(sP);

	ERR(suites.EffectSuite2()->AEGP_GetLayerNumEffects(layerH, &num_effectsL));

	for (A_long jL = 0; !err && jL < num_effectsL; jL++) {
		ERR(suites.EffectSuite2()->AEGP_GetLayerEffectByIndex(S_my_id, layerH, jL, &effectH));

		if (!err && effectH) {
			AEGP_InstalledEffectKey	key;

			ERR(suites.EffectSuite2()->AEGP_GetInstalledKeyFromLayerEffect(effectH, &key));
			ERR(suites.EffectSuite2()->AEGP_GetEffectName(key, effect_nameAC));

			if (!strcmp(effect_nameAC, "Shifter")) {
				A_Time					fake_timeT = { 0, 100 };

				err = suites.EffectSuite2()->AEGP_EffectCallGeneric(S_my_id, effectH, &fake_timeT, (void*)"ProjDumper says hello to Shifter");
			}
		}
		fprintf(out, "%s\t\t%d: %s\n", indent_stringAC, jL + 1, effect_nameAC);

		ERR(suites.StreamSuite2()->AEGP_GetEffectNumParamStreams(effectH, &num_paramsL));

		for (A_long kL = 1; !err && kL < num_paramsL; ++kL) {		// start at 1 to skip initial layer param

			ERR(suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(S_my_id, effectH, kL, &streamH));
			ERR(PrintAndDisposeStream(streamH, "%s\t\t\t%s", indent_stringAC, stream_nameAC, out));
		}
		ERR2(suites.EffectSuite2()->AEGP_DisposeEffect(effectH));
	}
	return err;
}


static A_Err
DumpComp(
FILE 				*out,
AEGP_ItemH			itemH,
A_Time				currT)
{
	A_Err 			err = A_Err_NONE;
	A_Time 			lay_inT, lay_durT, comp_inT, comp_durT;
	AEGP_CompH		compH;
	AEGP_LayerH		layerH;
	AEGP_ItemH		layerItemH;
	A_long			num_layersL, num_effectsL;

	A_char			layer_nameAC[AEGP_MAX_ITEM_NAME_SIZE] = { '\0' }, 
					stream_nameAC[AEGP_MAX_ITEM_NAME_SIZE] = { '\0' },
					indent_stringAC[128];

	AEGP_SuiteHandler	suites(sP);

	ERR(suites.CompSuite4()->AEGP_GetCompFromItem(itemH, &compH));
	ERR(suites.LayerSuite5()->AEGP_GetCompNumLayers(compH, &num_layersL));

	fprintf(out, "Num Layers: %d\n", num_layersL);

	for (A_long iL = 0; !err && iL < num_layersL; iL++) {

		AEGP_StreamRefH		streamH = NULL;

		ERR(suites.LayerSuite5()->AEGP_GetCompLayerByIndex(compH, iL, &layerH));
		ERR(suites.LayerSuite5()->AEGP_GetLayerName(layerH, layer_nameAC, 0));
		if (layer_nameAC[0] == '\0')
		{
			ERR(suites.LayerSuite5()->AEGP_GetLayerSourceItem(layerH, &layerItemH));
			ERR(suites.ItemSuite6()->AEGP_GetItemName(layerItemH, layer_nameAC));
		}
		ERR(suites.LayerSuite5()->AEGP_GetLayerInPoint(layerH, AEGP_LTimeMode_LayerTime, &lay_inT));
		ERR(suites.LayerSuite5()->AEGP_GetLayerDuration(layerH, AEGP_LTimeMode_LayerTime, &lay_durT));
		ERR(suites.LayerSuite5()->AEGP_GetLayerInPoint(layerH, AEGP_LTimeMode_CompTime, &comp_inT));
		ERR(suites.LayerSuite5()->AEGP_GetLayerDuration(layerH, AEGP_LTimeMode_CompTime, &comp_durT));
		ERR(suites.LayerSuite5()->AEGP_GetLayerCurrentTime(layerH, AEGP_LTimeMode_LayerTime, &currT));
		ERR(suites.EffectSuite2()->AEGP_GetLayerNumEffects(layerH, &num_effectsL));

		fprintf(out, "%s\t%d: %s LayIn: %1.2f LayDur: %1.2f CompIn: %1.2f CompDur: %1.2f Curr: %1.2f Num Effects: %d\n", indent_stringAC, iL + 1, layer_nameAC,
			(A_FpLong)(lay_inT.value) / (lay_inT.scale), (A_FpLong)(lay_durT.value) / (lay_durT.scale),
			(A_FpLong)(comp_inT.value) / (comp_inT.scale), (A_FpLong)(comp_durT.value) / (comp_durT.scale),
			(A_FpLong)(currT.value) / (currT.scale), num_effectsL);

		ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, layerH, AEGP_LayerStream_ANCHORPOINT, &streamH));
		ERR(PrintAndDisposeStream(streamH, "%s\t\t%s", indent_stringAC, stream_nameAC, out));

		ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, layerH, AEGP_LayerStream_POSITION, &streamH));
		ERR(PrintAndDisposeStream(streamH, "%s\t\t%s", indent_stringAC, stream_nameAC, out));

		ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, layerH, AEGP_LayerStream_SCALE, &streamH));
		ERR(PrintAndDisposeStream(streamH, "%s\t\t%s", indent_stringAC, stream_nameAC, out));

		ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, layerH, AEGP_LayerStream_ROTATION, &streamH));
		ERR(PrintAndDisposeStream(streamH, "%s\t\t%s", indent_stringAC, stream_nameAC, out));

		ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, layerH, AEGP_LayerStream_OPACITY, &streamH));
		ERR(PrintAndDisposeStream(streamH, "%s\t\t%s", indent_stringAC, stream_nameAC, out));

		ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, layerH, AEGP_LayerStream_AUDIO, &streamH));
		ERR(PrintAndDisposeStream(streamH, "%s\t\t%s", indent_stringAC, stream_nameAC, out));

		ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, layerH, AEGP_LayerStream_MARKER, &streamH));
		ERR(PrintAndDisposeStream(streamH, "%s\t\t%s", indent_stringAC, stream_nameAC, out));

		AEGP_LayerFlags layer_flags = AEGP_LayerFlag_NONE;
		ERR(suites.LayerSuite8()->AEGP_GetLayerFlags(layerH, &layer_flags));
		if (layer_flags & AEGP_LayerFlag_TIME_REMAPPING)
		{
			ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, layerH, AEGP_LayerStream_TIME_REMAP, &streamH));
			ERR(PrintAndDisposeStream(streamH, "%s\t\t%s", indent_stringAC, stream_nameAC, out));
		}

		DumpEffects(out, layerH, streamH);
	}
	return err;
}


static A_Err
RecursiveDump(
	FILE 				*out,
	A_long				indent_levelsL,
	AEGP_ItemH			itemH,
	AEGP_ItemH			parent_itemH0,
	AEGP_ItemH			*prev_itemPH)
{
	A_Err 				err		= A_Err_NONE;

	A_char				item_nameAC[AEGP_MAX_ITEM_NAME_SIZE]	= {'\0'},
						type_nameAC[AEGP_MAX_ITEM_NAME_SIZE]	= {'\0'},
						indent_stringAC[128];
	
	AEGP_ItemH			curr_parent_itemH = parent_itemH0;	
	
	AEGP_SuiteHandler	suites(sP);
	
	indent_stringAC[0] = '\0';
	
	for (A_long iL = 0; !err && iL < indent_levelsL; iL++) {
		suites.ANSICallbacksSuite1()->sprintf(indent_stringAC, "%s\t", indent_stringAC);
	}

	while (	!err	&& 
			(itemH) &&
			((parent_itemH0 == 0) || (parent_itemH0 == curr_parent_itemH))) {
			
		*prev_itemPH = itemH;
		
		AEGP_ProjectH	projH	=	NULL;
		
		ERR(suites.ProjSuite5()->AEGP_GetProjectByIndex(0, &projH));
		ERR(suites.ItemSuite6()->AEGP_GetNextProjItem(projH, itemH, &itemH));

		if (itemH) {
			AEGP_ItemType		item_type;
			
			ERR(suites.ItemSuite6()->AEGP_GetItemParentFolder(itemH, &curr_parent_itemH));

			if ((parent_itemH0 == 0) || (parent_itemH0 == curr_parent_itemH)) {
				
				ERR(suites.ItemSuite6()->AEGP_GetItemType(itemH, &item_type));
				ERR(suites.ItemSuite6()->AEGP_GetTypeName(item_type, type_nameAC));
				ERR(suites.ItemSuite6()->AEGP_GetItemName(itemH, item_nameAC));

				if (!err && (item_type != AEGP_ItemType_FOLDER)) {
					A_Time 				durationT, currT;
					A_long				widthL, heightL;
					AEGP_MemHandle		pathH;

					ERR(suites.ItemSuite6()->AEGP_GetItemDuration(itemH, &durationT));
					ERR(suites.ItemSuite6()->AEGP_GetItemCurrentTime(itemH, &currT));
					ERR(suites.ItemSuite6()->AEGP_GetItemDimensions(itemH, &widthL, &heightL));

					fprintf(out, "%s%s: %s Dur: %1.2f Curr: %1.2f Width: %d Height: %d ", indent_stringAC,
							type_nameAC, item_nameAC, (A_FpLong)(durationT.value) / (durationT.scale), (A_FpLong)(currT.value) / (currT.scale),
							widthL, heightL);
							
					if (!err && (item_type == AEGP_ItemType_FOOTAGE)) {
						AEGP_FootageH			footageH;
						AEGP_FootageInterp		interp;
						A_long					num_footage_filesL;
						
						ERR(suites.FootageSuite5()->AEGP_GetMainFootageFromItem(itemH, &footageH));
						ERR(suites.FootageSuite5()->AEGP_GetFootageInterpretation(itemH, false, &interp));
						ERR(suites.FootageSuite5()->AEGP_GetFootageNumFiles(footageH, &num_footage_filesL, 0));
						ERR(suites.FootageSuite5()->AEGP_GetFootagePath(footageH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &pathH));
						if (!err) {
							fprintf(out, "Num Files: %d\n", num_footage_filesL);
							ERR(suites.MemorySuite1()->AEGP_FreeMemHandle(pathH));
						}
					} else if (item_type == AEGP_ItemType_COMP) {

						ERR(DumpComp(out, itemH, currT));
					}

				} else {
					fprintf(out, "%s%s: %s\n", indent_stringAC, type_nameAC, item_nameAC);
					
					ERR(RecursiveDump(out, 
										indent_levelsL + 1, 
										itemH, 
										itemH, 
										prev_itemPH));

					itemH = *prev_itemPH;
				}
			}
		}
	}
	return err;
}


static A_Err
DumpProj(void)
{
	A_Err 				err 	= A_Err_NONE, 
						err2 	= A_Err_NONE;
	FILE 				*out 	= NULL;
	AEGP_ProjectH		projH	= NULL;
	AEGP_ItemH			itemH 	= NULL;
	A_char				proj_nameAC[AEGP_MAX_PROJ_NAME_SIZE];
	A_char				path_nameAC[AEGP_MAX_PROJ_NAME_SIZE + 16];		// so we can add "_Dump.txt"
	AEGP_SuiteHandler	suites(sP);
#ifdef AE_OS_WIN
	suites.ANSICallbacksSuite1()->strcpy(path_nameAC, "C:\\Windows\\Temp\\");
#elif defined AE_OS_MAC
	suites.ANSICallbacksSuite1()->strcpy(path_nameAC, "./");
#endif
	ERR(suites.UtilitySuite3()->AEGP_StartUndoGroup("Keepin' On"));

	if (!err) {
		ERR(suites.ProjSuite5()->AEGP_GetProjectByIndex(0, &projH));
		ERR(suites.ProjSuite5()->AEGP_GetProjectRootFolder(projH, &itemH));

		if (!err && itemH) {
			err = suites.ProjSuite5()->AEGP_GetProjectName(projH, proj_nameAC);
			strcat(path_nameAC, proj_nameAC);
			strcat(path_nameAC, "_Dump.txt");
			if (!err) {
				out = fopen(path_nameAC, "w");
				if (out) {
					AEGP_ItemH			prev_itemH = 0;
			
					err = RecursiveDump(out, 0, itemH, 0, &prev_itemH);

					fprintf(out, "\n\n");
					fclose(out);
				}
			}
		}
	}
	ERR2(suites.UtilitySuite3()->AEGP_EndUndoGroup());

	if (!err)
	{
#ifdef AE_OS_WIN
		suites.UtilitySuite5()->AEGP_ReportInfo(S_my_id, DUMP_SUCCEEDED_WIN);
#elif defined AE_OS_MAC
		suites.UtilitySuite5()->AEGP_ReportInfo(S_my_id, DUMP_SUCCEEDED_MAC);
#endif
	}
	else
	{
		suites.UtilitySuite5()->AEGP_ReportInfo(S_my_id, DUMP_FAILED);
	}

	return err;
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
	CFRange	range = {0, AEGP_MAX_PATH_SIZE};
	range.length = length;
	CFStringRef inputStringCFSR = CFStringCreateWithBytes(	kCFAllocatorDefault,
															reinterpret_cast<const UInt8 *>(inputString),
															length * sizeof(wchar_t),
															kCFStringEncodingUTF32LE,
															FALSE);
	CFStringGetBytes(	inputStringCFSR,
						range,
						kCFStringEncodingUTF16,
						0,
						FALSE,
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
MakeCuter(void)
{
	A_Err 				err 	= A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);
	AEGP_ItemH			active_itemH;
	ERR(suites.ItemSuite6()->AEGP_GetActiveItem(&active_itemH));
	
	AEGP_ItemFlags	item_flags;
	char			info_string_1AC[256];
	char			info_string_2AC[256];
	char			active_item_nameAC[AEGP_MAX_ITEM_NAME_SIZE] = "ain't got nothin";
	
	if (active_itemH){					
		ERR(suites.ItemSuite6()->AEGP_GetItemName(active_itemH, active_item_nameAC));
		if (!err) {
			suites.ANSICallbacksSuite1()->sprintf(info_string_1AC, "Active Item: %s", active_item_nameAC);
		}
		ERR(suites.ItemSuite6()->AEGP_GetItemFlags(active_itemH, &item_flags));
		if (!err) {
			suites.ANSICallbacksSuite1()->sprintf(	info_string_2AC, "m: %s hp: %s up: %s mp: %s v: %s a: %s s: %s",
										(item_flags & AEGP_ItemFlag_MISSING) ? "Y" : "N",
										(item_flags & AEGP_ItemFlag_HAS_PROXY) ? "Y" : "N",
										(item_flags & AEGP_ItemFlag_USING_PROXY) ? "Y" : "N",
										(item_flags & AEGP_ItemFlag_MISSING_PROXY) ? "Y" : "N",
										(item_flags & AEGP_ItemFlag_HAS_VIDEO) ? "Y" : "N",
										(item_flags & AEGP_ItemFlag_HAS_AUDIO) ? "Y" : "N",
										(item_flags & AEGP_ItemFlag_STILL) ? "Y" : "N");						
		}
	}
			
	/*	
		the code below creates footage, adds the footage to project (returns item) and then
		adds the item to the active item if it's a composition.
	*/
	if (!err) {
		AEGP_ProjectH		projH;
		AEGP_FootageH		footageH, proxy_footageH;
		AEGP_ItemH			footage_itemH, root_itemH;
		AEGP_LayerH			added_layerH;
		A_UTF16Char			layer_pathZ[AEGP_MAX_PATH_SIZE],
							proxy_pathZ[AEGP_MAX_PATH_SIZE];

		copyConvertStringLiteralIntoUTF16(LAYERED_PATH, layer_pathZ);
		copyConvertStringLiteralIntoUTF16(PROXY_FOOTAGE_PATH, proxy_pathZ);

		ERR(suites.FootageSuite5()->AEGP_NewFootage(	S_my_id, 
														layer_pathZ,
														NULL,
														NULL,
														FALSE,
														NULL,  
														&footageH));

		if (err)
		{
			suites.UtilitySuite5()->AEGP_ReportInfo(S_my_id, ERROR_MISSING_FOOTAGE);
		}

		ERR(suites.ProjSuite5()->AEGP_GetProjectByIndex(0, &projH));
		ERR(suites.ProjSuite5()->AEGP_GetProjectRootFolder(projH, &root_itemH));
		if (!err && root_itemH) {
			err = suites.FootageSuite5()->AEGP_AddFootageToProject(footageH, root_itemH, &footage_itemH);
		}
		if (!err && footage_itemH) {
			err = suites.FootageSuite5()->AEGP_NewFootage(	S_my_id, 
															proxy_pathZ,
															NULL,
															NULL,
															FALSE,  
															NULL,
															&proxy_footageH);

			ERR(suites.FootageSuite5()->AEGP_SetItemProxyFootage(proxy_footageH, footage_itemH));
			
			ERR(suites.ItemSuite6()->AEGP_SetItemUseProxy(footage_itemH, TRUE));

			if (!err) {
				AEGP_CompH		compH;
				A_Boolean		is_add_validB = FALSE;
				AEGP_ItemType	item_type;

				ERR(suites.ItemSuite6()->AEGP_GetItemType(active_itemH, &item_type));	
				
				if (item_type == AEGP_ItemType_COMP) {
					ERR(suites.CompSuite4()->AEGP_GetCompFromItem(active_itemH, &compH));
					ERR(suites.LayerSuite5()->AEGP_IsAddLayerValid(footage_itemH, compH, &is_add_validB));
				}
				
				if (is_add_validB) {
					ERR(suites.LayerSuite5()->AEGP_AddLayer(footage_itemH, compH, &added_layerH));
					ERR(suites.LayerSuite5()->AEGP_ReorderLayer(added_layerH, AEGP_REORDER_LAYER_TO_END));
					ERR(suites.LayerSuite5()->AEGP_SetLayerQuality(added_layerH, AEGP_LayerQual_BEST));
				}
			}
		}
		ERR(suites.CommandSuite1()->AEGP_SetMenuCommandName(S_other_cmd, "i changed my name"));
		ERR(suites.CommandSuite1()->AEGP_CheckMarkMenuCommand(S_other_cmd, TRUE));
	}
	return err;
}


static A_Err
UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,		/* >> */
	AEGP_UpdateMenuRefcon	refconPV,				/* >> */
	AEGP_WindowType			active_window)			/* >> */
{
	A_Err 				err = A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);

	if (S_dump_proj_cmd) {
		err = suites.CommandSuite1()->AEGP_EnableCommand(S_dump_proj_cmd);
	}

	if (!err && S_other_cmd) {
		AEGP_ItemH		active_itemH;
		
		ERR(suites.ItemSuite6()->AEGP_GetActiveItem(&active_itemH));
		if (!err && active_itemH) {
			ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_other_cmd));
			ERR(suites.CommandSuite1()->AEGP_CheckMarkMenuCommand(S_other_cmd, TRUE));
		}
	} else {
		ERR(suites.CommandSuite1()->AEGP_CheckMarkMenuCommand(S_other_cmd, FALSE));
	}
	return err;
}
						
static A_Err
CommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,		/* >> */
	AEGP_CommandRefcon	refconPV,				/* >> */
	AEGP_Command		command,				/* >> */
	AEGP_HookPriority	hook_priority,			/* >> */
	A_Boolean			already_handledB,		/* >> */
	A_Boolean			*handledPB)				/* << */
{
	A_Err 				err = A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);

	*handledPB = FALSE;

	if ((command == S_dump_proj_cmd) || (command == S_other_cmd)) {

		if (command == S_dump_proj_cmd) {
			err = DumpProj();
			*handledPB = TRUE;
		} else if (command == S_other_cmd) {
			err = MakeCuter();
			*handledPB = TRUE;
		}
	}
	return err;
}

A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,			/* >> */
	A_long				 	major_versionL,			/* >> */		
	A_long					minor_versionL,			/* >> */		
	AEGP_PluginID			aegp_plugin_id,			/* >> */
	AEGP_GlobalRefcon		*global_refconP)		/* << */
{
	A_Err 				err = A_Err_NONE;

	S_my_id				= aegp_plugin_id;

	sP 	= pica_basicP;
	
	AEGP_SuiteHandler	suites(pica_basicP);
	
	ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_dump_proj_cmd));
	ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(S_dump_proj_cmd, "ProjDumper", AEGP_Menu_EDIT, AEGP_MENU_INSERT_SORTED));

	ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_other_cmd));
	ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(S_other_cmd, "ProjDumper: Make Cuter", AEGP_Menu_EDIT, AEGP_MENU_INSERT_AT_BOTTOM));

	ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(	S_my_id, 
															AEGP_HP_BeforeAE, 
															AEGP_Command_ALL, 
															CommandHook, 
															NULL));

	ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(S_my_id, UpdateMenuHook, NULL));
	return err;
}
