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


#ifndef _H_AE_ChannelSuites
#define _H_AE_ChannelSuites

#include <AE_Effect.h>
#include <SPBasic.h>

#include <adobesdk/config/PreConfig.h>


#ifdef __cplusplus
	extern "C" {
#endif


/** PF_ChannelSuite

	PF_GetLayerChannelCount 
		use this to find the number of channels associated with a given source layer
		Most likely use is to get number of channels for iteration purposes.
		
		-param_index is the parameter index of the layer whose source you wish to interrogate
		-num_paramsL is the number of "auxillary" channels 

		
		PF_GetLayerIndexedChannelRefAndDesc
			Given a channel index return the opaque channelRef and a channel description
			channel index must lie between 0 and num_channels-1
			you will use the channelRef in all subsequent calls 

		PF_GetLayerTypedChannelRefAndDesc
			Given a type retrieve the channelRef and ChannelDescription
		
		PF_CheckoutLayerChannel
			given the time parameters and a channel reference, get the data
			The data chunk is allocated is of the type requested.
			The data is in chunky format.
			

			
					PF_ProgPtr			effect_ref,				>>		
					PF_ChannelRefPtr	channel_refP,		    >>
					A_long				what_time,				>>
					A_long				duration,				>>
					A_u_long		time_scale,				>>
					PF_DataType			data_type,				>>
					PF_ChannelData		*channel_chunkP)	<<------

		PF_CheckinLayerChannel
			The checked out channel must be checked in to avoid memory leaks.
					PF_ProgPtr			effect_ref,				
					PF_ChannelRefPtr	channel_refP,			
					PF_ChannelDataPtr	channel_data_chunkP	

**/

#define PF_CHANNEL_DEPTH_INFINITY 1e7

#define kPFChannelSuite1			"PF AE Channel Suite"
#define kPFChannelSuiteVersion1		1	/* frozen in AE 5.0 */




/**
 ** channel  data access macros with a check for the type
 **/

#define PF_GET_CHANNEL_FLOAT_DATA( CHUNK, FLOAT_PTR)		\
	do {													\
		if ( (CHUNK).data_type == PF_DataType_FLOAT) {		\
			FLOAT_PTR = (A_FpShort *) (CHUNK).dataPV;			\
		} else {											\
			FLOAT_PTR = NULL;								\
		}													\
	} while(0)


#define PF_GET_CHANNEL_DOUBLE_DATA( CHUNK, DOUBLE_PTR)		\
	do {													\
		if ((CHUNK).data_type == PF_DataType_DOUBLE) {		\
			DOUBLE_PTR = (A_FpLong *) (CHUNK).dataPV;			\
		} else {											\
			DOUBLE_PTR = NULL;								\
		}													\
	} while(0)


#define PF_GET_CHANNEL_LONG_DATA( CHUNK, LONG_PTR)			\
	do {													\
		if ((CHUNK).data_type == PF_DataType_LONG) {		\
			LONG_PTR = (A_long *) (CHUNK).dataPV;				\
		} else {											\
			LONG_PTR = NULL;								\
		}													\
	} while(0)



#define PF_GET_CHANNEL_SHORT_DATA( CHUNK, SHORT_PTR)		\
	do {													\
		if ((CHUNK).data_type == PF_DataType_SHORT) {		\
			SHORT_PTR = (A_short *) (CHUNK).dataPV;			\
		} else {											\
			SHORT_PTR = NULL;								\
		}													\
	} while(0)


#define PF_GET_CHANNEL_FIXED_DATA( CHUNK, FIXED_PTR)		\
	do {													\
		if ((CHUNK).data_type == PF_DataType_FIXED_16_16) {	\
			FIXED_PTR = (A_long *) (CHUNK).dataPV;			\
		} else {											\
			FIXED_PTR = NULL;								\
		}													\
	} while(0)




#define PF_GET_CHANNEL_CHAR_DATA( CHUNK, CHAR_PTR)			\
	do {													\
		if ((CHUNK).data_type == PF_DataType_CHAR) {		\
			CHAR_PTR = (A_char *) (CHUNK).dataPV;				\
		} else {											\
			CHAR_PTR = NULL;								\
		}													\
	} while(0)




#define PF_GET_CHANNEL_U_BYTE_DATA( CHUNK, BYTE_PTR)		\
	do {													\
		if ((CHUNK).data_type == PF_DataType_U_BYTE) {		\
			BYTE_PTR = (A_u_char *) (CHUNK).dataPV;	\
		} else {											\
			BYTE_PTR = NULL;								\
		}													\
	} while(0)





#define PF_GET_CHANNEL_U_SHORT_DATA( CHUNK, U_SHORT_PTR)	\
	do {													\
		if ((CHUNK).data_type == PF_DataType_U_SHORT) {		\
			U_SHORT_PTR = (A_u_short *) (CHUNK).dataPV;\
		} else {											\
			U_SHORT_PTR = NULL;								\
		}													\
	} while(0)




#define PF_GET_CHANNEL_U_FIXED_DATA( CHUNK, U_FIXED_PTR)	\
	do {													\
		if ((CHUNK).data_type == PF_DataType_U_FIXED_16_16) {	\
			U_FIXED_PTR = (A_u_long) (CHUNK).dataPV;	\
		} else {											\
			U_FIXED_PTR = NULL;								\
		}													\
	} while(0)




/**
 ** get a row of the chunk data
 **/
#define PF_GET_CHANNEL_ROW_FLOAT_DATA( CHUNK, ROW, FLOAT_PTR)		\
	do {															\
		if (((CHUNK).data_type == PF_DataType_FLOAT) && ((ROW) < (CHUNK).heightL)) {					\
			FLOAT_PTR = (A_FpShort *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			FLOAT_PTR = NULL;										\
		}															\
	} while(0)


#define PF_GET_CHANNEL_ROW_DOUBLE_DATA( CHUNK, ROW, DOUBLE_PTR)		\
	do {															\
		if (((CHUNK).data_type == PF_DataType_DOUBLE) && ((ROW) < (CHUNK).heightL)) {				\
			DOUBLE_PTR = (A_FpLong *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			DOUBLE_PTR = NULL;										\
		}															\
	} while(0)


#define PF_GET_CHANNEL_ROW_LONG_DATA( CHUNK, ROW, LONG_PTR)			\
	do {															\
		if (((CHUNK).data_type == PF_DataType_LONG) && ((ROW) < (CHUNK).heightL)) {			\
			LONG_PTR = (A_long *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			LONG_PTR = NULL;										\
		}															\
	} while(0)



#define PF_GET_CHANNEL_ROW_SHORT_DATA( CHUNK, ROW, SHORT_PTR)		\
	do {															\
		if (((CHUNK).data_type == PF_DataType_SHORT) && ((ROW) < (CHUNK).heightL)) {			\
			SHORT_PTR = (A_short *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			SHORT_PTR = NULL;										\
		}															\
	} while(0)


#define PF_GET_CHANNEL_ROW_FIXED_DATA( CHUNK, ROW, FIXED_PTR)		\
	do {															\
		if (((CHUNK).data_type == PF_DataType_FIXED_16_16) && ((ROW) < (CHUNK).heightL)) {	\
			FIXED_PTR = (A_long *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			FIXED_PTR = NULL;										\
		}															\
	} while(0)




#define PF_GET_CHANNEL_ROW_CHAR_DATA( CHUNK, ROW, CHAR_PTR)			\
	do {															\
		if (((CHUNK).data_type == PF_DataType_CHAR) && ((ROW) < (CHUNK).heightL)) {			\
			CHAR_PTR = (A_char *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			CHAR_PTR = NULL;										\
		}															\
	} while(0)




#define PF_GET_CHANNEL_ROW_U_BYTE_DATA( CHUNK, ROW, BYTE_PTR)		\
	do {															\
		if (((CHUNK).data_type == PF_DataType_U_BYTE) && ((ROW) < (CHUNK).heightL)) {		\
			BYTE_PTR = (A_u_char *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			BYTE_PTR = NULL;										\
		}															\
	} while(0)





#define PF_GET_CHANNEL_ROW_U_SHORT_DATA( CHUNK, ROW, U_SHORT_PTR)	\
	do {															\
		if (((CHUNK).data_type == PF_DataType_U_SHORT) && ((ROW) < (CHUNK).heightL)) {		\
			U_SHORT_PTR = (A_u_short *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			U_SHORT_PTR = NULL;										\
		}															\
	} while(0)




#define PF_GET_CHANNEL_ROW_U_FIXED_DATA( CHUNK, ROW, U_FIXED_PTR)	\
	do {															\
		if (((CHUNK).data_type == PF_DataType_U_FIXED_16_16) && ((ROW) < (CHUNK).heightL)) {	\
			U_FIXED_PTR = (A_u_long) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
		} else {													\
			U_FIXED_PTR = NULL;										\
		}															\
	} while(0)






/**
 ** get a item from the chunk data
 **/
#define PF_GET_CHANNEL_ROW_COL_FLOAT_DATA( CHUNK, ROW, COL, FLOAT_PTR)	\
	do {																\
		if (((CHUNK).data_type == PF_DataType_FLOAT)	&&				\
			((ROW) >= 0)								&&				\
			((COL) >= 0)								&&				\
			((ROW) < (CHUNK).heightL)					&&				\
			((COL) < (CHUNK).widthL)) {									\
			FLOAT_PTR = (A_FpShort *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
			FLOAT_PTR = (A_FpShort *)FLOAT_PTR + (COL) * (CHUNK).dimensionL;	\
		} else {														\
			FLOAT_PTR = NULL;											\
		}																\
	} while(0)


#define PF_GET_CHANNEL_ROW_COL_DOUBLE_DATA( CHUNK, ROW, COL, DOUBLE_PTR)	\
	do {																	\
		if (((CHUNK).data_type == PF_DataType_DOUBLE) 	&&					\
			((ROW) >= 0)								&&					\
			((COL) >= 0)								&&					\
			((ROW) < (CHUNK).heightL)					&&					\
			((COL) < (CHUNK).widthL)) {										\
			DOUBLE_PTR = (A_FpLong *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
			DOUBLE_PTR = (A_FpLong *)DOUBLE_PTR + (COL) * (CHUNK).dimensionL;	\
		} else {															\
			DOUBLE_PTR = NULL;												\
		}																	\
	} while(0)


#define PF_GET_CHANNEL_ROW_COL_LONG_DATA( CHUNK, ROW, COL, LONG_PTR)		\
	do {																	\
		if (((CHUNK).data_type == PF_DataType_LONG) 	&&					\
			((ROW) >= 0)								&&					\
			((COL) >= 0)								&&					\
			((ROW) < (CHUNK).heightL)					&&					\
			((COL) < (CHUNK).widthL)) {										\
				LONG_PTR = (A_long *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
				LONG_PTR = (A_long *) LONG_PTR + (COL) * (CHUNK).dimensionL;	\
		} else {															\
				LONG_PTR = NULL;											\
		}																	\
	} while(0)



#define PF_GET_CHANNEL_ROW_COL_SHORT_DATA( CHUNK, ROW, COL, SHORT_PTR)	\
	do {																\
		if (((CHUNK).data_type == PF_DataType_SHORT)	&&				\
			((ROW) >= 0)								&&				\
			((COL) >= 0)								&&				\
			((ROW) < (CHUNK).heightL)					&&				\
			((COL) < (CHUNK).widthL)) {									\
				SHORT_PTR = (A_short *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
				SHORT_PTR = (A_short *) SHORT_PTR + (COL) * (CHUNK).dimensionL;\
		} else {														\
			SHORT_PTR = NULL;											\
		}																\
	} while(0)


#define PF_GET_CHANNEL_ROW_COL_FIXED_DATA( CHUNK, ROW, COL, FIXED_PTR)	\
	do {																\
		if (((CHUNK).data_type == PF_DataType_FIXED_16_16)  		&&	\
			((ROW) >= 0)							&&					\
			((COL) >= 0)							&&					\
			((ROW) < (CHUNK).heightL)				&&					\
			((COL) < (CHUNK).widthL)) {									\
			FIXED_PTR = (A_long *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
			FIXED_PTR = (A_long *) FIXED_PTR + (COL) * (CHUNK).dimensionL;	\
		} else {														\
			FIXED_PTR = NULL;											\
		}																\
	} while(0)




#define PF_GET_CHANNEL_ROW_COL_CHAR_DATA( CHUNK, ROW, COL, CHAR_PTR)		\
	do {																	\
		if (((CHUNK).data_type == PF_DataType_CHAR)  	&&					\
			((ROW) >= 0)								&&					\
			((COL) >= 0)								&&					\
			((ROW) < (CHUNK).heightL)					&&					\
			((COL) < (CHUNK).widthL)) {										\
			CHAR_PTR = (A_char *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
			CHAR_PTR = (A_char *) CHAR_PTR + (COL) * (CHUNK).dimensionL;		\
		} else {															\
			CHAR_PTR = NULL;												\
		}																	\
	} while(0)




#define PF_GET_CHANNEL_ROW_COL_U_BYTE_DATA( CHUNK, ROW, COL, BYTE_PTR)		\
	do {																	\
		if (((CHUNK).data_type == PF_DataType_U_BYTE)   			&&		\
			((ROW) >= 0)											&&		\
			((COL) >= 0)											&&		\
			((ROW) < (CHUNK).heightL)								&&		\
			((COL) < (CHUNK).widthL)) {											\
			BYTE_PTR = (A_u_char *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
			BYTE_PTR = (A_u_char *)  BYTE_PTR + (COL) * (CHUNK).dimensionL;			\
		} else {															\
			BYTE_PTR = NULL;												\
		}																	\
	} while(0)





#define PF_GET_CHANNEL_ROW_COL_U_SHORT_DATA( CHUNK, ROW, COL, U_SHORT_PTR)	\
	do {																	\
		if (((CHUNK).data_type == PF_DataType_U_SHORT)   				&&	\
			((ROW) >= 0)												&&	\
			((COL) >= 0)												&&	\
			((ROW) < (CHUNK).heightL)									&&	\
			((COL) < (CHUNK).widthL)) {										\
			U_SHORT_PTR = (A_u_short *) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
			U_SHORT_PTR = (A_u_short *) U_SHORT_PTR + (COL) * (CHUNK).dimensionL;	\
		} else {															\
			U_SHORT_PTR = NULL;												\
		}																	\
	} while(0)




#define PF_GET_CHANNEL_ROW_COL_U_FIXED_DATA( CHUNK, ROW, COL, U_FIXED_PTR)	\
	do {																	\
		if (((CHUNK).data_type == PF_DataType_U_FIXED_16_16) 			&&	\
			((ROW) >= 0)												&&	\
			((COL) >= 0)												&&	\
			((ROW) < (CHUNK).heightL)									&&	\
			((COL) < (CHUNK).widthL)) {										\
			U_FIXED_PTR = (A_u_long) ((A_char *)(CHUNK).dataPV + (ROW) * (CHUNK).row_bytesL);	\
			U_FIXED_PTR = (A_u_long)  U_FIXED_PTR + (COL) * (CHUNK).dimensionL;	\
		} else {															\
			U_FIXED_PTR = NULL;												\
		}																	\
	} while(0)






/**
 ** the suite functions
 **/

typedef struct PF_ChannelSuite1 { /* frozen in AE 5.0 */

	SPAPI PF_Err	(*PF_GetLayerChannelCount)(
								PF_ProgPtr			effect_ref,			/* >> */
								PF_ParamIndex		param_index,		/* >> */ 
								A_long				*num_channelsPL);	/* << */

	SPAPI PF_Err	(*PF_GetLayerChannelIndexedRefAndDesc)(
								PF_ProgPtr			effect_ref,			/* >> */
								PF_ParamIndex		param_index,		/* >> */ 
								PF_ChannelIndex		channel_index,		/* >> */
								PF_Boolean			*foundPB,			/* << */
								PF_ChannelRef		*channel_refP,		/* << */
								PF_ChannelDesc		*channel_descP);	/* << */


	SPAPI PF_Err	(*PF_GetLayerChannelTypedRefAndDesc)(
								PF_ProgPtr			effect_ref,			/* >> */
								PF_ParamIndex		param_index,		/* >> */ 
								PF_ChannelType		channel_type,		/* >> */
								PF_Boolean			*foundPB,			/* << */
								PF_ChannelRef		*channel_refP,		/* << */
								PF_ChannelDesc		*channel_descP);	/* << */

	SPAPI PF_Err	(*PF_CheckoutLayerChannel)(
								PF_ProgPtr			effect_ref,			/* >> */
								PF_ChannelRefPtr	channel_refP,		/* >> */
								A_long				what_time,			/* >> */
								A_long				duration,			/* >> */
								A_u_long		time_scale,			/* >> */
								PF_DataType			data_type,			/* << */
								PF_ChannelChunk		*channel_chunkP);	/* << */


	SPAPI PF_Err	(*PF_CheckinLayerChannel)(		
								PF_ProgPtr			effect_ref,			/* >> */
								PF_ChannelRefPtr	channel_refP,		/* >> */
								PF_ChannelChunk		*channel_chunkP);	/* << */
						
	
} PF_ChannelSuite1;





#ifdef __cplusplus
	}		// end extern "C"
#endif


#include <adobesdk/config/PostConfig.h>


#endif
