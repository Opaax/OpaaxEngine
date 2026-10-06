#pragma once

#include <cstdint>
#include "Core/EngineAPI.h"

namespace Opaax
{
    using ServiceTypeID = uintptr_t;

    // =============================================================================
    // IAppService — base for every application-level service.
    // =============================================================================
    class IAppService
    {
    public:
        virtual ~IAppService() = default;

        virtual void          OnShutdown()              {}            // reverse-order teardown
        virtual ServiceTypeID GetTypeID() const noexcept = 0;
        virtual bool          IsNull() const noexcept   { return false; }
    };
}

// Add to each service interface (IPlatform, IPaths, ...). Define StaticTypeID in the
// interface's .cpp so there is exactly one ID per interface.
#define OPAAX_SERVICE_TYPE(Interface)                                       \
static ::Opaax::ServiceTypeID StaticTypeID() noexcept;                  \
::Opaax::ServiceTypeID GetTypeID() const noexcept override              \
{ return StaticTypeID(); }