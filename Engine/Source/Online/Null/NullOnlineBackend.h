#pragma once

#include "Online/IOnlineBackend.h"

namespace Opaax
{
    // =============================================================================
    // NullOnlineBackend — no store. It has no feature: the services answer every request Unavailable.
    // =============================================================================
    class NullOnlineBackend final : public IOnlineBackend
    {
        //~Begin IOnlineBackend interface
    public:
        EOnlineBackend     GetType() const noexcept override { return EOnlineBackend::None; }
        OnlineCapabilities GetCapabilities() const override  { return {}; }
        //~End IOnlineBackend interface
    };
}
