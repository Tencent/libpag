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

#ifndef _H_AE_ComputeCacheSuite
#define _H_AE_ComputeCacheSuite

#include <A.h>
#include <SPBasic.h>

#ifdef AEGP_INTERNAL
	#include <AE_GeneralPlug_Private.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif

///////////////////////////////////// MC Compute -- plugin registration of cached computations

// Globally unique identifier for the compute class, such as "adobe.ae.effect.test_effect.cache_v_1"
typedef const char *AEGP_CCComputeClassIdP;

typedef void *AEGP_CCComputeOptionsRefconP;	// opaque content provided by plugin for generating key and value. Input to the compute.
typedef void *AEGP_CCComputeValueRefconP;	// opaque compute result from the plugin


// GUID used as the cache key.
typedef struct AEGP_GUID {
	A_long bytes[4];
} AEGP_GUID;

typedef AEGP_GUID AEGP_CCComputeKey;
typedef AEGP_CCComputeKey *AEGP_CCComputeKeyP;
	
typedef void *AEGP_CCCheckoutReceiptP;

//
// The effect supplies implementations of these callbacks.
//
typedef struct AEGP_ComputeCacheCallbacks {
	// Cache key. Called when creating a cache entry and when doing a cache lookup. Should be fast to compute. All of the inputs
	// needed to uniquely address the cache entry must be hashed into the key. If a layer checkout is needed to calculate the cache
	// value, such as with a histogram, then the hash of that input must be included. See PF_ParamUtilsSuite::PF_GetCurrentState
	// to get the hash for a layer param. Note this is the hash of the inputs needed to generate the frame, not a hash the pixels
	// in the frame, thus a render is not triggered when making this call.
	A_Err (*generate_key)(
					AEGP_CCComputeOptionsRefconP	optionsP,
					AEGP_CCComputeKeyP				out_keyP);

	// The expensive call. Generate the cache data from the input parameters.
	A_Err (*compute)(
					AEGP_CCComputeOptionsRefconP	optionsP,
					AEGP_CCComputeValueRefconP		*out_valuePP);

	// The computed value may not be a flat data structure. This should return the total memory footprint. The size is an input
	// to the cache purging heuristic.
	size_t (*approx_size_value)(
					AEGP_CCComputeValueRefconP		valueP);

	// The computed value lives in the cache. This is called to free the value when the cached is to be purged. All resources
	// owned by the cache value must be freed here.
	void (*delete_compute_value)(
					AEGP_CCComputeValueRefconP		valueP);
} AEGP_ComputeCacheCallbacks;



#define kAEGPComputeCacheSuite				"AEGP Compute Cache"
#define kAEGPComputeCacheSuiteVersion1		1	/* frozen in AE 18.2 */

typedef struct AEGP_ComputeCacheSuite1 {

	// Register a cache type.
	SPAPI A_Err (*AEGP_ClassRegister)(
						AEGP_CCComputeClassIdP				compute_classP,
						const AEGP_ComputeCacheCallbacks	*callbacksP);

	// Unregister a cache type. Note that all cached values will be purged at this time since the delete_compute_value callback
	// is how cache entries are deleted, and delete_compute_value is not available after unregister.
	SPAPI A_Err (*AEGP_ClassUnregister)(
						AEGP_CCComputeClassIdP				compute_classP);


	// This is the main checkout call.
	//
	// When adding cache support one of the first questions to answer is if a single render call needs to checkout more than one
	// cache value. If more than one cache value is needed then the multi-checkout pattern, below, can be applied to concurrently
	// calculate the caches across multiple render calls and thus avoid serialization of the compute.
	//
	// SINGLE CACHE VALUE
	// If a render call only needs one cache value then set wait_for_other_threadB to true. The checkout call will return a receipt,
	// possibly calling the compute callback to populate the cache; or waiting on another thread that had already started the
	// needed computation.
	//
	// MULTI-CHECKOUT
	// If a render call needs multiple cache values then this pattern can be used to keep the render threads utilized and thus
	// avoid serializing the compute.
	//    Render()
	//    {
	//          bool first_err = AEGP_ComputeIfNeededAndCheckout(first_options, do_not_wait);
	//          bool second_err = AEGP_ComputeIfNeededAndCheckout(second_options, do_not_wait);
	//          // Add as many additional do_not_wait checkout calls here as needed.
	//
	//          if(first_err == A_Err_NOT_IN_CACHE_OR_COMPUTE_PENDING) {
	//              AEGP_ComputeIfNeededAndCheckout(wait);
	//          }
	//          if(second_err == A_Err_NOT_IN_CACHE_OR_COMPUTE_PENDING) {
	//              AEGP_ComputeIfNeededAndCheckout(wait);
	//          }
	//          // Add as many additional waiting checkout calls here as needed
	//    }
	//
	//                                    wait_for_other_threadB
	//      CACHE STATE   ||          FALSE                    TRUE          |
	//    ================++========================+========================+
	//        No cache    ||  Compute and checkout  |  Compute and checkout  |
	//    ----------------++------------------------+------------------------+
	//   Being computed   ||  Return error, see     |  Wait for the other    |
	//  by another thread ||  below.                |  thread and checkout   |
	//    ----------------++------------------------+------------------------+
	//        Cached      ||  Checkout              |  Checkout              |
	//    ----------------++------------------------+------------------------+
	//
	// Returns A_Err_NOT_IN_CACHE_OR_COMPUTE_PENDING if wait_for_other_threadB is false and another thread is currently computing
	// the cache value. Note that the host will not notify the user of this error; it will be silent to the user.
	//
	// Must call AEGP_CheckinComputeReceipt on success. Check-in must be done before returning to the host.
	SPAPI A_Err (*AEGP_ComputeIfNeededAndCheckout)(
						AEGP_CCComputeClassIdP				compute_classP,
						AEGP_CCComputeOptionsRefconP		opaque_optionsP,
						bool								wait_for_other_threadB,
						AEGP_CCCheckoutReceiptP				*compute_receiptPP);

	// This call does a cache check, and thus should return always quickly. It does not do compute nor does it wait for another
	// thread that is populating the cache.
	// This call could be used to implement a polling pattern where another piece of code is expected to populate the cache. For
	// example, a UI thread could poll the cache regularly for a histogram that is generated on a render thread.
	//
	// Either returns cache value or A_Err_NOT_IN_CACHE_OR_COMPUTE_PENDING if cache miss.
	// Must call AEGP_CheckinComputeReceipt on success. Check-in must be done before returning to the host.
	SPAPI A_Err (*AEGP_CheckoutCached)(
						AEGP_CCComputeClassIdP				compute_classP,
						AEGP_CCComputeOptionsRefconP		opaque_optionsP,
						AEGP_CCCheckoutReceiptP				*compute_receiptPP);

	// Get the cache value from a checkout receipt.
	SPAPI A_Err (*AEGP_GetReceiptComputeValue)(
						const AEGP_CCCheckoutReceiptP		compute_receiptP,
						AEGP_CCComputeValueRefconP			*compute_valuePP);

	// Check-in a receipt.
	SPAPI A_Err (*AEGP_CheckinComputeReceipt)(
						AEGP_CCCheckoutReceiptP				compute_receiptP );

} AEGP_ComputeCacheSuite1;

#ifdef __cplusplus
	}		// end extern "C"
#endif

#endif
