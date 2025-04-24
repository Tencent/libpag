/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2017 Adobe Systems Incorporated                       */
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


#ifndef	_H_AE_PLUGIN_DATA
#define	_H_AE_PLUGIN_DATA

#ifdef A_INTERNAL
	#include "PF_Private.h"
#endif

#include "A.h"

#include <adobesdk/config/PreConfig.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct PF_PluginData * PF_PluginDataPtr;


////////////////////////////////////////////////////////////////
//
// Version 2 of the plugin entry point
//   - adds 'inSupportURLPtr' to plugin data callback
//
////////////////////////////////////////////////////////////////


typedef A_Err (*PF_PluginDataCB2)(
                                    // The same plugin data pointer that was sent in the entry point
                                    PF_PluginDataPtr   inPtr,
                                    const A_u_char*  inNamePtr,
                                    const A_u_char*  inMatchNamePtr,
                                    const A_u_char*  inCategoryPtr,
                                    const A_u_char*  inEntryPointNamePtr,
                                    // Type of plugin ( VFlt,eFKT ,8BFM)
                                    A_long        inkind,
                                    // If the kind is eFKT, this argument must be PF_AE_PLUG_IN_VERSION
                                    A_long		  inApiVersionMajor,
                                    // If the kind is eFKT, this argument must be PF_AE_PLUG_IN_SUBVERS
                                    A_long		  inApiVersionMinor,
                                    A_long        inReservedInfo,
									const A_u_char*	 inSupportURLPtr			// newly added
                                    ) ;


/*
 * This is the Entry point function signature that must be implemented by the plugin
 * Name of the function must be PluginDataEntryFunction2
*/
typedef A_Err (*PluginDataEntryFunction2Ptr)(
                                            // An opaque pointer, which must be sent as an argument in the callback
                                            PF_PluginDataPtr inPtr,
                                            // Callback function pointer
                                            PF_PluginDataCB2 inPluginDataCallBackPtr,
                                            // SPBasicSuite function pointer
                                            struct SPBasicSuite* inSPBasicSuitePtr,
                                            // Name of the host application which is invoking the plugin
                                            const char* inHostName,
                                            // Exact version of the  host application  e.x. 10.1.3
                                            const char* inHostVersion
                                            );



////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//////////////////// OLDER VERSIONS ///////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////
//
// Version 1 of the plugin entry point
//
////////////////////////////////////////////////////////////////

typedef A_Err (*PF_PluginDataCB)(
                                    // The same plugin data pointer that was sent in the entry point
                                    PF_PluginDataPtr   inPtr,
                                    const A_u_char*  inNamePtr,
                                    const A_u_char*  inMatchNamePtr,
                                    const A_u_char*  inCategoryPtr,
                                    const A_u_char*  inEntryPointNamePtr,
                                    // Type of plugin ( VFlt,eFKT ,8BFM)
                                    A_long        inkind,
                                    // If the kind is eFKT, this argument must be PF_AE_PLUG_IN_VERSION
                                    A_long		  inApiVersionMajor,
                                    // If the kind is eFKT, this argument must be PF_AE_PLUG_IN_SUBVERS
                                    A_long		  inApiVersionMinor,
                                    A_long        inReservedInfo
                                    ) ;

/*
 * This is the Entry point function signature that must be implemented by the plugin
 * Name of the function must be PluginDataEntryFunction
*/
typedef A_Err (*PluginDataEntryFunctionPtr)(
                                            // An opaque pointer, which must be sent as an argument in the callback
                                            PF_PluginDataPtr inPtr,
                                            // Callback function pointer
                                            PF_PluginDataCB inPluginDataCallBackPtr,
                                            // SPBasicSuite function pointer
                                            struct SPBasicSuite* inSPBasicSuitePtr,
                                            // Name of the host application which is invoking the plugin
                                            const char* inHostName,
                                            // Exact version of the  host application  e.x. 10.1.3
                                            const char* inHostVersion
                                            );



#ifdef __cplusplus
}		// end extern "C"
#endif



#include <adobesdk/config/PostConfig.h>


#endif /* _H_AE_PLUGIN_DATA */
