/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2012 Adobe Systems Incorporated                       */
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


#ifndef PRGPUFILTERMODULE_H
#define PRGPUFILTERMODULE_H


#include "PrSDKGPUDeviceSuite.h"
#include "PrSDKGPUImageProcessingSuite.h"
#include "PrSDKGPUFilter.h"
#include "PrSDKMemoryManagerSuite.h"
#include "PrSDKPPixSuite.h"
#include "PrSDKPPix2Suite.h"
#include "PrSDKVideoSegmentSuite.h"
#include <sstream>


/**
**
*/
class PrGPUFilterBase
{
public:
	/**
	**
	*/
	virtual ~PrGPUFilterBase() {}
	
	/**
	**
	*/
	static prSuiteError Startup(
		piSuitesPtr piSuites,
		csSDK_int32 inIndex)
	{
		return suiteError_NoError;
	}
	static prSuiteError Shutdown(
		piSuitesPtr piSuites,
		csSDK_int32 inIndex)
	{
		return suiteError_NoError;
	}

	/**
	**
	*/
	static csSDK_int32 PluginCount()
	{
		return 1;
	}
	static PrSDKString MatchName(
		piSuitesPtr piSuites,
		csSDK_int32 inIndex)
	{
		return PrSDKString();
	}

	/**
	**
	*/
	virtual prSuiteError Initialize(
		PrGPUFilterInstance* ioInstanceData)
	{
		mSuites = ioInstanceData->piSuites;
		mDeviceIndex = ioInstanceData->inDeviceIndex;
		mTimelineID = ioInstanceData->inTimelineID;
		mNodeID = ioInstanceData->inNodeID;

		mSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKGPUDeviceSuite, kPrSDKGPUDeviceSuiteVersion, (const void**)&mGPUDeviceSuite);
		mSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKGPUImageProcessingSuite, kPrSDKGPUImageProcessingSuiteVersion, (const void**)&mGPUImageProcessingSuite);
		mSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKMemoryManagerSuite, kPrSDKMemoryManagerSuiteVersion, (const void**)&mMemoryManagerSuite);
		mSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion, (const void**)&mPPixSuite);
		mSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion, (const void**)&mPPix2Suite);
		mSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPFTransitionSuite, kPFTransitionSuiteVersion, (const void**)&mTransitionSuite);
		mSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKVideoSegmentSuite, kPrSDKVideoSegmentSuiteVersion, (const void**)&mVideoSegmentSuite);

		mGPUDeviceSuite->GetDeviceInfo(kPrSDKGPUDeviceSuiteVersion, mDeviceIndex, &mDeviceInfo);
		return suiteError_NoError;
	}
	
	/**
	**
	*/
	virtual prSuiteError GetFrameDependencies(
		const PrGPUFilterRenderParams* inRenderParams,
		csSDK_int32* ioQueryIndex,
		PrGPUFilterFrameDependency* outFrameRequirements)
	{
		return suiteError_NotImplemented;
	}

	/**
	**
	*/
	virtual prSuiteError Precompute(
		const PrGPUFilterRenderParams* inRenderParams,
		csSDK_int32 inIndex,
		PPixHand inFrame)
	{
		return suiteError_NotImplemented;
	}

	/**
	**
	*/
	virtual prSuiteError Render(
		const PrGPUFilterRenderParams* inRenderParams,
		const PPixHand* inFrames,
		csSDK_size_t inFrameCount,
		PPixHand* outFrame) = 0;

protected:
	template<typename T>
	prSuiteError GetProperty(
		const char* inKey,
		T& outValue)
	{
		PrMemoryPtr buffer;
		prSuiteError suiteError = mVideoSegmentSuite->GetNodeProperty(mNodeID, inKey, &buffer);
		if (PrSuiteErrorSucceeded(suiteError))
		{
			std::istringstream stream((const char*)buffer);
			stream >> outValue;
			mMemoryManagerSuite->PrDisposePtr(buffer);
		}
		return suiteError;
	}

	PrParam GetParam(
		csSDK_int32 inIndex,
		PrTime inTime)
	{
		inIndex -= 1; // GPU filters do not include the input frame

		PrParam param = {};
		mVideoSegmentSuite->GetParam(mNodeID, inIndex, inTime, &param);
		return param;
	}

	size_t RoundUp(
		size_t inValue,
		size_t inMultiple)
	{
		return inValue ? ((inValue + inMultiple - 1) / inMultiple) * inMultiple : 0;
	}

	int GetGPUBytesPerPixel(
		PrPixelFormat inPixelFormat)
	{
		return inPixelFormat == PrPixelFormat_GPU_BGRA_4444_32f ? 16 : 8;
	}

	PrSDKGPUDeviceSuite* mGPUDeviceSuite;
	PrSDKGPUImageProcessingSuite* mGPUImageProcessingSuite;
	PrSDKMemoryManagerSuite* mMemoryManagerSuite;
	PrSDKPPixSuite* mPPixSuite;
	PrSDKPPix2Suite* mPPix2Suite;
	PF_TransitionSuite* mTransitionSuite;
	PrSDKVideoSegmentSuite* mVideoSegmentSuite;

	piSuitesPtr mSuites;
	PrTimelineID mTimelineID;
	csSDK_int32 mNodeID;
	csSDK_uint32 mDeviceIndex;
	PrGPUDeviceInfo mDeviceInfo;
};

/**
**
*/
template<
	class GPUFilter>
struct PrGPUFilterModule
{
	/**
	**
	*/
	static prSuiteError Startup(
		piSuitesPtr piSuites,
		csSDK_int32* ioIndex,
		PrGPUFilterInfo* outFilterInfo)
	{
		csSDK_int32 index = *ioIndex;
		if (index + 1 > GPUFilter::PluginCount())
		{
			return suiteError_InvalidParms;
		}
		if (index + 1 < GPUFilter::PluginCount())
		{
			*ioIndex += 1;
		}

		outFilterInfo->outMatchName = GPUFilter::MatchName(piSuites, index);
		outFilterInfo->outInterfaceVersion = PrSDKGPUFilterInterfaceVersion;

		return GPUFilter::Startup(piSuites, *ioIndex);
	}
	
	/**
	**
	*/
	static prSuiteError Shutdown(
		piSuitesPtr piSuites,
		csSDK_int32* ioIndex)
	{
		return GPUFilter::Shutdown(piSuites, *ioIndex);
	}

	/**
	**
	*/
	static prSuiteError CreateInstance(
		PrGPUFilterInstance* ioInstanceData)
	{
		GPUFilter* gpuFilter = new GPUFilter();
		prSuiteError result = gpuFilter->Initialize(ioInstanceData);
		if (PrSuiteErrorSucceeded(result))
		{
			ioInstanceData->ioPrivatePluginData = gpuFilter;
		}
		else
		{
			delete gpuFilter;
		}
		return result;
	}

	/**
	**
	*/
	static prSuiteError DisposeInstance(
		PrGPUFilterInstance* ioInstanceData)
	{
		delete (GPUFilter*)ioInstanceData->ioPrivatePluginData;
		ioInstanceData->ioPrivatePluginData = 0;
		return suiteError_NoError;
	}

	/**
	**
	*/
	static prSuiteError GetFrameDependencies(
		PrGPUFilterInstance* inInstanceData,
		const PrGPUFilterRenderParams* inRenderParams,
		csSDK_int32* ioQueryIndex,
		PrGPUFilterFrameDependency* outFrameRequirements)
	{
		return ((GPUFilter*)inInstanceData->ioPrivatePluginData)->GetFrameDependencies(inRenderParams, ioQueryIndex, outFrameRequirements);
	}

	/**
	**
	*/
	static prSuiteError Precompute(
		PrGPUFilterInstance* inInstanceData,
		const PrGPUFilterRenderParams* inRenderParams,
		csSDK_int32 inIndex,
		PPixHand inFrame)
	{
		return ((GPUFilter*)inInstanceData->ioPrivatePluginData)->Precompute(inRenderParams, inIndex, inFrame);
	}

	/**
	**
	*/
	static prSuiteError Render(
		PrGPUFilterInstance* inInstanceData,
		const PrGPUFilterRenderParams* inRenderParams,
		const PPixHand* inFrames,
		csSDK_size_t inFrameCount,
		PPixHand* outFrame)
	{
		return ((GPUFilter*)inInstanceData->ioPrivatePluginData)->Render(inRenderParams, inFrames, inFrameCount, outFrame);
	}
};


/**
**
*/
#ifdef PRWIN_ENV
	#define GPUFILTER_PLUGIN_ENTRY extern "C" __declspec(dllexport)
#else
	#define GPUFILTER_PLUGIN_ENTRY extern "C" __attribute__((visibility("default")))
#endif

#define DECLARE_GPUFILTER_ENTRY(ClassName) \
	GPUFILTER_PLUGIN_ENTRY prSuiteError xGPUFilterEntry( \
	csSDK_uint32 inHostInterfaceVersion, \
	csSDK_int32* ioIndex, \
	prBool inStartup, \
	piSuitesPtr piSuites, \
	PrGPUFilter* outFilter, \
	PrGPUFilterInfo* outFilterInfo) \
	{ \
		if (inStartup) \
		{ \
			outFilter->CreateInstance = ClassName::CreateInstance; \
			outFilter->DisposeInstance = ClassName::DisposeInstance; \
			outFilter->GetFrameDependencies = ClassName::GetFrameDependencies; \
			outFilter->Precompute = ClassName::Precompute; \
			outFilter->Render = ClassName::Render; \
			return ClassName::Startup(piSuites, ioIndex, outFilterInfo); \
		} \
		else \
		{ \
			return ClassName::Shutdown(piSuites, ioIndex); \
		} \
	}

#endif