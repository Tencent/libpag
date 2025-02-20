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

#include "FBIO.h"

/*	Statics and globals are bad. Never use them in production code.  */

static	AEGP_PluginID			S_mem_id					=	0;

static A_Err DeathHook(	
	AEGP_GlobalRefcon unused1 ,
	AEGP_DeathRefcon unused2)
{
	return A_Err_NONE;
}

static A_Err
PretendToReadFileHeader(
						AEIO_BasicData	*basic_dataP,
						FBIO_FileHeader	*file)
{
	A_Err err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	file->fpsT.value		=	0;
	file->fpsT.scale		=	0;
	file->hasVideo			=	TRUE;
	file->hasAudio			=	FALSE;//TRUE;
	file->heightLu			=	480;
	file->widthLu			=	720;
	file->rowbytesLu		=	(4 * file->widthLu);
	file->numFramesLu		=	1;
	file->bitdepthS			=	32;
	file->durationT.value	=	0;
	file->durationT.scale	=	1;
	file->duration_in_msF	=	0;//(file->durationT.value / file->durationT.scale) * 1000;

	return err;
}

static A_Err	
FBIO_InitInSpecFromFile(
	AEIO_BasicData		*basic_dataP,
	const A_UTF16Char	*file_pathZ,
	AEIO_InSpecH		specH)
{ 
	/*	Read the file referenced by the path. Use the 
		file information to set all fields of the AEIO_InSpecH.
	*/

	A_Err			err				=	A_Err_NONE,
					err2			=	A_Err_NONE;
	AEIO_Handle		optionsH		=	NULL,
					old_optionsH	=	NULL;
	FBIO_FileHeader	*headerP;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	if (!err) {
		/*		What are we doing here? 

			+	Allocate a new OptionsH to hold our file header info.
			+	Lock it in memory, copy our local header into the OptionsH.
			+	Unlock handle so AE can move it at will.
		
		*/
		
		ERR(suites.MemorySuite1()->AEGP_NewMemHandle(	S_mem_id,
														"SDK_IO optionsH", 
														sizeof(FBIO_FileHeader), 
														AEGP_MemFlag_CLEAR, 
														&optionsH));
													
		if (optionsH){
			ERR(suites.MemorySuite1()->AEGP_LockMemHandle(optionsH, reinterpret_cast<void **>(&headerP)));
		}	
		if (!err){
			
			ERR(PretendToReadFileHeader(basic_dataP, headerP));

			ERR(suites.IOInSuite4()->AEGP_SetInSpecOptionsHandle(	specH, 
																	optionsH, 
																	reinterpret_cast<void **>(&old_optionsH)));
		
			// [23547] Do _not_ free the old options handle here.  This results in a crash when user chooses 
			// Reload Footage, the handle is freed here, and FBIO_DisposeOutputOptions() is called later on.
			//if (old_optionsH){
				// ERR(suites.MemorySuite1()->AEGP_FreeMemHandle(old_optionsH));
			//}
		}

		/*	
			Set specH information based on what we (pretended to) read from the file.
		*/

		ERR(suites.IOInSuite4()->AEGP_SetInSpecDepth(specH, headerP->bitdepthS)); // always 32 bits for .fak files
		ERR(suites.IOInSuite4()->AEGP_SetInSpecDuration(specH, &(headerP->durationT)));
		ERR(suites.IOInSuite4()->AEGP_SetInSpecDimensions(	specH, 
															static_cast<A_long>(headerP->widthLu),
															static_cast<A_long>(headerP->heightLu)));

		if (!err){
			AEIO_AlphaLabel	alpha;
			AEFX_CLR_STRUCT(alpha);

			alpha.alpha		=	AEIO_Alpha_PREMUL;
			alpha.flags		=	AEIO_AlphaPremul;
			alpha.version	=	AEIO_AlphaLabel_VERSION;
			alpha.red		=	146;
			alpha.green		=	123;
			alpha.blue		=	23;
			
			err = suites.IOInSuite4()->AEGP_SetInSpecAlphaLabel(specH, &alpha);
		}
		ERR2(suites.MemorySuite1()->AEGP_UnlockMemHandle(optionsH));
	}
	return err;
}

static A_Err
FBIO_DisposeInSpec(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH)
{ 
	A_Err				err			=	A_Err_NONE; 
	AEIO_Handle			optionsH	=	NULL;
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);	

	ERR(suites.IOInSuite4()->AEGP_GetInSpecOptionsHandle(specH, reinterpret_cast<void**>(&optionsH)));

	if (optionsH) {
		ERR(suites.MemorySuite1()->AEGP_FreeMemHandle(optionsH));
	}
	return err;
};

static A_Err
FBIO_FlattenOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	AEIO_Handle		*flat_optionsPH)
{ 
	A_Err	err		=	A_Err_NONE,
			err2	=	A_Err_NONE; 

	AEIO_Handle			optionsH	= NULL;
	
	FBIO_FileHeader		*flat_headerP	= NULL,
						*old_headerP	= NULL;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);	
	
	//	Given the AEIO_InSpecH, acquire the non-flat
	//	options handle and use it to create a flat
	//	version. Do NOT de-allocate the non-flat
	//	options handle!
	
	ERR(suites.IOInSuite4()->AEGP_GetInSpecOptionsHandle(specH, reinterpret_cast<void**>(&optionsH)));

	if (optionsH) {
		ERR(suites.MemorySuite1()->AEGP_LockMemHandle(optionsH, reinterpret_cast<void**>(&old_headerP)));
	}
			
	if (old_headerP){
		ERR(suites.MemorySuite1()->AEGP_NewMemHandle(	S_mem_id,
														"flattened_options", 
														sizeof(FBIO_FileHeader), 
														AEGP_MemFlag_CLEAR, 
														flat_optionsPH));
	}

	if (*flat_optionsPH){
		ERR(suites.MemorySuite1()->AEGP_LockMemHandle(*flat_optionsPH, (void**)&flat_headerP));
	}			
	if (!err && flat_headerP) {
		// Here is where you should provide a disk-safe copy of your options data
		ERR(PretendToReadFileHeader(basic_dataP, flat_headerP));
	}

	if (optionsH)
	{
		ERR2(suites.MemorySuite1()->AEGP_UnlockMemHandle(optionsH));
	}
	if (*flat_optionsPH)
	{
		ERR2(suites.MemorySuite1()->AEGP_UnlockMemHandle(*flat_optionsPH));
	}

	return err;
};		

static A_Err
FBIO_InflateOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	AEIO_Handle		flat_optionsH)
{ 
	/*	During this function, 
	
		+	Create a non-flat options structure, containing the info from the 
			flat_optionsH you're passed.
		
		+	Set the options for the InSpecH using AEGP_SetInSpecOptionsHandle().
	*/	
	return A_Err_NONE; 
};		

static A_Err
FBIO_SynchInSpec(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	A_Boolean		*changed0)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
};

static A_Err	
FBIO_GetActiveExtent(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,				/* >> */
	const A_Time	*tr,				/* >> */
	A_LRect			*extent)			/* << */
{ 
	return AEIO_Err_USE_DFLT_CALLBACK; 
};		

static A_Err	
FBIO_GetInSpecInfo(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	AEIO_Verbiage	*verbiageP)
{ 
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	
	suites.ANSICallbacksSuite1()->strcpy(verbiageP->name, "Made-up name");
	suites.ANSICallbacksSuite1()->strcpy(verbiageP->type, "(SDK) Fake File");
	suites.ANSICallbacksSuite1()->strcpy(verbiageP->sub_type, "no particular subtype");

	return A_Err_NONE;
};


static A_Err	
FBIO_DrawSparseFrame(
	AEIO_BasicData					*basic_dataP,
	AEIO_InSpecH					specH, 
	const AEIO_DrawSparseFramePB	*sparse_framePPB, 
	PF_EffectWorld					*wP,
	AEIO_DrawingFlags				*draw_flagsP)
{ 
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	PF_Pixel			color 	= 	{255, 123, 223, 0};
	A_Rect				rectR	=	{0, 0, 0, 0};
	
	//	If the sparse_frame required rect is NOT all zeroes,
	//	use it. Otherwise, just blit the whole thing.
	
	if (!(sparse_framePPB->required_region.top	==
		sparse_framePPB->required_region.left	==
		sparse_framePPB->required_region.bottom ==
		sparse_framePPB->required_region.right)){
		
		rectR.top		= sparse_framePPB->required_region.top;
		rectR.bottom	= sparse_framePPB->required_region.bottom;
		rectR.left		= sparse_framePPB->required_region.left;
		rectR.right		= sparse_framePPB->required_region.right;
	}

	return suites.FillMatteSuite2()->fill(	0, 
											&color, 
											NULL,//reinterpret_cast<const Rect*>(&rectR),
											wP);
};

static A_Err	
FBIO_GetDimensions(
	AEIO_BasicData			*basic_dataP,
	AEIO_InSpecH			 specH, 
	const AEIO_RationalScale *rs0,
	A_long					 *width0, 
	A_long					 *height0)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK; 
};
					
static A_Err	
FBIO_GetDuration(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	A_Time			*tr)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK; 
};

static A_Err	
FBIO_GetTime(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	A_Time			*tr)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK; 
};

static A_Err	
FBIO_GetSound(
	AEIO_BasicData				*basic_dataP,	
	AEIO_InSpecH				specH,
	AEIO_SndQuality				quality,
	const AEIO_InterruptFuncs	*interrupt_funcsP0,	
	const A_Time				*startPT,	
	const A_Time				*durPT,	
	A_u_long					start_sampLu,
	A_u_long					num_samplesLu,
	void						*dataPV)
{ 
	A_Err			err			=	A_Err_NONE;
	AEIO_Handle		optionsH	=	NULL;
	FBIO_FileHeader	*headerP		=	NULL; 
	
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	
	ERR(suites.IOInSuite4()->AEGP_GetInSpecOptionsHandle(specH, reinterpret_cast<void**>(&optionsH)));

	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(optionsH, reinterpret_cast<void**>(&headerP)));

	if (!err) {
		A_char report[AEGP_MAX_ABOUT_STRING_SIZE] = {'\0'};
		suites.ANSICallbacksSuite1()->sprintf(report, "SDK_IO : %d samples of audio requested.", num_samplesLu); 
			
		ERR(suites.UtilitySuite3()->AEGP_ReportInfo(basic_dataP->aegp_plug_id, report));

		ERR(suites.MemorySuite1()->AEGP_UnlockMemHandle(optionsH));
	}

	return err;
};

static A_Err	
FBIO_InqNextFrameTime(
	AEIO_BasicData			*basic_dataP,
	AEIO_InSpecH			specH, 
	const A_Time			*base_time_tr,
	AEIO_TimeDir			time_dir, 
	A_Boolean				*found0,
	A_Time					*key_time_tr0)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK; 
};

static A_Err	
FBIO_DisposeOutputOptions(
	AEIO_BasicData	*basic_dataP,
	void			*optionsPV)
{ 
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	AEIO_Handle			optionsH	=	reinterpret_cast<AEIO_Handle>(optionsPV);
	A_Err				err			=	A_Err_NONE;
	
	if (optionsH){
		ERR(suites.MemorySuite1()->AEGP_FreeMemHandle(optionsH));
	}
	return err;
};
static A_Err	
FBIO_UserOptionsDialog(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	const PF_EffectWorld	*sample0,
	A_Boolean				*user_interacted0)
{ 
	basic_dataP->msg_func(1, "Use the message func hanging off the AEIO_BasicData!");
	return A_Err_NONE;
};

static A_Err	
FBIO_GetOutputInfo(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH,
	AEIO_Verbiage		*verbiageP)
{ 
	A_Err err			= A_Err_NONE;
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	suites.ANSICallbacksSuite1()->strcpy(verbiageP->name, "filename");
	suites.ANSICallbacksSuite1()->strcpy(verbiageP->type, "(SDK) Fake File");
	suites.ANSICallbacksSuite1()->strcpy(verbiageP->sub_type, "no subtypes supported");

	return err;
};

static A_Err	
FBIO_SetOutputFile(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH, 
	const A_UTF16Char	*file_pathZ)
{ 
  	return AEIO_Err_USE_DFLT_CALLBACK;
}

static A_Err	
FBIO_StartAdding(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH, 
	A_long				flags)
{ 
	return A_Err_NONE;  
};

static A_Err	
FBIO_AddFrame(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	A_long					frame_index, 
	A_long					frames,
	const PF_EffectWorld	*wP, 
	const A_LPoint			*origin0,
	A_Boolean				was_compressedB,	
	AEIO_InterruptFuncs		*inter0)
{ 
	return A_Err_NONE; 
};
								
static A_Err	
FBIO_EndAdding(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH			outH, 
	A_long					flags)
{ 
	return A_Err_NONE; 
};

// Since this is a frame-based file format, we need to write out a file per frame
// and so OutputFrame is where this all happens
static A_Err	
FBIO_OutputFrame(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	const PF_EffectWorld	*wP)
{
	A_Err				err			=	A_Err_NONE;
	FBIO_FileHeader		header;
	A_Time				duration	=	{0,1};
	A_short				depth		=	0;
	A_Boolean			deep_worldB	=	0;
	A_Fixed				fps			=	0;
	A_long				widthL 		= 	0,
						heightL 	= 	0;
	A_char				name[AEGP_MAX_PATH_SIZE] = {'\0'};
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	AEGP_MemHandle		unicode_path_memH;
	A_Boolean			fileReservedB;
	A_UTF16Char			*unicode_pathP;

	AEFX_CLR_STRUCT(header);

	ERR(suites.IOOutSuite4()->AEGP_GetOutSpecDuration(outH, &duration));
	ERR(suites.IOOutSuite4()->AEGP_GetOutSpecDimensions(outH, &widthL, &heightL));
	ERR(suites.IOOutSuite4()->AEGP_GetOutSpecDepth(outH, &depth));

	deep_worldB = PF_WORLD_IS_DEEP(wP);

	if (name && widthL && heightL) {
		ERR(suites.IOOutSuite4()->AEGP_GetOutSpecFPS(outH, &fps));
		
		if (!err){
			header.fpsT.value	=	duration.value;
			header.fpsT.scale	=	duration.scale;

			header.hasVideo		=	TRUE;	
			header.hasAudio		=	FALSE;
			header.widthLu		=	(unsigned long)widthL;
			header.heightLu		=	(unsigned long)heightL;
			
			if (depth > 32){
				header.rowbytesLu	=	(unsigned long)(8 * widthL);
			} else {
				header.rowbytesLu	=	(unsigned long)(4 * widthL);
			}

			header.numFramesLu	=	(unsigned long) fps; 

			if (!err){
				suites.IOOutSuite4()->AEGP_GetOutSpecFilePath(outH, &unicode_path_memH, &fileReservedB);
				suites.MemorySuite1()->AEGP_LockMemHandle(unicode_path_memH, reinterpret_cast<void **>(&unicode_pathP));

				/*
					+	Open file
					+	Write header
					+	Re-interpret the supplied PF_EffectWorld in your own
						format, and save it out to the outH's path.
					+	Close file
				*/

				suites.MemorySuite1()->AEGP_UnlockMemHandle(unicode_path_memH);
				suites.MemorySuite1()->AEGP_FreeMemHandle(unicode_path_memH);
			}
		}
	}

	return err;
};

static A_Err	
FBIO_WriteLabels(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	AEIO_LabelFlags	*written)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
};

static A_Err	
FBIO_GetSizes(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	A_u_longlong	*free_space, 
	A_u_longlong	*file_size)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
};

static A_Err	
FBIO_Flush(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH)
{ 
	/*	free any temp buffers you kept around for
		writing.
	*/
	return A_Err_NONE; 
};

static A_Err	
FBIO_Idle(
	AEIO_BasicData			*basic_dataP,
	AEIO_ModuleSignature	sig,
	AEIO_IdleFlags			*idle_flags0)
{ 
	return A_Err_NONE; 
};	


static A_Err	
FBIO_GetDepths(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	AEIO_SupportedDepthFlags		*which)
{ 
	/*	Enumerate possible output depths by OR-ing 
		together different AEIO_SupportedDepthFlags.
	*/
	
	*which =	AEIO_SupportedDepthFlags_DEPTH_24	|	// 8-bit
				AEIO_SupportedDepthFlags_DEPTH_32	|	// 8-bit with alpha
				AEIO_SupportedDepthFlags_DEPTH_48	|	// 16-bit
				AEIO_SupportedDepthFlags_DEPTH_64;		// 16-bit with alpha

	return A_Err_NONE; 
};

static A_Err	
FBIO_GetOutputSuffix(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	A_char			*suffix)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
};


static A_Err	
FBIO_SeqOptionsDlg(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,  
	A_Boolean		*user_interactedPB0)
{ 
	basic_dataP->msg_func(0, "IO: Here's my sequence options dialog!");
	return A_Err_NONE; 
};


static A_Err	
FBIO_CloseSourceFiles(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			seqH)
{ 
	return A_Err_NONE; 
};		// TRUE for close, FALSE for unclose

static A_Err	
FBIO_CountUserData(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			inH,
	A_u_long 				typeLu,
	A_u_long				max_sizeLu,
	A_u_long				*num_of_typePLu)
{ 
	return A_Err_NONE; 
};

static A_Err	
FBIO_SetUserData(    
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH			outH,
	A_u_long				typeLu,
	A_u_long				indexLu,
	const AEIO_Handle		dataH)
{ 
	return A_Err_NONE; 
};

static A_Err	
FBIO_GetUserData(   
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			inH,
	A_u_long 				typeLu,
	A_u_long				indexLu,
	A_u_long				max_sizeLu,
	AEIO_Handle				*dataPH)
{ 
	return A_Err_NONE; 
};
                            
static A_Err	
FBIO_AddMarker(		
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH 			outH, 
	A_long 					frame_index, 
	AEIO_MarkerType 		marker_type, 
	void					*marker_dataPV, 
	AEIO_InterruptFuncs		*inter0)
{ 
	/*	The type of the marker is in marker_type,
		and the text they contain is in 
		marker_dataPV.
	*/
	return A_Err_NONE; 
};

static A_Err	
FBIO_VerifyFileImportable(
	AEIO_BasicData			*basic_dataP,
	AEIO_ModuleSignature	sig, 
	const A_UTF16Char		*file_pathZ, 
	A_Boolean				*importablePB)
{
	//	This function is an appropriate place to do
	//	in-depth evaluation of whether or not a given
	//	file will be importable; AE already does basic
	//	extension checking. Keep in mind that this 
	//	function should be fairly speedy, to keep from
	//	bogging down the user while selecting files.
	
	//	-bbb 3/31/04

	*importablePB = TRUE;
	return A_Err_NONE; 
}		
	
static A_Err	
FBIO_InitOutputSpec(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	A_Boolean				*user_interacted)
{
	A_Err err						= A_Err_NONE;
	AEIO_Handle		optionsH		= NULL, 
					old_optionsH	= 0;
	FBIO_FileHeader	*fileP	=	new FBIO_FileHeader;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEFX_CLR_STRUCT(*fileP);
	
	ERR(suites.MemorySuite1()->AEGP_NewMemHandle(	S_mem_id, 
													"InitOutputSpec options", 
													sizeof(FBIO_FileHeader), 
													AEGP_MemFlag_CLEAR, 
													&optionsH));

	if (!err)
	{
		ERR(PretendToReadFileHeader(basic_dataP, fileP));

		ERR(suites.MemorySuite1()->AEGP_LockMemHandle(optionsH, reinterpret_cast<void**>(&fileP)));

		if (!err) {
			basic_dataP->msg_func(err, "Here is where you would modify the output spec.");

			if (rand() % 7){
				*user_interacted = TRUE;
			} else {
				*user_interacted = FALSE;
			}
			ERR(suites.MemorySuite1()->AEGP_UnlockMemHandle(optionsH));

			ERR(suites.IOOutSuite4()->AEGP_SetOutSpecOptionsHandle(outH, optionsH, reinterpret_cast<void**>(&old_optionsH)));

			if (old_optionsH) {
				ERR(suites.MemorySuite1()->AEGP_FreeMemHandle(old_optionsH));
			}
		}
	}
	return err;
}


static A_Err	
FBIO_OutputInfoChanged(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH)
{
	/*	This function is called when either the user 
		or the plug-in has changed the output options info.
		You may want to update your plug-in's internal
		representation of the output at this time. 
		We've exercised the likely getters below.
	*/
	
	A_Err err					=	A_Err_NONE;
	
	AEIO_AlphaLabel	alpha;
	AEFX_CLR_STRUCT(alpha);
	
	FIEL_Label		fields;
	AEFX_CLR_STRUCT(fields);

	A_short			depthS		=	0;
	A_Time			durationT	=	{0,1};

	A_Fixed			native_fps	=	0;
	A_Ratio			hsf			=	{1,1};
	A_Boolean		is_missingB	=	TRUE;
	
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);	
	
	ERR(suites.IOOutSuite4()->AEGP_GetOutSpecIsMissing(outH, &is_missingB));
	
	if (!is_missingB)
	{
		// Find out all the details of the output spec; update
		// your options data as necessary.
		
		ERR(suites.IOOutSuite4()->AEGP_GetOutSpecAlphaLabel(outH, &alpha));
		ERR(suites.IOOutSuite4()->AEGP_GetOutSpecDepth(outH, &depthS));
		ERR(suites.IOOutSuite4()->AEGP_GetOutSpecInterlaceLabel(outH, &fields));
		ERR(suites.IOOutSuite4()->AEGP_GetOutSpecDuration(outH, &durationT));
		ERR(suites.IOOutSuite4()->AEGP_GetOutSpecFPS(outH, &native_fps));
		ERR(suites.IOOutSuite4()->AEGP_GetOutSpecHSF(outH, &hsf));
	}
	return err;
}



static A_Err	
FBIO_GetFlatOutputOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	AEIO_Handle		*optionsPH)
{
	A_Err			err			=	A_Err_NONE;
	FBIO_FileHeader	*infoP		=	0;
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	
	ERR(suites.MemorySuite1()->AEGP_NewMemHandle(	S_mem_id, 
													"flat optionsH", 
													sizeof(FBIO_FileHeader), 
													AEGP_MemFlag_CLEAR, 
													optionsPH));

	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(*optionsPH, reinterpret_cast<void**>(&infoP)));

	if (!err){
		suites.ANSICallbacksSuite1()->strcpy(infoP->name, "flatoptions");
		infoP->widthLu	=	42;
		infoP->heightLu	=	32;

		ERR(suites.MemorySuite1()->AEGP_UnlockMemHandle(*optionsPH));
	}
	return err;
}

static A_Err
ConstructModuleInfo(
	SPBasicSuite		*pica_basicP,			
	AEIO_ModuleInfo		*info)
{
	A_Err err = A_Err_NONE;

	AEGP_SuiteHandler	suites(pica_basicP);
	
	if (info) {
		info->sig						=	'FFK_';
		info->max_width					=	720;
		info->max_height				=	480;
		info->num_filetypes				=	1;
		info->num_extensions			=	1;
		info->num_clips					=	0;
		
		info->create_kind.type			=	'FFK_';
		info->create_kind.creator		=	AEIO_ANY_CREATOR;

		info->create_ext.pad			=	'.';
		info->create_ext.extension[0]	=	'f';
		info->create_ext.extension[1]	=	'f';
		info->create_ext.extension[2]	=	'k';
		info->num_aux_extensionsS		=	0;
		
		suites.ANSICallbacksSuite1()->strcpy(info->name, "FFK files (SDK)");
		

		info->flags						=	AEIO_MFlag_INPUT					| 
											AEIO_MFlag_OUTPUT					| 
											AEIO_MFlag_FILE						|
											AEIO_MFlag_STILL					|
											AEIO_MFlag_NO_TIME					| 
											AEIO_MFlag_HOST_FRAME_START_DIALOG	|
											AEIO_MFlag_NO_OPTIONS;

		info->read_kinds[0].mac.type			=	'FFK_';
		info->read_kinds[0].mac.creator			=	AEIO_ANY_CREATOR;
		info->read_kinds[1].ext					=	info->create_ext;
		info->read_kinds[2].ext.pad				=	'.';
		info->read_kinds[2].ext.extension[0]	=	'f';
		info->read_kinds[2].ext.extension[1]	=	'f';
		info->read_kinds[2].ext.extension[2]	=	'k';
	}
	else
	{
		err = A_Err_STRUCT;
	}
	return err;
}

A_Err
ConstructFunctionBlock(
	AEIO_FunctionBlock4	*funcs)
{
	if (funcs)	{
		funcs->AEIO_AddFrame				=	FBIO_AddFrame;
		funcs->AEIO_CloseSourceFiles		=	FBIO_CloseSourceFiles;
		funcs->AEIO_CountUserData			=	FBIO_CountUserData;
		funcs->AEIO_DisposeInSpec			=	FBIO_DisposeInSpec;
		funcs->AEIO_DisposeOutputOptions	=	FBIO_DisposeOutputOptions;
		funcs->AEIO_DrawSparseFrame			=	FBIO_DrawSparseFrame;
		funcs->AEIO_EndAdding				=	FBIO_EndAdding;
		funcs->AEIO_FlattenOptions			=	FBIO_FlattenOptions;
		funcs->AEIO_Flush					=	FBIO_Flush;
		funcs->AEIO_GetActiveExtent			=	FBIO_GetActiveExtent;
		funcs->AEIO_GetDepths				=	FBIO_GetDepths;
		funcs->AEIO_GetDimensions			=	FBIO_GetDimensions;
		funcs->AEIO_GetDuration				=	FBIO_GetDuration;
		funcs->AEIO_GetSound				=	FBIO_GetSound;
		funcs->AEIO_GetInSpecInfo			=	FBIO_GetInSpecInfo;
		funcs->AEIO_GetOutputInfo			=	FBIO_GetOutputInfo;
		funcs->AEIO_GetOutputSuffix			=	FBIO_GetOutputSuffix;
		funcs->AEIO_GetSizes				=	FBIO_GetSizes;
		funcs->AEIO_GetTime					=	FBIO_GetTime;
		funcs->AEIO_GetUserData				=	FBIO_GetUserData;
		funcs->AEIO_InflateOptions			=	FBIO_InflateOptions;
		funcs->AEIO_InitInSpecFromFile		=	FBIO_InitInSpecFromFile;
		funcs->AEIO_InqNextFrameTime		=	FBIO_InqNextFrameTime;
		funcs->AEIO_OutputFrame				=	FBIO_OutputFrame;
		funcs->AEIO_SeqOptionsDlg			=	FBIO_SeqOptionsDlg;
		funcs->AEIO_SetOutputFile			=	FBIO_SetOutputFile;
		funcs->AEIO_SetUserData				=	FBIO_SetUserData;
		funcs->AEIO_StartAdding				=	FBIO_StartAdding;
		funcs->AEIO_SynchInSpec				=	FBIO_SynchInSpec;
		funcs->AEIO_UserOptionsDialog		=	FBIO_UserOptionsDialog;
		funcs->AEIO_VerifyFileImportable	=	FBIO_VerifyFileImportable;
		funcs->AEIO_WriteLabels				=	FBIO_WriteLabels;
		funcs->AEIO_InitOutputSpec			=	FBIO_InitOutputSpec;
		funcs->AEIO_GetFlatOutputOptions	=	FBIO_GetFlatOutputOptions;
		funcs->AEIO_OutputInfoChanged		=	FBIO_OutputInfoChanged;
		funcs->AEIO_AddMarker				=	FBIO_AddMarker;
		funcs->AEIO_Idle					=	FBIO_Idle;

		return A_Err_NONE;
	} else {
		return A_Err_STRUCT;
	}
}

A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,			/* >> */
	A_long				 	major_versionL,			/* >> */		
	A_long					minor_versionL,			/* >> */		
	AEGP_PluginID			aegp_plugin_id,			/* >> */
	AEGP_GlobalRefcon		*global_refconP)		/* << */
{
	A_Err 				err					= A_Err_NONE;

	AEIO_ModuleInfo		info;
	AEIO_FunctionBlock4	funcs;
	AEGP_SuiteHandler	suites(pica_basicP);	

	AEFX_CLR_STRUCT(info);
	AEFX_CLR_STRUCT(funcs);
	
	ERR(suites.RegisterSuite5()->AEGP_RegisterDeathHook(aegp_plugin_id, DeathHook, 0));
	ERR(ConstructModuleInfo(pica_basicP, &info));
	ERR(ConstructFunctionBlock(&funcs));

	ERR(suites.RegisterSuite5()->AEGP_RegisterIO(	aegp_plugin_id,
													0,
													&info, 
													&funcs));

	ERR(suites.UtilitySuite3()->AEGP_RegisterWithAEGP(	NULL,
														"FBIO",
														&S_mem_id));
	return err;
}
