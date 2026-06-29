#pragma once

#include "IAppService.h"
#include "IPaths.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include <spdlog/spdlog.h>

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
    
    struct LogCategory
    {
        constexpr explicit LogCategory(const char* InName)
            : Name(InName)
        {
        }

        const char* Name;
    };

    namespace 
    {
        spdlog::level::level_enum ToSpdLevel(ELogLevel InLevel)
        {
            switch (InLevel)
            {
            case ELogLevel::Trace:    return spdlog::level::trace;
            case ELogLevel::Info:     return spdlog::level::info;
            case ELogLevel::Warn:     return spdlog::level::warn;
            case ELogLevel::Error:    return spdlog::level::err;
            case ELogLevel::Critical: return spdlog::level::critical;
            }
            return spdlog::level::info;
        }
    }
    
    //DEFAULT OPAAX
    inline constexpr LogCategory LogOpaaxApplication{"OpaaxApplication"};
    inline constexpr LogCategory LogOpaaxEngine     {"OpaaxEngine"};

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
        
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        //----- extension point ------------------------------------------------
        virtual void Log(ELogLevel InLevel, const OpaaxString& InMessage) = 0;
        virtual void Log(ELogLevel InLevel,const char* Category, const OpaaxString& InMessage) = 0;

        //----- convenience (inline, non-virtual) ------------------------------
        void Trace(const OpaaxString& InMessage)    { Log(ELogLevel::Trace,    InMessage); }
        void Info(const OpaaxString& InMessage)     { Log(ELogLevel::Info,     InMessage); }
        void Warn(const OpaaxString& InMessage)     { Log(ELogLevel::Warn,     InMessage); }
        void Error(const OpaaxString& InMessage)    { Log(ELogLevel::Error,    InMessage); }
        void Critical(const OpaaxString& InMessage) { Log(ELogLevel::Critical, InMessage); }

        //----- null object ----------------------------------------------------
        static ILogger& Null();
        
        SharedPtr<spdlog::logger> AppLogger;
    };

    // =============================================================================
    // Logger — facade forwarding to OpaaxLog's engine (core) spdlog logger.
    // =============================================================================
    class OPAAX_API Logger final : public ILogger
    {
    public:
        explicit Logger(Opaax::IPaths& InPaths);
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void Log(ELogLevel InLevel, const OpaaxString& InMessage) override;
        void Log(ELogLevel InLevel,const char* Category, const OpaaxString& InMessage) override;
    };
}

#define OPAAX_LOG(Category,Level, Format,...) ::Opaax::OpaaxApplication::GetAppService<::Opaax::ILogger>().AppLogger->log(ToSpdLevel(::Opaax::ELogLevel::##Level), "[{}] " Format, Category.Name, __VA_ARGS__);

#define OPAAX_APP_LOG(Level, ...)       OPAAX_LOG(LogOpaaxApplication, Level,  ##__VA_ARGS__);
#define OPAAX_ENGINE_LOG(Level, ...)    OPAAX_LOG(LogOpaaxEngine, Level,  ##__VA_ARGS__);
