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