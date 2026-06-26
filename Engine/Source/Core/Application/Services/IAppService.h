#pragma once

#include <cstdint>
#include "Core/EngineAPI.h"

namespace Opaax
{
    using ServiceTypeID = uintptr_t;

    // =============================================================================
    // IAppService — base for every application-level service.
    // =============================================================================
    class OPAAX_API IAppService
    {
    public:
        virtual ~IAppService() = default;

        virtual void          OnShutdown()              {}            // reverse-order teardown
        virtual ServiceTypeID GetTypeID() const noexcept = 0;
        virtual bool          IsNull() const noexcept   { return false; }
    };
}

// Stamp on each *interface* (IPlatform, ILogger, ...). NOTE: StaticTypeID is
// declared here but DEFINED out-of-line in the interface's .cpp — that gives one
// tag shared across the DLL/exe boundary (see your dll-static-template-hazard note).
#define OPAAX_SERVICE_TYPE(Interface)                                       \
static ::Opaax::ServiceTypeID StaticTypeID() noexcept;                  \
::Opaax::ServiceTypeID GetTypeID() const noexcept override              \
{ return StaticTypeID(); }