/*
	File: AE_General.r

	Copyright 1991-96 by Adobe Systems Inc..

	Photoshop's Rinclude with AE extensions
	
*/

#ifndef __AEPIGeneral_r__
#define __AEPIGeneral_r__

#ifndef kPIPropertiesVersion
#define kPIPropertiesVersion 0
#endif

type 'PiPL'
	{
	longint = kPIPropertiesVersion;
	longint = $$CountOf(properties);
	array properties
		{
		switch
			{

/* Properties for all types of plug-ins. */

			case Kind:
				longint = '8BIM';
				key longint = 'kind';
				longint = 0;
				longint = 4;
				literal longint Filter = '8BFM', Parser = '8BYM', ImageFormat='8BIF',
						Extension = '8BXM', Acquire = '8BAM', Export = '8BEM',
						/* AE specific */
						AEEffect = 'eFKT', AEImageFormat = 'FXIF', AEAccelerator = 'eFST',
						AEGeneral = 'AEgp', AEGP = 'AEgx', AEForeignProjectFormat = 'eFPF';

			case Version:
				longint = '8BIM';
				key longint = 'vers';
				longint = 0;
				longint = 4;
				longint;

			case Priority:
				longint = '8BIM';
				key longint = 'prty';
				longint = 0;
				longint = 4;
				longint;

			case RequiredHost:
				longint = '8BIM';
				key longint = 'host';
				longint = 0;
				longint = 4;
				literal longint;

			case Name:
				longint = '8BIM';
				key longint = 'name';
				longint = 0;
#if DeRez
				fill long;
#else
				longint = (nameEnd[$$ArrayIndex(properties)] - nameStart[$$ArrayIndex(properties)]) / 8;
#endif
			  nameStart:
				pstring;
			  nameEnd:
				align long;

			case Category:
				longint = '8BIM';
				key longint = 'catg';
				longint = 0;
#if DeRez
				fill long;
#else
				longint = (catgEnd[$$ArrayIndex(properties)] - catgStart[$$ArrayIndex(properties)]) / 8;
#endif
			  catgStart:
				pstring;
			  catgEnd:
				align long;

			case Code68k:
				longint = '8BIM';
				key longint = 'm68k';
				longint = 0;
				longint = 6;
				literal longint;
				integer;
				align long;

			case Code68kFPU:
				longint = '8BIM';
				key longint = '68fp';
				longint = 0;
				longint = 6;
				literal longint;
				integer;
				align long;

			case CodePowerPC:
				longint = '8BIM';
				key longint = 'pwpc';
				longint = 0;
#if DeRez
				fill long;
#else
				longint = (pwpcEnd[$$ArrayIndex(properties)] - pwpcStart[$$ArrayIndex(properties)]) / 8;
#endif
			  pwpcStart:
				longint;
				longint;
				pstring;
			  pwpcEnd:
				align long;

			case CodeCarbonPowerPC:
				longint = '8BIM';
				key longint = 'ppcb';
				longint = 0;
				#if DeRez
					fill long;
				#else
					longint = (pwpcCarbonEnd[$$ArrayIndex(properties)] - pwpcCarbonStart[$$ArrayIndex(properties)]) / 8;
				#endif
			  pwpcCarbonStart:
				longint;
				longint;
				pstring;
			  pwpcCarbonEnd:
				align long;
				
			// Mach-O Support
			case CodeMachOPowerPC:
				longint = '8BIM';
				key longint = 'mach';
				longint = 0;
				#if DeRez
					fill long;
				#else
					longint = (pwpcMachEnd[$$ArrayIndex(properties)] - pwpcMachStart[$$ArrayIndex(properties)]) / 8;
				#endif
			  pwpcMachStart:
				pstring;
			  pwpcMachEnd:
				align long;

			// Mach-O Intel 32bit Support
			case CodeMacIntel32:
				longint = '8BIM';
				key longint = 'mi32';
				longint = 0;
				#if DeRez
					fill long;
				#else
					longint = (pwpcMacIntel32End[$$ArrayIndex(properties)] - pwpcMacIntel32Start[$$ArrayIndex(properties)]) / 8;
				#endif
			  pwpcMacIntel32Start:
				pstring;
			  pwpcMacIntel32End:
				align long;

			// Mach-O Intel 64bit Support
			case CodeMacIntel64:
				longint = '8BIM';
				key longint = 'mi64';
				longint = 0;
				#if DeRez
					fill long;
				#else
					longint = (pwpcMacIntel64End[$$ArrayIndex(properties)] - pwpcMacIntel64Start[$$ArrayIndex(properties)]) / 8;
				#endif
			  pwpcMacIntel64Start:
				pstring;
			  pwpcMacIntel64End:
				align long;

			case CodeMacARM64:
				longint = '8BIM';
				key longint = 'ma64';
				longint = 0;
				#if DeRez
					fill long;
				#else
					longint = (pwpcMacARM64End[$$ArrayIndex(properties)] - pwpcMacARM64Start[$$ArrayIndex(properties)]) / 8;
				#endif
			  pwpcMacARM64Start:
				pstring;
			  pwpcMacARM64End:
				align long;


#ifdef AE_OS_WIN /* For documentation purposes only. */
			case CodeWin32X86:
				longint = '8BIM';
				key longint = 'wx86';
				longint = 0;
				longint = (win32x86End[$$ArrayIndex(properties)] - win32x86Start[$$ArrayIndex(properties)]) / 8;
			  win32x86Start:
				cstring;
			  win32x86End:
				align long;
				
			case CodeWin64X86:
				longint = '8BIM';
				key longint = '8664';
				longint = 0;
				longint = (win64x86End[$$ArrayIndex(properties)] - win64x86Start[$$ArrayIndex(properties)]) / 8;
			  win64x86Start:
				cstring;
			  win64x86End:
				align long;
#endif

			case SupportedModes:
				longint = '8BIM';
				key longint = 'mode';
				longint = 0;
#if DeRez
				fill long;
#else
				longint = (modeEnd[$$ArrayIndex(properties)] - modeStart[$$ArrayIndex(properties)]) / 8;
#endif
			  modeStart:
				boolean noBitmap, doesSupportBitmap;
				boolean noGrayScale, doesSupportGrayScale;
				boolean noIndexedColor, doesSupportIndexedColor;
				boolean noRGBColor, doesSupportRGBColor;
				boolean noCMYKColor, doesSupportCMYKColor;
				boolean noHSLColor, doesSupportHSLColor;
				boolean noHSBColor, doesSupportHSBColor;
				boolean noMultichannel, doesSupportMultichannel;
				boolean noDuotone, doesSupportDuotone;
				boolean noLABColor, doesSupportLABColor;
				boolean noGray16, doesSupportGray16;
				boolean noRGB48, doesSupportRGB48;
				boolean noLab48, doesSupportLab48;
				boolean noCMYK64, doesSupportCMYK64;
				boolean noDeepMultichannel, doesSupportDeepMultichannel;
				boolean noDuotone16, doesSupportDuotone16;
			  modeEnd:
				align long;

/* Filter plug-in properties. */

			case FilterCaseInfo:
				longint = '8BIM';
				key longint = 'fici';
				longint = 0;
				longint = 28;
				array [7]
					{
					byte inCantFilter = 0,
						 inStraightData = 1,
						 inBlackMat = 2,
						 inGrayMat = 3,
						 inWhiteMat = 4,
						 inDefringe = 5,
						 inBlackZap = 6,
						 inGrayZap = 7,
						 inWhiteZap = 8,
						 inBackgroundZap = 10,
						 inForegroundZap = 11;
					byte outCantFilter = 0,
						 outStraightData = 1,
						 outBlackMat = 2,
						 outGrayMat = 3,
						 outWhiteMat = 4,
						 outFillMask = 9;
					fill bit [5];
					boolean doesNotFilterLayerMasks, filtersLayerMasks;
					boolean doesNotWorkWithBlankData, worksWithBlankData;
					boolean copySourceToDestination, doNotCopySourceToDestination;
					fill byte;
					} ;

/* Export plug-in properties. */

			case ExportFlags:
				longint = '8BIM';
				key longint = 'expf';
				longint = 0;
#if DeRez
				fill long;
#else
				longint = (expFlagsEnd[$$ArrayIndex(properties)] - expFlagsStart[$$ArrayIndex(properties)]) / 8;
#endif
			 expFlagsStart:
				boolean expDoesNotSupportTransparency, expSupportsTransparency;
				fill bit[7];
			  expFlagsEnd:
				align long;

/* Format plug-in properties. */
// If this property is present, then its on.  No parameters
			case SupportsPOSIXIO:
				longint = '8BIM';
				key longint = 'fxio';
				longint = 0;
				longint = 4;
				literal longint = 1;

			case FmtFileType:
				longint = '8BIM';
				key longint = 'fmTC';
				longint = 0;
				longint = 8;
				literal longint; /* Default file type. */
				literal longint; /* Default file creator. */

			case ReadTypes:
				longint = '8BIM';
				key longint = 'RdTy';
				longint = 0;
				longint = $$CountOf(ReadableTypes) * 8;
				wide array ReadableTypes { literal longint; literal longint; } ;

			case WriteTypes:
				longint = '8BIM';
				key longint = 'WrTy';
				longint = 0;
				longint = $$CountOf(WritableTypes) * 8;
				wide array WritableTypes { literal longint; literal longint; } ;

			case FilteredTypes:
				longint = '8BIM';
				key longint = 'fftT';
				longint = 0;
				longint = $$CountOf(FilteredTypes) * 8;
				wide array FilteredTypes { literal longint; literal longint; } ;

			case ReadExtensions:
				longint = '8BIM';
				key longint = 'RdEx';
				longint = 0;
				longint = $$CountOf(ReadableExtensions) * 4;
				wide array ReadableExtensions { literal longint; } ;

			case WriteExtensions:
				longint = '8BIM';
				key longint = 'WrEx';
				longint = 0;
				longint = $$CountOf(WriteableExtensions) * 4;
				wide array WriteableExtensions { literal longint; } ;

			case FilteredExtensions:
				longint = '8BIM';
				key longint = 'fftE';
				longint = 0;
				longint = $$CountOf(FilteredExtensions) * 4;
				wide array FilteredExtensions { literal longint; } ;

			case FormatFlags:
				longint = '8BIM';
				key longint = 'fmtf';
				longint = 0;
/*				longint = (fmtFlagsEnd[$$ArrayIndex(properties)] - fmtFlagsStart[$$ArrayIndex(properties)]) / 8; */
				longint = 1;
			 fmtFlagsStart:
				boolean fmtReadsAllTypes, fmtDoesntReadAllTypes;
				boolean fmtDoesNotSaveImageResources, fmtSavesImageResources;
				boolean fmtCannotRead, fmtCanRead;
				boolean fmtCannotWrite, fmtCanWrite;
				boolean fmtWritesAll, fmtCanWriteIfRead;
				fill bit[3];
			  fmtFlagsEnd:
				align long;
				
			case FormatICCFlags:
				longint = '8BIM';
				key longint = 'fmip';
				longint = 0;
				//longint = (iccFlagsEnd[$ArrayIndex(properties)] - iccFlagsStart[$ArrayIndex(properties)]) / 8;
				longint = 1;
			iccFlagsStart:
				boolean iccCannotEmbedGray, iccCanEmbedGray;
				boolean iccCannotEmbedIndexed, iccCanEmbedIndexed;
				boolean iccCannotEmbedRGB, iccCanEmbedRGB;
				boolean iccCannotEmbedCMYK, iccCanEmbedCMYK;
				fill bit[4];
			iccFlagsEnd:
				align long;

			case FormatMaxSize:
				longint = '8BIM';
				key longint = 'mxsz';
				longint = 0;
				longint = 4;
				Point;

			case FormatMaxChannels:
				longint = '8BIM';
				key longint = 'mxch';
				longint = 0;
				longint = $$CountOf(ChannelsSupported) * 2;
				wide array ChannelsSupported { integer; } ;
				align long;
				
			/* after effects specific PiPL atoms */

			case AE_PiPL_Version:
				longint = '8BIM';
				key longint = 'ePVR';
				longint = 0;
				longint = 4;
				integer;
				integer;
				
			case AE_Effect_Spec_Version:
				longint = '8BIM';
				key longint = 'eSVR';
				longint = 0;
				longint = 4;
				integer;
				integer;


			case AE_Effect_Version:
				longint = '8BIM';
				key longint = 'eVER';
				longint = 0;
				longint = 4;
				longint;
				
				
			case AE_Effect_Info_Flags:
				longint = '8BIM';
				key longint = 'eINF';
				longint = 0;
				longint = 2;
				integer;
				align long;
/*				
			case AE_Effect_Global_OutFlags:
				longint = '8BIM';
				key longint = 'eGLO';
				longint = 0;
#if DeRez
				fill long;
#else
				longint = (gloEnd[$$ArrayIndex(properties)] - gloStart[$$ArrayIndex(properties)]) / 8;
#endif
			  gloStart:
				fill bit[6];
				boolean noReserved4, reserved4;
				boolean noReserved3, reserved3;
				boolean noReserved2, reserved2;
				boolean noReserved1, reserved1;
				boolean noObsolete, obsolete;
				boolean noAudio, audio;
				boolean noShutterAngle, shutterAngle;
				boolean noNopRender, nopRender;
				boolean noRefreshUI, refreshUI;
				boolean noCustomNtrp, customNtrp;
				boolean noCustomUI, customUI;
				boolean noNonSquareOnly, nonSquareOnly;
				boolean noWorksInPlace, worksInPlace;
				boolean noShrinkBuf, shrinkBuf;
				boolean noWriteInput, writeInput;
				boolean noPixIndep, pixIndep;
				boolean noExpandBuf, expandBuf;
				boolean noErrMsg, errMsg;
				boolean noSendDialog, sendDialog;
				boolean noOutputExtent, outputExtent;
				boolean noDialog, dialog;
				boolean noSeqFlatten, seqFlatten;
				boolean noSendParamsUpdate, sendParamsUpdate;
				boolean noRandomness, randomness;
				boolean noWideTimeInput, wideTimeInput;
				boolean noKeepRsrcOpen, keepRsrcOpen;
			  gloEnd:
				align long;
*/
			case AE_Effect_Global_OutFlags:
				longint = '8BIM';
				key longint = 'eGLO';
				longint = 0;
				longint = 4;
				longint;

			case AE_Effect_Global_OutFlags_2:
				longint = '8BIM';
				key longint = 'eGL2';
				longint = 0;
				longint = 4;
				longint;

			case AE_Effect_Match_Name:
				longint = '8BIM';
				key longint = 'eMNA';
				longint = 0;
#if DeRez
				fill long;
#else
				longint = (matchNameEnd[$$ArrayIndex(properties)] - matchNameStart[$$ArrayIndex(properties)]) / 8;
#endif
			  matchNameStart:
				pstring;
			  matchNameEnd:
				align long;
				
			case AE_ImageFormat_Extension_Info:
				longint = '8BIM';
				key longint = 'FXMF';
				longint = 0;
				longint = 16;
				integer;		/* major version */
				integer;		/* minor version */
				fill bit[21];
				boolean hasOptions, hasNoOptions;
				boolean sequentialOnly, nonSequentialOk;
				boolean noInteractRequired, mustInteract;
				boolean noInteractPut, hasInteractPut;
				boolean noInteractGet, hasInteractGet;
				boolean hasTime, hasNoTime;
				boolean noVideo, hasVideo;
				boolean noStill, still;
				boolean noFile, hasFile;
				boolean noOutput, output;
				boolean noInput, input;				
				longint = 0;	/* reserved */
				literal longint;/* signature */
				
			
			case AE_Reserved:
				longint = '8BIM';
				key longint = 'aeRD';
				longint = 0;
				longint = 4;
				longint;

			case AE_Reserved_Info:
				longint = '8BIM';
				key longint = 'aeFL';
				longint = 0;
				longint = 4;
				longint;

			case AE_Effect_Support_URL:
				longint = '8BIM';
				key longint = 'eURL';
				longint = 0;
#if DeRez
				fill long;
#else
				longint = (supportURLEnd[$$ArrayIndex(properties)] - supportURLStart[$$ArrayIndex(properties)]) / 8;
#endif
			  supportURLStart:
				pstring;
			  supportURLEnd:
				align long;

			};
		};
	};


#endif
