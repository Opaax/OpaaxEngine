#pragma once

// ===== Platform Detection =====
#ifdef _WIN32
#ifdef _WIN64
#define OPAAX_PLATFORM_WINDOWS
#else
#error "x86 not supported!"
#endif
#elif defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC == 1
#define OPAAX_PLATFORM_MACOS
#else
#error "iOS not supported!"
#endif
#elif defined(__linux__)
#define OPAAX_PLATFORM_LINUX
#else
#error "Plateform not supported!"
#endif

#ifdef ENGINE_EXPORTS
    #define OPAAX_API __declspec(dllexport)
#else
    #define OPAAX_API __declspec(dllimport)
#endif

// Check si l'éditeur est compilé
#ifndef OPAAX_WITH_EDITOR
    #define OPAAX_WITH_EDITOR 0
#endif

#ifdef OPAAX_DEBUG
#define OPAAX_ENABLE_ASSERTS
#endif

// ===== Debug Break =====
#ifdef OPAAX_DEBUG
#ifdef OPAAX_PLATFORM_WINDOWS
#define OPAAX_DEBUGBREAK() __debugbreak()
#elif defined(OPAAX_PLATFORM_LINUX)
#include <signal.h>
#define OPAAX_DEBUGBREAK() raise(SIGTRAP)
#else
#define OPAAX_DEBUGBREAK()
#endif
#endif

// ===== Assertions =====
#ifdef OPAAX_ENABLE_ASSERTS
#define OPAAX_ASSERT(x) { if(!(x)) { OPAAX_DEBUGBREAK(); } }
#define OPAAX_CORE_ASSERT(x) { if(!(x)) { OPAAX_DEBUGBREAK(); } }
#else
#define OPAAX_ASSERT(x, ...)
#define OPAAX_CORE_ASSERT(x, ...)
#endif

// ===== Bit Manipulation =====
#define BIT(x) (1 << x)