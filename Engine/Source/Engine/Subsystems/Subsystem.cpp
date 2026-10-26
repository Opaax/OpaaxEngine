#include "Engine/Subsystems/Subsystem.h"

#include "Core/Algo/Algo.h"
#include "Core/String/CString.h"
#include "Object/ObjectClassRegistry.h"

namespace Opaax
{
    TDynArray<const ObjectClass*> CollectSubsystemClasses(const ObjectClassRegistry& InClasses,
                                                          const ObjectClass& InBase)
    {
        struct Candidate
        {
            const ObjectClass* Class = nullptr;
            Int32              Order = 0;
        };

        TDynArray<Candidate> lCandidates;

        for (const ObjectClass* lClass : InClasses.GetClassesOf(InBase, /*bInConcreteOnly*/true))
        {
            const Subsystem* lDefault = Cast<Subsystem>(lClass->GetDefaultObject());

            if (lDefault != nullptr)
            {
                lCandidates.push_back({ lClass, lDefault->GetTickOrder() });
            }
        }

        Algo::Sort(lCandidates, [](const Candidate& InA, const Candidate& InB)
        {
            if (InA.Order != InB.Order)
            {
                return InA.Order < InB.Order;
            }
            return CString::Compare(InA.Class->GetName().CStr(), InB.Class->GetName().CStr()) < 0;
        });

        TDynArray<const ObjectClass*> lClasses;
        lClasses.reserve(lCandidates.size());

        for (const Candidate& lCandidate : lCandidates)
        {
            lClasses.push_back(lCandidate.Class);
        }

        return lClasses;
    }
}
