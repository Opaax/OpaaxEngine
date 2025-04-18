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
	return new SandboxApplication();
}