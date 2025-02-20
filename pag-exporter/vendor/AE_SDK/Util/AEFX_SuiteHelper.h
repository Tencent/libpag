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

#ifndef AEFX_SUITE_HELPER_H
#define AEFX_SUITE_HELPER_H


/** AEFX_SuiteHelper.h

	Contains helper routines for acquiring/releasing suites.
	
	NOTE: 	If you're writing C++ plug-ins that support exceptions, use the AEGP_SuiteHandler class or AEFX_SuiteScoper.

**/

#include <AEFX_SuiteHandlerTemplate.h>
#include <AE_EffectCB.h>
#include <AE_EffectSuites.h>
#include <adobesdk/DrawbotSuite.h>



#ifdef __cplusplus
	extern "C" {
#endif

PF_Err AEFX_AcquireSuite(	PF_InData		*in_data,			/* >> */
							PF_OutData		*out_data,			/* >> */
							const char		*name,				/* >> */
							int32_t			version,			/* >> */
							const char		*error_stringPC0,	/* >> */
							void			**suite);			/* << */


PF_Err AEFX_ReleaseSuite(	PF_InData		*in_data,			/* >> */
							PF_OutData		*out_data,			/* >> */
							const char		*name,				/* >> */
							int32_t			version,			/* >> */
							const char		*error_stringPC0);	/* >> */


PF_Err AEFX_AcquireDrawbotSuites(	PF_InData			*in_data,			/* >> */
									PF_OutData			*out_data,			/* >> */
									DRAWBOT_Suites		*suiteP);			/* << */


PF_Err AEFX_ReleaseDrawbotSuites(	PF_InData			*in_data,			/* >> */
									PF_OutData			*out_data);			/* >> */


#ifdef __cplusplus
	}
#endif


#ifdef __cplusplus

	template<typename SuiteType>
	class AEFX_SuiteHelperT
	{
	public:
		AEFX_SuiteHelperT(	PF_InData		*in_data,			/* >> */
							PF_OutData		*out_data,			/* >> */
							const char		*name,				/* >> */
							int32_t			version) :			/* >> */
			mInDataP(in_data), mOutDataP(out_data), mSuiteName(name), mSuiteVersion(version), mSuiteP(NULL)
		{
			void	*suiteP(NULL);
			
			PF_Err err = AEFX_AcquireSuite(mInDataP, mOutDataP, mSuiteName, mSuiteVersion, NULL, &suiteP);
			
			if (err) {
				A_THROW(err);
			}
			
			mSuiteP = reinterpret_cast<SuiteType*>(suiteP);
		}

		~AEFX_SuiteHelperT()
		{
			(void)AEFX_ReleaseSuite(mInDataP, mOutDataP, mSuiteName, mSuiteVersion, NULL);
		}

		const SuiteType* operator->() const
		{
			return mSuiteP;
		}

		SuiteType* get() const
		{
			return mSuiteP;
		}

	private:

		PF_InData		*mInDataP;
		PF_OutData		*mOutDataP;
		const char		*mSuiteName;
		int32_t			mSuiteVersion;
		SuiteType		*mSuiteP;
	};



// clients of this class probably should just be using the regular template
// instead

class AEFX_DrawbotSuitesScoper
{
public:
	AEFX_DrawbotSuitesScoper(PF_InData *in_data, PF_OutData *out_data)
			:
			i_in_data(in_data),
			i_out_data(out_data)
	{
		PF_Err err = AEFX_AcquireDrawbotSuites(in_data, out_data, &i_suites);
		
		if (err) {
			A_THROW(err);
		}
	}
	
	inline	DRAWBOT_Suites*	Get()
	{
		return &i_suites;
	}
	
	~AEFX_DrawbotSuitesScoper()
	{
		AEFX_ReleaseDrawbotSuites(i_in_data, i_out_data);
	}
	
private:
	DRAWBOT_Suites		i_suites;
	PF_InData			*i_in_data;
	PF_OutData			*i_out_data;
};

#endif


#endif // AEFX_SUITE_HELPER_H
