#include "UI/UIWidgetNames.h"

#include "Core/String/UniqueName.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    namespace
    {
        bool Contains(const TDynArray<OpaaxString>& InNames, const OpaaxString& InName)
        {
            for (const OpaaxString& lName : InNames)
            {
                if (lName == InName) { return true; }
            }
            return false;
        }

        void NameSubtree(const UIWidget& InTree, UIWidget& InNew, TDynArray<OpaaxString>& InOutGiven)
        {
            if (!InNew.Name.IsEmpty())
            {
                InNew.Name = UniqueName::Make(InNew.Name, [&InTree, &InOutGiven](const OpaaxString& InCandidate)
                {
                    return Contains(InOutGiven, InCandidate) || UIWidgetNames::IsTaken(InTree, InCandidate);
                });
                InOutGiven.emplace_back(InNew.Name);
            }

            for (const TStrongObjectPtr<UIWidget>& lChild : InNew.GetChildren())
            {
                NameSubtree(InTree, *lChild, InOutGiven);
            }
        }

        void CollectNames(const UIWidget& InTree, TDynArray<OpaaxString>& OutSeen, TDynArray<OpaaxString>& OutDuplicates)
        {
            if (!InTree.Name.IsEmpty())
            {
                if (!Contains(OutSeen, InTree.Name))
                {
                    OutSeen.emplace_back(InTree.Name);
                }
                else if (!Contains(OutDuplicates, InTree.Name))
                {
                    OutDuplicates.emplace_back(InTree.Name);
                }
            }

            for (const TStrongObjectPtr<UIWidget>& lChild : InTree.GetChildren())
            {
                CollectNames(*lChild, OutSeen, OutDuplicates);
            }
        }
    }

    bool UIWidgetNames::IsTaken(const UIWidget& InTree, const OpaaxString& InName, const UIWidget* InExcept)
    {
        if (&InTree != InExcept && InTree.Name == InName) { return true; }

        for (const TStrongObjectPtr<UIWidget>& lChild : InTree.GetChildren())
        {
            if (IsTaken(*lChild, InName, InExcept)) { return true; }
        }
        return false;
    }

    void UIWidgetNames::NameUniquely(const UIWidget& InTree, UIWidget& InNew)
    {
        TDynArray<OpaaxString> lGiven;
        NameSubtree(InTree, InNew, lGiven);
    }

    TDynArray<OpaaxString> UIWidgetNames::FindDuplicates(const UIWidget& InTree)
    {
        TDynArray<OpaaxString> lSeen;
        TDynArray<OpaaxString> lDuplicates;
        CollectNames(InTree, lSeen, lDuplicates);
        return lDuplicates;
    }
}
