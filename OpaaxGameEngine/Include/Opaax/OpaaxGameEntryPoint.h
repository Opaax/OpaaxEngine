#pragma once
#include "Log/OPLogMacro.h"

#ifdef OPAAX_PLATFORM_WINDOWS
extern OPAAX::OpaaxApplication* OPAAX::CreateApplication();

inline int main(int argc, char** argv)
{
	OPAAX_VERBOSE("======================= Entry point =======================")
	
	auto lApp = OPAAX::CreateApplication();
	OPAAX_LOG("[OpaaxGameEntryPoint], created Application: %1%", %typeid(*lApp).name())
	
	lApp->Initialize();
	lApp->Run();
	// lApp Shutting down it-self.
	
	delete lApp;
	return 0;
}
#else
#error Opaax Game Engine only supports Windows!
#endif