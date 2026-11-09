#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Online/Config/OnlineConfigData.h"
#include "Online/IOnlineBackend.h"
#include "Online/OnlineBackendChoice.h"

namespace Opaax
{
    // =============================================================================
    // OnlineAPI — makes the store backends (implemented in OnlineBackendFactory.cpp, the only file that
    //   includes one). Like AudioAPI: a store more is one more case there, nothing else changes.
    // =============================================================================
    class OnlineAPI
    {
    public:
        /** The stores this build has: the fake one always, a real one when its SDK was found. */
        static TDynArray<EOnlineBackend> GetBuiltInBackends();

        /**
         * Starts InBackend.
         * @param OutReason Why it did not start ("Steam is not running"); untouched on success
         * @return Null when it did not start
         */
        static TUniquePtr<IOnlineBackend> CreateBackend(EOnlineBackend InBackend, const OnlineConfigData& InConfig,
                                                        EFakeStoreProfile InFakeProfile, OpaaxString& OutReason);

        /**
         * The first of InChoice's stores that starts, or the null backend.
         * @param OutReason Why that one runs, or why none does: each store tried and why it did not start
         */
        static TUniquePtr<IOnlineBackend> CreateFirstBackend(const OnlineBackendChoice& InChoice,
                                                             const OnlineConfigData& InConfig, OpaaxString& OutReason);
    };
}
