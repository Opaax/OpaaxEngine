#pragma once
#include "Log/OPLogMacro.h"

#ifdef OPAAX_PLATFORM_WINDOWS
extern OPAAX::OpaaxApplication* OPAAX::CreateApplication();

int main(int argc, char** argv)
{
	auto lApp = OPAAX::CreateApplication();
	OPAAX_VERBOSE("Starting Opaax Application. [%1%]", %typeid(*lApp).name())
	lApp->Run();
	delete lApp;

	return 0;
}
#else
#error Opaax Game Engine only supports Windows!
#endif