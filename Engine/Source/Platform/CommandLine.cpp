#include "Platform/CommandLine.h"

namespace Opaax
{
    CommandLine::CommandLine(TDynArray<OpaaxString> InArguments)
        : m_Arguments(Move(InArguments))
    {
    }

    bool CommandLine::HasFlag(const OpaaxStringView InFlag) const
    {
        for (Uint64 lIndex = 1; lIndex < m_Arguments.size(); ++lIndex)
        {
            if (m_Arguments[lIndex].View() == InFlag) { return true; }
        }
        return false;
    }

    const OpaaxString* CommandLine::FindValue(const OpaaxStringView InFlag) const
    {
        for (Uint64 lIndex = 1; lIndex + 1 < m_Arguments.size(); ++lIndex)
        {
            if (m_Arguments[lIndex].View() == InFlag) { return &m_Arguments[lIndex + 1]; }
        }
        return nullptr;
    }

    OpaaxString CommandLine::GetValue(const OpaaxStringView InFlag) const
    {
        const OpaaxString* lValue = FindValue(InFlag);
        return (lValue != nullptr) ? *lValue : OpaaxString();
    }
}
