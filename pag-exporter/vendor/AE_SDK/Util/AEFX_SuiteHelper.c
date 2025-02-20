/**************************************************************************
*
* ADOBE CONFIDENTIAL
* ___________________
*
*  Copyright 2009 Adobe Systems Incorporated
*  All Rights Reserved.
*
* NOTICE:  All information contained herein is, and remains the property of
* Adobe Systems Incorporated  and its suppliers,  if any.  The intellectual 
* and technical concepts contained herein are proprietary to  Adobe Systems 
* Incorporated  and its suppliers  and may be  covered by U.S.  and Foreign 
* Patents,patents in process,and are protected by trade secret or copyright 
* law.  Dissemination of this  information or reproduction of this material
* is strictly  forbidden  unless prior written permission is  obtained from 
* Adobe Systems Incorporated.
**************************************************************************/


/** AEFX_SuiteHelper.c

	Contains helper routines for acquiring/releasing suites.
	
**/

#include "AEFX_SuiteHelper.h"
#include <adobesdk/DrawbotSuite.h>

PF_Err AEFX_AcquireSuite(	PF_InData		*in_data,			/* >> */
							PF_OutData		*out_data,			/* >> */
							const char		*name,				/* >> */
							int32_t			version,			/* >> */
							const char		*error_stringPC0,	/* >> */
							void			**suite)			/* << */
{
	PF_Err			err = PF_Err_NONE;
	SPBasicSuite	*bsuite;

	bsuite = in_data->pica_basicP;

	if (bsuite) {
		(*bsuite->AcquireSuite)((char*)name, version, (const void**)suite);

		if (!*suite) {
			err = PF_Err_BAD_CALLBACK_PARAM;
		}
	} else {
		err = PF_Err_BAD_CALLBACK_PARAM;
	}

	if (err) {
		const char	*error_stringPC	= error_stringPC0 ? error_stringPC0 : "Not able to acquire AEFX Suite.";
	
		out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;

		PF_SPRINTF(out_data->return_msg, error_stringPC);
	}

	return err;
}



PF_Err AEFX_ReleaseSuite(	PF_InData		*in_data,			/* >> */
							PF_OutData		*out_data,			/* >> */
							const char		*name,				/* >> */
							int32_t			version,			/* >> */
							const char		*error_stringPC0)	/* >> */
{
	PF_Err			err = PF_Err_NONE;
	SPBasicSuite	*bsuite;

	bsuite = in_data->pica_basicP;

	if (bsuite) {
		(*bsuite->ReleaseSuite)((char*)name, version);
	} else {
		err = PF_Err_BAD_CALLBACK_PARAM;
	}

	if (err) {
		const char	*error_stringPC	= error_stringPC0 ? error_stringPC0 : "Not able to release AEFX Suite.";
	
		out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;

		PF_SPRINTF(out_data->return_msg, error_stringPC);
	}

	return err;
}


PF_Err AEFX_AcquireDrawbotSuites(	PF_InData				*in_data,			/* >> */
									PF_OutData				*out_data,			/* >> */
									DRAWBOT_Suites			*suitesP)			/* << */
{
	PF_Err			err = PF_Err_NONE;
	
	if (suitesP == NULL) {
		out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;

		PF_SPRINTF(out_data->return_msg, "NULL suite pointer passed to AEFX_AcquireDrawbotSuites");

		err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
	}

	if (!err) {
		err = AEFX_AcquireSuite(in_data, out_data, kDRAWBOT_DrawSuite, kDRAWBOT_DrawSuite_VersionCurrent, NULL, (void **)&suitesP->drawbot_suiteP);
	}
	if (!err) {
		err = AEFX_AcquireSuite(in_data, out_data, kDRAWBOT_SupplierSuite, kDRAWBOT_SupplierSuite_VersionCurrent, NULL, (void **)&suitesP->supplier_suiteP);
	}
	if (!err) {
		err = AEFX_AcquireSuite(in_data, out_data, kDRAWBOT_SurfaceSuite, kDRAWBOT_SurfaceSuite_VersionCurrent, NULL, (void **)&suitesP->surface_suiteP);
	}
	if (!err) {
		err = AEFX_AcquireSuite(in_data, out_data, kDRAWBOT_PathSuite, kDRAWBOT_PathSuite_VersionCurrent, NULL, (void **)&suitesP->path_suiteP);
	}
	
	return err;
}


PF_Err AEFX_ReleaseDrawbotSuites(	PF_InData		*in_data,			/* >> */
									PF_OutData		*out_data)			/* >> */
{
	PF_Err			err = PF_Err_NONE;
	
	AEFX_ReleaseSuite(in_data, out_data, kDRAWBOT_DrawSuite, kDRAWBOT_DrawSuite_VersionCurrent, NULL);
	AEFX_ReleaseSuite(in_data, out_data, kDRAWBOT_SupplierSuite, kDRAWBOT_SupplierSuite_VersionCurrent, NULL);
	AEFX_ReleaseSuite(in_data, out_data, kDRAWBOT_SurfaceSuite, kDRAWBOT_SurfaceSuite_VersionCurrent, NULL);
	AEFX_ReleaseSuite(in_data, out_data, kDRAWBOT_PathSuite, kDRAWBOT_PathSuite_VersionCurrent, NULL);
	
	return err;
}

								


