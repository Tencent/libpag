#ifndef AECONFIG_H
#define AECONFIG_H

/*
Usage guidelines for these defines. 4/12/2006

Always use the most specific kind for your problem. Try and use
the form

#ifdef AE_XXXXX (i.e. AE_OS_MAC)

#elif defined(AE_NOT_XXXX) (i.e. AE_OS_WIN)

#else
	#error "unknown"
#endif

when creating conditionally compiled blocks.

AE_OS_MAC | AE_OS_WIN

are used to select between the most popular version of the operating
system API's. As of this writing AE_OS_MAC implies 10.4.4u SDK, and
AE_OS_WIN implies the platform SDK shipped with VS8 and _WIN32. 


AE_PROC_PPC | AE_PROC_INTEL

are used for code that will only execute on one processor family or
the other. Essentially should only be used for asm, or asm based 
intrinsics. Note: we do not currently have a define for things
like SSE or SSE3 as we don't build the whole code base targetted
for different instances of processors. You'll need to do runtime checks for SSE3
(though our minimum requirement is Pentium4 so SSE2 can be assumed).


AE_BIG_ENDIAN | AE_LITTLE_ENDIAN

PowerPC on Mac was BIG endian (as is network byte order).
x86 is a little endian architecture.

Try and avoid writing new code that uses this. Use XML or dva::filesupport
for writing things to disk.

These defines should be seldom used, instead use the byte swapping macro's
already defined in U which will be a no-op on platforms that don't need it.

For the record, our file format is stored big endian.


*/
//Define our OS defines
#if defined(_WIN32)
	#define AE_OS_WIN
#elif defined(__GNUC__) && defined(__MACH__)
	#include "TargetConditionals.h"	// defines the nesting of TARGET_OS_*
	#if TARGET_OS_MAC
		#if TARGET_OS_IPHONE
			// Older SDKs ( before TV, WATCH ) do not have TARGET_OS_IOS defined.
			#if TARGET_OS_IOS || !defined(TARGET_OS_IOS)
				#define AE_OS_IOS
			#endif
		#else
			#define AE_OS_MAC
		#endif // TARGET_OS_IPHONE

	#if TARGET_OS_SIMULATOR
		#define AE_OS_APPLE_SIM
	#endif
	#endif // TARGET_OS_MAC
#elif Rez
	#define AE_OS_MAC
#elif defined(__ANDROID__)
	#define AE_OS_ANDROID
#else
	#error "unrecognized AE platform"
#endif

//Define our Processor defines
#if defined(__i386__) || defined(_M_IX86)
	#define AE_PROC_INTEL
#elif defined(_M_X64) || defined(__amd64__) || defined(__x86_64__)
	#define AE_PROC_INTELx64
#elif defined(__arm64__) || defined(__aarch64__)
    #define AE_PROC_ARM64
#else
	#error  "unrecognized AE processor"
#endif

#if defined(AE_OS_MAC) && defined(AE_PROC_ARM64)
// This is used to enable/disable features that are still being
// ported/tested to run on Mac arm64based M1 processors, aka Apple Silicon.
// It is intended for Adobe internal use, and will be going away once the mac
// arm64 work is completed.
#define AE_MAC_ARM64_PROTOTYPE_TODO
#endif

//Define our Byte order
#define AE_LITTLE_ENDIAN


#endif
