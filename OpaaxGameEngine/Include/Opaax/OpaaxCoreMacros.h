#pragma once

#ifdef OPAAX_PLATFORM_WINDOWS
	#ifdef OPAAX_BUILD_DLL
		#define OPAAX_API __declspec(dllexport)
	#else
		#define OPAAX_API __declspec(dllimport)
	#endif
#else
	#error Opaax Game Engine only supports Windows!
#endif

#if defined(_MSC_VER)
  #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
  #define FORCEINLINE inline __attribute__((always_inline))
#else
  #define FORCEINLINE inline
#endif

#define OP_STRINGIFY(x) #x
/**
 * With compiler like clang or gcc this can be very useful to display the type of e.g. OP_TO_STRING(T) --> float if T is float
 * @param x 
 */
#define OP_TO_STRING(x) OP_STRINGIFY(x)

/**
 * Only here to remind we can '+'before the int8 value to explicitly display the digital value instead of the char.
 * @param Int Int8 to specify
 */
#define OP_INT8_AS_NUM(Int) (+(Int))