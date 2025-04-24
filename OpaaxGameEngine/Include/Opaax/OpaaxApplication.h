#pragma once

#include "Opaax/OpaaxCoreMacros.h"
#include "Window/OpaaxWindow.h"

namespace OPAAX {
	class IOpaaxRendererContext;
}

namespace OPAAX
{
	class OPAAX_API OpaaxApplication
	{
	private:
		TUniquePtr<OpaaxWindow> m_opaaxWindow = nullptr;
		TUniquePtr<IOpaaxRendererContext> m_opaaxRenderer = nullptr;

		bool bIsInitialize = false;
		bool bIsRunning = false;
	public:
		OpaaxApplication();
		virtual ~OpaaxApplication();

		void CreateInitMainWindow();
		void CreateInitRenderer();

		void Initialize();
		void Run();
		void Shutdown();
	};

	/*
	* To be implemented by the client
	*/
	OpaaxApplication* CreateApplication();
}
