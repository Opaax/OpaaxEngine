#include "AutoRegistration.h"

#include <algorithm>
#include <cstring>
#include <tuple>

namespace Opaax
{
    namespace
    {
        // Head of the program-wide list. Constant-initialized, so it is valid before any
        // registration object is constructed, whatever the static initialization order.
        constinit const AutoRegistration* g_AutoRegistrationHead = nullptr;
    }

    AutoRegistration::AutoRegistration(const EAutoRegistrationKind InKind, const TypeId InType, const Int32 InOrder,
                                       const char* InName, const FRegister InRegister) noexcept
        : Kind(InKind)
        , Type(InType)
        , Order(InOrder)
        , Name(InName)
        , Register(InRegister)
        , Next(g_AutoRegistrationHead)
    {
        // Static initialization runs on one thread, before main.
        g_AutoRegistrationHead = this;
    }

    TDynArray<const AutoRegistration*> CollectAutoRegistrations()
    {
        TDynArray<const AutoRegistration*> lRegistrations;
        for (const AutoRegistration* lNode = g_AutoRegistrationHead; lNode != nullptr; lNode = lNode->Next)
        {
            lRegistrations.push_back(lNode);
        }

        std::sort(lRegistrations.begin(), lRegistrations.end(),
                  [](const AutoRegistration* InA, const AutoRegistration* InB)
                  {
                      if (InA->Kind != InB->Kind)   { return InA->Kind < InB->Kind; }
                      if (InA->Order != InB->Order) { return InA->Order < InB->Order; }

                      const int lByName = std::strcmp(InA->Name, InB->Name);
                      if (lByName != 0)             { return lByName < 0; }

                      return InA->Type < InB->Type;
                  });

        // The same type registered from several translation units (a macro in a header) once.
        const auto lDuplicate = std::unique(lRegistrations.begin(), lRegistrations.end(),
                                            [](const AutoRegistration* InA, const AutoRegistration* InB)
                                            {
                                                return InA->Kind == InB->Kind && InA->Type == InB->Type;
                                            });
        lRegistrations.erase(lDuplicate, lRegistrations.end());

        return lRegistrations;
    }

    Uint64 RunAutoRegistrations(ModuleRegistrar& InRegistrar)
    {
        const TDynArray<const AutoRegistration*> lRegistrations = CollectAutoRegistrations();

        for (const AutoRegistration* lRegistration : lRegistrations)
        {
            lRegistration->Register(InRegistrar);
        }

        return static_cast<Uint64>(lRegistrations.size());
    }
}
