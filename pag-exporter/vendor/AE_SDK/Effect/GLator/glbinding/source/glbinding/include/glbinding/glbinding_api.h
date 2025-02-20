#pragma once

#ifdef _MSC_VER
#   define GLBINDING_API_EXPORT_DECLARATION __declspec(dllexport)
#   define GLBINDING_API_IMPORT_DECLARATION __declspec(dllimport)
#elif __GNUC__
#    define GLBINDING_API_EXPORT_DECLARATION __attribute__ ((visibility ("default")))
#    define GLBINDING_API_IMPORT_DECLARATION __attribute__ ((visibility ("default")))
#else
#   define GLBINDING_API_EXPORT_DECLARATION __attribute__ ((visibility ("default")))
#   define GLBINDING_API_IMPORT_DECLARATION __attribute__ ((visibility ("default")))
#endif

#ifndef GLBINDING_STATIC
#ifdef GLBINDING_EXPORTS
#   define GLBINDING_API GLBINDING_API_EXPORT_DECLARATION
#else
#   define GLBINDING_API GLBINDING_API_IMPORT_DECLARATION
#endif
#else
#   define GLBINDING_API
#endif

#ifdef N_DEBUG
#   define IF_DEBUG(statement)
#   define IF_NDEBUG(statement) statement
#else
#   define IF_DEBUG(statement) statement
#   define IF_NDEBUG(statement)
#endif // N_DEBUG

// http://stackoverflow.com/questions/18387640/how-to-deal-with-noexcept-in-visual-studio
#ifndef NOEXCEPT
#   ifdef _MSC_VER
#       define NOEXCEPT
#   else
#       define NOEXCEPT noexcept
#   endif
#endif

#ifdef _MSC_VER
#define THREAD_LOCAL  __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif
