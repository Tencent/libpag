#ifndef _H_FIEL_PUBLIC
#define _H_FIEL_PUBLIC

/*	FIEL_Public.h
	
	(c) 1993 CoSA
	
	The purpose of this header is to define a standard way to communicate interlace information
	within common image file formats. The FIEL_Label structure should be included as a
	user data of type 'FIEL' of a QuickTime movie, and as 'FIEL' resouce 128 in an
	image or animation file.
	
	The FIEL_Label structure may also be appended to the end of an ImageDescription if the creator
	is unable to add the resource or user data. Only the first FIEL_Label in the movie may be
	honored, however. If a sequence of frames is composed of multiple files, the FIEL_Label from only
	the first frame may be honored. In a QuickTime movie, the first user data item (index 1) will be honored.
	
	If the version is increased, to preserve backward compatibility we will only add types
	to the existing fields or add to the end of the FIEL_Label structure.

	***	Please note that most applications will only support interlaced full-height frames. The other
	***	formats are included so the spec is as general as possible. If you choose to store field-rendered
	***	video in one of the other formats it may not be de-interlaced properly by most applications.

	CoSA After Effects 1.0/1.1 outputs FIEL_Label version 0, with the obsolete tag 'Fiel' (not 'FIEL').
	The struct has a short version (set to 0) followed by a std::int32_t type that is 0 if field rendered, 1
	if upper field is first, and 2 if lower field is first. All field rendered frames output from
	AE 1.0/1.1 are interlaced.
	
	Future versions of CoSA After Effects will label all output with a version 1 or higher FIEL_Label.
	
*/

#include <stdint.h>

#define		FIEL_Label_VERSION		1

#define		FIEL_Tag				'FIEL'		// use as udata and resource type
#define		FIEL_ResID				128


enum {
	FIEL_Type_FRAME_RENDERED	= 0,		// FIEL_Order is irrelevant
	FIEL_Type_INTERLACED		= 1,
	FIEL_Type_HALF_HEIGHT		= 2,
	FIEL_Type_FIELD_DOUBLED		= 3,		// 60 full size field-doubled frames/sec
	FIEL_Type_UNSPECIFIED		= 4			// do not use!
};
typedef uint32_t	FIEL_Type;


/*
	If the frames are interlaced, the following structure tells which of the interlaced fields is
	temporally first. If the frames are not interlaced but the animation was field rendered 
	(i.e. half height or field doubled), the structure tells which field the first sample (if the
	label is attached to a multi-sample file like a QT movie) or the current sample (if the label is
	attached to a single sample like a PICT file) contains.
*/

enum {
	FIEL_Order_UPPER_FIRST		= 0,
	FIEL_Order_LOWER_FIRST		= 1
};
typedef uint32_t	FIEL_Order;

#pragma pack(push, CoSAalign, 2)
	typedef struct {
		uint32_t	signature;		// always FIEL_Tag
		int16_t			version;
		FIEL_Type		type;
		FIEL_Order		order;
		uint32_t	reserved;
	} FIEL_Label;
	#if	defined(A_INTERNAL) && defined(__cplusplus)
		#include "AE_StructSizeAssert.h"
		AE_STRUCT_SIZE_ASSERT(FIEL_Label, 18);
	#endif

#pragma pack(pop, CoSAalign)

#endif
