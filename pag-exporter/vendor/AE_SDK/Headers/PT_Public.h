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

#ifndef	_H_PT_Public
#define	_H_PT_Public

/***

	This header file defines public interface for tracker or pixel-
	matching plug-ins.
	
	Their function is to implement a pixel matching algorithm that a
	tracker in After Effects could use to track user specified features.
	
	Tracker in the application provides UI and sets the main parameters.
	Tracker plug-ins, however, can provide a simple dialog-based UI for
	parameters specific to the implemented algorithm(s).
	
	
***/

#include <A.h>

#pragma pack( push, 4 )

#ifdef	__cplusplus
	extern "C" {
#endif

#define	PT_TRACKER_API_VERSION_MAJOR	1
#define	PT_TRACKER_API_VERSION_MINOR	0

#define	PT_TRACKER_MATCH_NAME_LEN		31
#define	PT_TRACKER_NAME_LEN				31

typedef	A_long	PT_Index;


typedef	struct	PT_Tracker				*PT_TrackerPtr;
typedef	struct	PT_TrackerInstance		*PT_TrackerInstancePtr;
typedef	struct	PT_TrackingContext		*PT_TrackingContextPtr;

//	plug-in entry points
typedef	A_Err	(*PT_GlobalSetupFunc)(
					const	PT_TrackerPtr	trackerP,
					AEGP_MemHandle			*global_dataPH);			// <<
					
typedef	A_Err	(*PT_GlobalSetdownFunc)(
					const	PT_TrackerPtr	trackerP);

typedef	A_Err	(*PT_GlobalDoAboutFunc)(
					const	PT_TrackerPtr	trackerP);

typedef	A_Err	(*PT_InstanceSetupFunc)(
					const	PT_TrackerInstancePtr	tracker_instanceP,
					AEGP_MemHandle					flat_instance_dataH0,
					AEGP_MemHandle					*instance_dataPH);	// currently has to be flat (no handles inside a handle)
					
typedef	A_Err	(*PT_InstanceSetdownFunc)(
					const	PT_TrackerInstancePtr	tracker_instanceP);
					
typedef A_Err	(*PT_InstanceFlattenFunc)(
					const	PT_TrackerInstancePtr	tracker_instanceP,
					AEGP_MemHandle					*flat_instance_dataPH);
					
typedef A_Err	(*PT_InstanceDoOptionsFunc)(
					const	PT_TrackerInstancePtr	tracker_instanceP);
					
typedef	A_Err	(*PT_PrepareTrackFunc)(
					const	PT_TrackingContextPtr	contextP,
					AEGP_MemHandle					*tracker_dataPH);	// <<
					
typedef	A_Err	(*PT_TrackFunc)(
					const	PT_TrackingContextPtr	contextP);

typedef	A_Err	(*PT_FinishTrackFunc)(
					const	PT_TrackingContextPtr	contextP);
					

typedef struct {
	PT_GlobalSetupFunc				global_setup_func;
	PT_GlobalSetdownFunc			global_setdown_func;
	PT_GlobalDoAboutFunc			global_do_about_func;
	
	PT_InstanceSetupFunc			instance_setup_func;
	PT_InstanceSetdownFunc			instance_setdown_func;
	PT_InstanceFlattenFunc			instance_flatten_func;
	PT_InstanceDoOptionsFunc		instance_do_options_func;
	
	PT_PrepareTrackFunc				track_prepare_func;
	PT_TrackFunc					track_func;
	PT_FinishTrackFunc				track_finish_func;
} PT_TrackerEntryPoints;


#pragma pack( pop )

#ifdef	__cplusplus
	}
#endif

#endif	// _H_PT_Public