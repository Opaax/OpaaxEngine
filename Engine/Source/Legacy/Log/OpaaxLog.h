#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace Opaax 
{
	class OPAAX_API OpaaxLog
	{

	// =============================================================================
	// Statics
	// =============================================================================
	private:
		static SharedPtr<spdlog::logger> s_CoreLogger;    // Engine logger
		static SharedPtr<spdlog::logger> s_ClientLogger;  // Other logger
		
	// =============================================================================
	// FUNCTIONS
	// =============================================================================
	public:
		static void Init();
		static void Shutdown();

		//------------------------ GET - SET ------------------------//
		static SharedPtr<spdlog::logger>& GetCoreLogger();
		static SharedPtr<spdlog::logger>& GetClientLogger();
	};
}

// =============================================================================
// Log macro (EngineCore)
// =============================================================================
#define OPAAX_CORE_TRACE(...)    ::Opaax::OpaaxLog::GetCoreLogger()->trace(__VA_ARGS__)
#define OPAAX_CORE_INFO(...)     ::Opaax::OpaaxLog::GetCoreLogger()->info(__VA_ARGS__)
#define OPAAX_CORE_WARN(...)     ::Opaax::OpaaxLog::GetCoreLogger()->warn(__VA_ARGS__)
#define OPAAX_CORE_ERROR(...)    ::Opaax::OpaaxLog::GetCoreLogger()->error(__VA_ARGS__)
#define OPAAX_CORE_CRITICAL(...) ::Opaax::OpaaxLog::GetCoreLogger()->critical(__VA_ARGS__)

// =============================================================================
// Log macro (SandboxGame, Editor, etc.)
// =============================================================================
#define OPAAX_TRACE(...)    ::Opaax::OpaaxLog::GetClientLogger()->trace(__VA_ARGS__)
#define OPAAX_INFO(...)     ::Opaax::OpaaxLog::GetClientLogger()->info(__VA_ARGS__)
#define OPAAX_WARN(...)     ::Opaax::OpaaxLog::GetClientLogger()->warn(__VA_ARGS__)
#define OPAAX_ERROR(...)    ::Opaax::OpaaxLog::GetClientLogger()->error(__VA_ARGS__)
#define OPAAX_CRITICAL(...) ::Opaax::OpaaxLog::GetClientLogger()->critical(__VA_ARGS__)