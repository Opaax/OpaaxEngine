#pragma once

#ifdef OPAAX_PLATFORM_WINDOWS
extern OPAAX::OpaaxApplication* OPAAX::CreateApplication();

int main(int argc, char** argv)
{
	auto lApp = OPAAX::CreateApplication();
	lApp->Run();
	delete lApp;

	return 0;
}
#else
#error Opaax Game Engine only supports Windows!
#endif