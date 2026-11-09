#include "Online/OnlineBackendChoice.h"

#include "Core/Algo/Algo.h"

namespace Opaax
{
    namespace
    {
        bool IsBuiltIn(const OnlineLaunch& InLaunch, const EOnlineBackend InBackend)
        {
            return Algo::Contains(InLaunch.BuiltIn, InBackend);
        }

        OnlineBackendChoice FromCommandLine(const OnlineLaunch& InLaunch)
        {
            OnlineBackendChoice lChoice;
            const OpaaxString lValue = InLaunch.CommandLine.ToLower();

            if (lValue == "none")
            {
                lChoice.Reason = "--online none";
                return lChoice;
            }

            if (lValue == "fake" || lValue.StartsWith("fake:"))
            {
                const OpaaxString lProfile = (lValue == "fake") ? OpaaxString() : lValue.SubString(5);
                if (!lProfile.IsEmpty() && !ParseFakeStoreProfile(lProfile.View(), lChoice.FakeProfile))
                {
                    lChoice.Reason = OpaaxString::Format("--online {}: no such fake profile (full, steam, epic, gog)",
                                                         InLaunch.CommandLine.CStr());
                    return lChoice;
                }
                lChoice.Candidates.push_back(EOnlineBackend::Fake);
                lChoice.Reason = OpaaxString::Format("fake store, {} profile, from --online", ToString(lChoice.FakeProfile));
                return lChoice;
            }

            if (lValue == "steam")
            {
                if (!IsBuiltIn(InLaunch, EOnlineBackend::Steam))
                {
                    lChoice.Reason = "--online steam, but Steam is not built in";
                    return lChoice;
                }
                lChoice.Candidates.push_back(EOnlineBackend::Steam);
                lChoice.Reason = "from --online";
                return lChoice;
            }

            lChoice.Reason = OpaaxString::Format("--online {}: not none, fake[:profile] or steam", InLaunch.CommandLine.CStr());
            return lChoice;
        }
    }

    OnlineBackendChoice ChooseOnlineBackends(const TDynArray<EOnlineBackend>& InConfigured, const OnlineLaunch& InLaunch)
    {
        if (!InLaunch.CommandLine.IsEmpty())
        {
            return FromCommandLine(InLaunch);
        }

        OnlineBackendChoice lChoice;
        if (InLaunch.bEditor)
        {
            lChoice.Reason = "the editor never uses a store: --online fake gives it a fake one";
            return lChoice;
        }

        // The configured stores this build has, the one that launched the game first.
        TDynArray<EOnlineBackend> lMissing;
        for (const EOnlineBackend lBackend : InConfigured)
        {
            if (lBackend == EOnlineBackend::None || Algo::Contains(lChoice.Candidates, lBackend))
            {
                continue;
            }
            if (!IsBuiltIn(InLaunch, lBackend))
            {
                lMissing.push_back(lBackend);
                continue;
            }

            const bool bLauncher = InLaunch.bLaunchedBySteam && lBackend == EOnlineBackend::Steam;
            if (bLauncher)
            {
                lChoice.Candidates.insert(lChoice.Candidates.begin(), lBackend);
            }
            else
            {
                lChoice.Candidates.push_back(lBackend);
            }
        }

        if (!lChoice.Candidates.empty())
        {
            lChoice.Reason = (InLaunch.bLaunchedBySteam && lChoice.Candidates.front() == EOnlineBackend::Steam)
                                 ? "launched by Steam" : "from the online config";
            return lChoice;
        }

        if (lMissing.empty())
        {
            lChoice.Reason = "no store in the online config";
            return lChoice;
        }

        OpaaxString lNames;
        for (const EOnlineBackend lBackend : lMissing)
        {
            lNames += lNames.IsEmpty() ? "" : ", ";
            lNames += ToString(lBackend);
        }
        lChoice.Reason = OpaaxString::Format("{} not built in", lNames.CStr());
        return lChoice;
    }
}
