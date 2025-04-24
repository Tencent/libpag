/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Inc. and its suppliers, if                    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Inc. and its                    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

#include "AEGP_SuiteHandler.h"

void AEGP_SuiteHandler::MissingSuiteError() const
{
	//	Yes, we've read Scott Meyers, and know throwing
	//	a stack-based object can cause problems. Since
	//	the err is just a long, and since we aren't de-
	//	referencing it in any way, risk is mimimal.

	//	As always, we expect those of you who use
	//	exception-based code to do a little less rudi-
	//	mentary job of it than we are here. 
	
	//	Also, excuse the Madagascar-inspired monkey 
	//	joke; couldn't resist. 
	//								-bbb 10/10/05
	
	PF_Err poop = PF_Err_BAD_CALLBACK_PARAM;

	throw poop;
}

