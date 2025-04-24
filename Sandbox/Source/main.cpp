#include <iostream>
#include "OpaaxInclude.h"

class SandboxApplication : public OPAAX::OpaaxApplication
{
public:
	SandboxApplication()
	{
	}
	
	~SandboxApplication() override {
	}
};

OPAAX::OpaaxApplication* OPAAX::CreateApplication()
{
	OPAAX_VERBOSE("======================= Creating Application =======================")
	return new SandboxApplication();
}