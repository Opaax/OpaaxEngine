#pragma once

#include "IAppService.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // ELogLevel — severity, mapped to the backend (spdlog) by the Logger facade.
    // =============================================================================
    enum class ELogLevel : Uint8
    {
        Trace,
        Info,
        Warn,
        Error,
        Critical
    };

    // =============================================================================
    // ILogger — locator-resolvable logging facade over the engine (core) logger.
    //
    // The OPAAX_CORE_* macros remain the always-on global path; this service is the
    // injectable face for code that resolves logging through the locator. A single
    // virtual Log(level, msg) is the extension point; the level helpers are inline.
    // =============================================================================
    class OPAAX_API ILogger : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(ILogger)

        //----- extension point ------------------------------------------------
        virtual void Log(ELogLevel InLevel, const OpaaxString& InMessage) = 0;

        //----- convenience (inline, non-virtual) ------------------------------
        void Trace(const OpaaxString& InMessage)    { Log(ELogLevel::Trace,    InMessage); }
        void Info(const OpaaxString& InMessage)     { Log(ELogLevel::Info,     InMessage); }
        void Warn(const OpaaxString& InMessage)     { Log(ELogLevel::Warn,     InMessage); }
        void Error(const OpaaxString& InMessage)    { Log(ELogLevel::Error,    InMessage); }
        void Critical(const OpaaxString& InMessage) { Log(ELogLevel::Critical, InMessage); }

        //----- null object ----------------------------------------------------
        static ILogger& Null();
    };

    // =============================================================================
    // Logger — facade forwarding to OpaaxLog's engine (core) spdlog logger.
    // =============================================================================
    class OPAAX_API Logger final : public ILogger
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void Log(ELogLevel InLevel, const OpaaxString& InMessage) override;
    };
}
