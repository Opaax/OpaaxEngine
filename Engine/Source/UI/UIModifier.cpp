#include "UI/UIModifier.h"

#include "Object/ObjectClassRegistry.h"
#include "UI/UICanvas.h"
#include "UI/UIWidget.h"

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

    void UIModifier::WakeUp()
    {
        if (m_bAwake)
        {
            return;
        }

        m_bAwake = true;
        if (m_Owner != nullptr && m_Owner->GetCanvas() != nullptr)
        {
            m_Owner->GetCanvas()->AddAwake(*this);
        }
    }

    void UIModifier::Sleep()
    {
        if (!m_bAwake)
        {
            return;
        }

        m_bAwake = false;
        if (m_Owner != nullptr && m_Owner->GetCanvas() != nullptr)
        {
            m_Owner->GetCanvas()->RemoveAwake(*this);
        }
    }
}
