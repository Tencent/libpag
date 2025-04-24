/***********************************************************************/
/*                                                                     */
/* SPProps.h                                                           */
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

#ifndef __SPProperties__
#define __SPProperties__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include "SPMData.h"
#include "SPPiPL.h"

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 ** Constants
 **
 **/

/** PICA properties suite name */
#define kSPPropertiesSuite				"SP Properties Suite"
/** PICA properties suite version */
#define kSPPropertiesSuiteVersion2		2
/** PICA properties suite version */
#define kSPPropertiesSuiteVersion		kSPPropertiesSuiteVersion2	// minimal is default
/** PICA properties suite version */
#define kSPPropertiesSuiteVersion3		3

/** @ingroup Callers
	PICA plug-in property operation; sent with \c #SPPropertiesMessage.
	See \c #SPPropertiesSuite. */
#define kSPPropertiesCaller				"SP Properties"
/** @ingroup Selectors
	Acquire PICA plug-in properties; sent with \c #SPPropertiesMessage.
	See \c #SPPropertiesSuite.*/
#define kSPPropertiesAcquireSelector	"Acquire"
/** @ingroup Selectors
	Release PICA plug-in properties; sent with \c #SPPropertiesMessage.
	See \c #SPPropertiesSuite.*/
#define kSPPropertiesReleaseSelector	"Release"


/*******************************************************************************
 **
 ** Types
 **
 **/
/** An opaque reference to a plug-in property. Access with the \c #SPPropertiesSuite. */
typedef struct SPProperty *SPPropertyRef;
/** An opaque reference to a plug-in property list. Create and access with the \c #SPPropertiesSuite. */
typedef struct SPPropertyList *SPPropertyListRef;
/** An opaque reference to an iterator for a plug-in property list. Create and access with the \c #SPPropertiesSuite. */
typedef struct SPPropertyListIterator *SPPropertyListIteratorRef;

/** Message passed with the \c #kSPPropertiesCaller. */
typedef struct SPPropertiesMessage {
	/** The message data. */
	SPMessageData d;

	/** Unique identifier for the vendor defining this property type. This allows
		you to define your own properties in a way that
		does not conflict with either Adobe or other vendors.
		Use a registered application creator code to ensure uniqueness.
		All PICA properties use \c #PIAdobeVendorID.*/
	PIType vendorID;
	/** The property type key code, typically identifies a resource type. */
	PIType propertyKey;
	/** The unique property identifier, for multiple resources of a given
		type. Normally, there is only one, and the ID value is 0. */
	int32 propertyID;

	/** A structure containing the property data, or value. */
	void *property;
	/** Reference count. Increment when a property is acquired, decrement
		when it is released. */
	int32 refCon;
	/** True (non-zero) if this property does not change betweeen sessions
		and can be cached by the application in the start-up preferences
		file, false (0) otherwise. Typically true. */
	int32 cacheable;

} SPPropertiesMessage;


/*******************************************************************************
 **
 ** Suite
 **
 **/
/** @ingroup Suites
	Use these functions to create, access, and manage plug-in property lists
	associated with a specific plug-in. Plug-in properties provide the
	application with resource information for the plug-in, such as the types
	and locations of code files, and the plug-in version.

	A plug-in can be associated with multiple properties lists.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPPropertiesSuite and \c #kSPPropertiesSuiteVersion.
	*/
typedef struct SPPropertiesSuite {

	/** Creates a new plug-in property list.
			@param stringPool The string pool in which to keep plug-in names.
			@param propertyList [out] A buffer in which to return the new list object.
		*/
	SPAPI SPErr (*AllocatePropertyList)( SPPropertyListRef *propertyList );
	/** Frees a list of plug-in properties allocated with \c #AllocatePropertyList(), and
		also frees any entries in the list. If the list is one of a chain, frees the
		entire chain.
			@param propertyList The plug-in properties list object.
			@see \c #SPHasMultiplePropertyLists()
		*/
	SPAPI SPErr (*FreePropertyList)( SPPropertyListRef propertyList );

	/** Adds a set of properties to a plug-in properties list. This set is typically
		read from a resource file.Creates an \c #SPPropertyRef for each property,
		but does not return these objects.
			@param propertyList The plug-in properties list object.
			@param pList A pointer to the low-level structure for the set of properties to add.
			@param refCon The initial reference count for properties in the set.
			@param cacheable True (non-zero) if these properties do not change betweeen sessions
				and can be cached by the application in the start-up preferences
				file, false (0) otherwise. Typically true.
			@see \c #AddProperty()
		*/
	SPAPI SPErr (*AddProperties)( SPPropertyListRef propertyList, PIPropertyList *pList, int32 refCon, int32 cacheable );

	/** Creates a new individual property and adds it to a plug-in properties list.
		Typically called to install a property returned from an \c #kSPPropertiesAcquireSelector message.
			@param propertyList The plug-in properties list object.
			@param vendorID	The vendor identifier for the new property.
			@param propertyKey The type key code for the new property.
			@param propertyID The unique identifier for the individual property (normally 0).
			@param p A pointer to the property value structure.
		<<this contains the vID/key/pID, so why have those args?>>
			@param refCon The initial reference count for property.
			@param cacheable True (non-zero) if this property does not change betweeen sessions
				and can be cached by the application in the start-up preferences
				file, false (0) otherwise. Typically true.
			@param property [out] A buffer in which to return the new property object.
			@see \c #AllocatePropertyList(), \c #AddProperties()
		*/
	SPAPI SPErr (*AddProperty)( SPPropertyListRef propertyList, PIType vendorID, PIType propertyKey, int32 propertyID, PIProperty *p,
				int32 refCon, int32 cacheable, SPPropertyRef *property );

	/**	Retrieves a property from a plug-in properties list, or from any list in its chain.
			@param propertyList The plug-in properties list object.
			@param vendorID	The vendor identifier for the new property.
			@param propertyKey The type key code for the new property.
			@param propertyID The unique identifier for the individual property (normally 0).
			@param property [out] A buffer in which to return the property object, or \c NULL if a
				matching property is not found.
			@param \c #FindPropertyLocal()
		*/
	SPAPI SPErr (*FindProperty)( SPPropertyListRef propertyList, PIType vendorID, PIType propertyKey, int32 propertyID, SPPropertyRef *property );

	/** Creates an iterator object with which to traverse a plug-in properties list.
		The iterator is initially set to the first property in the list.
			@param propertyList The plug-in properties list object.
			@param iter [out] A buffer in which to return the new iterator object.
			@see \c #NextProperty(), \c #DeletePropertyListIterator()
		*/
	SPAPI SPErr (*NewPropertyListIterator)( SPPropertyListRef propertyList, SPPropertyListIteratorRef *iter );
	/** Retrieves the current property and advances a plug-in properties list iterator
		to the next property in the list.
			@param iter The plug-in properties list iterator object.
			@param property [out] A buffer in which to return the current property object, \c NULL
				if the end of the list has been reached.
		    @see \c #NewPropertyListIterator(),
		*/
	SPAPI SPErr (*NextProperty)( SPPropertyListIteratorRef iter, SPPropertyRef *property );
	/** Frees a plug-in properties list iterator that is no longer needed.
			@param iter The plug-in properties list iterator object.
			@see \c #NewPropertyListIterator(),
		*/
	SPAPI SPErr (*DeletePropertyListIterator)( SPPropertyListIteratorRef iter );

	/**	Retrieves the low-level property structure of a property object.
			@param property The property object.
			@param p [out] A buffer in which to return a pointer to the property structure.
		*/
	SPAPI SPErr (*GetPropertyPIProperty)( SPPropertyRef property, PIProperty **p );
	/**	Retrieves the current reference count of a property.
			@param property The property object.
			@param refCon [out] A buffer in which to return the reference count.
		*/
	SPAPI SPErr (*GetPropertyRefCon)( SPPropertyRef property, int32 *refCon );
	/**	Reports whether a property is cacheable.
			@param property The property object.
			@param cacheable [out] A buffer in which to return true (non-zero)
				if this property does not change betweeen sessions
				and can be cached by the application in the start-up preferences
				file, false (0) otherwise.
		*/
	SPAPI SPErr (*GetPropertyCacheable)( SPPropertyRef property, int32 *cacheable );
	/**	Reports whether a property was allocated by the plug-in that contains it.
			@param property The property object.
			@param allocatedByPlugin [out] A buffer in which to return true (non-zero)
				if this property was created after being acquired from a
				\c #kSPPropertiesAcquireSelector message, false (0) if the
				property was read from a resource file.
			@see \c #AddProperty(), \c #AddProperties()
		*/
	SPAPI SPErr (*GetPropertyAllocatedByPlugin)( SPPropertyRef property, int32 *allocatedByPlugin );

	// kSPPropertiesSuiteVersion3
	/**	Reports whether a plug-in properties list is one of a chain of properties lists for its plug-in.
		(Note that this function returns a boolean value, not an error code.)
			@param propertyList The plug-in properties list object.
			@return True (non-zero) if the list is one of a chain, false (0) otherwise.
			@see \c #GetNextPropertyList()
		*/
	SPAPI SPBoolean (*SPHasMultiplePropertyLists)(SPPropertyListRef propertyList);
	/**	Retrieves the next plug-in properties list in a properties-list chain.
			@param propertyList The current plug-in properties list object.
			@param nextPropertyList [out] A buffer in which to return the next properties list object,
				or \c NULL if the end of the chain has been reached.
		*/
	SPAPI SPErr (*GetNextPropertyList)(SPPropertyListRef propertyList, SPPropertyListRef *nextPropertyList);
	/**	Retrieves a property from a plug-in properties list, but does not search in other
		lists in the chain.
			@param propertyList The plug-in properties list object.
			@param vendorID	The vendor identifier for the new property.
			@param propertyKey The type key code for the new property.
			@param propertyID The unique identifier for the individual property (normally 0).
			@param property [out] A buffer in which to return the property object, or \c NULL if a
				matching property is not found.
			@see \c #FindProperty()
		*/
	SPAPI SPErr (*FindPropertyLocal)( SPPropertyListRef propertyList, PIType vendorID, PIType propertyKey,
				int32 propertyID, SPPropertyRef *property );

} SPPropertiesSuite;


/** Internal */
SPAPI SPErr SPAllocatePropertyList( SPPropertyListRef *propertyList );
/** Internal */
SPAPI SPErr SPFreePropertyList( SPPropertyListRef propertyList );

/** Internal */
SPAPI SPErr SPAddProperties( SPPropertyListRef propertyList, PIPropertyList *pList, int32 refCon,
			int32 cacheable );

/** Internal */
SPAPI SPErr SPAddProperty( SPPropertyListRef propertyList, PIType vendorID, PIType propertyKey,
			int32 propertyID, PIProperty *p, int32 refCon, int32 cacheable, SPPropertyRef *property );

/** Internal */
SPAPI SPErr SPFindProperty( SPPropertyListRef propertyList, PIType vendorID, PIType propertyKey,
			int32 propertyID, SPPropertyRef *property );

/** Internal */
SPAPI SPErr SPNewPropertyListIterator( SPPropertyListRef propertyList, SPPropertyListIteratorRef *iter );
/** Internal */
SPAPI SPErr SPNextProperty( SPPropertyListIteratorRef iter, SPPropertyRef *property );
/** Internal */
SPAPI SPErr SPDeletePropertyListIterator( SPPropertyListIteratorRef iter );

/** Internal */
SPAPI SPErr SPGetPropertyPIProperty( SPPropertyRef property, PIProperty **p );
/** Internal */
SPAPI SPErr SPGetPropertyRefCon( SPPropertyRef property, int32 *refCon );
/** Internal */
SPAPI SPErr SPGetPropertyCacheable( SPPropertyRef property, int32 *cacheable );
/** Internal */
SPAPI SPErr SPGetPropertyAllocatedByPlugin( SPPropertyRef property, int32 *allocatedByPlugin );

/** Internal */
SPAPI SPBoolean SPHasMultiplePropertyLists(SPPropertyListRef propertyList);
/** Internal */
SPAPI SPErr SPGetNextPropertyList(SPPropertyListRef propertyList, SPPropertyListRef *nextPropertyList);

/** Internal */
SPAPI SPErr SPFindPropertyLocal( SPPropertyListRef propertyList, PIType vendorID,
			PIType propertyKey, int32 propertyID, SPPropertyRef *property );

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
