#include "OpaaxEngine.h"
#include "Core/OPLogger.h"
#include "Core/OPLogMacro.h"

Engine& Engine::Get()
{
	static Engine instance;
	return instance;
}

void Engine::Run()
{
	OPAAX_LOG("Opaax Engine is running!")

	int loop = 10;
	while (loop-- > 0)
	{
		// Main loop placeholder
	}
}

void Engine::Shutdown()
{
	OPAAX_LOG("Opaax Engine is shutting down!")
	// Cleanup code placeholder
}
