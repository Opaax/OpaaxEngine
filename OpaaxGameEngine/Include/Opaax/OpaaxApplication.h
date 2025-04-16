#pragma once

#include "Opaax/OpaaxCoreMacros.h"

namespace OPAAX
{
	class OPAAX_API OpaaxApplication
	{
	public:
		OpaaxApplication();
		virtual ~OpaaxApplication();

		void Run();
	};

	/*
	* Tobe implemented by the client
	*/
	OpaaxApplication* CreateApplication();
}
