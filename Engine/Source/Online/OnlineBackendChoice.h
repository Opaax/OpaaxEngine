#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Online/Fake/FakeOnlineBackend.h"
#include "Online/OnlineTypes.h"

namespace Opaax
{
    /** What the choice of a store looks at, besides the config. */
    struct OnlineLaunch
    {
        /** --online's value; empty when not given. */
        OpaaxString CommandLine;

        /** The app does not start its game at once: the editor. */
        bool bEditor = false;

        /** The game was started by Steam (Steam set its app id in the environment). */
        bool bLaunchedBySteam = false;

        /** The stores this build has. */
        TDynArray<EOnlineBackend> BuiltIn;
    };

    /** The stores to try, in order, and why. */
    struct OnlineBackendChoice
    {
        TDynArray<EOnlineBackend> Candidates;

        /** The fake store's profile, when it is a candidate. */
        EFakeStoreProfile FakeProfile = EFakeStoreProfile::Full;

        /** Why these, or why none ("the editor never uses a store"). */
        OpaaxString Reason;
    };

    /**
     * The stores a game tries, first to last: --online wins (none, fake[:profile], steam); the editor
     * tries none; a game tries the configured ones that are built in, the store that launched it first.
     * A store not built in is left out and named in the reason.
     */
    OnlineBackendChoice ChooseOnlineBackends(const TDynArray<EOnlineBackend>& InConfigured, const OnlineLaunch& InLaunch);
}
