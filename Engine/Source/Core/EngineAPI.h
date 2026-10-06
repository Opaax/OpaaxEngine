#pragma once

// =============================================================================
// Platform Detection
// =============================================================================
#if defined(_WIN32)
    #if defined(_WIN64)
        #define OPAAX_PLATFORM_WINDOWS
    #else
        #error "32-bit Windows is not supported."
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_OSX == 1
        #define OPAAX_PLATFORM_MACOS
    #else
        #error "Only macOS is supported among Apple platforms."
    #endif
#elif defined(__linux__)
    #define OPAAX_PLATFORM_LINUX
#else
    #error "Platform not supported."
#endif

#if defined(OPAAX_PLATFORM_LINUX) || defined(OPAAX_PLATFORM_MACOS)
    #define OPAAX_PLATFORM_POSIX
#endif

// =============================================================================
// Force Inline
// Always inline. Use on hot trivial accessors only.
// =============================================================================
#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCEINLINE __attribute__((always_inline)) inline
#else
    #define FORCEINLINE inline
#endif

// =============================================================================
// Editor
// =============================================================================
#ifndef OPAAX_WITH_EDITOR
    #define OPAAX_WITH_EDITOR 0
#endif

// =============================================================================
// Debug / Assertions
// =============================================================================
#ifdef OPAAX_DEBUG
    #define OPAAX_ENABLE_ASSERTS
#endif

#ifdef OPAAX_DEBUG
    #if defined(_MSC_VER)
        #define OPAAX_DEBUGBREAK() __debugbreak()
    #elif defined(__clang__)
        #define OPAAX_DEBUGBREAK() __builtin_debugtrap()
    #elif defined(OPAAX_PLATFORM_POSIX)
        #include <signal.h>
        #define OPAAX_DEBUGBREAK() raise(SIGTRAP)
    #else
        #define OPAAX_DEBUGBREAK()
    #endif
#else
    #define OPAAX_DEBUGBREAK()
#endif

#ifdef OPAAX_ENABLE_ASSERTS
    #define OPAAX_ASSERT(x)      { if(!(x)) { OPAAX_DEBUGBREAK(); } }
    #define OPAAX_CORE_ASSERT(x) { if(!(x)) { OPAAX_DEBUGBREAK(); } }
#else
    #define OPAAX_ASSERT(x)
    #define OPAAX_CORE_ASSERT(x)
#endif

// =============================================================================
// Bit Helpers
// =============================================================================
#define BIT(x) (1u << (x))
