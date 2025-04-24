/********************************************************************
* ADOBE CONFIDENTIAL
* __________________
*
*  Copyright 2020 Adobe Inc.
*  All Rights Reserved.
*
* NOTICE:  All information contained herein is, and remains
* the property of Adobe and its suppliers, if any. The intellectual
* and technical concepts contained herein are proprietary to Adobe
* and its suppliers and are protected by all applicable intellectual
* property laws, including trade secret and copyright laws.
* Dissemination of this information or reproduction of this material
* is strictly forbidden unless prior written permission is obtained
* from Adobe.
********************************************************************/

#ifndef _H_AE_HashSuite
	#define _H_AE_HashSuite

#ifdef AEGP_INTERNAL
	#include <AE_GeneralPlug_Private.h>
#endif

#include <AE_ComputeCacheSuite.h>

#ifdef __cplusplus
	extern "C" {
#endif

#define kAEGPHashSuite				"AEGP Hash Suite"
#define kAEGPHashSuiteVersion1		1	/* frozen in AE 17.5.1 */

typedef struct {

	SPAPI A_Err (*AEGP_CreateHashFromPtr)(
							const A_u_longlong		buf_sizeLu,		/* >> size of the buffer */
							const void				*bufPV,			/* >> the buffer pointer */
							AEGP_GUID				*hashP);		/* << result */

	SPAPI A_Err	(*AEGP_HashMixInPtr)(
							const A_u_longlong		buf_sizeLu,		/* >> size of the buffer */
							const void				*bufPV,			/* >> the buffer pointer */
							AEGP_GUID				*hashP);		/* <> guid to be mixed in */

} AEGP_HashSuite1;

#ifdef __cplusplus
	}		// end extern "C"
#endif

#endif
