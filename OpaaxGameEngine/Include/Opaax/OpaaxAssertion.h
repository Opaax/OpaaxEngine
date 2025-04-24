#pragma once

#include "OpaaxTypes.h"

// Platform-specific debugger break
#if defined(_MSC_VER)
#define OP_DEBUG_BREAK() (__nop(), __debugbreak());
#elif defined(__GNUC__) || defined(__clang__)
    #include <signal.h>
    #define OP_DEBUG_BREAK() raise(SIGTRAP)
#else
    #define OP_DEBUG_BREAK() std::abort()
#endif

/**
 * Log error with OPAAX_ERROR
 * Log error with std::cerr too
 * Break;
 * Abort;
 * @param condition assertion check
 * @param message message to log
 */
#ifdef OPAAX_DEBUG_MODE
#define OPAAX_ASSERT(CONDITION, FORMAT, ...) \
if (!(CONDITION))\
{                                             \
OSTDString lMessage = (BSTFormat(##FORMAT) __VA_ARGS__).str();\
OPAAX_ERROR("Assertion failed: %1% File:[%2%], Line:[%3%]: %4%", %#CONDITION %__FILE__ %__LINE__ %lMessage) \
std::cerr << "Assertion failed: (" << #CONDITION << "), " \
<< "file: " << __FILE__ << ", line: " << __LINE__ \
<< "\nMessage: " << lMessage << std::endl; \
OP_DEBUG_BREAK() \
std::abort();\
}

/**
 * 
 * @param condition 
 * @param message 
 */
// #define OP_STATIC_ASSERT(CONDITION, MESSAGE) \
//     static_assert(CONDITION, MESSAGE);

#define OP_STATIC_ASSERT(CONDITION, ...) \
static_assert(CONDITION, \
"[" __FILE__ ":" OP_TO_STRING(__LINE__) "] " __VA_ARGS__);
#else
    #define OPAAX_ASSERT(condition, message) ((void)0) // Disabled in release builds
#endif