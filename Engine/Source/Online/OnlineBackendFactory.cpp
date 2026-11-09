#include "Online/OnlineAPI.h"

#include "Online/Fake/FakeOnlineBackend.h"
#include "Online/Null/NullOnlineBackend.h"

namespace Opaax
{
    TDynArray<EOnlineBackend> OnlineAPI::GetBuiltInBackends()
    {
        TDynArray<EOnlineBackend> lBuiltIn = { EOnlineBackend::Fake };
#ifdef OPAAX_ONLINE_STEAM
        lBuiltIn.push_back(EOnlineBackend::Steam);
#endif
        return lBuiltIn;
    }

    TUniquePtr<IOnlineBackend> OnlineAPI::CreateBackend(const EOnlineBackend InBackend, const OnlineConfigData& InConfig,
                                                        const EFakeStoreProfile InFakeProfile, OpaaxString& OutReason)
    {
        (void)InConfig;

        switch (InBackend)
        {
            case EOnlineBackend::None:
                return MakeUnique<NullOnlineBackend>();

            case EOnlineBackend::Fake:
                return MakeUnique<FakeOnlineBackend>(InFakeProfile);

            case EOnlineBackend::Steam:
                OutReason = "Steam is not built in";
                return nullptr;
        }

        OutReason = OpaaxString::Format("unknown store {}", static_cast<Uint32>(InBackend));
        return nullptr;
    }

    TUniquePtr<IOnlineBackend> OnlineAPI::CreateFirstBackend(const OnlineBackendChoice& InChoice,
                                                             const OnlineConfigData& InConfig, OpaaxString& OutReason)
    {
        OpaaxString lFailures;
        for (const EOnlineBackend lBackend : InChoice.Candidates)
        {
            OpaaxString lWhyNot;
            if (TUniquePtr<IOnlineBackend> lStarted = CreateBackend(lBackend, InConfig, InChoice.FakeProfile, lWhyNot))
            {
                OutReason = lFailures.IsEmpty() ? InChoice.Reason
                                                : OpaaxString::Format("{}; {}", InChoice.Reason.CStr(), lFailures.CStr());
                return lStarted;
            }

            lFailures += lFailures.IsEmpty() ? "" : "; ";
            lFailures += lWhyNot;
        }

        OutReason = InChoice.Candidates.empty() ? InChoice.Reason : lFailures;
        return MakeUnique<NullOnlineBackend>();
    }
}
