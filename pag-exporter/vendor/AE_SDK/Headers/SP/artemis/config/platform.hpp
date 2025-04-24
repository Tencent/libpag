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

#ifndef ARTEMIS_PLATFORM_HPP
#define ARTEMIS_PLATFORM_HPP

/*
    The ARTEMIS_PLATFORM() macro is used to conditionalize code based on platform or platform
    attibutes.

    It is used as:

    #if ARTEMIS_PLATFORM(MACOS)
    #endif

    The attributes currently create a hierarchy from general to specific, items lower in the
    hierarchy imply their parent are defined as '1', all others are '0'.

    This graph may eventually become a DAG. Higher level nodes represent a set of services that the
    lower level nodes have in common. As such, it should rarely be necessary to write an expression
    using more than one of these terms. If you find yourself doing so, please name the common
    service and add the flag.

    An unqualified else clause should only be written entirely in terms of standard constructs that
    has no platform dependencies and should not assume that 'not X' implies some other 'Y'.

    POSIX - any Posix compliant platform
        APPLE - any Apple platform
            MACOS - compiled for macOS
            IOS - compiled for iOS
        LINUX - compiled for Linux
        ANDROID - compiled for Android
    MICROSOFT - any Microsoft platform
        UWP - Compiled for UWP
        WIN32 - Compiled for Win32
*/
#define ARTEMIS_PLATFORM(X) (ARTEMIS_PRIVATE_PLATFORM_##X())

/*
    The ARTEMIS_ARCH() macro is used to conditionalize code based on processor architecture.

    It is used as:

    #if ARTEMIS_ARCH(ARM)
    #endif

    The currently supported platforms include ARM, X86, and WASM. The values are mutually
    exclusive.

    The value of this macro does not encapsulate whether compiling for a 32 or 64 bit system. See
    ARTEMIS_BITS instead.
*/
#define ARTEMIS_ARCH(X) (ARTEMIS_PRIVATE_ARCH_##X())


/*
    The ARTEMIS_BITS() macro is used to conditionalize code based on processor bit width.

    It is used as:

    #if ARTEMIS_BITS(32)
    #endif

    The currently supported values are 32 and 64. The values are mutually exclusive.
*/
#define ARTEMIS_BITS(X) (ARTEMIS_PRIVATE_BITS_##X())

/*
    The ARTEMIS_CPU() macro is used as a short hand to conditionalize code based on processor
    architecture and bit width.

    It is used as:

    #if ARTEMIS_CPU(X86, 64)
    #endif

    The above is equivalent to (ARTEMIS_ARCH(X86) && ARTEMIS_BITS(64))
*/
#define ARTEMIS_CPU(A,B) (ARTEMIS_PRIVATE_ARCH_##A() && ARTEMIS_PRIVATE_BITS_##B())

/*
    The ARTEMIS_PLATFORM_ARCH() macro is used as a short hand to conditionalize code based on
    target platform and processor architecture.

    It is used as:

    #if ARTEMIS_PLATFORM_ARCH(APPLE, X86)
    #endif

    The above is equivalent to (ARTEMIS_PLATFORM(APPLE) && ARTEMIS_ARCH(X86))
*/
#define ARTEMIS_PLATFORM_ARCH(X, A) (ARTEMIS_PRIVATE_PLATFORM_##X() && ARTEMIS_PRIVATE_ARCH_##A())

/*
    The ARTEMIS_PLATFORM_ARCH_BITS() macro is used as a short hand to conditionalize code based on
    target platform, processor architecture, and bit width.

    It is used as:

    #if ARTEMIS_PLATFORM_ARCH_BITS(APPLE, X86, 64)
    #endif

    The above is equivalent to (ARTEMIS_PLATFORM(APPLE) && ARTEMIS_ARCH(X86) && ARTEMIS_BITS(64))
*/
#define ARTEMIS_PLATFORM_ARCH_BITS(X, A, B) (ARTEMIS_PRIVATE_PLATFORM_##X() && ARTEMIS_PRIVATE_ARCH_##A() && ARTEMIS_PRIVATE_BITS_##B())

/**************************************************************************************************/

// The *_PRIVATE_* macros are just that. Don't use them directly.

#define ARTEMIS_PRIVATE_PLATFORM_ANDROID() 0
#define ARTEMIS_PRIVATE_PLATFORM_WEB() 0
#define ARTEMIS_PRIVATE_PLATFORM_APPLE() 0
#define ARTEMIS_PRIVATE_PLATFORM_IOS() 0
#define ARTEMIS_PRIVATE_PLATFORM_LINUX() 0
#define ARTEMIS_PRIVATE_PLATFORM_MACOS() 0
#define ARTEMIS_PRIVATE_PLATFORM_MICROSOFT() 0
#define ARTEMIS_PRIVATE_PLATFORM_POSIX() 0
#define ARTEMIS_PRIVATE_PLATFORM_UWP() 0
#define ARTEMIS_PRIVATE_PLATFORM_WIN32() 0

#define ARTEMIS_PRIVATE_ARCH_ARM() 0
#define ARTEMIS_PRIVATE_ARCH_X86() 0
#define ARTEMIS_PRIVATE_ARCH_WASM() 0

#define ARTEMIS_PRIVATE_BITS_32() 0
#define ARTEMIS_PRIVATE_BITS_64() 0

#if defined(__ANDROID__)
    #undef ARTEMIS_PRIVATE_PLATFORM_POSIX
    #define ARTEMIS_PRIVATE_PLATFORM_POSIX() 1
    #undef ARTEMIS_PRIVATE_PLATFORM_ANDROID
    #define ARTEMIS_PRIVATE_PLATFORM_ANDROID() 1

    #if defined(__arm__)
        #undef ARTEMIS_PRIVATE_ARCH_ARM
        #define ARTEMIS_PRIVATE_ARCH_ARM() 1
        #undef ARTEMIS_PRIVATE_BITS_32
        #define ARTEMIS_PRIVATE_BITS_32() 1
    #endif

    #if defined(__aarch64__)
        #undef ARTEMIS_PRIVATE_ARCH_ARM
        #define ARTEMIS_PRIVATE_ARCH_ARM() 1
        #undef ARTEMIS_PRIVATE_BITS_64
        #define ARTEMIS_PRIVATE_BITS_64() 1
    #endif

    #if defined(__i386__)
        #undef ARTEMIS_PRIVATE_ARCH_X86
        #define ARTEMIS_PRIVATE_ARCH_X86() 1
        #undef ARTEMIS_PRIVATE_BITS_32
        #define ARTEMIS_PRIVATE_BITS_32() 1
    #endif

    #if defined(__x86_64__)
        #undef ARTEMIS_PRIVATE_ARCH_X86
        #define ARTEMIS_PRIVATE_ARCH_X86() 1
        #undef ARTEMIS_PRIVATE_BITS_64
        #define ARTEMIS_PRIVATE_BITS_64() 1
    #endif

#elif defined(_WIN32)
    #include <sdkddkver.h> // for #define WINVER
    #include <winapifamily.h>

    #undef ARTEMIS_PRIVATE_PLATFORM_MICROSOFT
    #define ARTEMIS_PRIVATE_PLATFORM_MICROSOFT() 1

    #if defined(WINAPI_FAMILY) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        #undef ARTEMIS_PRIVATE_PLATFORM_UWP
        #define ARTEMIS_PRIVATE_PLATFORM_UWP() 1
    #else
        #undef ARTEMIS_PRIVATE_PLATFORM_WIN32
        #define ARTEMIS_PRIVATE_PLATFORM_WIN32() 1
    #endif

    #if defined(_M_ARM)
        #undef ARTEMIS_PRIVATE_ARCH_ARM
        #define ARTEMIS_PRIVATE_ARCH_ARM() 1
        #undef ARTEMIS_PRIVATE_BITS_32
        #define ARTEMIS_PRIVATE_BITS_32() 1
    #endif

    #if defined(_M_ARM64)
        #undef ARTEMIS_PRIVATE_ARCH_ARM
        #define ARTEMIS_PRIVATE_ARCH_ARM() 1
        #undef ARTEMIS_PRIVATE_BITS_64
        #define ARTEMIS_PRIVATE_BITS_64() 1
    #endif


    #if defined(_M_IX86)
        #undef ARTEMIS_PRIVATE_ARCH_X86
        #define ARTEMIS_PRIVATE_ARCH_X86() 1
        #undef ARTEMIS_PRIVATE_BITS_32
        #define ARTEMIS_PRIVATE_BITS_32() 1
    #endif

    #if defined(_M_X64)
        #undef ARTEMIS_PRIVATE_ARCH_X86
        #define ARTEMIS_PRIVATE_ARCH_X86() 1
        #undef ARTEMIS_PRIVATE_BITS_64
        #define ARTEMIS_PRIVATE_BITS_64() 1
    #endif
#elif defined(__APPLE__) && !defined(SIMULATED_WASM)
    #if __has_include("TargetConditionals.h")
        #include "TargetConditionals.h"
    #endif

    #undef ARTEMIS_PRIVATE_PLATFORM_POSIX
    #define ARTEMIS_PRIVATE_PLATFORM_POSIX() 1
    #undef ARTEMIS_PRIVATE_PLATFORM_APPLE
    #define ARTEMIS_PRIVATE_PLATFORM_APPLE() 1

    #if TARGET_OS_SIMULATOR || TARGET_OS_IPHONE
        #undef ARTEMIS_PRIVATE_PLATFORM_IOS
        #define ARTEMIS_PRIVATE_PLATFORM_IOS() 1
    #elif TARGET_OS_MAC
        #undef ARTEMIS_PRIVATE_PLATFORM_MACOS
        #define ARTEMIS_PRIVATE_PLATFORM_MACOS() 1
    #endif

    #if TARGET_CPU_ARM
        #undef ARTEMIS_PRIVATE_ARCH_ARM
        #define ARTEMIS_PRIVATE_ARCH_ARM() 1
        #undef ARTEMIS_PRIVATE_BITS_32
        #define ARTEMIS_PRIVATE_BITS_32() 1
    #endif

    #if TARGET_CPU_ARM64
        #undef ARTEMIS_PRIVATE_ARCH_ARM
        #define ARTEMIS_PRIVATE_ARCH_ARM() 1
        #undef ARTEMIS_PRIVATE_BITS_64
        #define ARTEMIS_PRIVATE_BITS_64() 1
    #endif

    #if TARGET_CPU_X86
        #undef ARTEMIS_PRIVATE_ARCH_X86
        #define ARTEMIS_PRIVATE_ARCH_X86() 1
        #undef ARTEMIS_PRIVATE_BITS_32
        #define ARTEMIS_PRIVATE_BITS_32() 1
    #endif

    #if TARGET_CPU_X86_64
        #undef ARTEMIS_PRIVATE_ARCH_X86
        #define ARTEMIS_PRIVATE_ARCH_X86() 1
        #undef ARTEMIS_PRIVATE_BITS_64
        #define ARTEMIS_PRIVATE_BITS_64() 1
    #endif
#elif defined(__LINUX__) || defined(__linux__)
    #undef ARTEMIS_PRIVATE_PLATFORM_POSIX
    #define ARTEMIS_PRIVATE_PLATFORM_POSIX() 1
    #undef ARTEMIS_PRIVATE_PLATFORM_LINUX
    #define ARTEMIS_PRIVATE_PLATFORM_LINUX() 1

    #if defined(__x86_64__)
        #undef ARTEMIS_PRIVATE_ARCH_X86
        #define ARTEMIS_PRIVATE_ARCH_X86() 1
        #undef ARTEMIS_PRIVATE_BITS_64
        #define ARTEMIS_PRIVATE_BITS_64() 1
    #endif

#elif defined(__EMSCRIPTEN__) || defined(SIMULATED_WASM)
    #undef ARTEMIS_PRIVATE_PLATFORM_POSIX
    #define ARTEMIS_PRIVATE_PLATFORM_POSIX() 1
    #undef ARTEMIS_PRIVATE_PLATFORM_WEB
    #define ARTEMIS_PRIVATE_PLATFORM_WEB() 1

    #undef ARTEMIS_PRIVATE_ARCH_WASM
    #define ARTEMIS_PRIVATE_ARCH_WASM() 1
    #undef ARTEMIS_PRIVATE_BITS_32
    #define ARTEMIS_PRIVATE_BITS_32() 1
#endif

#if (ARTEMIS_PRIVATE_ARCH_ARM() + ARTEMIS_PRIVATE_ARCH_X86() + ARTEMIS_PRIVATE_ARCH_WASM()) != 1
    #error "Unable to determine processor type"
#endif

#if (ARTEMIS_PRIVATE_BITS_32() + ARTEMIS_PRIVATE_BITS_64()) != 1
    #error "Unable to determine whether compiling for 32 or 64 bit"
#endif


#endif // ARTEMIS_PLATFORM_HPP
