#include "UI/UIModifier.h"

#include "Object/ObjectClassRegistry.h"

namespace Opaax
{
    TStrongObjectPtr<UIModifier> UIModifier::Create(const OpaaxStringID InClassName, const ObjectClassRegistry& InClasses)
    {
        const ObjectClass* lClass = InClasses.FindByName(InClassName);

        if (lClass == nullptr || lClass->IsAbstract() || !lClass->IsChildOf(StaticClass()))
        {
            return {};
        }

        return Cast<UIModifier>(lClass->CreateInstance());
    }
}
