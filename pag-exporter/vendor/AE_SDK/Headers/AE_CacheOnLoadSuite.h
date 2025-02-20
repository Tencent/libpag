/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2003 Adobe Systems Incorporated                       */
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

#ifndef AECACHEONLOADSUITE
#define AECACHEONLOADSUITE

/**
**	CacheOnLoadSuite, used to signal whether or not the plugin needs to load from
**	disk every time on startup.
*/
#define kPFCacheOnLoadSuite			"PF Cache On Load Suite"
#define kPFCacheOnLoadSuiteVersion1	1

typedef struct PF_CacheOnLoadSuite1 {

	SPAPI PF_Err	(*PF_SetNoCacheOnLoad)(	PF_ProgPtr			effect_ref,
											A_long				effectAvailable);
												
} PF_CacheOnLoadSuite1;

#endif // AECACHEONLOADSUITE