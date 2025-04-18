#pragma once

#include "Opaax/OpaaxCoreMacros.h"
#include "Window/OpaaxWindow.h"

namespace OPAAX
{
	class OPAAX_API OpaaxApplication
	{
	private:
		TUniquePtr<OpaaxWindow> m_opaaxWindow;

		bool bIsRunning = false;
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
