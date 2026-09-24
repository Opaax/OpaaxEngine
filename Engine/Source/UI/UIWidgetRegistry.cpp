#include "UI/UIWidgetRegistry.h"

namespace Opaax
{
    bool UIWidgetRegistry::RegisterFactory(const OpaaxStringID InName, FWidgetFactory InFactory)
    {
        if (!InName.IsValid())
        {
            OPAAX_LOG(LogUIWidgetRegistry, Warn, "Register — an unnamed widget type was dropped.");
            return false;
        }

        if (m_bSealed)
        {
            OPAAX_LOG(LogUIWidgetRegistry, Warn, "Register '{}' — the registry is sealed; dropped.", InName);
            return false;
        }

        if (IsRegistered(InName))
        {
            OPAAX_LOG(LogUIWidgetRegistry, Warn, "Register '{}' — that name is taken; dropped.", InName);
            return false;
        }

        m_Names.emplace_back(InName);
        m_Factories.emplace_back(Move(InFactory));

        OPAAX_LOG(LogUIWidgetRegistry, Trace, "Registered UI widget '{}' ({} total)", InName, static_cast<Uint64>(m_Names.size()));
        return true;
    }

    void UIWidgetRegistry::Seal()
    {
        if (m_bSealed) { return; }

        m_bSealed = true;
        OPAAX_LOG(LogUIWidgetRegistry, Info, "Sealed with {} UI widget type(s).", static_cast<Uint64>(m_Names.size()));
    }

    TUniquePtr<UIWidget> UIWidgetRegistry::Create(const OpaaxStringID InName) const
    {
        for (Uint64 lIndex = 0; lIndex < m_Names.size(); ++lIndex)
        {
            if (m_Names[lIndex] == InName)
            {
                return m_Factories[lIndex]();
            }
        }

        return nullptr;
    }

    bool UIWidgetRegistry::IsRegistered(const OpaaxStringID InName) const noexcept
    {
        for (const OpaaxStringID& lName : m_Names)
        {
            if (lName == InName) { return true; }
        }

        return false;
    }
}
