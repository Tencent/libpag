//
//  ADOBE CONFIDENTIAL
//  __________________
//
//  Copyright 2016 Adobe
//  All Rights Reserved.
//
//  NOTICE:  All information contained herein is, and remains
//  the property of Adobe and its suppliers, if any. The intellectual
//  and technical concepts contained herein are proprietary to Adobe
//  and its suppliers and are protected by all applicable intellectual
//  property laws, including trade secret and copyright laws.
//  Dissemination of this information or reproduction of this material
//  is strictly forbidden unless prior written permission is obtained
//  from Adobe.
//

#ifndef PHOTOSHOP_PLATFORM_HPP
#define PHOTOSHOP_PLATFORM_HPP

/**************************************************************************************************/

#ifdef RC_INVOKED // Windows Resource Compiler

  // #error Windows resource compiler - do not include this file!

#else

/**************************************************************************************************/

#include <artemis/config/platform.hpp>

/**************************************************************************************************/

// alias to the private artemis macros via concatenation because calling PHOTOSHOP_PLATFORM(X) errors
// when PHOTOSHOP_PLATFORM(X) = (ARTEMIS_PLATFORM(X)) when the value of X is itself defined as a
// macro. This is especially problematic for WIN32.

#define PHOTOSHOP_PLATFORM(X) (ARTEMIS_PRIVATE_PLATFORM_##X())
#define PHOTOSHOP_ARCH(X) (ARTEMIS_PRIVATE_ARCH_##X())
#define PHOTOSHOP_BITS(X) (ARTEMIS_PRIVATE_BITS_##X())
#define PHOTOSHOP_CPU(A,B) (ARTEMIS_PRIVATE_ARCH_##A() && ARTEMIS_PRIVATE_BITS_##B())
#define PHOTOSHOP_PLATFORM_ARCH(X, A) (ARTEMIS_PRIVATE_PLATFORM_##X() && ARTEMIS_PRIVATE_ARCH_##A())
#define PHOTOSHOP_PLATFORM_ARCH_BITS(X, A, B) (ARTEMIS_PRIVATE_PLATFORM_##X() && ARTEMIS_PRIVATE_ARCH_##A() && ARTEMIS_PRIVATE_BITS_##B())

// Really, these need to die a horrible death. However, they are still required
// for rez_defines.h (See the comment in rez_platform.h for more details.)
#define PS_PLATFORM_ANDROID PHOTOSHOP_PLATFORM(ANDROID)
#define PS_PLATFORM_WEB PHOTOSHOP_PLATFORM(WEB)
#define PS_PLATFORM_APPLE PHOTOSHOP_PLATFORM(APPLE)
#define PS_PLATFORM_IOS PHOTOSHOP_PLATFORM(IOS)
#define PS_PLATFORM_LINUX PHOTOSHOP_PLATFORM(LINUX)
#define PS_PLATFORM_MACOS PHOTOSHOP_PLATFORM(MACOS)
#define PS_PLATFORM_MS PHOTOSHOP_PLATFORM(MICROSOFT)
#define PS_PLATFORM_POSIX PHOTOSHOP_PLATFORM(POSIX)
#define PS_PLATFORM_UWP PHOTOSHOP_PLATFORM(UWP)
#define PS_PLATFORM_WIN32 PHOTOSHOP_PLATFORM(WIN32)

// Is there a reason why the above are replicated here?
#ifndef PS_OS_WIN // Some Windows projects define this in their props file
  #define PS_OS_WIN PHOTOSHOP_PLATFORM(MICROSOFT)
#endif
#ifndef PS_OS_IOS // Some Windows projects define this in their props file
  #define PS_OS_IOS PHOTOSHOP_PLATFORM(IOS)
#endif
#ifndef PS_OS_MAC // Some Windows projects define this in their props file
  #define PS_OS_MAC PHOTOSHOP_PLATFORM(MACOS)
#endif
#ifndef PS_OS_ANDROID // Some Windows projects define this in their props file
  #define PS_OS_ANDROID PHOTOSHOP_PLATFORM(ANDROID)
#endif

#define PS_OS_WEB POISONED_MACRO_PS_OS_WEB()

/**************************************************************************************************/

// Deprecated

#define qPSIsWin (PHOTOSHOP_PLATFORM(MICROSOFT))
#define qPSIsMac (PHOTOSHOP_PLATFORM(APPLE))

// REVISIT (sparent) : This also come from Switches.h... sometimes. Untangle

//#ifndef MSWindows
//  #define MSWindows (PHOTOSHOP_PLATFORM(MICROSOFT))
//#endif

// Macintosh is only defined for apple platforms
#if PHOTOSHOP_PLATFORM(APPLE) && !defined(Macintosh)
  #define Macintosh 1
#endif

/**************************************************************************************************/
#endif // RC_INVOKED
#endif // PHOTOSHOP_PLATFORM_HPP
