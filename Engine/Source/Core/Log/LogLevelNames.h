#pragma once

#include "Core/Log/Logger.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES

namespace Opaax
{
    /** A log level's name ("Warn"), as a config saves it. */
    inline const char* ToString(const ELogLevel InLevel) noexcept
    {
        switch (InLevel)
        {
            case ELogLevel::Trace:    return "Trace";
            case ELogLevel::Info:     return "Info";
            case ELogLevel::Warn:     return "Warn";
            case ELogLevel::Error:    return "Error";
            case ELogLevel::Critical: return "Critical";
        }
        return "Info";
    }
}

OPAAX_ENUM_VALUES(Opaax::ELogLevel, Trace, Info, Warn, Error, Critical)
