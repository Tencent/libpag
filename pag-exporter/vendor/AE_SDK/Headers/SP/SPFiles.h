/***********************************************************************/
/*                                                                     */
/* SPFiles.h                                                           */
/*                                                                     */
/* Copyright 1995-2006 Adobe Systems Incorporated.                     */
/* All Rights Reserved.                                                */
/*                                                                     */
/* Patents Pending                                                     */
/*                                                                     */
/* NOTICE: All information contained herein is the property of Adobe   */
/* Systems Incorporated. Many of the intellectual and technical        */
/* concepts contained herein are proprietary to Adobe, are protected   */
/* as trade secrets, and are made available only to Adobe licensees    */
/* for their internal use. Any reproduction or dissemination of this   */
/* software is strictly forbidden unless prior written permission is   */
/* obtained from Adobe.                                                */
/*                                                                     */
/***********************************************************************/

#ifndef __SPFiles__
#define __SPFiles__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include "SPProps.h"

#include "photoshop/config/platform.hpp"

#if PS_PLATFORM_APPLE
#include "CoreFoundation/CoreFoundation.h"
#if PS_OS_MAC
#include "Carbon/Carbon.h"
#endif
#endif

#if PS_OS_IOS
#include "photoshop/platform/EmulateEndianOnIOS.h"
#include "photoshop/platform/EmulateMacErrorsOnIOS.h"
#include "photoshop/platform/EmulateQuickdrawOnIOS.h"
#include "photoshop/platform/EmulateOSAPIsOnIOS.h"
#include "photoshop/platform/EmulateCarbonFilesOnIOS.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 **
 ** Constants
 **
 **/
/** Files suite name */
#define kSPFilesSuite				"SP Files Suite"
/** Files suite version */
#define kSPFilesSuitePrevVers		3
#define kSPFilesSuiteVersion		4

/** PICA global list of potential plug-in files. .
	@see \c #SPRuntimeSuite::GetRuntimeFileList().*/
#define kSPRuntimeFileList			((SPFileListRef)NULL)

/** Return value for \c #SPFilesSuite::GetFilePropertyList(),
	indicating that the file has no property list. */
#define kFileDoesNotHavePiPL		(SPPropertyListRef)((intptr_t)-1)
/** Return value for \c #SPFilesSuite::GetFilePropertyList(),
	indicating that the file has multiple property lists. <<is this right? how do you retrieve them?>> */
#define kFileHasMulitplePiPLs		NULL

/*******************************************************************************
 **
 ** Types
 **
 **/

/** The maximum number of characters allowed in a file path specification. */
#define kMaxPathLength 300

/** Opaque reference to a file. Access with the \c #SPFilesSuite. */
typedef struct SPFile *SPFileRef;
/** Opaque reference to a file list. Access with the \c #SPFilesSuite. */
typedef struct SPFileList *SPFileListRef;
/** Opaque reference to a file-list iterator. Access with the \c #SPFilesSuite. */
typedef struct SPFileListIterator *SPFileListIteratorRef;
/** Opaque reference to a platform-specific file specification. Access with the \c #SPFilesSuite. */
typedef struct OpaqueSPPlatformFileRef SPPlatformFileRef;

#define kXPlatFileSpecVersion	1

/** The replacement for SPPlatformFileSpecification and SPPlatformFileSpecificationW,
** used in SP Files Suite version 4 and later */
typedef struct XPlatFileSpec {
	int32		mFileSpecVersion;		///< \brief kXPlatFileSpecVersion
										///<
										///< A value of zero implies "not initialized",
										///< and you should not attempt to use mFileReference
#if defined(WIN_ENV) || defined(ANDROID_ENV) || WEB_ENV || defined(UNIX_ENV)
	uint16*		mFileReference;			///< \brief mFileReference could be as long as 64K but MUST be NULL terminated.
#elif defined(MAC_ENV)
	CFURLRef	mFileReference;			///< \brief A CFURLRef, as defined by Mac OS X
#endif
} XPlatFileSpec;


/*******************************************************************************/

#if defined (MAC_ENV)
#if PRAGMA_STRUCT_ALIGN		// DRSWAT
#pragma options align=mac68k
#endif	// PRAGMA_STRUCT_ALIGN


/** A file specification:
		\li In Mac OS 32 bit, the same as \c FSSpec.
		\li In Windows, a path string. */


#if USE_POSIX_API

typedef struct SPPlatformFileSpecification {	/* this handles unicode file names */
    /** The file path string. */
    char path[kMaxPathLength];
}  SPPlatformFileSpecification;

typedef struct SPPlatformFileSpecificationW {	/* this handles unicode file names */
    /** mReference could be as long as 64K but MUST be NULL terminated. */
    uint16 *mReference;
} SPPlatformFileSpecificationW;

/**Platform-specific file metadata. */
typedef struct SPPlatformFileInfo {
    /** File attribute flags; see Windows documentation. */
    uint32 attributes;
    /** Least-significant byte of the file creation date-time (Windows).*/
    uint32 lowCreationTime;
    /** Most-significant byte of the file creation date-time (Windows).*/
    uint32 highCreationTime;
    /** Least-significant byte of the file modification date-time (Windows).*/
    uint32 lowModificationTime;
    /** Most-significant byte of the file cremodification date-time (Windows).*/
    uint32 highModificationTime;
    /** The file-name extension indicating the file type (Windows). */
    const char *extension;
} SPPlatformFileInfo;

#elif PS_PLATFORM_APPLE

typedef struct SPPlatformFileSpecification {	/* this handles unicode file names */
	FSRef mReference;
}  SPPlatformFileSpecification;

typedef struct SPPlatformFileSpecificationW {	/* this handles unicode file names */
	FSRef mReference;
} SPPlatformFileSpecificationW;

#if PRAGMA_STRUCT_ALIGN	// DRSWAT
#pragma options align=reset
#endif

    /*******************************************************************************/

    /** Platform-specific file metadata. */
    typedef struct SPPlatformFileInfo {	 /* On Mac OS*/
        /** Not used. */
        uint32 attributes; 	//Unused, but still required to maintain binary compatibility
        /** Date file was created (Mac OS). */
        uint32 creationDate;
        /** Data file was last modified (Mac OS). */
        uint32 modificationDate;
        /** Type of file for Finder (Mac OS). */
        uint32 finderType;
        /** File creator (Mac OS). */
        uint32 finderCreator;
        /** File flags for Finder; see Mac OS documentation. */
        uint16 finderFlags;
    } SPPlatformFileInfo;
#endif // PS_PLATFORM_APPLE

#endif	// MAC_ENV

/*******************************************************************************/

#ifdef WIN_ENV
/** A file specification in Windows. */
typedef struct SPPlatformFileSpecification {
	/** The file path string. */
	char path[kMaxPathLength];
} SPPlatformFileSpecification;

/** A widechar file specification in Windows to handle unicode file names. */
typedef struct SPPlatformFileSpecificationW {
	/** mReference could be as long as 64K but MUST be NULL terminated. */
	uint16 *mReference;
} SPPlatformFileSpecificationW;

/*******************************************************************************/

/**Platform-specific file metadata. */
typedef struct SPPlatformFileInfo {
	/** File attribute flags; see Windows documentation. */
	uint32 attributes;
	/** Least-significant byte of the file creation date-time (Windows).*/
	uint32 lowCreationTime;
	/** Most-significant byte of the file creation date-time (Windows).*/
	uint32 highCreationTime;
	/** Least-significant byte of the file modification date-time (Windows).*/
	uint32 lowModificationTime;
	/** Most-significant byte of the file cremodification date-time (Windows).*/
	uint32 highModificationTime;
	/** The file-name extension indicating the file type (Windows). */
	const uint16 *extension;
} SPPlatformFileInfo;
#endif	// WIN_ENV

/*******************************************************************************/


#if defined(__ANDROID__) || defined(__LINUX__) || defined (__EMSCRIPTEN__) || defined(SIMULATED_WASM)
typedef struct SPPlatformFileSpecification {
	/** The file path string. */
	char path[kMaxPathLength];
} SPPlatformFileSpecification;

/** A widechar file specification in Windows to handle unicode file names. */
typedef struct SPPlatformFileSpecificationW {
	/** mReference could be as long as 64K but MUST be NULL terminated. */
	uint16 *mReference;
} SPPlatformFileSpecificationW;

/*******************************************************************************/

/**Platform-specific file metadata. */
typedef struct SPPlatformFileInfo {
	uint32 attributes;
	uint32 lowCreationTime;
	uint32 highCreationTime;
	uint32 lowModificationTime;
	uint32 highModificationTime;
	const uint16 *extension;
} SPPlatformFileInfo;
#endif	// __ANDROID__

/** Conversion from new platform file specification to old platform file
** specification, and vice-versa. It's okay to pass NULL pointers to these
** functions. If you do, the value returned will be a default constructed
** structure (i.e. zero-filled)
*/
/** Internal ? */
SPAPI SPPlatformFileSpecification OldFileSpecFromXplatFileSpec(const XPlatFileSpec* newSpec);

SPAPI SPPlatformFileSpecificationW OldFileSpecWFromXplatFileSpec(const XPlatFileSpec* newSpec);

/// Caller owns the CFURLRef contained in XPlatFileSpec::mFileReference
SPAPI XPlatFileSpec XplatFileSpecFromOldFileSpec(const SPPlatformFileSpecification* oldSpec);

/// Caller owns the CFURLRef contained in XPlatFileSpec::mFileReference
SPAPI XPlatFileSpec XplatFileSpecFromOldFileSpecW(const SPPlatformFileSpecificationW* oldSpecW);

/*******************************************************************************/

/** Internal */
typedef SPBoolean (*SPAddPiPLFilterProc)( SPPlatformFileInfo *info );

/*******************************************************************************
 **
 ** Suite
 **
 **/
/** @ingroup Suites
	This suite allows you to access the PICA files list. This list, created at startup,
    contains references to every file in the application's plug-in folder, including
    any resolved file and folder aliases. PICA maintains this list, and uses it to find plug-ins.

    Use this suite to access the plug-in file list, in order to avoid redundant directory
    scans. Adapters looking for their own plug-ins and PICA plug-ins looking for
    support files should scan the list to locate relevant files rather than walking
    platform directory  structures on their own.

	Similarly, you can use this suite to create, maintain, and access your own lists
	of files in a platform-independant and efficient manner.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPFilesSuite and \c #kSPFilesSuiteVersion (4).
    */

typedef struct SPFilesSuite {
	/** Creates a new file list. Typically, you use the main PICA file list to access
		plug-in files, available through \c #SPRuntimeSuite::GetRuntimeFileList().
		You can use this to track other file collections. If you create a new list, you
		must free it when it is no longer needed, using \c #FreeFileList().
			@param fileList [out] A buffer in which to return the new file list object.
		*/
	SPAPI SPErr (*AllocateFileList)( SPFileListRef *fileList );
	/** Frees a file list created with \c #AllocateFileList(), and any entries in the list.
			@param fileList The file list object.
		*/
	SPAPI SPErr (*FreeFileList)( SPFileListRef fileList );

	/** Adds a file or all files in a directory to a file list. Searches a directory
		recursively for contained files.
			@param fileList The file list object.
			@param file The file or directory specification.
		*/
	SPAPI SPErr (*AddFiles)( SPFileListRef fileList, const XPlatFileSpec *file );

	/** Creates a file-list iterator object to use with \c #NextFile() for iterating
		through a file list. The iterator is initially set to the first file in the list.
		When the iterator is no longer needed, free it with \c #DeleteFileListIterator().
			@param fileList The file list object.
			@param iter [out] A buffer in which to return the new iterator object.
		*/
	SPAPI SPErr (*NewFileListIterator)( SPFileListRef fileList, SPFileListIteratorRef *iter );
	/** Retrieves the current file from a file list iterator, and advances the iterator.
			@param iter The iterator object.
			@param file [out] A buffer in which to return the current file object, or \c NULL
				if the end of the list has been reached.
		*/
	SPAPI SPErr (*NextFile)( SPFileListIteratorRef iter, SPFileRef *file );
	/** Frees a file-list iterator created with /c #NewFileListIterator().
			@param iter The iterator object.
		*/
	SPAPI SPErr (*DeleteFileListIterator)( SPFileListIteratorRef iter );

	/** Retrieves the platform-specific file specification for a file.
			@param file The file object.
			@param fileSpec [out] A buffer in which to return the file specification.
		*/
	SPAPI SPErr (*GetFileSpecification)( SPFileRef file, XPlatFileSpec *fileSpec );
	/** Retrieves the  metadata for a file.
			@param file The file object.
			@param info [out] A buffer in which to return the file information.
		*/
	SPAPI SPErr (*GetFileInfo)( SPFileRef file, SPPlatformFileInfo *info );

	/** Reports whether a file in a file list is a plug-in.
			@param file The file object.
			@param isAPlugin [out] A buffer in which to return true if the file is a plug-in.
		*/
	SPAPI SPErr (*GetIsAPlugin)( SPFileRef file, SPBoolean *isAPlugin );
	/** Sets whether a file in a file list is a plug-in.
			@param file The file object.
			@param isAPlugin True to mark the file as a plug-in, false to mark it as not a plug-in.
		*/
	SPAPI SPErr (*SetIsAPlugin)( SPFileRef file, SPBoolean isAPlugin );

	/** Retrieves the property list for a file.
			@param file The file object.
			@param propertList [out] A buffer in which to return the property list,
				or \c #kFileDoesNotHavePiPL if the file does not have a property list,
				or \c #kFileHasMulitplePiPLs if the file has multiple property lists.
			@see \c SPPiPL.h
		*/
	SPAPI SPErr (*GetFilePropertyList)( SPFileRef file, SPPropertyListRef *propertList );
	/** Sets the property list for a file.
			@param file The file object.
			@param propertList The new property list.
		*/
	SPAPI SPErr (*SetFilePropertyList)( SPFileRef file, SPPropertyListRef propertList );

} SPFilesSuite;


/*******************************************************************************
 **
 ** Suite
 **
 **/
/** @ingroup Suites
	This suite allows you to access the PICA files list. This list, created at startup,
    contains references to every file in the application's plug-in folder, including
    any resolved file and folder aliases. PICA maintains this list, and uses it to find plug-ins.

    Use this suite to access the plug-in file list, in order to avoid redundant directory
    scans. Adapters looking for their own plug-ins and PICA plug-ins looking for
    support files should scan the list to locate relevant files rather than walking
    platform directory  structures on their own.

	Similarly, you can use this suite to create, maintain, and access your own lists
	of files in a platform-independant and efficient manner.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPFilesSuite and \c #kSPFilesSuitePrevVers (3).
    */
typedef struct SPFilesV3Suite {
	/** Creates a new file list. Typically, you use the main PICA file list to access
		plug-in files, available through \c #SPRuntimeSuite::GetRuntimeFileList().
		You can use this to track other file collections. If you create a new list, you
		must free it when it is no longer needed, using \c #FreeFileList().
			@param fileList [out] A buffer in which to return the new file list object.
		*/
	SPAPI SPErr (*AllocateFileList)( SPFileListRef *fileList );
	/** Frees a file list created with \c #AllocateFileList(), and any entries in the list.
			@param fileList The file list object.
		*/
	SPAPI SPErr (*FreeFileList)( SPFileListRef fileList );

	/** Adds a file or all files in a directory to a file list. Searches a directory
		recursively for contained files.
			@param fileList The file list object.
			@param file The file or directory specification.
		*/
	SPAPI SPErr (*AddFiles)( SPFileListRef fileList, const SPPlatformFileSpecification *file );

	/** Creates a file-list iterator object to use with \c #NextFile() for iterating
		through a file list. The iterator is initially set to the first file in the list.
		When the iterator is no longer needed, free it with \c #DeleteFileListIterator().
			@param fileList The file list object.
			@param iter [out] A buffer in which to return the new iterator object.
		*/
	SPAPI SPErr (*NewFileListIterator)( SPFileListRef fileList, SPFileListIteratorRef *iter );
	/** Retrieves the current file from a file list iterator, and advances the iterator.
			@param iter The iterator object.
			@param file [out] A buffer in which to return the current file object, or \c NULL
				if the end of the list has been reached.
		*/
	SPAPI SPErr (*NextFile)( SPFileListIteratorRef iter, SPFileRef *file );
	/** Frees a file-list iterator created with /c #NewFileListIterator().
			@param iter The iterator object.
		*/
	SPAPI SPErr (*DeleteFileListIterator)( SPFileListIteratorRef iter );

	/** Retrieves the platform-specific file specification for a file.
			@param file The file object.
			@param fileSpec [out] A buffer in which to return the file specification.
		*/
	SPAPI SPErr (*GetFileSpecification)( SPFileRef file, SPPlatformFileSpecification *fileSpec );
	/** Retrieves the  metadata for a file.
			@param file The file object.
			@param info [out] A buffer in which to return the file information.
		*/
	SPAPI SPErr (*GetFileInfo)( SPFileRef file, SPPlatformFileInfo *info );

	/** Reports whether a file in a file list is a plug-in.
			@param file The file object.
			@param isAPlugin [out] A buffer in which to return true if the file is a plug-in.
		*/
	SPAPI SPErr (*GetIsAPlugin)( SPFileRef file, SPBoolean *isAPlugin );
	/** Sets whether a file in a file list is a plug-in.
			@param file The file object.
			@param isAPlugin True to mark the file as a plug-in, false to mark it as not a plug-in.
		*/
	SPAPI SPErr (*SetIsAPlugin)( SPFileRef file, SPBoolean isAPlugin );

	/** Retrieves the property list for a file.
			@param file The file object.
			@param propertList [out] A buffer in which to return the property list,
				or \c #kFileDoesNotHavePiPL if the file does not have a property list,
				or \c #kFileHasMulitplePiPLs if the file has multiple property lists.
			@see \c SPPiPL.h
		*/
	SPAPI SPErr (*GetFilePropertyList)( SPFileRef file, SPPropertyListRef *propertList );
	/** Sets the property list for a file.
			@param file The file object.
			@param propertList The new property list.
		*/
	SPAPI SPErr (*SetFilePropertyList)( SPFileRef file, SPPropertyListRef propertList );

} SPFilesV3Suite;


/** Internal */
SPAPI SPErr SPAllocateFileList( SPFileListRef *fileList );
/** Internal */
SPAPI SPErr SPFreeFileList( SPFileListRef fileList );
/** Internal */
SPAPI SPErr SPAddFiles( SPFileListRef fileList, const SPPlatformFileSpecification *file );
/** Internal */
SPAPI SPErr SPNewAddFiles( SPFileListRef fileList, const XPlatFileSpec *file );

/** Internal */
SPAPI SPErr SPNewFileListIterator( SPFileListRef fileList, SPFileListIteratorRef *iter );
/** Internal */
SPAPI SPErr SPNextFile( SPFileListIteratorRef iter, SPFileRef *file );
/** Internal */
SPAPI SPErr SPDeleteFileListIterator( SPFileListIteratorRef iter );

/** Internal */
SPAPI SPErr SPGetFileSpecification( SPFileRef file, SPPlatformFileSpecification *fileSpec );
/** Internal */
SPAPI SPErr SPNewGetFileSpecification( SPFileRef file, XPlatFileSpec *fileSpec );
/** Internal */
SPAPI SPErr SPGetFileInfo( SPFileRef file, SPPlatformFileInfo *info );
/** Internal */
SPAPI SPErr SPGetIsAPlugin( SPFileRef file, SPBoolean *isAPlugin );
/** Internal */
SPAPI SPErr SPSetIsAPlugin( SPFileRef file, SPBoolean isAPlugin );

/** Internal */
SPAPI SPErr SPGetFilePropertyList( SPFileRef file, SPPropertyListRef *propertList );
/** Internal */
SPAPI SPErr SPSetFilePropertyList( SPFileRef file, SPPropertyListRef propertList );

/*******************************************************************************
 **
 **	Errors
 **
 **/

#include "SPErrorCodes.h"

#ifdef __cplusplus
}
#endif

#endif
